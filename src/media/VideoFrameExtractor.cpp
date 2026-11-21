#include "media/VideoFrameExtractor.h"

#include <QByteArray>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTemporaryDir>

#include <mpv/client.h>

#include <utility>

namespace kmedia {

namespace {

QImage grabFrame(const QString& filePath, const QString& outputDir, double startSeconds,
                 int timeoutMs)
{
    mpv_handle* mpv = mpv_create();
    if (mpv == nullptr) {
        return {};
    }
    // The image output writes every shown frame to a folder; one frame is all it shows here.
    static constexpr std::pair<const char*, const char*> kOptions[] = {
        {"config", "no"}, {"terminal", "no"}, {"load-scripts", "no"},
        {"ytdl", "no"},   {"vo", "image"},    {"vo-image-format", "png"},
        {"ao", "null"},   {"aid", "no"},      {"sid", "no"},
        {"frames", "1"},  {"hr-seek", "yes"}, {"keep-open", "no"},
    };
    for (const auto& [name, value] : kOptions) {
        mpv_set_option_string(mpv, name, value);
    }
    const QByteArray folder = QDir::toNativeSeparators(outputDir).toUtf8();
    const QByteArray start = QByteArray::number(startSeconds, 'f', 3);
    mpv_set_option_string(mpv, "vo-image-outdir", folder.constData());
    mpv_set_option_string(mpv, "start", start.constData());
    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        return {};
    }

    const QByteArray path = filePath.toUtf8();
    const char* command[] = {"loadfile", path.constData(), nullptr};
    if (mpv_command(mpv, command) >= 0) {
        QElapsedTimer timer;
        timer.start();
        bool finished = false;
        while (!finished && timer.elapsed() < timeoutMs) {
            const mpv_event* event = mpv_wait_event(mpv, 0.25);
            finished =
                event->event_id == MPV_EVENT_END_FILE || event->event_id == MPV_EVENT_SHUTDOWN;
        }
    }
    mpv_terminate_destroy(mpv);

    const QFileInfoList frames = QDir(outputDir).entryInfoList(
        {QStringLiteral("*.png")}, QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    return frames.isEmpty() ? QImage() : QImage(frames.first().absoluteFilePath());
}

} // namespace

QImage VideoFrameExtractor::extract(const QString& filePath, int timeoutMs)
{
    if (!QFileInfo(filePath).isFile()) {
        return {};
    }
    const QTemporaryDir folder;
    if (!folder.isValid()) {
        return {};
    }
    QImage frame = grabFrame(filePath, folder.path(), kFrameSeconds, timeoutMs);
    if (frame.isNull()) {
        // Shorter than the usual frame time, or a still cover: take the first frame instead.
        frame = grabFrame(filePath, folder.path(), 0.0, timeoutMs);
    }
    return frame;
}

} // namespace kmedia
