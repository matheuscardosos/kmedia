#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <utility>

class MpvRendererItem;

namespace kmedia {

class PdfViewerController;

class PlaybackController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(MediaKind mediaKind READ mediaKind NOTIFY mediaChanged FINAL)
    Q_PROPERTY(QUrl source READ source NOTIFY mediaChanged FINAL)
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaChanged FINAL)
    Q_PROPERTY(
        bool standbyActive READ isStandbyActive WRITE setStandbyActive NOTIFY standbyChanged FINAL)
    Q_PROPERTY(QUrl standbyImageUrl READ standbyImageUrl WRITE setStandbyImageUrl NOTIFY
                   standbyChanged FINAL)
    // Standby that shows plain black instead of the standby picture.
    Q_PROPERTY(bool blackoutActive READ isBlackoutActive NOTIFY standbyChanged FINAL)
    Q_PROPERTY(bool paused READ isPaused WRITE setPaused NOTIFY transportChanged FINAL)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged FINAL)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged FINAL)
    Q_PROPERTY(qint64 trimStartMs READ trimStartMs NOTIFY trimChanged FINAL)
    Q_PROPERTY(qint64 trimEndMs READ trimEndMs NOTIFY trimChanged FINAL)
    Q_PROPERTY(double zoomScale READ zoomScale WRITE setZoomScale NOTIFY viewportChanged FINAL)
    Q_PROPERTY(double panX READ panX WRITE setPanX NOTIFY viewportChanged FINAL)
    Q_PROPERTY(double panY READ panY WRITE setPanY NOTIFY viewportChanged FINAL)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY audioChanged FINAL)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged FINAL)
    // Counts the live preview frames of a playing video, for the preview panel to reload.
    Q_PROPERTY(int previewSerial READ previewSerial NOTIFY previewFrameChanged FINAL)
    // How wide the grabbed frames are at most: small for the panel, larger for a big preview.
    Q_PROPERTY(int previewFrameWidth READ previewFrameWidth WRITE setPreviewFrameWidth NOTIFY
                   previewFrameWidthChanged FINAL)
    Q_PROPERTY(bool previewActive READ isPreviewActive WRITE setPreviewActive NOTIFY
                   previewActiveChanged FINAL)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY audioChanged FINAL)
    Q_PROPERTY(QString audioDevice READ audioDevice WRITE setAudioDevice NOTIFY audioChanged FINAL)
    // The outputs mpv offers ({name, description}) and the description of the one in use.
    Q_PROPERTY(QVariantList audioDevices READ audioDevices NOTIFY audioChanged FINAL)
    Q_PROPERTY(QString audioDeviceDescription READ audioDeviceDescription NOTIFY audioChanged FINAL)
    Q_PROPERTY(bool subtitlesVisible READ areSubtitlesVisible WRITE setSubtitlesVisible NOTIFY
                   subtitlesChanged FINAL)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY tracksChanged FINAL)
    Q_PROPERTY(int currentSubtitleTrack READ currentSubtitleTrack WRITE setCurrentSubtitleTrack
                   NOTIFY tracksChanged FINAL)
    Q_PROPERTY(QString formattedPosition READ formattedPosition NOTIFY positionChanged FINAL)
    Q_PROPERTY(QString formattedDuration READ formattedDuration NOTIFY durationChanged FINAL)

