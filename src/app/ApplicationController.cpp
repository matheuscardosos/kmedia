#include "ApplicationController.h"

#include "core/NetworkManager.h"
#include "core/ProfileManager.h"
#include "core/RuntimeInfo.h"
#include "core/SettingsStore.h"
#include "core/SystemUtils.h"
#include "integrations/DisplayManager.h"
#include "media/BackgroundMusicEngine.h"
#include "media/MpvRendererItem.h"
#include "media/PdfViewerController.h"
#include "media/PlaybackController.h"
#include "media/ThumbnailCache.h"
#include "network/CatalogClient.h"
#include "network/DownloadService.h"
#include "parsers/CloudSyncWatcher.h"

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent), m_profileManager(std::make_unique<kmedia::ProfileManager>()),
      m_networkManager(std::make_unique<kmedia::NetworkManager>()),
      m_thumbnailCache(std::make_unique<kmedia::ThumbnailCache>()),
      m_pdfController(std::make_unique<kmedia::PdfViewerController>()),
      m_playbackController(std::make_unique<kmedia::PlaybackController>()),
      m_backgroundMusic(std::make_unique<kmedia::BackgroundMusicEngine>()),
      m_displayManager(std::make_unique<kmedia::DisplayManager>()),
      m_downloadService(std::make_unique<kmedia::DownloadService>(m_networkManager.get())),
      m_catalogClient(std::make_unique<kmedia::CatalogClient>(m_networkManager.get())),
      m_cloudWatcher(std::make_unique<kmedia::CloudSyncWatcher>())
{
    m_playbackController->attachPdfController(m_pdfController.get());

    connect(m_profileManager.get(), &kmedia::ProfileManager::activeProfileChanged, this,
            [this](const QString&) {
                applyProfileSettings();
                emit activeProfileChanged();
            });

    if (m_profileManager->initialize(kmedia::SystemUtils::standardAppDataDir())) {
        const qint64 cacheLimitBytes =
            static_cast<qint64>(m_profileManager->activeSettings()->thumbnailCacheMaxMb()) *
            1024LL * 1024LL;
        const bool cacheReady = m_thumbnailCache->initialize(
            kmedia::SystemUtils::standardCacheDir() + QStringLiteral("/thumbnails"),
            cacheLimitBytes);
        Q_UNUSED(cacheReady)
        applyProfileSettings();
    }
}

ApplicationController::~ApplicationController() = default;

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

QString ApplicationController::memoryUsage() const
{
    return kmedia::SystemUtils::formatFileSize(kmedia::SystemUtils::processMemoryRssBytes());
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

void ApplicationController::bindPresentationMpv(MpvRendererItem* mpvItem)
{
    m_playbackController->attachMpvItem(mpvItem);
}

void ApplicationController::applyProfileSettings()
{
    m_downloadService->setDatabaseManager(m_profileManager->activeDatabase());
    m_catalogClient->setDatabaseManager(m_profileManager->activeDatabase());

    const auto* settings = m_profileManager->activeSettings();
    if (settings == nullptr) {
        return;
    }

    m_backgroundMusic->setFadeDurationMs(settings->backgroundMusicFadeMs());
    m_downloadService->setMaxConcurrent(settings->maxConcurrentDownloads());

    const QString deviceId =
        settings->stringValue(QLatin1StringView(kmedia::settings_keys::kAudioOutputDevice));
    if (!deviceId.isEmpty()) {
        m_backgroundMusic->setAudioDevice(deviceId);
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
