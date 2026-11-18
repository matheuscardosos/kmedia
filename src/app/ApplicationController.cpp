#include "ApplicationController.h"

#include "core/ProfileManager.h"
#include "core/RuntimeInfo.h"
#include "core/SettingsStore.h"
#include "core/SystemUtils.h"
#include "media/ThumbnailCache.h"

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent), m_profileManager(std::make_unique<kmedia::ProfileManager>()),
      m_thumbnailCache(std::make_unique<kmedia::ThumbnailCache>())
{
    connect(m_profileManager.get(), &kmedia::ProfileManager::activeProfileChanged, this,
            [this](const QString&) { emit activeProfileChanged(); });

    if (m_profileManager->initialize(kmedia::SystemUtils::standardAppDataDir())) {
        const qint64 cacheLimitBytes =
            static_cast<qint64>(m_profileManager->activeSettings()->thumbnailCacheMaxMb()) *
            1024LL * 1024LL;
        const bool cacheReady = m_thumbnailCache->initialize(
            kmedia::SystemUtils::standardCacheDir() + QStringLiteral("/thumbnails"),
            cacheLimitBytes);
        Q_UNUSED(cacheReady)
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
