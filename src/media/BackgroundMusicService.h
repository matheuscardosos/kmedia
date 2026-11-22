#pragma once

#include "media/BackgroundMusicPlanner.h"

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QTimer>

#include <functional>
#include <optional>
#include <vector>

namespace kmedia {

class BackgroundMusicEngine;
class BackgroundMusicLibrary;

/**
 * Plays the songbook as background music, inspired by Meeting Media Manager. Before a meeting of
 * the same day the queue is timed to end when the meeting starts, with that meeting's songs last;
 * at any other time the whole songbook plays in a loop. With auto start on, the music starts by
 * itself in the hour and a quarter before a meeting, unless it was stopped by hand for it.
 */
class BackgroundMusicService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool autoStart READ isAutoStart WRITE setAutoStart NOTIFY autoStartChanged FINAL)
    Q_PROPERTY(bool loading READ isLoading NOTIFY stateChanged FINAL)
    // When a timed queue ends (the meeting start), or invalid while the songbook loops.
    Q_PROPERTY(QDateTime stopTime READ stopTime NOTIFY stateChanged FINAL)

public:
    struct Meeting
    {
        QDateTime start;
        QList<int> songTracks; // the day's songs, in meeting order
    };
    using MeetingProvider = std::function<std::optional<Meeting>(const QDate& date)>;
    using Clock = std::function<QDateTime()>;

    static constexpr int kAutoStartWindowSeconds = 75 * 60;
    static constexpr int kAutoStartMinimumSeconds = 90;
    // After an automatic start that could not load the songbook.
    static constexpr int kAutoStartRetrySeconds = 5 * 60;

    BackgroundMusicService(BackgroundMusicEngine* engine, BackgroundMusicLibrary* library,
                           QObject* parent = nullptr);

    void setMeetingProvider(MeetingProvider provider);
    void setClock(Clock clock);
    // Meeting media playing keeps the music from starting by itself.
    void setMediaPlaying(bool playing);

    [[nodiscard]] bool isAutoStart() const;
    void setAutoStart(bool enabled);
    [[nodiscard]] bool isLoading() const;
    [[nodiscard]] QDateTime stopTime() const;
    [[nodiscard]] bool shouldAutoStart() const;

    Q_INVOKABLE void start();
    // Fades out; the music then waits for a hand start before the same meeting.
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();
    void checkAutoStart();

signals:
    void autoStartChanged();
    void stateChanged();
    // Only a start by hand reports that the songbook could not be loaded.
    void startFailed();

private:
    void startMusic(bool byHand);
    void startQueue(const std::vector<BackgroundSong>& songs);
    [[nodiscard]] std::optional<Meeting> upcomingMeetingToday() const;

    BackgroundMusicEngine* m_engine{nullptr};
    BackgroundMusicLibrary* m_library{nullptr};
    MeetingProvider m_meetingProvider;
    Clock m_clock;
    QTimer m_autoStartTimer;
    std::vector<BackgroundSong> m_queue;
    QDateTime m_stopTime;
    QDateTime m_stoppedByHandFor;
    QDateTime m_autoStartRetryAt;
    bool m_autoStart{false};
    bool m_loading{false};
    bool m_mediaPlaying{false};
};

} // namespace kmedia
