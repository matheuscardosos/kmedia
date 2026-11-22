#include "ApplicationController.h"

#include "core/BoothSecurity.h"
#include "core/DatabaseManager.h"
#include "core/MeetingCalendarModel.h"
#include "core/MeetingSections.h"
#include "core/MeetingTimer.h"
#include "core/NetworkManager.h"
#include "core/ProfileManager.h"
#include "core/RuntimeInfo.h"
#include "core/SettingsStore.h"
#include "core/SystemUtils.h"
#include "integrations/DisplayManager.h"
#include "integrations/ObsWebSocketClient.h"
#include "integrations/ProcessMonitor.h"
#include "integrations/ZoomBridge.h"
#include "media/BackgroundMusicEngine.h"
#include "media/MpvRendererItem.h"
#include "media/PdfViewerController.h"
#include "media/PlaybackController.h"
#include "media/ThumbnailCache.h"
#include "media/VideoFrameExtractor.h"
#include "models/MeetingDayModel.h"
#include "models/PlaylistModel.h"
#include "network/CatalogClient.h"
#include "network/DownloadService.h"
#include "network/MeetingCatalogService.h"
#include "network/MeetingSyncEngine.h"
#include "network/PublicationStore.h"
#include "parsers/CloudSyncWatcher.h"

#include <QCoreApplication>
#include <QDir>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>

bool ApplicationController::s_startupDemoMode = false;
QString ApplicationController::s_startupThemeOverride;
int ApplicationController::s_startupTabIndex = 0;

static void ensureTranslationsInitialized()
{
    Q_INIT_RESOURCE(kmedia_translations);
}

void ApplicationController::setStartupDemoMode(bool enabled)
{
    s_startupDemoMode = enabled;
}

void ApplicationController::setStartupThemeOverride(const QString& theme)
{
    s_startupThemeOverride = theme.trimmed().toLower();
}

void ApplicationController::setStartupTabIndex(int tabIndex)
{
    s_startupTabIndex = qBound(0, tabIndex, 1);
}

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent), m_translator(std::make_unique<QTranslator>()),
      m_profileManager(std::make_unique<kmedia::ProfileManager>()),
      m_networkManager(std::make_unique<kmedia::NetworkManager>()),
      m_thumbnailCache(std::make_unique<kmedia::ThumbnailCache>()),
      m_pdfController(std::make_unique<kmedia::PdfViewerController>()),
      m_playbackController(std::make_unique<kmedia::PlaybackController>()),
      m_backgroundMusic(std::make_unique<kmedia::BackgroundMusicEngine>()),
      m_displayManager(std::make_unique<kmedia::DisplayManager>()),
      m_downloadService(std::make_unique<kmedia::DownloadService>(m_networkManager.get())),
      m_catalogClient(std::make_unique<kmedia::CatalogClient>(m_networkManager.get())),
      m_cloudWatcher(std::make_unique<kmedia::CloudSyncWatcher>()),
      m_processMonitor(std::make_unique<kmedia::ProcessMonitor>()),
      m_obsClient(std::make_unique<kmedia::ObsWebSocketClient>()),
      m_zoomBridge(std::make_unique<kmedia::ZoomBridge>()),
      m_playlistModel(std::make_unique<kmedia::PlaylistModel>()),
      m_meetingDayModel(std::make_unique<kmedia::MeetingDayModel>(m_playlistModel.get())),
      m_meetingTimer(std::make_unique<kmedia::MeetingTimer>()),
      m_meetingCatalog(std::make_unique<kmedia::MeetingCatalogService>(
          m_catalogClient.get(), m_downloadService.get(), m_playlistModel.get())),
      m_calendarModel(std::make_unique<kmedia::MeetingCalendarModel>())
{
    m_publicationStore =
        std::make_unique<kmedia::PublicationStore>(m_catalogClient.get(), m_downloadService.get());
    m_syncEngine = std::make_unique<kmedia::MeetingSyncEngine>(m_publicationStore.get());
    m_playbackController->attachPdfController(m_pdfController.get());
    m_playlistModel->attachPlaybackController(m_playbackController.get());
    m_thumbnailCache->setCustomExtractor([](const QString& sourcePath, int) {
        return kmedia::VideoFrameExtractor::extract(sourcePath);
    });
    m_playlistModel->attachThumbnailCache(m_thumbnailCache.get());

    connect(m_profileManager.get(), &kmedia::ProfileManager::activeProfileChanged, this,
            [this](const QString&) {
                applyProfileSettings();
                emit activeProfileChanged();
            });
    connect(m_profileManager.get(), &kmedia::ProfileManager::profilesChanged, this,
            &ApplicationController::profilesChanged);

    wireIntegrations();

    if (s_startupThemeOverride == QStringLiteral("light")) {
        m_darkTheme = false;
    } else if (s_startupThemeOverride == QStringLiteral("dark")) {
        m_darkTheme = true;
    }

    if (s_startupDemoMode) {
        enterDemoMode();
        return;
    }

    if (m_profileManager->initialize(kmedia::SystemUtils::standardAppDataDir())) {
        const qint64 cacheLimitBytes =
            static_cast<qint64>(m_profileManager->activeSettings()->thumbnailCacheMaxMb()) *
            1024LL * 1024LL;
        const bool cacheReady = m_thumbnailCache->initialize(
            kmedia::SystemUtils::standardCacheDir() + QStringLiteral("/thumbnails"),
            cacheLimitBytes);
        Q_UNUSED(cacheReady)
        applyProfileSettings();
        m_processMonitor->startMonitoring();
    }
}

