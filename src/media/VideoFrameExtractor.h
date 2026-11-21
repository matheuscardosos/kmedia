#pragma once

#include <QImage>
#include <QString>

namespace kmedia {

/**
 * Reads one still picture from a video or audio file with a headless mpv instance: the frame five
 * seconds in (the first frame for shorter videos), or the cover art of an audio file. Each call
 * uses its own mpv instance, so it is safe to call from worker threads.
 */
class VideoFrameExtractor
{
public:
    VideoFrameExtractor() = delete;

    static constexpr double kFrameSeconds = 5.0;
    static constexpr int kTimeoutMs = 10000;

    [[nodiscard]] static QImage extract(const QString& filePath, int timeoutMs = kTimeoutMs);
};

} // namespace kmedia
