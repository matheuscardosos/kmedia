#pragma once

#include <QDate>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

#include <memory>
#include <vector>

Q_MOC_INCLUDE("core/MeetingCalendarModel.h")
Q_MOC_INCLUDE("core/MeetingTimer.h")
Q_MOC_INCLUDE("integrations/DisplayManager.h")
Q_MOC_INCLUDE("integrations/ObsWebSocketClient.h")
Q_MOC_INCLUDE("integrations/ProcessMonitor.h")
Q_MOC_INCLUDE("integrations/ZoomBridge.h")
Q_MOC_INCLUDE("media/BackgroundMusicEngine.h")
Q_MOC_INCLUDE("media/BackgroundMusicService.h")
Q_MOC_INCLUDE("media/MediaAutoExport.h")
Q_MOC_INCLUDE("media/MpvRendererItem.h")
Q_MOC_INCLUDE("media/PdfViewerController.h")
Q_MOC_INCLUDE("media/PdfPageImporter.h")
Q_MOC_INCLUDE("media/PlaybackController.h")
Q_MOC_INCLUDE("models/MeetingDayModel.h")
Q_MOC_INCLUDE("models/PlaylistModel.h")
Q_MOC_INCLUDE("network/DownloadService.h")
Q_MOC_INCLUDE("network/MeetingCatalogService.h")
Q_MOC_INCLUDE("network/MeetingSyncEngine.h")
Q_MOC_INCLUDE("network/PublicTalkMedia.h")
Q_MOC_INCLUDE("network/PublicationMediaPicker.h")
Q_MOC_INCLUDE("network/WatchedFolderMedia.h")
Q_MOC_INCLUDE("network/JwVideoCatalog.h")
Q_MOC_INCLUDE("network/BibleMediaCatalog.h")
Q_MOC_INCLUDE("network/StudyBibleMedia.h")
Q_MOC_INCLUDE("network/YeartextProvider.h")
Q_MOC_INCLUDE("media/CameraController.h")
Q_MOC_INCLUDE("parsers/CloudSyncWatcher.h")

class MpvRendererItem;
class QQmlEngine;
class QTemporaryDir;
class QTimer;
class QTranslator;

