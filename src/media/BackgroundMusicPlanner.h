#pragma once

#include <QList>
#include <QString>

#include <vector>

class QRandomGenerator;

namespace kmedia {

/** One song of the songbook as background music plays it. */
struct BackgroundSong
{
    int track{0};
    QString title;
    double durationSeconds{0.0};
    QString remoteUrl;
    QString checksum;
    qint64 fileSize{0};
    // One of the songs of the day's meeting, played last before it starts.
    bool meetingSong{false};
};

/**
 * Orders the songbook for background music, inspired by Meeting Media Manager: a shuffle that
 * slightly favors newer songs, and before a meeting a queue that ends exactly when the music has
 * to stop, with the songs of that meeting played last.
 */
class BackgroundMusicPlanner
{
public:
    BackgroundMusicPlanner() = delete;

    struct MeetingQueue
    {
        std::vector<BackgroundSong> songs;
        // How far into the first song to start, so the last one ends on time.
        double startOffsetSeconds{0.0};
    };

    // Weighted random order (key = u^(1/w)): a song's weight grows with its track number, so
    // newer songs tend to come first without leaving the older ones out.
    [[nodiscard]] static std::vector<BackgroundSong>
    weightedShuffle(std::vector<BackgroundSong> songs, QRandomGenerator& random);

    // Fills `secondsAvailable` with songs from `library` in order, cycling if needed, with the
    // songs whose tracks are in `meetingTracks` played last in meeting order. Songs without a
    // known duration are left out, since they cannot be timed.
    [[nodiscard]] static MeetingQueue meetingQueue(const std::vector<BackgroundSong>& library,
                                                   const QList<int>& meetingTracks,
                                                   double secondsAvailable);
};

} // namespace kmedia
