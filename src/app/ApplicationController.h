#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>

Q_MOC_INCLUDE("integrations/DisplayManager.h")
Q_MOC_INCLUDE("media/BackgroundMusicEngine.h")
Q_MOC_INCLUDE("media/MpvRendererItem.h")
Q_MOC_INCLUDE("media/PdfViewerController.h")
Q_MOC_INCLUDE("media/PlaybackController.h")
Q_MOC_INCLUDE("network/DownloadService.h")
Q_MOC_INCLUDE("parsers/CloudSyncWatcher.h")

class MpvRendererItem;

namespace kmedia {
class BackgroundMusicEngine;
class CatalogClient;
class CloudSyncWatcher;
class DisplayManager;
class DownloadService;
class NetworkManager;
class PdfViewerController;
class PlaybackController;
class ProfileManager;
class ThumbnailCache;
} // namespace kmedia

class ApplicationController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString appVersion READ appVersion CONSTANT FINAL)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT FINAL)
    Q_PROPERTY(QString mpvVersion READ mpvVersion CONSTANT FINAL)
    Q_PROPERTY(QString activeProfileName READ activeProfileName NOTIFY activeProfileChanged FINAL)
    Q_PROPERTY(QString memoryUsage READ memoryUsage CONSTANT FINAL)
    Q_PROPERTY(kmedia::PlaybackController* playback READ playback CONSTANT FINAL)
    Q_PROPERTY(kmedia::BackgroundMusicEngine* backgroundMusic READ backgroundMusic CONSTANT FINAL)
    Q_PROPERTY(kmedia::DisplayManager* displayManager READ displayManager CONSTANT FINAL)
    Q_PROPERTY(kmedia::PdfViewerController* pdf READ pdfController CONSTANT FINAL)
    Q_PROPERTY(kmedia::DownloadService* downloads READ downloadService CONSTANT FINAL)
    Q_PROPERTY(kmedia::CloudSyncWatcher* cloudWatcher READ cloudWatcher CONSTANT FINAL)

public:
    explicit ApplicationController(QObject* parent = nullptr);
    ~ApplicationController() override;

    Q_DISABLE_COPY_MOVE(ApplicationController)

    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] QString qtVersion() const;
    [[nodiscard]] QString mpvVersion() const;
    [[nodiscard]] QString activeProfileName() const;
    [[nodiscard]] QString memoryUsage() const;

    [[nodiscard]] kmedia::PlaybackController* playback() const;
    [[nodiscard]] kmedia::BackgroundMusicEngine* backgroundMusic() const;
    [[nodiscard]] kmedia::DisplayManager* displayManager() const;
    [[nodiscard]] kmedia::PdfViewerController* pdfController() const;
    [[nodiscard]] kmedia::DownloadService* downloadService() const;
    [[nodiscard]] kmedia::CloudSyncWatcher* cloudWatcher() const;

    Q_INVOKABLE void bindPresentationMpv(MpvRendererItem* mpvItem);

signals:
    void activeProfileChanged();

private:
    void applyProfileSettings();

    std::unique_ptr<kmedia::ProfileManager> m_profileManager;
    std::unique_ptr<kmedia::NetworkManager> m_networkManager;
    std::unique_ptr<kmedia::ThumbnailCache> m_thumbnailCache;
    std::unique_ptr<kmedia::PdfViewerController> m_pdfController;
    std::unique_ptr<kmedia::PlaybackController> m_playbackController;
    std::unique_ptr<kmedia::BackgroundMusicEngine> m_backgroundMusic;
    std::unique_ptr<kmedia::DisplayManager> m_displayManager;
    std::unique_ptr<kmedia::DownloadService> m_downloadService;
    std::unique_ptr<kmedia::CatalogClient> m_catalogClient;
    std::unique_ptr<kmedia::CloudSyncWatcher> m_cloudWatcher;
};
