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
#include "media/BackgroundMusicService.h"
#include "media/CameraController.h"
#include "media/MediaAutoExport.h"
#include "media/MpvRendererItem.h"
#include "media/PdfPageImporter.h"
#include "media/PdfViewerController.h"
#include "media/PlaybackController.h"
#include "media/ThumbnailCache.h"
#include "media/VideoFrameExtractor.h"
#include "models/MeetingDayModel.h"
#include "models/PlaylistModel.h"
#include "network/BackgroundMusicLibrary.h"
#include "network/BibleMediaCatalog.h"
#include "network/CatalogClient.h"
#include "network/DocumentMediaImporter.h"
#include "network/DownloadService.h"
#include "network/JwVideoCatalog.h"
#include "network/MeetingCatalogService.h"
#include "network/MeetingSyncEngine.h"
#include "network/PublicTalkMedia.h"
#include "network/PublicationMediaPicker.h"
#include "network/PublicationStore.h"
#include "network/StudyBibleMedia.h"
#include "network/WatchedFolderMedia.h"
#include "network/YeartextProvider.h"
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
      m_meetingCatalog(std::make_unique<kmedia::MeetingCatalogService>(m_catalogClient.get())),
      m_calendarModel(std::make_unique<kmedia::MeetingCalendarModel>())
{
    m_publicationStore =
        std::make_unique<kmedia::PublicationStore>(m_catalogClient.get(), m_downloadService.get());
    m_syncEngine = std::make_unique<kmedia::MeetingSyncEngine>(m_publicationStore.get());
    m_documentImporter = std::make_unique<kmedia::DocumentMediaImporter>(m_publicationStore.get(),
                                                                         m_playlistModel.get());
    m_publicTalks = std::make_unique<kmedia::PublicTalkMedia>(m_publicationStore.get(),
                                                              m_documentImporter.get());
    m_publicationPicker = std::make_unique<kmedia::PublicationMediaPicker>(
        m_publicationStore.get(), m_documentImporter.get());
    m_pdfPages = std::make_unique<kmedia::PdfPageImporter>(m_playlistModel.get());
    m_yeartext = std::make_unique<kmedia::YeartextProvider>(m_catalogClient.get());
    m_camera = std::make_unique<kmedia::CameraController>();
    connect(m_camera.get(), &kmedia::CameraController::cameraIdChanged, this, [this]() {
        auto* settings = m_demoMode ? nullptr : m_profileManager->activeSettings();
        const auto key = QLatin1StringView(kmedia::settings_keys::kPresentationCamera);
        if (settings != nullptr && settings->stringValue(key) != m_camera->cameraId()) {
            (void)settings->setValue(key, m_camera->cameraId());
        }
    });
    m_studyBible = std::make_unique<kmedia::StudyBibleMedia>(m_publicationStore.get(),
                                                             m_documentImporter.get());
    m_bibleCatalog = std::make_unique<kmedia::BibleMediaCatalog>(
        m_catalogClient.get(), m_publicationStore.get(), m_documentImporter.get());
    m_videoCatalog = std::make_unique<kmedia::JwVideoCatalog>(
        m_catalogClient.get(), m_publicationStore.get(), m_documentImporter.get());
    m_musicLibrary = std::make_unique<kmedia::BackgroundMusicLibrary>(m_catalogClient.get(),
                                                                      m_downloadService.get());
    m_musicService = std::make_unique<kmedia::BackgroundMusicService>(m_backgroundMusic.get(),
                                                                      m_musicLibrary.get());
    m_musicService->setMeetingProvider(
        [this](const QDate& date) -> std::optional<kmedia::BackgroundMusicService::Meeting> {
            const QDateTime start = m_calendarModel->meetingStartForDate(date);
            if (!start.isValid()) {
                return std::nullopt;
            }
            kmedia::BackgroundMusicService::Meeting meeting;
            meeting.start = start;
            if (const auto* db = m_profileManager->activeDatabase(); db != nullptr) {
                meeting.songTracks = db->listSongNumbersForDate(date.toString(Qt::ISODate));
            }
            return meeting;
        });
    connect(m_musicService.get(), &kmedia::BackgroundMusicService::startFailed, this, [this]() {
        showToast(tr("The songbook could not be loaded. Check the internet connection."),
                  QStringLiteral("warning"));
    });
    m_watchedFolder =
        std::make_unique<kmedia::WatchedFolderMedia>(m_cloudWatcher.get(), m_playlistModel.get());
    m_autoExport = std::make_unique<kmedia::MediaAutoExport>();
    m_autoExport->setSongLabel(tr("Song"));
    // Edits settle for a second before the open day is exported again.
    m_exportTimer = new QTimer(this);
    m_exportTimer->setSingleShot(true);
    m_exportTimer->setInterval(1000);
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

kmedia::BackgroundMusicService* ApplicationController::music() const
{
    return m_musicService.get();
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

kmedia::PublicTalkMedia* ApplicationController::publicTalks() const
{
    return m_publicTalks.get();
}

kmedia::PublicationMediaPicker* ApplicationController::publicationMedia() const
{
    return m_publicationPicker.get();
}

kmedia::JwVideoCatalog* ApplicationController::videos() const
{
    return m_videoCatalog.get();
}

kmedia::BibleMediaCatalog* ApplicationController::bible() const
{
    return m_bibleCatalog.get();
}

kmedia::StudyBibleMedia* ApplicationController::studyBible() const
{
    return m_studyBible.get();
}

kmedia::YeartextProvider* ApplicationController::yeartext() const
{
    return m_yeartext.get();
}

kmedia::CameraController* ApplicationController::camera() const
{
    return m_camera.get();
}

kmedia::PdfPageImporter* ApplicationController::pdfPages() const
{
    return m_pdfPages.get();
}

kmedia::WatchedFolderMedia* ApplicationController::watchedFolder() const
{
    return m_watchedFolder.get();
}

kmedia::MediaAutoExport* ApplicationController::autoExport() const
{
    return m_autoExport.get();
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
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kObsSceneStage),
                             m_obsClient->stageScene());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kObsSceneMedia),
                             m_obsClient->mediaScene());
    (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kObsSceneImage),
                             m_obsClient->imageScene());
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