ApplicationController::~ApplicationController() = default;

void ApplicationController::attachQmlEngine(QQmlEngine* engine)
{
    m_qmlEngine = engine;
}

QString ApplicationController::appVersion() const
{
    return kmedia::RuntimeInfo::appVersion();
}

QString ApplicationController::qtVersion() const
{
    return kmedia::RuntimeInfo::qtVersion();
}

QString ApplicationController::mpvVersion() const
{
    return kmedia::RuntimeInfo::mpvVersion();
}

QString ApplicationController::activeProfileName() const
{
    const auto profile = m_profileManager->activeProfile();
    return profile ? profile->name : QStringLiteral("Default Congregation");
}

QString ApplicationController::storageAvailableText() const
{
    const auto profile = m_profileManager->activeProfile();
    const QString path = (profile && !profile->storagePath.isEmpty())
                             ? profile->storagePath
                             : kmedia::SystemUtils::standardAppDataDir();
    const qint64 freeBytes = kmedia::SystemUtils::availableDiskBytes(path);
    return kmedia::SystemUtils::formatFileSize(freeBytes);
}

QString ApplicationController::memoryUsage() const
{
    return kmedia::SystemUtils::formatFileSize(kmedia::SystemUtils::processMemoryRssBytes());
}

QString ApplicationController::uiLanguage() const
{
    return m_uiLanguage;
}

bool ApplicationController::isDarkTheme() const
{
    return m_darkTheme;
}

void ApplicationController::setDarkTheme(bool dark)
{
    if (m_darkTheme == dark) {
        return;
    }
    m_darkTheme = dark;
    if (!m_demoMode) {
        if (auto* settings = m_profileManager->activeSettings()) {
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kUiDarkTheme), dark);
        }
    }
    emit darkThemeChanged();
}

void ApplicationController::toggleDarkTheme()
{
    setDarkTheme(!m_darkTheme);
}

void ApplicationController::showToast(const QString& message, const QString& level)
{
    const QString cleaned = message.trimmed();
    if (!cleaned.isEmpty()) {
        emit toastRequested(cleaned, level);
    }
}

bool ApplicationController::isDemoMode() const
{
    return m_demoMode;
}

QStringList ApplicationController::availableProfileNames() const
{
    QStringList names;
    for (const auto& p : m_profileManager->profiles()) {
        names.append(p.name);
    }
    return names;
}

QStringList ApplicationController::availableProfileIds() const
{
    QStringList ids;
    for (const auto& p : m_profileManager->profiles()) {
        ids.append(p.id);
    }
    return ids;
}

kmedia::PlaybackController* ApplicationController::playback() const
{
    return m_playbackController.get();
}

kmedia::BackgroundMusicEngine* ApplicationController::backgroundMusic() const
{
    return m_backgroundMusic.get();
}

kmedia::DisplayManager* ApplicationController::displayManager() const
{
    return m_displayManager.get();
}

kmedia::PdfViewerController* ApplicationController::pdfController() const
{
    return m_pdfController.get();
}

kmedia::DownloadService* ApplicationController::downloadService() const
{
    return m_downloadService.get();
}

kmedia::CloudSyncWatcher* ApplicationController::cloudWatcher() const
{
    return m_cloudWatcher.get();
}

kmedia::ProcessMonitor* ApplicationController::processMonitor() const
{
    return m_processMonitor.get();
}

kmedia::ObsWebSocketClient* ApplicationController::obsClient() const
{
    return m_obsClient.get();
}

kmedia::ZoomBridge* ApplicationController::zoomBridge() const
{
    return m_zoomBridge.get();
}

kmedia::PlaylistModel* ApplicationController::playlist() const
{
    return m_playlistModel.get();
}

kmedia::MeetingDayModel* ApplicationController::meetingDay() const
{
    return m_meetingDayModel.get();
}

kmedia::MeetingTimer* ApplicationController::meetingTimer() const
{
    return m_meetingTimer.get();
}

kmedia::MeetingCatalogService* ApplicationController::catalog() const
{
    return m_meetingCatalog.get();
}

kmedia::MeetingCalendarModel* ApplicationController::calendar() const
{
    return m_calendarModel.get();
}

kmedia::MeetingSyncEngine* ApplicationController::syncEngine() const
{
    return m_syncEngine.get();
}

void ApplicationController::bindPresentationMpv(MpvRendererItem* mpvItem)
{
    m_playbackController->attachMpvItem(mpvItem);
}

