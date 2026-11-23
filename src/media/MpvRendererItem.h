#pragma once

#include <QImage>
#include <QQuickFramebufferObject>
#include <QSharedPointer>
#include <QString>
#include <QUrl>
#include <QVariantList>

#include <memory>

struct mpv_handle;

class QQuickItemGrabResult;

class MpvRendererItem : public QQuickFramebufferObject
{
    Q_OBJECT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged FINAL)
    Q_PROPERTY(bool paused READ isPaused WRITE setPaused NOTIFY pausedChanged FINAL)
    Q_PROPERTY(bool mediaLoaded READ isMediaLoaded NOTIFY mediaLoadedChanged FINAL)
    Q_PROPERTY(QString hardwareDecoder READ hardwareDecoder NOTIFY hardwareDecoderChanged FINAL)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged FINAL)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged FINAL)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged FINAL)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY mutedChanged FINAL)
    Q_PROPERTY(
        QString audioDevice READ audioDevice WRITE setAudioDevice NOTIFY audioDeviceChanged FINAL)
    Q_PROPERTY(QVariantList audioDevices READ audioDevices NOTIFY audioDevicesChanged FINAL)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY tracksChanged FINAL)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY tracksChanged FINAL)
    Q_PROPERTY(int currentAudioTrack READ currentAudioTrack WRITE setCurrentAudioTrack NOTIFY
                   tracksChanged FINAL)
    Q_PROPERTY(int currentSubtitleTrack READ currentSubtitleTrack WRITE setCurrentSubtitleTrack
                   NOTIFY tracksChanged FINAL)
    Q_PROPERTY(bool subtitlesVisible READ areSubtitlesVisible WRITE setSubtitlesVisible NOTIFY
                   tracksChanged FINAL)
    Q_PROPERTY(double zoomScale READ zoomScale WRITE setZoomScale NOTIFY viewportChanged FINAL)
    Q_PROPERTY(double panX READ panX WRITE setPanX NOTIFY viewportChanged FINAL)
    Q_PROPERTY(double panY READ panY WRITE setPanY NOTIFY viewportChanged FINAL)

public:
    struct WakeupBridge;

    explicit MpvRendererItem(QQuickItem* parent = nullptr);
    ~MpvRendererItem() override;

    Q_DISABLE_COPY_MOVE(MpvRendererItem)

    [[nodiscard]] Renderer* createRenderer() const override;

    [[nodiscard]] QUrl source() const;
    void setSource(const QUrl& source);

    [[nodiscard]] bool isPaused() const;
    void setPaused(bool paused);

    [[nodiscard]] bool isMediaLoaded() const;
    [[nodiscard]] QString hardwareDecoder() const;

    [[nodiscard]] qint64 positionMs() const;
    [[nodiscard]] qint64 durationMs() const;

    [[nodiscard]] int volume() const;
    void setVolume(int volume);
    // Playback speed, 1.0 for normal.
    void setSpeed(double speed);
    // Asks for the frame on screen, rendered at most `maxWidth` wide; it arrives through
    // videoFrameReady. Asynchronous, and only while the item is on screen.
    void requestVideoFrame(int maxWidth);

    [[nodiscard]] bool isMuted() const;
    void setMuted(bool muted);

    [[nodiscard]] QString audioDevice() const;
    void setAudioDevice(const QString& deviceId);
    [[nodiscard]] QVariantList audioDevices() const;

    [[nodiscard]] QVariantList audioTracks() const;
    [[nodiscard]] QVariantList subtitleTracks() const;
    [[nodiscard]] int currentAudioTrack() const;
    void setCurrentAudioTrack(int trackId);
    [[nodiscard]] int currentSubtitleTrack() const;
    void setCurrentSubtitleTrack(int trackId);
    [[nodiscard]] bool areSubtitlesVisible() const;
    void setSubtitlesVisible(bool visible);

    [[nodiscard]] double zoomScale() const;
    void setZoomScale(double zoomScale);
    [[nodiscard]] double panX() const;
    void setPanX(double panX);
    [[nodiscard]] double panY() const;
    void setPanY(double panY);

    [[nodiscard]] std::shared_ptr<mpv_handle> sharedHandle() const;

    [[nodiscard]] static double zoomScaleToMpvLog2(double zoomScale);

public slots:
    void loadFile(const QUrl& url);
    void loadMedia(const QUrl& url, qint64 trimStartMs, qint64 trimEndMs);
    void play();
    void pause();
    void togglePause();
    void stop();
    void seekMs(qint64 positionMs);
    void seekRelativeMs(qint64 deltaMs);
    void setTrimRange(qint64 trimStartMs, qint64 trimEndMs);
    void resetViewport();

signals:
    void sourceChanged();
    void pausedChanged();
    void mediaLoadedChanged();
    void videoFrameReady(const QImage& frame);
    void hardwareDecoderChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void mutedChanged();
    void audioDeviceChanged();
    void audioDevicesChanged();
    void tracksChanged();
    void viewportChanged();
    void playbackFinished();
    void playbackError(const QString& message);

private slots:
    void processMpvEvents();

private:
    void initializeMpv();
    void applyTrimProperties(qint64 trimStartMs, qint64 trimEndMs);
    void handleTrackListNode(const void* nodePtr);
    void handleAudioDeviceListNode(const void* nodePtr);

    std::shared_ptr<mpv_handle> m_mpv;
    std::shared_ptr<WakeupBridge> m_wakeupBridge;
    QUrl m_source;
    bool m_paused{true};
    bool m_mediaLoaded{false};
    bool m_frameRequested{false};
    QSharedPointer<QQuickItemGrabResult> m_frameGrab;
    QString m_hardwareDecoder{QStringLiteral("auto-safe")};
    qint64 m_positionMs{0};
    qint64 m_durationMs{0};
    int m_volume{100};
    double m_speed{1.0};
    bool m_muted{false};
    QString m_audioDevice{QStringLiteral("auto")};
    QVariantList m_audioDevices;
    QVariantList m_audioTracks;
    QVariantList m_subtitleTracks;
    int m_currentAudioTrack{1};
    int m_currentSubtitleTrack{0};
    bool m_subtitlesVisible{true};
    double m_zoomScale{1.0};
    double m_panX{0.0};
    double m_panY{0.0};
};