void ApplicationController::saveAudioOutputDevice(const QString& deviceName)
{
    const QString cleaned =
        deviceName.trimmed() == QStringLiteral("auto") ? QString{} : deviceName.trimmed();
    if (!m_demoMode) {
        if (auto* settings = m_profileManager->activeSettings()) {
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kAudioOutputDevice),
                                     cleaned);
        }
    }
    syncAudioRouting();
}

void ApplicationController::saveMusicAutoStart(bool enabled)
{
    m_musicService->setAutoStart(enabled);
    if (!m_demoMode) {
        if (auto* settings = m_profileManager->activeSettings()) {
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kMusicAutoStart),
                                     enabled);
        }
    }
}

void ApplicationController::saveStandbyImage(const QUrl& imageUrl)
{
    const QString path = imageUrl.isLocalFile() ? imageUrl.toLocalFile() : QString{};
    m_playbackController->setStandbyImageUrl(path.isEmpty() ? QUrl{} : QUrl::fromLocalFile(path));
    if (!m_demoMode) {
        if (auto* settings = m_profileManager->activeSettings()) {
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kStandbyImage), path);
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
        m_publicationStore->setCacheRoot(profile->storagePath);
        m_musicLibrary->setCacheRoot(profile->storagePath);
        m_pdfPages->setCacheRoot(profile->storagePath);
        // Each congregation keeps its own S-34mp, unpacked once.
        m_publicTalks->setFolder(
            QDir(profile->storagePath).filePath(QStringLiteral("Publications/S-34")));
    }

    const auto* settings = m_profileManager->activeSettings();
    if (settings == nullptr) {
        return;
    }

    const QString mediaLang = settings->mediaLanguage();
    if (!mediaLang.isEmpty()) {
        m_meetingCatalog->setSelectedLanguageCode(mediaLang);
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
    // Like Meeting Media Manager, music starts by itself before meetings unless turned off.
    m_musicService->setAutoStart(
        !m_demoMode &&
        settings->boolValue(QLatin1StringView(kmedia::settings_keys::kMusicAutoStart), true));
    m_downloadService->setMaxConcurrent(settings->maxConcurrentDownloads());

    m_obsClient->setHost(settings->stringValue(QLatin1StringView(kmedia::settings_keys::kObsHost),
                                               QStringLiteral("127.0.0.1")));
    const int obsPort =
        settings->intValue(QLatin1StringView(kmedia::settings_keys::kObsPort), 4455);
    m_obsClient->setPort(obsPort > 0 ? obsPort : 4455);
    m_obsClient->setPassword(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kObsPassword)));
    m_obsClient->setStageScene(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kObsSceneStage)));
    m_obsClient->setMediaScene(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kObsSceneMedia)));
    m_obsClient->setImageScene(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kObsSceneImage)));
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

    const QString standbyImage =
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kStandbyImage));
    m_playbackController->setStandbyImageUrl(
        standbyImage.isEmpty() ? QUrl{} : QUrl::fromLocalFile(standbyImage));
    m_camera->setCameraId(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kPresentationCamera)));

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

    applyFolderSettings();

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
    m_backgroundMusic->setAudioDevice(musicTarget.isEmpty() ? QStringLiteral("auto") : musicTarget);
}