bool ApplicationController::setUiLanguage(const QString& languageCode)
{
    const QString normalized = languageCode.trimmed().toLower().startsWith(QStringLiteral("pt"))
                                   ? QStringLiteral("pt")
                                   : QStringLiteral("en");

    if (auto* app = QCoreApplication::instance()) {
        app->removeTranslator(m_translator.get());
        if (normalized == QStringLiteral("pt")) {
            ensureTranslationsInitialized();
            if (!m_translator->load(QStringLiteral(":/i18n/kmedia_pt.qm"))) {
                return false;
            }
            app->installTranslator(m_translator.get());
        }
    }

    m_uiLanguage = normalized;
    if (!m_demoMode) {
        if (auto* settings = m_profileManager->activeSettings()) {
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kUiLanguage),
                                     normalized);
        }
        if (auto profile = m_profileManager->activeProfile(); profile.has_value()) {
            profile->uiLanguage = normalized;
            (void)m_profileManager->updateProfile(*profile);
        }
    }
    if (m_qmlEngine != nullptr) {
        m_qmlEngine->retranslate();
    }
    emit uiLanguageChanged();
    return true;
}

bool ApplicationController::switchProfile(const QString& profileId)
{
    return m_profileManager->switchProfile(profileId);
}

QString ApplicationController::createProfile(const QString& name, const QString& mediaLanguage)
{
    return m_profileManager->createProfile(name, mediaLanguage);
}

void ApplicationController::saveCloudFolder(const QString& folderPath)
{
    const QString trimmed = folderPath.trimmed();
    if (!m_demoMode) {
        if (auto* settings = m_profileManager->activeSettings()) {
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kCloudSyncFolders),
                                     trimmed);
        }
    }
    m_cloudWatcher->clearWatchedFolders();
    if (!trimmed.isEmpty()) {
        for (const QString& folder : trimmed.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            m_cloudWatcher->addWatchedFolder(folder.trimmed());
        }
    }
}

void ApplicationController::saveObsAndZoomSettings()
{
    syncAudioRouting();
    if (m_demoMode) {
        return;
    }
    auto* settings = m_profileManager->activeSettings();
    if (settings == nullptr) {
        return;
    }
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kObsEnabled),
                             m_obsClient->isEnabled());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kObsHost),
                             m_obsClient->host());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kObsPort),
                             m_obsClient->port());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kObsPassword),
                             m_obsClient->password());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kObsScenePlay),
                             m_obsClient->playScene());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kObsScenePause),
                             m_obsClient->pauseScene());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kObsSceneStop),
                             m_obsClient->stopScene());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kObsSceneStandby),
                             m_obsClient->standbyScene());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kZoomMeetingId),
                             m_zoomBridge->meetingId());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kZoomMeetingPass),
                             m_zoomBridge->meetingPassword());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kZoomDisplayName),
                             m_zoomBridge->displayName());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kZoomAudioDevice),
                             m_zoomBridge->dedicatedAudioDevice());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kZoomAutoShare),
                             m_zoomBridge->isAutoShareOnPlayback());
    if (auto profile = m_profileManager->activeProfile(); profile.has_value()) {
        profile->obsHost = m_obsClient->host();
        profile->obsPort = m_obsClient->port();
        profile->zoomMeetingId = m_zoomBridge->meetingId();
        (void)m_profileManager->updateProfile(*profile);
    }
}

void ApplicationController::saveFadeDurationMs(int ms)
{
    const int cleanMs = qBound(500, ms, 30000);
    m_backgroundMusic->setFadeDurationMs(cleanMs);
    if (!m_demoMode) {
        if (auto* settings = m_profileManager->activeSettings()) {
            (void)settings->setValue(
                QLatin1StringView(kmedia::settings_keys::kBackgroundMusicFadeMs), cleanMs);
        }
    }
}

int ApplicationController::startupTabIndex() const
{
    return s_startupTabIndex;
}

bool ApplicationController::isBoothPinEnabled() const
{
    return !m_boothPinHash.isEmpty();
}

bool ApplicationController::isBoothUnlocked() const
{
    return m_boothUnlocked || m_boothPinHash.isEmpty();
}

bool ApplicationController::enableBoothPin(const QString& newPin)
{
    if (!kmedia::BoothSecurity::isValidPinFormat(newPin)) {
        return false;
    }
    const QString salt = kmedia::BoothSecurity::generateSalt();
    const QString hash = kmedia::BoothSecurity::hashPin(newPin, salt);
    if (hash.isEmpty()) {
        return false;
    }
    m_boothPinSalt = salt;
    m_boothPinHash = hash;
    m_boothUnlocked = true;
    if (!m_demoMode) {
        if (auto* settings = m_profileManager->activeSettings()) {
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kSecurityPinSalt),
                                     salt);
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kSecurityPinHash),
                                     hash);
        }
    }
    emit boothLockChanged();
    return true;
}