namespace kmedia {
class BackgroundMusicEngine;
class BackgroundMusicLibrary;
class BackgroundMusicService;
class CatalogClient;
class CloudSyncWatcher;
class DisplayManager;
class DownloadService;
class MeetingCalendarModel;
class MeetingCatalogService;
class MeetingDayModel;
class MeetingSyncEngine;
class MeetingTimer;
class NetworkManager;
class ObsWebSocketClient;
class PdfViewerController;
class PlaybackController;
class PlaylistModel;
class ProcessMonitor;
class ProfileManager;
class DocumentMediaImporter;
class JwVideoCatalog;
class BibleMediaCatalog;
class StudyBibleMedia;
class YeartextProvider;
class CameraController;
class PdfPageImporter;
class MediaAutoExport;
class WatchedFolderMedia;
class PublicTalkMedia;
class PublicationMediaPicker;
class PublicationStore;
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
    Q_PROPERTY(bool floatingPreview READ isFloatingPreviewEnabled WRITE setFloatingPreviewEnabled
                   NOTIFY floatingPreviewChanged FINAL)
    Q_PROPERTY(bool timerWindowVisible READ isTimerWindowVisible WRITE setTimerWindowVisible NOTIFY
                   timerWindowVisibleChanged FINAL)
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
    Q_PROPERTY(kmedia::BackgroundMusicService* music READ music CONSTANT FINAL)
    Q_PROPERTY(kmedia::DisplayManager* displayManager READ displayManager CONSTANT FINAL)
    Q_PROPERTY(kmedia::PdfViewerController* pdf READ pdfController CONSTANT FINAL)
    Q_PROPERTY(kmedia::DownloadService* downloads READ downloadService CONSTANT FINAL)
    Q_PROPERTY(kmedia::CloudSyncWatcher* cloudWatcher READ cloudWatcher CONSTANT FINAL)
    Q_PROPERTY(kmedia::ProcessMonitor* processMonitor READ processMonitor CONSTANT FINAL)
    Q_PROPERTY(kmedia::ObsWebSocketClient* obsClient READ obsClient CONSTANT FINAL)
    Q_PROPERTY(kmedia::ZoomBridge* zoomBridge READ zoomBridge CONSTANT FINAL)
    Q_PROPERTY(kmedia::PlaylistModel* playlist READ playlist CONSTANT FINAL)
    Q_PROPERTY(kmedia::MeetingDayModel* meetingDay READ meetingDay CONSTANT FINAL)
    Q_PROPERTY(kmedia::MeetingTimer* meetingTimer READ meetingTimer CONSTANT FINAL)
    Q_PROPERTY(kmedia::MeetingCatalogService* catalog READ catalog CONSTANT FINAL)
    Q_PROPERTY(kmedia::MeetingCalendarModel* calendar READ calendar CONSTANT FINAL)
    Q_PROPERTY(kmedia::MeetingSyncEngine* sync READ syncEngine CONSTANT FINAL)
    Q_PROPERTY(kmedia::PublicTalkMedia* publicTalks READ publicTalks CONSTANT FINAL)
    Q_PROPERTY(
        kmedia::PublicationMediaPicker* publicationMedia READ publicationMedia CONSTANT FINAL)
    Q_PROPERTY(kmedia::JwVideoCatalog* videos READ videos CONSTANT FINAL)
    Q_PROPERTY(kmedia::BibleMediaCatalog* bible READ bible CONSTANT FINAL)
    Q_PROPERTY(kmedia::StudyBibleMedia* studyBible READ studyBible CONSTANT FINAL)
    Q_PROPERTY(kmedia::YeartextProvider* yeartext READ yeartext CONSTANT FINAL)
    Q_PROPERTY(kmedia::CameraController* camera READ camera CONSTANT FINAL)
    Q_PROPERTY(kmedia::PdfPageImporter* pdfPages READ pdfPages CONSTANT FINAL)
    Q_PROPERTY(kmedia::WatchedFolderMedia* watchedFolder READ watchedFolder CONSTANT FINAL)
    Q_PROPERTY(kmedia::MediaAutoExport* autoExport READ autoExport CONSTANT FINAL)

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
    [[nodiscard]] bool isFloatingPreviewEnabled() const;
    void setFloatingPreviewEnabled(bool enabled);
    [[nodiscard]] bool isTimerWindowVisible() const;
    void setTimerWindowVisible(bool visible);
    [[nodiscard]] bool isDemoMode() const;
    [[nodiscard]] int startupTabIndex() const;
    [[nodiscard]] bool isBoothPinEnabled() const;
    [[nodiscard]] bool isBoothUnlocked() const;
    [[nodiscard]] QStringList availableProfileNames() const;
    [[nodiscard]] QStringList availableProfileIds() const;

    [[nodiscard]] kmedia::PlaybackController* playback() const;
    [[nodiscard]] kmedia::BackgroundMusicEngine* backgroundMusic() const;
    [[nodiscard]] kmedia::BackgroundMusicService* music() const;
    [[nodiscard]] kmedia::DisplayManager* displayManager() const;
    [[nodiscard]] kmedia::PdfViewerController* pdfController() const;
    [[nodiscard]] kmedia::DownloadService* downloadService() const;
    [[nodiscard]] kmedia::CloudSyncWatcher* cloudWatcher() const;
    [[nodiscard]] kmedia::ProcessMonitor* processMonitor() const;
    [[nodiscard]] kmedia::ObsWebSocketClient* obsClient() const;
    [[nodiscard]] kmedia::ZoomBridge* zoomBridge() const;
    [[nodiscard]] kmedia::PlaylistModel* playlist() const;
    [[nodiscard]] kmedia::MeetingDayModel* meetingDay() const;
    [[nodiscard]] kmedia::MeetingTimer* meetingTimer() const;
    [[nodiscard]] kmedia::MeetingCatalogService* catalog() const;
    [[nodiscard]] kmedia::MeetingCalendarModel* calendar() const;
    [[nodiscard]] kmedia::MeetingSyncEngine* syncEngine() const;
    [[nodiscard]] kmedia::PublicTalkMedia* publicTalks() const;
    [[nodiscard]] kmedia::PublicationMediaPicker* publicationMedia() const;
    [[nodiscard]] kmedia::JwVideoCatalog* videos() const;
    [[nodiscard]] kmedia::BibleMediaCatalog* bible() const;
    [[nodiscard]] kmedia::StudyBibleMedia* studyBible() const;
    [[nodiscard]] kmedia::YeartextProvider* yeartext() const;
    [[nodiscard]] kmedia::CameraController* camera() const;
    [[nodiscard]] kmedia::PdfPageImporter* pdfPages() const;
    [[nodiscard]] kmedia::WatchedFolderMedia* watchedFolder() const;
    [[nodiscard]] kmedia::MediaAutoExport* autoExport() const;