std::vector<QDate> ApplicationController::meetingDaysInWindow() const
{
    std::vector<QDate> days;
    for (const QDate& date : kmedia::MeetingSyncEngine::lookupWindow(
             QDate::currentDate(), m_syncEngine->settings().meteredConnection)) {
        const QString kind = m_calendarModel->meetingKindForDate(date.toString(Qt::ISODate));
        if (kind == QStringLiteral("midweek") || kind == QStringLiteral("weekend") ||
            kind == QStringLiteral("memorial")) {
            days.push_back(date);
        }
    }
    return days;
}

void ApplicationController::applyFolderSettings()
{
    auto* settings = m_profileManager->activeSettings();
    if (settings == nullptr) {
        return;
    }
    QString watched =
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kWatchedFolder));
    bool watchEnabled =
        settings->boolValue(QLatin1StringView(kmedia::settings_keys::kWatchedFolderEnabled), false);
    // The folders of the old watcher become the shared folder once.
    const QString legacy =
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kCloudSyncFolders));
    if (watched.isEmpty() && !legacy.isEmpty()) {
        watched = legacy.split(QLatin1Char(';'), Qt::SkipEmptyParts).value(0).trimmed();
        watchEnabled = !watched.isEmpty();
    }
    const auto meetingKind = [this](const QDate& date) {
        return m_calendarModel->meetingKindForDate(date.toString(Qt::ISODate));
    };
    const auto coWeek = [this](const QDate& date) {
        return m_calendarModel->eventTypeForDate(date.toString(Qt::ISODate)) ==
               QStringLiteral("co_visit");
    };
    const QSignalBlocker blockWatched(m_watchedFolder.get());
    const QSignalBlocker blockExport(m_autoExport.get());
    kmedia::DatabaseManager* database = m_demoMode ? nullptr : m_profileManager->activeDatabase();

    m_autoExport->setScheduleProviders(meetingKind, coWeek);
    m_autoExport->setDatabase(database);
    if (const auto profile = m_profileManager->activeProfile();
        profile.has_value() && !profile->storagePath.isEmpty()) {
        m_autoExport->setConversionCacheDir(
            QDir(profile->storagePath).filePath(QStringLiteral("Conversions")));
    }
    m_autoExport->setEnabled(
        settings->boolValue(QLatin1StringView(kmedia::settings_keys::kAutoExportEnabled), false));
    m_autoExport->setFolder(
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kAutoExportFolder)));
    m_autoExport->setConvertToMp4(settings->boolValue(
        QLatin1StringView(kmedia::settings_keys::kAutoExportConvertMp4), false));

    m_watchedFolder->setScheduleProviders(meetingKind, coWeek);
    m_watchedFolder->setBlockedFolder(m_autoExport->folder());
    m_watchedFolder->setEnabled(watchEnabled);
    m_watchedFolder->setFolder(watched);
    m_watchedFolder->setDeletePastFolders(settings->boolValue(
        QLatin1StringView(kmedia::settings_keys::kWatchedFolderDeletePast), true));
    m_watchedFolder->setDatabase(database);
}

