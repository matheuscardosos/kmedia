#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QTimer>
#include <QUrl>

#include <memory>

struct mpv_handle;

namespace kmedia {

struct MeetingScheduleSpec
{
    bool enabled{false};
    int dayOfWeek{1}; // Qt::DayOfWeek (1=Monday .. 7=Sunday)
    QTime startTime;
};

class BackgroundMusicEngine : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool playing READ isPlaying NOTIFY playbackStateChanged FINAL)
    Q_PROPERTY(bool fading READ isFading NOTIFY fadeStateChanged FINAL)
    Q_PROPERTY(bool shuffle READ isShuffle WRITE setShuffle NOTIFY queueChanged FINAL)
    Q_PROPERTY(int targetVolume READ targetVolume WRITE setTargetVolume NOTIFY volumeChanged FINAL)
    Q_PROPERTY(int currentVolume READ currentVolume NOTIFY volumeChanged FINAL)
    Q_PROPERTY(
        int fadeDurationMs READ fadeDurationMs WRITE setFadeDurationMs NOTIFY scheduleChanged FINAL)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY queueChanged FINAL)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY queueChanged FINAL)
    Q_PROPERTY(QString currentTrackTitle READ currentTrackTitle NOTIFY queueChanged FINAL)
    Q_PROPERTY(bool scheduleEnabled READ isScheduleEnabled NOTIFY scheduleChanged FINAL)

public:
    struct WakeupBridge;

    explicit BackgroundMusicEngine(QObject* parent = nullptr);
    ~BackgroundMusicEngine() override;

    Q_DISABLE_COPY_MOVE(BackgroundMusicEngine)

    [[nodiscard]] bool initialize(bool nullAudioOutput = false);
    [[nodiscard]] bool isInitialized() const;

    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] bool isFading() const;
    [[nodiscard]] bool isShuffle() const;
    void setShuffle(bool shuffle);

    [[nodiscard]] int targetVolume() const;
    void setTargetVolume(int volume);
    [[nodiscard]] int currentVolume() const;

    [[nodiscard]] int fadeDurationMs() const;
    void setFadeDurationMs(int durationMs);

    [[nodiscard]] int currentIndex() const;
    [[nodiscard]] int trackCount() const;
    [[nodiscard]] QList<QUrl> playlist() const;
    [[nodiscard]] QString currentTrackTitle() const;

    [[nodiscard]] bool isScheduleEnabled() const;
    [[nodiscard]] bool isOperatorOverrideActive() const;
    [[nodiscard]] QDateTime nextFadeTrigger() const;

    void setAudioDevice(const QString& deviceId);
    void configureSchedules(const MeetingScheduleSpec& midweek, const MeetingScheduleSpec& weekend,
                            int fadeDurationMs = 5000);
    void resetOperatorOverride();

    [[nodiscard]] static QDateTime computeNextFadeTrigger(const QDateTime& now,
                                                          const MeetingScheduleSpec& midweek,
                                                          const MeetingScheduleSpec& weekend,
                                                          int fadeDurationMs);

    // A queue with titles: the first song starts `firstTrackStartSeconds` in, and without
    // `loop` the music stops after the last song instead of starting over.
    void setQueue(const QList<QUrl>& tracks, const QStringList& titles,
                  double firstTrackStartSeconds, bool loop);
    [[nodiscard]] bool isLooping() const;

public slots:
    void setPlaylist(const QList<QUrl>& tracks);
    void addTrack(const QUrl& track);
    void clearPlaylist();
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void fadeOutAndStop(int durationMs = -1);

signals:
    void playbackStateChanged();
    void fadeStateChanged();
    void queueChanged();
    void volumeChanged();
    void scheduleChanged();
    void scheduledFadeTriggered();
    void fadeOutCompleted();
    void trackStarted(int index);
    void queueFinished();
    void engineError(const QString& message);

private slots:
    void processMpvEvents();
    void onFadeTick();
    void onScheduledFadeTimeout();

private:
    void loadTrackAtIndex(int index);
    void applyMpvVolume(int volume);
    void updateScheduleTimer();
    void stopFadeTimer();

    std::shared_ptr<mpv_handle> m_mpv;
    std::shared_ptr<WakeupBridge> m_wakeupBridge;
    QList<QUrl> m_playlist;
    QStringList m_titles;
    double m_firstTrackStartSeconds{0.0};
    bool m_loop{true};
    int m_currentIndex{-1};
    bool m_playing{false};
    bool m_fading{false};
    bool m_shuffle{false};
    int m_targetVolume{70};
    int m_currentVolume{70};
    int m_fadeStartVolume{70};
    int m_fadeDurationMs{5000};
    int m_activeFadeDurationMs{5000};
    qint64 m_fadeElapsedMs{0};
    bool m_operatorOverrideActive{false};

    MeetingScheduleSpec m_midweekSchedule;
    MeetingScheduleSpec m_weekendSchedule;
    QDateTime m_nextFadeTrigger;

    QTimer m_fadeTimer;
    QTimer m_scheduleTimer;
};

} // namespace kmedia