    Q_INVOKABLE void bindPresentationMpv(MpvRendererItem* mpvItem);
    Q_INVOKABLE bool setUiLanguage(const QString& languageCode);
    Q_INVOKABLE bool switchProfile(const QString& profileId);
    Q_INVOKABLE QString createProfile(const QString& name,
                                      const QString& mediaLanguage = QStringLiteral("E"));
    Q_INVOKABLE void saveObsAndZoomSettings();
    Q_INVOKABLE void saveFadeDurationMs(int ms);
    // The output of media and background music; an empty name means the system default.
    Q_INVOKABLE void saveAudioOutputDevice(const QString& deviceName);
    // The picture shown on the presentation display during standby; an empty url clears it.
    Q_INVOKABLE void saveStandbyImage(const QUrl& imageUrl);
    Q_INVOKABLE void saveMusicAutoStart(bool enabled);
    Q_INVOKABLE bool enableBoothPin(const QString& newPin);
    Q_INVOKABLE bool disableBoothPin(const QString& currentPin);
    Q_INVOKABLE bool unlockBoothSettings(const QString& candidatePin);
    Q_INVOKABLE void lockBoothSettings();
    Q_INVOKABLE void enterDemoMode();
    Q_INVOKABLE void toggleDarkTheme();
    Q_INVOKABLE void openDateWorkspace(const QString& isoDate, bool forceSync = false);
    Q_INVOKABLE void showToast(const QString& message,
                               const QString& level = QStringLiteral("info"));

signals:
    void activeProfileChanged();
    void profilesChanged();
    void uiLanguageChanged();
    void darkThemeChanged();
    void floatingPreviewChanged();
    void timerWindowVisibleChanged();
    void demoModeChanged();
    void boothLockChanged();
    void toastRequested(const QString& message, const QString& level);

private:
    void applyProfileSettings();
    void syncAudioRouting();
    void applySyncSettings(bool resetWhenChanged);
    void applyFolderSettings();
    // Points the meeting timer at the open day, with what was kept for that day.
    void syncMeetingTimerDay();
    void saveMeetingTimerDay(const QString& dateIso, const QString& dayState);
    [[nodiscard]] std::vector<QDate> meetingDaysInWindow() const;
    void wireIntegrations();

    static bool s_startupDemoMode;
    static QString s_startupThemeOverride;
    static int s_startupTabIndex;

    QQmlEngine* m_qmlEngine{nullptr};
    std::unique_ptr<QTranslator> m_translator;
    std::unique_ptr<QTemporaryDir> m_demoSandbox;
    bool m_demoMode{false};
    bool m_darkTheme{true};
    bool m_floatingPreview{true};
    bool m_timerWindowVisible{false};
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
    std::unique_ptr<kmedia::MeetingDayModel> m_meetingDayModel;
    std::unique_ptr<kmedia::MeetingTimer> m_meetingTimer;
    std::unique_ptr<kmedia::MeetingCatalogService> m_meetingCatalog;
    std::unique_ptr<kmedia::MeetingCalendarModel> m_calendarModel;
    std::unique_ptr<kmedia::PublicationStore> m_publicationStore;
    std::unique_ptr<kmedia::MeetingSyncEngine> m_syncEngine;
    std::unique_ptr<kmedia::DocumentMediaImporter> m_documentImporter;
    std::unique_ptr<kmedia::PublicTalkMedia> m_publicTalks;
    std::unique_ptr<kmedia::PublicationMediaPicker> m_publicationPicker;
    std::unique_ptr<kmedia::JwVideoCatalog> m_videoCatalog;
    std::unique_ptr<kmedia::BibleMediaCatalog> m_bibleCatalog;
    std::unique_ptr<kmedia::StudyBibleMedia> m_studyBible;
    std::unique_ptr<kmedia::YeartextProvider> m_yeartext;
    std::unique_ptr<kmedia::CameraController> m_camera;
    std::unique_ptr<kmedia::PdfPageImporter> m_pdfPages;
    std::unique_ptr<kmedia::BackgroundMusicLibrary> m_musicLibrary;
    std::unique_ptr<kmedia::BackgroundMusicService> m_musicService;
    std::unique_ptr<kmedia::WatchedFolderMedia> m_watchedFolder;
    std::unique_ptr<kmedia::MediaAutoExport> m_autoExport;
    QTimer* m_exportTimer{nullptr};
};