public:
    enum class MediaKind {
        None = 0,
        Video,
        Audio,
        Image,
        Pdf,
    };
    Q_ENUM(MediaKind)

    explicit PlaybackController(QObject* parent = nullptr);

    Q_DISABLE_COPY_MOVE(PlaybackController)

    void attachMpvItem(MpvRendererItem* mpvItem);
    void attachPdfController(PdfViewerController* pdfController);

    [[nodiscard]] MediaKind mediaKind() const;
    [[nodiscard]] QUrl source() const;
    [[nodiscard]] QString mediaTitle() const;

    [[nodiscard]] bool isStandbyActive() const;
    [[nodiscard]] bool isBlackoutActive() const;
    void setStandbyActive(bool active);
    [[nodiscard]] QUrl standbyImageUrl() const;
    void setStandbyImageUrl(const QUrl& url);

    [[nodiscard]] bool isPaused() const;
    void setPaused(bool paused);
    [[nodiscard]] bool isActivePresentation() const;

    [[nodiscard]] qint64 positionMs() const;
    [[nodiscard]] qint64 durationMs() const;
    [[nodiscard]] qint64 trimStartMs() const;
    [[nodiscard]] qint64 trimEndMs() const;

    [[nodiscard]] double zoomScale() const;
    void setZoomScale(double zoomScale);
    [[nodiscard]] double panX() const;
    void setPanX(double panX);
    [[nodiscard]] double panY() const;
    void setPanY(double panY);

    [[nodiscard]] int volume() const;
    void setVolume(int volume);
    [[nodiscard]] double speed() const;
    [[nodiscard]] int previewSerial() const;
    [[nodiscard]] int previewFrameWidth() const;
    void setPreviewFrameWidth(int width);
    [[nodiscard]] QImage previewFrame() const;
    [[nodiscard]] bool isPreviewActive() const;
    // Frames are grabbed only while a video plays and someone shows the preview.
    void setPreviewActive(bool active);
    void setSpeed(double speed);
    [[nodiscard]] bool isMuted() const;
    void setMuted(bool muted);
    [[nodiscard]] QString audioDevice() const;
    [[nodiscard]] QVariantList audioDevices() const;
    [[nodiscard]] QString audioDeviceDescription() const;
    void setAudioDevice(const QString& deviceId);
    [[nodiscard]] bool areSubtitlesVisible() const;
    void setSubtitlesVisible(bool visible);
    [[nodiscard]] QVariantList subtitleTracks() const;
    [[nodiscard]] int currentSubtitleTrack() const;
    void setCurrentSubtitleTrack(int trackId);

    [[nodiscard]] QString formattedPosition() const;
    [[nodiscard]] QString formattedDuration() const;

    [[nodiscard]] static MediaKind detectMediaKind(const QUrl& url, bool fallbackToVideo = true);
    [[nodiscard]] static std::pair<qint64, qint64> normalizeTrimRange(qint64 startMs, qint64 endMs);

public slots:
    void loadMedia(const QUrl& url, const QString& title = {}, qint64 trimStartMs = 0,
                   qint64 trimEndMs = 0);
    void loadMediaWithKind(const QUrl& url, MediaKind kind, const QString& title = {},
                           qint64 trimStartMs = 0, qint64 trimEndMs = 0);
    void play();
    void pause();
    // Covers the presentation with black, without the standby picture, and pauses the media.
    void blackout();
    void togglePause();
    void stop();
    void seekMs(qint64 positionMs);
    void setFallbackDurationMs(qint64 durationMs);
    void setTrimRange(qint64 startMs, qint64 endMs);
    void setViewport(double zoomScale, double panX, double panY);
    void resetViewport();
    void toggleStandby();
    void toggleSubtitles();

signals:
    void mediaChanged();
    void standbyChanged();
    void transportChanged();
    void speedChanged();
    void previewFrameChanged();
    void previewFrameWidthChanged();
    void previewActiveChanged();
    void positionChanged();
    void durationChanged();
    void trimChanged();
    void viewportChanged();
    void audioChanged();
    void subtitlesChanged();
    void tracksChanged();
    void playbackFinished();
    void playbackError(const QString& message);

private:
    void syncViewportToActiveTarget();

    QPointer<MpvRendererItem> m_mpvItem;
    QPointer<PdfViewerController> m_pdfController;

    MediaKind m_mediaKind{MediaKind::None};
    QUrl m_source;
    QString m_mediaTitle;
    bool m_standbyActive{false};
    bool m_blackoutActive{false};
    QUrl m_standbyImageUrl;
    bool m_paused{true};
    qint64 m_positionMs{0};
    qint64 m_durationMs{0};
    qint64 m_trimStartMs{0};
    qint64 m_trimEndMs{0};
    double m_zoomScale{1.0};
    double m_panX{0.0};
    double m_panY{0.0};
    int m_volume{100};
    double m_speed{1.0};
    void grabPreviewFrame();
    void storePreviewFrame(const QImage& frame);
    void clearPreviewFrame();
    QTimer m_previewTimer;
    mutable QMutex m_previewMutex;
    QImage m_previewFrame;
    int m_previewSerial{0};
    int m_previewFrameWidth{480};
    bool m_previewActive{false};
    bool m_muted{false};
    QString m_audioDevice{QStringLiteral("auto")};
    bool m_subtitlesVisible{true};
};

} // namespace kmedia
