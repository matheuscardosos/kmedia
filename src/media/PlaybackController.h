#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

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
    Q_PROPERTY(bool paused READ isPaused WRITE setPaused NOTIFY transportChanged FINAL)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged FINAL)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged FINAL)
    Q_PROPERTY(qint64 trimStartMs READ trimStartMs NOTIFY trimChanged FINAL)
    Q_PROPERTY(qint64 trimEndMs READ trimEndMs NOTIFY trimChanged FINAL)
    Q_PROPERTY(double zoomScale READ zoomScale WRITE setZoomScale NOTIFY viewportChanged FINAL)
    Q_PROPERTY(double panX READ panX WRITE setPanX NOTIFY viewportChanged FINAL)
    Q_PROPERTY(double panY READ panY WRITE setPanY NOTIFY viewportChanged FINAL)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY audioChanged FINAL)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY audioChanged FINAL)
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
    void setStandbyActive(bool active);
    [[nodiscard]] QUrl standbyImageUrl() const;
    void setStandbyImageUrl(const QUrl& url);

    [[nodiscard]] bool isPaused() const;
    void setPaused(bool paused);

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
    [[nodiscard]] bool isMuted() const;
    void setMuted(bool muted);

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
    void togglePause();
    void stop();
    void seekMs(qint64 positionMs);
    void setTrimRange(qint64 startMs, qint64 endMs);
    void setViewport(double zoomScale, double panX, double panY);
    void resetViewport();
    void toggleStandby();

signals:
    void mediaChanged();
    void standbyChanged();
    void transportChanged();
    void positionChanged();
    void durationChanged();
    void trimChanged();
    void viewportChanged();
    void audioChanged();
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
    bool m_muted{false};
};

} // namespace kmedia