bool ApplicationController::disableBoothPin(const QString& currentPin)
{
    if (!isBoothPinEnabled()) {
        return true;
    }
    if (!kmedia::BoothSecurity::verifyPin(currentPin, m_boothPinSalt, m_boothPinHash)) {
        return false;
    }
    m_boothPinSalt.clear();
    m_boothPinHash.clear();
    m_boothUnlocked = true;
    if (!m_demoMode) {
        if (auto* settings = m_profileManager->activeSettings()) {
            (void)settings->remove(QLatin1StringView(kmedia::settings_keys::kSecurityPinSalt));
            (void)settings->remove(QLatin1StringView(kmedia::settings_keys::kSecurityPinHash));
        }
    }
    emit boothLockChanged();
    return true;
}

bool ApplicationController::unlockBoothSettings(const QString& candidatePin)
{
    if (!isBoothPinEnabled()) {
        m_boothUnlocked = true;
        emit boothLockChanged();
        return true;
    }
    if (!kmedia::BoothSecurity::verifyPin(candidatePin, m_boothPinSalt, m_boothPinHash)) {
        return false;
    }
    m_boothUnlocked = true;
    emit boothLockChanged();
    return true;
}

void ApplicationController::lockBoothSettings()
{
    if (!isBoothPinEnabled() || !m_boothUnlocked) {
        return;
    }
    m_boothUnlocked = false;
    emit boothLockChanged();
}

void ApplicationController::enterDemoMode()
{
    m_demoSandbox = std::make_unique<QTemporaryDir>();
    m_demoSandbox->setAutoRemove(true);
    m_demoMode = true;

    const QString sandboxRoot = m_demoSandbox->path();
    (void)m_thumbnailCache->initialize(QDir(sandboxRoot).filePath(QStringLiteral("thumbs")),
                                       32LL * 1024LL * 1024LL);
    (void)m_profileManager->initialize(sandboxRoot);
    const QString demoProfileId =
        m_profileManager->createProfile(QStringLiteral("Riverside Congregation"));
    if (!demoProfileId.isEmpty()) {
        (void)m_profileManager->switchProfile(demoProfileId);
    }

    m_playlistModel->attachDatabase(m_profileManager->activeDatabase());
    m_playlistModel->populateDemoWorkspace(
        QDir(sandboxRoot).filePath(QStringLiteral("demo-media")));
    m_processMonitor->applySimulatedState(true, true);

    emit demoModeChanged();
    emit activeProfileChanged();
    emit profilesChanged();
}

