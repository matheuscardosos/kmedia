#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>

Q_MOC_INCLUDE("core/MeetingCalendarModel.h")
Q_MOC_INCLUDE("core/MeetingTimer.h")
Q_MOC_INCLUDE("integrations/DisplayManager.h")
Q_MOC_INCLUDE("integrations/ObsWebSocketClient.h")
Q_MOC_INCLUDE("integrations/ProcessMonitor.h")
Q_MOC_INCLUDE("integrations/ZoomBridge.h")
Q_MOC_INCLUDE("media/BackgroundMusicEngine.h")
Q_MOC_INCLUDE("media/MpvRendererItem.h")
Q_MOC_INCLUDE("media/PdfViewerController.h")
Q_MOC_INCLUDE("media/PlaybackController.h")
Q_MOC_INCLUDE("models/PlaylistModel.h")
Q_MOC_INCLUDE("network/DownloadService.h")
Q_MOC_INCLUDE("network/MeetingCatalogService.h")
Q_MOC_INCLUDE("parsers/CloudSyncWatcher.h")

class MpvRendererItem;
class QQmlEngine;
class QTemporaryDir;
class QTranslator;

namespace kmedia {
class BackgroundMusicEngine;
class CatalogClient;
class CloudSyncWatcher;
class DisplayManager;
class DownloadService;
class MeetingCalendarModel;
class MeetingCatalogService;
class MeetingTimer;
class NetworkManager;
class ObsWebSocketClient;
class PdfViewerController;
class PlaybackController;
class PlaylistModel;
class ProcessMonitor;
class ProfileManager;
class ThumbnailCache;
class ZoomBridge;
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
    Q_PROPERTY(
        QString storageAvailableText READ storageAvailableText NOTIFY activeProfileChanged FINAL)
    Q_PROPERTY(QString memoryUsage READ memoryUsage CONSTANT FINAL)
    Q_PROPERTY(QString uiLanguage READ uiLanguage NOTIFY uiLanguageChanged FINAL)
    Q_PROPERTY(bool darkTheme READ isDarkTheme WRITE setDarkTheme NOTIFY darkThemeChanged FINAL)
    Q_PROPERTY(bool demoMode READ isDemoMode NOTIFY demoModeChanged FINAL)
    Q_PROPERTY(int startupTabIndex READ startupTabIndex CONSTANT FINAL)
    Q_PROPERTY(bool boothPinEnabled READ isBoothPinEnabled NOTIFY boothLockChanged FINAL)
    Q_PROPERTY(bool boothUnlocked READ isBoothUnlocked NOTIFY boothLockChanged FINAL)
    Q_PROPERTY(
        QStringList availableProfileNames READ availableProfileNames NOTIFY profilesChanged FINAL)
    Q_PROPERTY(
        QStringList availableProfileIds READ availableProfileIds NOTIFY profilesChanged FINAL)
    Q_PROPERTY(kmedia::PlaybackController* playback READ playback CONSTANT FINAL)
    Q_PROPERTY(kmedia::BackgroundMusicEngine* backgroundMusic READ backgroundMusic CONSTANT FINAL)
    Q_PROPERTY(kmedia::DisplayManager* displayManager READ displayManager CONSTANT FINAL)
    Q_PROPERTY(kmedia::PdfViewerController* pdf READ pdfController CONSTANT FINAL)
    Q_PROPERTY(kmedia::DownloadService* downloads READ downloadService CONSTANT FINAL)
    Q_PROPERTY(kmedia::CloudSyncWatcher* cloudWatcher READ cloudWatcher CONSTANT FINAL)
    Q_PROPERTY(kmedia::ProcessMonitor* processMonitor READ processMonitor CONSTANT FINAL)
    Q_PROPERTY(kmedia::ObsWebSocketClient* obsClient READ obsClient CONSTANT FINAL)
    Q_PROPERTY(kmedia::ZoomBridge* zoomBridge READ zoomBridge CONSTANT FINAL)
    Q_PROPERTY(kmedia::PlaylistModel* playlist READ playlist CONSTANT FINAL)
    Q_PROPERTY(kmedia::MeetingTimer* meetingTimer READ meetingTimer CONSTANT FINAL)
    Q_PROPERTY(kmedia::MeetingCatalogService* catalog READ catalog CONSTANT FINAL)
    Q_PROPERTY(kmedia::MeetingCalendarModel* calendar READ calendar CONSTANT FINAL)

public:
    static void setStartupDemoMode(bool enabled);
    static void setStartupThemeOverride(const QString& theme);
    static void setStartupTabIndex(int tabIndex);