void ApplicationController::wireIntegrations()
{
    connect(m_syncEngine.get(), &kmedia::MeetingSyncEngine::dayContentChanged, this,
            [this](const QString& dateIso) {
                if (m_playlistModel->currentDateIso() == dateIso) {
                    m_playlistModel->reloadFromDatabase();
                }
                m_autoExport->exportDay(dateIso);
            });
    // Once the window is synced: day folders for the coming meetings, and every day exported.
    connect(m_syncEngine.get(), &kmedia::MeetingSyncEngine::syncIdle, this, [this]() {
        const std::vector<QDate> days = meetingDaysInWindow();
        m_watchedFolder->prepareDayFolders(days);
        m_autoExport->exportDays(days);
    });
    connect(m_watchedFolder.get(), &kmedia::WatchedFolderMedia::dayChanged, this,
            [this](const QString& dateIso) {
                if (m_playlistModel->currentDateIso() == dateIso) {
                    m_playlistModel->reloadFromDatabase();
                }
                m_syncEngine->refreshAddedMediaDays();
                m_autoExport->exportDay(dateIso);
            });
    connect(m_watchedFolder.get(), &kmedia::WatchedFolderMedia::folderConflict, this, [this]() {
        showToast(tr("The shared folder and the export folder must be different"),
                  QStringLiteral("warning"));
    });
    connect(m_autoExport.get(), &kmedia::MediaAutoExport::dayExported, this,
            [this](const QString&, int, int conversionFailures) {
                if (conversionFailures > 0) {
                    showToast(
                        tr("%n file(s) could not be converted to MP4", nullptr, conversionFailures),
                        QStringLiteral("warning"));
                }
            });
    // Any edit of the open day is exported a second after the last change.
    const auto exportOpenDaySoon = [this]() {
        if (m_autoExport->isActive()) {
            m_exportTimer->start();
        }
    };
    connect(m_exportTimer, &QTimer::timeout, this,
            [this]() { m_autoExport->exportDay(m_playlistModel->currentDateIso()); });
    connect(m_playlistModel.get(), &QAbstractItemModel::dataChanged, this, exportOpenDaySoon);
    connect(m_playlistModel.get(), &QAbstractItemModel::rowsInserted, this, exportOpenDaySoon);
    connect(m_playlistModel.get(), &QAbstractItemModel::rowsRemoved, this, exportOpenDaySoon);
    connect(m_playlistModel.get(), &QAbstractItemModel::rowsMoved, this, exportOpenDaySoon);
    connect(m_playlistModel.get(), &kmedia::PlaylistModel::sectionsChanged, this,
            exportOpenDaySoon);
    // Folder settings are saved per congregation as they change.
    connect(m_watchedFolder.get(), &kmedia::WatchedFolderMedia::settingsChanged, this, [this]() {
        m_watchedFolder->setBlockedFolder(m_autoExport->folder());
        if (auto* settings = m_demoMode ? nullptr : m_profileManager->activeSettings()) {
            (void)settings->setValue(
                QLatin1StringView(kmedia::settings_keys::kWatchedFolderEnabled),
                m_watchedFolder->isEnabled());
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kWatchedFolder),
                                     m_watchedFolder->folder());
            (void)settings->setValue(
                QLatin1StringView(kmedia::settings_keys::kWatchedFolderDeletePast),
                m_watchedFolder->deletePastFolders());
        }
    });
    connect(m_autoExport.get(), &kmedia::MediaAutoExport::settingsChanged, this, [this]() {
        m_watchedFolder->setBlockedFolder(m_autoExport->folder());
        if (auto* settings = m_demoMode ? nullptr : m_profileManager->activeSettings()) {
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kAutoExportEnabled),
                                     m_autoExport->isEnabled());
            (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kAutoExportFolder),
                                     m_autoExport->folder());
            (void)settings->setValue(
                QLatin1StringView(kmedia::settings_keys::kAutoExportConvertMp4),
                m_autoExport->convertToMp4());
        }
        m_autoExport->exportDays(meetingDaysInWindow());
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
        const auto kind = m_playbackController->mediaKind();
        const bool hasMedia = kind != kmedia::PlaybackController::MediaKind::None;
        const bool stillImage = kind == kmedia::PlaybackController::MediaKind::Image ||
                                kind == kmedia::PlaybackController::MediaKind::Pdf;
        const bool activePresentation = m_playbackController->isActivePresentation();
        const bool standby = m_playbackController->isStandbyActive();
        m_obsClient->applyPlaybackState(hasMedia, stillImage, standby);
        m_musicService->setMediaPlaying(hasMedia && !stillImage && activePresentation);
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
        }
    });
    connect(
        m_meetingCatalog.get(), &kmedia::MeetingCatalogService::congregationScheduleApplied, this,
        [this](const QString& name, const QString&, int midDay, const QString& midTime, int endDay,
               const QString& endTime) {
            m_calendarModel->setScheduleDays(midDay, midTime, endDay, endTime);
            showToast(tr("Applied congregation schedule: %1").arg(name), QStringLiteral("success"));
        });
    connect(m_meetingCatalog.get(), &kmedia::MeetingCatalogService::catalogError, this,
            [this](const QString& errorMessage) {
                showToast(errorMessage, QStringLiteral("warning"));
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
            });

    connect(m_calendarModel.get(), &kmedia::MeetingCalendarModel::weekOpenRequested, this,
            [this](const QDate&, bool syncNow) {
                const QString dateIso = m_calendarModel->selectedDateIso();
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
    QStringList languages{next.languageCode};
    if (!next.fallbackLanguageCode.isEmpty() && next.fallbackLanguageCode != next.languageCode) {
        languages << next.fallbackLanguageCode;
    }
    m_documentImporter->setLanguages(languages);
    m_publicationPicker->setLanguages(languages);
    m_studyBible->setLanguages(languages);
    m_camera->setSignLanguage(next.signLanguage);
    m_yeartext->setLanguages(m_demoMode ? QStringList{} : languages, next.signLanguage,
                             QDate::currentDate().year());
    m_videoCatalog->setLanguage(next.languageCode);
    m_bibleCatalog->setLanguage(next.languageCode, next.signLanguage);
    m_musicLibrary->setLanguage(next.languageCode, next.signLanguage);
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

    if (!m_demoMode && forceSync) {
        m_syncEngine->resyncDay(effectiveIso);
    }
}