void ApplicationController::applyProfileSettings()
{
    m_downloadService->setDatabaseManager(m_profileManager->activeDatabase());
    m_catalogClient->setDatabaseManager(m_profileManager->activeDatabase());
    if (!m_demoMode) {
        m_playlistModel->attachDatabase(m_profileManager->activeDatabase());
    }

    const auto profile = m_profileManager->activeProfile();
    if (profile.has_value() && !profile->storagePath.isEmpty()) {
        m_playlistModel->setPackageExtractDir(
            QDir(profile->storagePath).filePath(QStringLiteral("packages")));
        m_meetingCatalog->setStorageRoot(profile->storagePath);
        m_publicationStore->setCacheRoot(profile->storagePath);
        m_calendarModel->setStorageRoot(profile->storagePath);
    }

    const auto* settings = m_profileManager->activeSettings();
    if (settings == nullptr) {
        return;
    }

    const QString mediaLang = settings->mediaLanguage();
    if (!mediaLang.isEmpty()) {
        m_meetingCatalog->setSelectedLanguageCode(mediaLang);
        m_calendarModel->setMediaLanguage(mediaLang);
    }
    const QString fallbackLang = settings->stringValue(
        QLatin1StringView(kmedia::settings_keys::kMediaFallbackLanguage), QStringLiteral("E"));
    m_meetingCatalog->setFallbackLanguageCode(fallbackLang);
    m_meetingCatalog->setIncludePrinted(settings->boolValue(
        QLatin1StringView(kmedia::settings_keys::kFilterIncludePrinted), false));
    m_meetingCatalog->setExcludeFootnotes(settings->boolValue(
        QLatin1StringView(kmedia::settings_keys::kFilterExcludeFootnotes), true));
    m_meetingCatalog->setExcludeWtParagraphVideos(settings->boolValue(
        QLatin1StringView(kmedia::settings_keys::kFilterExcludeWtParagraphVideos), false));
    m_meetingCatalog->setExcludeTh(
        settings->boolValue(QLatin1StringView(kmedia::settings_keys::kFilterExcludeTh), false));
    m_meetingCatalog->setExcludeCbsPubs(settings->boolValue(
        QLatin1StringView(kmedia::settings_keys::kFilterExcludeCbsPubs), false));
    m_meetingCatalog->setAutoLoadWeekMedia(
        settings->boolValue(QLatin1StringView(kmedia::settings_keys::kAutoLoadWeekMedia), true));

    const int midDay =
        settings->intValue(QLatin1StringView(kmedia::settings_keys::kMeetingMidweekDay), 3);
    const QString midTime = settings->stringValue(
        QLatin1StringView(kmedia::settings_keys::kMeetingMidweekTime),
        profile.has_value() ? profile->midweekSchedule : QStringLiteral("19:30"));
    const int endDay =
        settings->intValue(QLatin1StringView(kmedia::settings_keys::kMeetingWeekendDay), 7);
    const QString endTime = settings->stringValue(
        QLatin1StringView(kmedia::settings_keys::kMeetingWeekendTime),
        profile.has_value() ? profile->weekendSchedule : QStringLiteral("09:30"));
    m_calendarModel->setScheduleDays(midDay, midTime, endDay, endTime);
    m_calendarModel->loadSpecialEventsJson(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kMeetingSpecialEvents)));
    m_calendarModel->loadTempSchedulesJson(settings->stringValue(
        QLatin1StringView(kmedia::settings_keys::kMeetingTempScheduleChanges)));
    m_meetingCatalog->setSpecialWeekEvents(m_calendarModel->specialEventsMap());
    m_syncEngine->setDatabase(m_demoMode ? nullptr : m_profileManager->activeDatabase());
    m_syncEngine->setScheduleProviders(
        [this](const QDate& date) {
            return m_calendarModel->meetingKindForDate(date.toString(Qt::ISODate));
        },
        [this](const QDate& date) {
            return m_calendarModel->eventTypeForDate(date.toString(Qt::ISODate)) ==
                   QStringLiteral("co_visit");
        });
    applySyncSettings(false);
    if (!m_demoMode) {
        // The meeting media page opens on the first day from today on with a meeting or media.
        m_syncEngine->refreshAddedMediaDays();
        const QString firstDay =
            m_syncEngine->dayWithContent(QDate::currentDate().toString(Qt::ISODate), 0);
        if (!firstDay.isEmpty()) {
            m_calendarModel->selectDate(firstDay);
        }
        const QString dateIso = m_calendarModel->selectedDateIso();
        const QString kind = m_calendarModel->meetingKindForDate(dateIso);
        const bool isCo =
            (m_calendarModel->eventTypeForDate(dateIso) == QStringLiteral("co_visit"));
        (void)m_playlistModel->openWorkspaceForDate(dateIso, kind, {}, isCo);
        m_meetingCatalog->setTargetMeetingContext(m_calendarModel->selectedDate(), kind);
        QTimer::singleShot(0, m_syncEngine.get(), &kmedia::MeetingSyncEngine::refresh);
    }

    m_backgroundMusic->configureSchedules(
        kmedia::MeetingScheduleSpec{
            true, m_calendarModel->midweekDay(),
            QTime::fromString(m_calendarModel->midweekTime(), QStringLiteral("HH:mm"))},
        kmedia::MeetingScheduleSpec{
            true, m_calendarModel->weekendDay(),
            QTime::fromString(m_calendarModel->weekendTime(), QStringLiteral("HH:mm"))},
        settings->backgroundMusicFadeMs());

    m_backgroundMusic->setFadeDurationMs(settings->backgroundMusicFadeMs());
    m_downloadService->setMaxConcurrent(settings->maxConcurrentDownloads());

    m_obsClient->setHost(settings->stringValue(QLatin1StringView(kmedia::settings_keys::kObsHost),
                                               QStringLiteral("127.0.0.1")));
    const int obsPort =
        settings->intValue(QLatin1StringView(kmedia::settings_keys::kObsPort), 4455);
    m_obsClient->setPort(obsPort > 0 ? obsPort : 4455);
    m_obsClient->setPassword(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kObsPassword)));
    m_obsClient->setPlayScene(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kObsScenePlay)));
    m_obsClient->setPauseScene(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kObsScenePause)));
    m_obsClient->setStopScene(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kObsSceneStop)));
    m_obsClient->setStandbyScene(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kObsSceneStandby)));
    m_obsClient->setEnabled(
        settings->boolValue(QLatin1StringView(kmedia::settings_keys::kObsEnabled), false));

    m_zoomBridge->setMeetingId(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kZoomMeetingId)));
    m_zoomBridge->setMeetingPassword(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kZoomMeetingPass)));
    m_zoomBridge->setDisplayName(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kZoomDisplayName)));
    m_zoomBridge->setDedicatedAudioDevice(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kZoomAudioDevice)));
    m_zoomBridge->setAutoShareOnPlayback(
        settings->boolValue(QLatin1StringView(kmedia::settings_keys::kZoomAutoShare), false));

    syncAudioRouting();

    if (!m_demoMode) {
        const QString savedLang = settings->uiLanguage();
        if (!savedLang.isEmpty() && savedLang != m_uiLanguage) {
            (void)setUiLanguage(savedLang);
        }
        if (s_startupThemeOverride.isEmpty()) {
            const bool savedDark =
                settings->boolValue(QLatin1StringView(kmedia::settings_keys::kUiDarkTheme), true);
            if (savedDark != m_darkTheme) {
                m_darkTheme = savedDark;
                emit darkThemeChanged();
            }
        }
    }

    m_cloudWatcher->clearWatchedFolders();
    const QString rawFolders =
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kCloudSyncFolders));
    if (!rawFolders.isEmpty()) {
        for (const QString& folder : rawFolders.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            m_cloudWatcher->addWatchedFolder(folder.trimmed());
        }
    }

    m_boothPinSalt =
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kSecurityPinSalt));
    m_boothPinHash =
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kSecurityPinHash));
    m_boothUnlocked = m_boothPinHash.isEmpty();
    emit boothLockChanged();
}

