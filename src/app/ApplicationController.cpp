#include "ApplicationController.h"

#include "core/DatabaseManager.h"
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
#include "models/PlaylistModel.h"
#include "network/CatalogClient.h"
#include "network/DownloadService.h"
#include "network/MeetingCatalogService.h"
#include "parsers/CloudSyncWatcher.h"

#include <QCoreApplication>
#include <QDir>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QTranslator>

bool ApplicationController::s_startupDemoMode = false;
QString ApplicationController::s_startupThemeOverride;

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
      m_meetingTimer(std::make_unique<kmedia::MeetingTimer>()),
      m_meetingCatalog(std::make_unique<kmedia::MeetingCatalogService>(
          m_catalogClient.get(), m_downloadService.get(), m_playlistModel.get()))
{
    m_playbackController->attachPdfController(m_pdfController.get());
    m_playlistModel->attachPlaybackController(m_playbackController.get());
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

kmedia::MeetingTimer* ApplicationController::meetingTimer() const
{
    return m_meetingTimer.get();
}

kmedia::MeetingCatalogService* ApplicationController::catalog() const
{
    return m_meetingCatalog.get();
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
    if (auto* settings = m_profileManager->activeSettings()) {
        (void)settings->setValue(QLatin1StringView(kmedia::settings_keys::kCloudSyncFolders),
                                 trimmed);
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
    }

    const auto* settings = m_profileManager->activeSettings();
    if (settings == nullptr) {
        return;
    }

    const QString mediaLang = settings->mediaLanguage();
    if (!mediaLang.isEmpty()) {
        m_meetingCatalog->setSelectedLanguageCode(mediaLang);
    }

    m_backgroundMusic->setFadeDurationMs(settings->backgroundMusicFadeMs());
    m_downloadService->setMaxConcurrent(settings->maxConcurrentDownloads());

    const QString deviceId =
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kAudioOutputDevice));
    if (!deviceId.isEmpty()) {
        m_backgroundMusic->setAudioDevice(deviceId);
    }

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
}

void ApplicationController::wireIntegrations()
{
    connect(m_processMonitor.get(), &kmedia::ProcessMonitor::statusChanged, this, [this]() {
        m_obsClient->setProcessRunning(m_processMonitor->isObsRunning());
        m_zoomBridge->setZoomRunning(m_processMonitor->isZoomRunning());
    });

    auto syncPlaybackIntegrations = [this]() {
        const bool hasMedia =
            m_playbackController->mediaKind() != kmedia::PlaybackController::MediaKind::None;
        const bool paused = m_playbackController->isPaused();
        const bool standby = m_playbackController->isStandbyActive();
        m_obsClient->applyPlaybackState(hasMedia, paused, standby);
        if (hasMedia && !paused && !standby) {
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
                   kmedia::CloudSyncWatcher::CloudProvider provider) {
                (void)m_playlistModel->importLocalFile(
                    filePath, kmedia::CloudSyncWatcher::providerDisplayName(provider));
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
            });
    connect(m_meetingCatalog.get(), &kmedia::MeetingCatalogService::weekSynced, this,
            [this](int importedCount) {
                showToast(tr("Synced %1 week item(s) into playlist").arg(importedCount),
                          QStringLiteral("success"));
            });
    connect(m_meetingCatalog.get(), &kmedia::MeetingCatalogService::catalogError, this,
            [this](const QString& errorMessage) {
                showToast(errorMessage, QStringLiteral("warning"));
            });
}