    explicit ApplicationController(QObject* parent = nullptr);
    ~ApplicationController() override;

    Q_DISABLE_COPY_MOVE(ApplicationController)

    void attachQmlEngine(QQmlEngine* engine);

    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] QString qtVersion() const;
    [[nodiscard]] QString mpvVersion() const;
    [[nodiscard]] QString activeProfileName() const;
    [[nodiscard]] QString storageAvailableText() const;
    [[nodiscard]] QString memoryUsage() const;
    [[nodiscard]] QString uiLanguage() const;
    [[nodiscard]] bool isDarkTheme() const;
    void setDarkTheme(bool dark);
    [[nodiscard]] bool isDemoMode() const;
    [[nodiscard]] int startupTabIndex() const;
    [[nodiscard]] bool isBoothPinEnabled() const;
    [[nodiscard]] bool isBoothUnlocked() const;
    [[nodiscard]] QStringList availableProfileNames() const;
    [[nodiscard]] QStringList availableProfileIds() const;

    [[nodiscard]] kmedia::PlaybackController* playback() const;
    [[nodiscard]] kmedia::BackgroundMusicEngine* backgroundMusic() const;
    [[nodiscard]] kmedia::DisplayManager* displayManager() const;
    [[nodiscard]] kmedia::PdfViewerController* pdfController() const;
    [[nodiscard]] kmedia::DownloadService* downloadService() const;
    [[nodiscard]] kmedia::CloudSyncWatcher* cloudWatcher() const;
    [[nodiscard]] kmedia::ProcessMonitor* processMonitor() const;
    [[nodiscard]] kmedia::ObsWebSocketClient* obsClient() const;
    [[nodiscard]] kmedia::ZoomBridge* zoomBridge() const;
    [[nodiscard]] kmedia::PlaylistModel* playlist() const;
    [[nodiscard]] kmedia::MeetingTimer* meetingTimer() const;
    [[nodiscard]] kmedia::MeetingCatalogService* catalog() const;
    [[nodiscard]] kmedia::MeetingCalendarModel* calendar() const;

    Q_INVOKABLE void bindPresentationMpv(MpvRendererItem* mpvItem);
    Q_INVOKABLE bool setUiLanguage(const QString& languageCode);
    Q_INVOKABLE bool switchProfile(const QString& profileId);
    Q_INVOKABLE QString createProfile(const QString& name,
                                      const QString& mediaLanguage = QStringLiteral("E"));
    Q_INVOKABLE void saveCloudFolder(const QString& folderPath);
    Q_INVOKABLE void saveObsAndZoomSettings();
    Q_INVOKABLE void saveFadeDurationMs(int ms);
    Q_INVOKABLE bool enableBoothPin(const QString& newPin);
    Q_INVOKABLE bool disableBoothPin(const QString& currentPin);
    Q_INVOKABLE bool unlockBoothSettings(const QString& candidatePin);
    Q_INVOKABLE void lockBoothSettings();
    Q_INVOKABLE void enterDemoMode();
    Q_INVOKABLE void toggleDarkTheme();
    Q_INVOKABLE void showToast(const QString& message,
                               const QString& level = QStringLiteral("info"));

signals:
    void activeProfileChanged();
    void profilesChanged();
    void uiLanguageChanged();
    void darkThemeChanged();
    void demoModeChanged();
    void boothLockChanged();
    void toastRequested(const QString& message, const QString& level);

private:
    void applyProfileSettings();
    void wireIntegrations();

    static bool s_startupDemoMode;
    static QString s_startupThemeOverride;
    static int s_startupTabIndex;

    QQmlEngine* m_qmlEngine{nullptr};
    std::unique_ptr<QTranslator> m_translator;
    std::unique_ptr<QTemporaryDir> m_demoSandbox;
    bool m_demoMode{false};
    bool m_darkTheme{true};
    QString m_uiLanguage{QStringLiteral("en")};
    QString m_boothPinSalt;
    QString m_boothPinHash;
    bool m_boothUnlocked{true};

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
    std::unique_ptr<kmedia::ProcessMonitor> m_processMonitor;
    std::unique_ptr<kmedia::ObsWebSocketClient> m_obsClient;
    std::unique_ptr<kmedia::ZoomBridge> m_zoomBridge;
    std::unique_ptr<kmedia::PlaylistModel> m_playlistModel;
    std::unique_ptr<kmedia::MeetingTimer> m_meetingTimer;
    std::unique_ptr<kmedia::MeetingCatalogService> m_meetingCatalog;
    std::unique_ptr<kmedia::MeetingCalendarModel> m_calendarModel;
};