void ApplicationController::syncAudioRouting()
{
    const auto* settings = m_profileManager->activeSettings();
    const QString generalDevice =
        (settings != nullptr)
            ? settings->stringValue(QLatin1StringView(kmedia::settings_keys::kAudioOutputDevice))
            : QString{};
    const QString zoomDevice = m_zoomBridge->dedicatedAudioDevice().trimmed();
    const QString playbackTarget = zoomDevice.isEmpty() ? generalDevice : zoomDevice;
    const QString musicTarget = generalDevice.isEmpty() ? zoomDevice : generalDevice;

    m_playbackController->setAudioDevice(playbackTarget.isEmpty() ? QStringLiteral("auto")
                                                                  : playbackTarget);
    if (!musicTarget.isEmpty()) {
        m_backgroundMusic->setAudioDevice(musicTarget);
    }
}

void ApplicationController::wireIntegrations()
{
    connect(m_syncEngine.get(), &kmedia::MeetingSyncEngine::dayContentChanged, this,
            [this](const QString& dateIso) {
                if (m_playlistModel->currentDateIso() == dateIso) {
                    m_playlistModel->reloadFromDatabase();
                }
                m_calendarModel->refreshReadiness();
            });
    // Adding or removing media changes which days the date picker marks.
    connect(m_playlistModel.get(), &kmedia::PlaylistModel::countChanged, m_syncEngine.get(),
            &kmedia::MeetingSyncEngine::refreshAddedMediaDays);
    for (auto signal : {&kmedia::MeetingCatalogService::selectedLanguageChanged,
                        &kmedia::MeetingCatalogService::fallbackLanguageChanged,
                        &kmedia::MeetingCatalogService::filtersChanged}) {
        connect(m_meetingCatalog.get(), signal, this, [this]() { applySyncSettings(true); });
    }
    for (auto signal : {&kmedia::MeetingCalendarModel::scheduleChanged,
                        &kmedia::MeetingCalendarModel::specialEventsChanged,
                        &kmedia::MeetingCalendarModel::tempSchedulesChanged}) {
        connect(m_calendarModel.get(), signal, this, [this]() {
            if (!m_demoMode) {
                m_syncEngine->refresh();
            }
        });
    }

    connect(m_processMonitor.get(), &kmedia::ProcessMonitor::statusChanged, this, [this]() {
        m_obsClient->setProcessRunning(m_processMonitor->isObsRunning());
        m_zoomBridge->setZoomRunning(m_processMonitor->isZoomRunning());
    });

    connect(m_zoomBridge.get(), &kmedia::ZoomBridge::audioDeviceChanged, this,
            &ApplicationController::syncAudioRouting);

    connect(m_zoomBridge.get(), &kmedia::ZoomBridge::shareTriggerRequested, this,
            [this](const QString& shortcutHint, const QString& targetWindowTitle) {
                showToast(
                    tr("Zoom screen share ready (%1): %2").arg(shortcutHint, targetWindowTitle),
                    QStringLiteral("info"));
            });

    auto syncPlaybackIntegrations = [this]() {
        const bool hasMedia =
            m_playbackController->mediaKind() != kmedia::PlaybackController::MediaKind::None;
        const bool activePresentation = m_playbackController->isActivePresentation();
        const bool effectivelyPaused = hasMedia && !activePresentation;
        const bool standby = m_playbackController->isStandbyActive();
        m_obsClient->applyPlaybackState(hasMedia, effectivelyPaused, standby);
        if (activePresentation && !standby) {
            m_zoomBridge->notifyPresentationPlaybackStarted();
        }
    };

    connect(m_playbackController.get(), &kmedia::PlaybackController::mediaChanged, this,
            syncPlaybackIntegrations);
    connect(m_playbackController.get(), &kmedia::PlaybackController::transportChanged, this,
            syncPlaybackIntegrations);
    connect(m_playbackController.get(), &kmedia::PlaybackController::standbyChanged, this,
            syncPlaybackIntegrations);

    connect(m_cloudWatcher.get(), &kmedia::CloudSyncWatcher::mediaDiscovered, this,
            [this](const QString& filePath, kmedia::CloudSyncWatcher::MediaCategory, const QString&,
                   kmedia::CloudSyncWatcher::CloudProvider) {
                (void)m_playlistModel->importLocalFile(filePath);
            });

    connect(m_playlistModel.get(), &kmedia::PlaylistModel::importNotice, this,
            [this](const QString& message) { showToast(message, QStringLiteral("warning")); });

    connect(m_meetingCatalog.get(), &kmedia::MeetingCatalogService::selectedLanguageChanged, this,
            [this]() {
                if (m_demoMode) {
                    return;
                }
                if (auto* settings = m_profileManager->activeSettings()) {
                    (void)settings->setValue(
                        QLatin1StringView(kmedia::settings_keys::kMediaLanguage),
                        m_meetingCatalog->selectedLanguageCode());
                }
                if (auto profile = m_profileManager->activeProfile(); profile.has_value()) {
                    profile->mediaLanguage = m_meetingCatalog->selectedLanguageCode();
                    (void)m_profileManager->updateProfile(*profile);
                }
                m_calendarModel->setMediaLanguage(m_meetingCatalog->selectedLanguageCode());
            });
    connect(m_meetingCatalog.get(), &kmedia::MeetingCatalogService::fallbackLanguageChanged, this,
            [this]() {
                if (m_demoMode) {
                    return;
                }
                if (auto* settings = m_profileManager->activeSettings()) {
                    (void)settings->setValue(
                        QLatin1StringView(kmedia::settings_keys::kMediaFallbackLanguage),
                        m_meetingCatalog->fallbackLanguageCode());
                }
            });
    connect(m_meetingCatalog.get(), &kmedia::MeetingCatalogService::filtersChanged, this, [this]() {
        if (m_demoMode) {
            return;
        }
        if (auto* settings = m_profileManager->activeSettings()) {
            (void)settings->setValue(
                QLatin1StringView(kmedia::settings_keys::kFilterIncludePrinted),
                m_meetingCatalog->includePrinted());
            (void)settings->setValue(
                QLatin1StringView(kmedia::settings_keys::kFilterExcludeFootnotes),
                m_meetingCatalog->excludeFootnotes());
            (void)settings->setValue(
                QLatin1StringView(kmedia::settings_keys::kFilterExcludeWtParagraphVideos),
                m_meetingCatalog->excludeWtParagraphVideos());
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kFilterExcludeTh),
                                     m_meetingCatalog->excludeTh());
            (void)settings->setValue(
                QLatin1StringView(kmedia::settings_keys::kFilterExcludeCbsPubs),
                m_meetingCatalog->excludeCbsPubs());
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kAutoLoadWeekMedia),
                                     m_meetingCatalog->autoLoadWeekMedia());
        }
    });
    connect(
        m_meetingCatalog.get(), &kmedia::MeetingCatalogService::congregationScheduleApplied, this,
        [this](const QString& name, const QString&, int midDay, const QString& midTime, int endDay,
               const QString& endTime) {
            m_calendarModel->setScheduleDays(midDay, midTime, endDay, endTime);
            showToast(tr("Applied congregation schedule: %1").arg(name), QStringLiteral("success"));
        });
    connect(m_meetingCatalog.get(), &kmedia::MeetingCatalogService::weekSynced, this,
            [this](int importedCount) {
                m_calendarModel->refreshReadiness();
                showToast(tr("Synced %1 week item(s) into playlist").arg(importedCount),
                          QStringLiteral("success"));
            });
    connect(m_meetingCatalog.get(), &kmedia::MeetingCatalogService::catalogError, this,
            [this](const QString& errorMessage) {
                showToast(errorMessage, QStringLiteral("warning"));
            });

    connect(m_downloadService.get(), &kmedia::DownloadService::taskFinished, this,
            [this](const QString&, bool success) {
                if (success) {
                    m_calendarModel->refreshReadiness();
                }
            });

    connect(m_calendarModel.get(), &kmedia::MeetingCalendarModel::scheduleChanged, this, [this]() {
        m_backgroundMusic->configureSchedules(
            kmedia::MeetingScheduleSpec{
                true, m_calendarModel->midweekDay(),
                QTime::fromString(m_calendarModel->midweekTime(), QStringLiteral("HH:mm"))},
            kmedia::MeetingScheduleSpec{
                true, m_calendarModel->weekendDay(),
                QTime::fromString(m_calendarModel->weekendTime(), QStringLiteral("HH:mm"))},
            m_backgroundMusic->fadeDurationMs());
        if (m_demoMode) {
            return;
        }
        if (auto* settings = m_profileManager->activeSettings()) {
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kMeetingMidweekDay),
                                     m_calendarModel->midweekDay());
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kMeetingMidweekTime),
                                     m_calendarModel->midweekTime());
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kMeetingWeekendDay),
                                     m_calendarModel->weekendDay());
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kMeetingWeekendTime),
                                     m_calendarModel->weekendTime());
        }
        if (auto profile = m_profileManager->activeProfile(); profile.has_value()) {
            profile->midweekSchedule = m_calendarModel->midweekTime();
            profile->weekendSchedule = m_calendarModel->weekendTime();
            (void)m_profileManager->updateProfile(*profile);
        }
    });

    connect(m_calendarModel.get(), &kmedia::MeetingCalendarModel::specialEventsChanged, this,
            [this]() {
                m_meetingCatalog->setSpecialWeekEvents(m_calendarModel->specialEventsMap());
                if (m_demoMode) {
                    return;
                }
                if (auto* settings = m_profileManager->activeSettings()) {
                    (void)settings->setValue(
                        QLatin1StringView(kmedia::settings_keys::kMeetingSpecialEvents),
                        m_calendarModel->exportSpecialEventsJson());
                }
            });

    connect(m_calendarModel.get(), &kmedia::MeetingCalendarModel::tempSchedulesChanged, this,
            [this]() {
                if (m_demoMode) {
                    return;
                }
                if (auto* settings = m_profileManager->activeSettings()) {
                    (void)settings->setValue(
                        QLatin1StringView(kmedia::settings_keys::kMeetingTempScheduleChanges),
                        m_calendarModel->exportTempSchedulesJson());
                }
            });

    connect(m_calendarModel.get(), &kmedia::MeetingCalendarModel::daySelected, this,
            [this](const QDate& date, const QString& meetingKind, bool isCoVisit) {
                const QString dateIso = date.toString(Qt::ISODate);
                (void)m_playlistModel->openWorkspaceForDate(dateIso, meetingKind, {}, isCoVisit);
                m_meetingCatalog->setTargetMeetingContext(date, meetingKind);
            });

    connect(m_calendarModel.get(), &kmedia::MeetingCalendarModel::weekOpenRequested, this,
            [this](const QDate& weekMonday, bool syncNow) {
                const QString dateIso = m_calendarModel->selectedDateIso();
                const QString kind = m_calendarModel->meetingKindForDate(dateIso);
                m_meetingCatalog->setTargetMeetingContext(m_calendarModel->selectedDate(), kind);
                if (syncNow) {
                    m_syncEngine->resyncDay(dateIso);
                } else if (!m_calendarModel->selectedWeekEventType().isEmpty()) {
                    m_playlistModel->setPlaylistSubtitle(
                        QStringLiteral("%1 | %2").arg(m_calendarModel->selectedWeekLabel(),
                                                      m_calendarModel->selectedWeekEventLabel()));
                    if (m_calendarModel->selectedWeekEventType() == QStringLiteral("co_visit")) {
                        (void)m_playlistModel->ensureSection(
                            kmedia::MeetingSections::kCircuitOverseer);
                    }
                } else {
                    m_meetingCatalog->setTargetWeekDate(weekMonday);
                }
            });
}

