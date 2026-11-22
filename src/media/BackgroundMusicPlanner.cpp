#include "media/BackgroundMusicPlanner.h"

#include <QRandomGenerator>

#include <algorithm>
#include <cmath>
#include <utility>

namespace kmedia {

std::vector<BackgroundSong>
BackgroundMusicPlanner::weightedShuffle(std::vector<BackgroundSong> songs, QRandomGenerator& random)
{
    int maxTrack = 1;
    for (const BackgroundSong& song : songs) {
        maxTrack = std::max(maxTrack, song.track);
    }
    const double bias = static_cast<double>(maxTrack) * 0.5;

    std::vector<std::pair<double, BackgroundSong>> keyed;
    keyed.reserve(songs.size());
    for (BackgroundSong& song : songs) {
        const double normalizedTrack =
            static_cast<double>(std::max(0, song.track)) / static_cast<double>(maxTrack);
        const double weight = 1.0 + (normalizedTrack * bias);
        keyed.emplace_back(std::pow(random.generateDouble(), 1.0 / weight), std::move(song));
    }
    std::stable_sort(keyed.begin(), keyed.end(),
                     [](const auto& left, const auto& right) { return left.first > right.first; });

    std::vector<BackgroundSong> ordered;
    ordered.reserve(keyed.size());
    for (auto& entry : keyed) {
        ordered.push_back(std::move(entry.second));
    }
    return ordered;
}

BackgroundMusicPlanner::MeetingQueue
BackgroundMusicPlanner::meetingQueue(const std::vector<BackgroundSong>& library,
                                     const QList<int>& meetingTracks, double secondsAvailable)
{
    MeetingQueue result;
    if (secondsAvailable <= 0.0) {
        return result;
    }

    std::vector<BackgroundSong> others;
    for (const BackgroundSong& song : library) {
        if (song.durationSeconds > 0.0 && !meetingTracks.contains(song.track)) {
            others.push_back(song);
        }
    }
    std::vector<BackgroundSong> meetingSongs;
    for (const int track : meetingTracks) {
        const auto found = std::find_if(library.begin(), library.end(), [track](const auto& song) {
            return song.track == track && song.durationSeconds > 0.0;
        });
        const bool alreadyIn =
            std::any_of(meetingSongs.begin(), meetingSongs.end(),
                        [track](const BackgroundSong& song) { return song.track == track; });
        if (found != library.end() && !alreadyIn) {
            meetingSongs.push_back(*found);
            meetingSongs.back().meetingSong = true;
        }
    }

    // Walk back from the moment the music stops: the meeting songs from the last one, then
    // the rest of the library, cycling until the available time is covered.
    std::vector<BackgroundSong> backwards;
    double total = 0.0;
    for (auto it = meetingSongs.rbegin(); it != meetingSongs.rend() && total < secondsAvailable;
         ++it) {
        backwards.push_back(*it);
        total += it->durationSeconds;
    }
    if (!others.empty()) {
        std::size_t next = 0;
        while (total < secondsAvailable) {
            const BackgroundSong& song = others.at(next % others.size());
            backwards.push_back(song);
            total += song.durationSeconds;
            ++next;
        }
    }
    if (backwards.empty()) {
        return result;
    }

    result.songs.assign(backwards.rbegin(), backwards.rend());
    result.startOffsetSeconds = std::max(0.0, total - secondsAvailable);
    return result;
}

} // namespace kmedia