void ApplicationController::applySyncSettings(bool resetWhenChanged)
{
    const kmedia::MeetingSyncEngine::Settings current = m_syncEngine->settings();
    kmedia::MeetingSyncEngine::Settings next = current;
    next.languageCode = m_meetingCatalog->selectedLanguageCode();
    next.fallbackLanguageCode = m_meetingCatalog->fallbackLanguageCode();
    next.includePrinted = m_meetingCatalog->includePrinted();
    next.excludeFootnotes = m_meetingCatalog->excludeFootnotes();
    next.excludeWtParagraphVideos = m_meetingCatalog->excludeWtParagraphVideos();
    next.excludeTh = m_meetingCatalog->excludeTh();
    next.excludeCbsVideos = m_meetingCatalog->excludeCbsPubs();
    next.signLanguage = m_meetingCatalog->isSignLanguage(next.languageCode);
    // Sign languages publish their songs in their own songbook.
    next.songbookSymbol = next.signLanguage ? QStringLiteral("sjj") : QStringLiteral("sjjm");
    // Only a real change of what gets synced resets the synced media of the window.
    const bool mediaChanged = next.languageCode != current.languageCode ||
                              next.fallbackLanguageCode != current.fallbackLanguageCode ||
                              next.includePrinted != current.includePrinted ||
                              next.excludeFootnotes != current.excludeFootnotes ||
                              next.excludeWtParagraphVideos != current.excludeWtParagraphVideos ||
                              next.excludeTh != current.excludeTh ||
                              next.excludeCbsVideos != current.excludeCbsVideos;
    m_syncEngine->setSettings(next);
    if (resetWhenChanged && mediaChanged && !m_demoMode) {
        m_syncEngine->resetAll();
    }
}

void ApplicationController::openDateWorkspace(const QString& isoDate, bool forceSync)
{
    m_calendarModel->selectDate(isoDate);
    const QString effectiveIso = m_calendarModel->selectedDateIso();
    const QString kind = m_calendarModel->meetingKindForDate(effectiveIso);
    const bool isCoVisit =
        (m_calendarModel->eventTypeForDate(effectiveIso) == QStringLiteral("co_visit"));
    (void)m_playlistModel->openWorkspaceForDate(effectiveIso, kind, {}, isCoVisit);
    m_meetingCatalog->setTargetMeetingContext(m_calendarModel->selectedDate(), kind);

    if (!m_demoMode && forceSync) {
        m_syncEngine->resyncDay(effectiveIso);
    }
}
