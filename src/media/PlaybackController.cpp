#include "media/PlaybackController.h"

#include "core/SystemUtils.h"
#include "media/MpvRendererItem.h"
#include "media/PdfViewerController.h"

#include <QFileInfo>

namespace kmedia {

PlaybackController::PlaybackController(QObject* parent) : QObject(parent)
{
    // About four preview frames a second: enough to follow a video, cheap to grab.
    m_previewTimer.setInterval(250);
    connect(&m_previewTimer, &QTimer::timeout, this, &PlaybackController::grabPreviewFrame);
}

int PlaybackController::previewSerial() const
{
    return m_previewSerial;
}

QImage PlaybackController::previewFrame() const
{
    const QMutexLocker lock(&m_previewMutex);
    return m_previewFrame;
}

bool PlaybackController::isPreviewActive() const
{
    return m_previewActive;
}

void PlaybackController::setPreviewActive(bool active)
{
    if (m_previewActive == active) {
        return;
    }
    m_previewActive = active;
    if (active) {
        m_previewTimer.start();
    } else {
        m_previewTimer.stop();
    }
    emit previewActiveChanged();
}

int PlaybackController::previewFrameWidth() const
{
    return m_previewFrameWidth;
}

void PlaybackController::setPreviewFrameWidth(int width)
{
    width = qBound(64, width, 3840);
    if (m_previewFrameWidth == width) {
        return;
    }
    m_previewFrameWidth = width;
    emit previewFrameWidthChanged();
}

void PlaybackController::grabPreviewFrame()
{
    if (m_mediaKind != MediaKind::Video || activeMpvTarget() != m_mpvItem.data() ||
        m_mpvItem.isNull()) {
        return;
    }
    m_mpvItem->requestVideoFrame(m_previewFrameWidth);
}

void PlaybackController::storePreviewFrame(const QImage& frame)
{
    {
        const QMutexLocker lock(&m_previewMutex);
        m_previewFrame = frame;
    }
    ++m_previewSerial;
    emit previewFrameChanged();
}

void PlaybackController::clearPreviewFrame()
{
    {
        const QMutexLocker lock(&m_previewMutex);
        m_previewFrame = {};
    }
    if (m_previewSerial != 0) {
        m_previewSerial = 0;
        emit previewFrameChanged();
    }
}

MpvRendererItem* PlaybackController::activeMpvTarget() const
{
    if (m_presentationVisible && !m_mpvItem.isNull()) {
        return m_mpvItem.data();
    }
    if (!m_previewMpvItem.isNull()) {
        return m_previewMpvItem.data();
    }
    return m_mpvItem.data();
}

void PlaybackController::applySettingsToTarget(MpvRendererItem* item) const
{
    if (item == nullptr) {
        return;
    }
    item->setVolume(m_volume);
    item->setSpeed(m_speed);
    item->setMuted(m_muted);
    item->setAudioDevice(m_audioDevice);
    item->setSubtitlesVisible(m_subtitlesVisible);
    item->setZoomScale(m_zoomScale);
    item->setPanX(m_panX);
    item->setPanY(m_panY);
}

void PlaybackController::bindMpvSignals(MpvRendererItem* item)
{
    if (item == nullptr) {
        return;
    }

    connect(item, &MpvRendererItem::pausedChanged, this, [this, item]() {
        if (activeMpvTarget() != item) {
            return;
        }
        const bool pausedNow = item->isPaused();
        if (m_paused != pausedNow) {
            m_paused = pausedNow;
            emit transportChanged();
        }
    });

    connect(item, &MpvRendererItem::positionChanged, this, [this, item]() {
        if (activeMpvTarget() != item) {
            return;
        }
        m_positionMs = item->positionMs();
        emit positionChanged();
    });

    connect(item, &MpvRendererItem::durationChanged, this, [this, item]() {
        if (activeMpvTarget() != item) {
            return;
        }
        m_durationMs = item->durationMs();
        emit durationChanged();
    });

    connect(item, &MpvRendererItem::tracksChanged, this, [this, item]() {
        if (activeMpvTarget() != item) {
            return;
        }
        const bool subVisible = item->areSubtitlesVisible();
        if (m_subtitlesVisible != subVisible) {
            m_subtitlesVisible = subVisible;
            emit subtitlesChanged();
        }
        emit tracksChanged();
    });

    connect(item, &MpvRendererItem::videoFrameReady, this, [this, item](const QImage& frame) {
        if (activeMpvTarget() == item) {
            storePreviewFrame(frame);
        }
    });
    connect(item, &MpvRendererItem::audioDevicesChanged, this, [this, item]() {
        if (activeMpvTarget() == item) {
            emit audioChanged();
        }
    });
    connect(item, &MpvRendererItem::playbackFinished, this, [this, item]() {
        if (activeMpvTarget() == item) {
            emit playbackFinished();
        }
    });
    connect(item, &MpvRendererItem::playbackError, this, [this, item](const QString& message) {
        if (activeMpvTarget() == item) {
            emit playbackError(message);
        }
    });
}

void PlaybackController::attachMpvItem(MpvRendererItem* mpvItem)
{
    if (m_mpvItem == mpvItem) {
        return;
    }
    if (!m_mpvItem.isNull()) {
        disconnect(m_mpvItem.data(), nullptr, this, nullptr);
    }

    m_mpvItem = mpvItem;
    if (m_mpvItem.isNull()) {
        return;
    }

    bindMpvSignals(m_mpvItem.data());
    applySettingsToTarget(m_mpvItem.data());
}

void PlaybackController::attachPreviewMpvItem(MpvRendererItem* mpvItem)
{
    if (m_previewMpvItem == mpvItem) {
        return;
    }
    if (!m_previewMpvItem.isNull()) {
        disconnect(m_previewMpvItem.data(), nullptr, this, nullptr);
    }

    m_previewMpvItem = mpvItem;
    if (m_previewMpvItem.isNull()) {
        return;
    }

    bindMpvSignals(m_previewMpvItem.data());
    applySettingsToTarget(m_previewMpvItem.data());

    if (!m_presentationVisible &&
        (m_mediaKind == MediaKind::Video || m_mediaKind == MediaKind::Audio) &&
        m_source.isValid() && !m_source.isEmpty()) {
        const qint64 resumePos = m_positionMs;
        const bool wasPaused = m_paused;
        if (!m_mpvItem.isNull()) {
            m_mpvItem->stop();
        }
        m_previewMpvItem->setPaused(wasPaused);
        m_previewMpvItem->loadMedia(m_source, m_trimStartMs, m_trimEndMs,
                                    m_mediaKind == MediaKind::Video);
        if (resumePos > m_trimStartMs) {
            m_previewMpvItem->seekMs(resumePos);
        }
    }
}

void PlaybackController::detachPreviewMpvItem(MpvRendererItem* mpvItem)
{
    if (mpvItem == nullptr || m_previewMpvItem != mpvItem) {
        return;
    }
    disconnect(m_previewMpvItem.data(), nullptr, this, nullptr);
    m_previewMpvItem->stop();
    m_previewMpvItem.clear();
}

void PlaybackController::setPresentationVisible(bool visible)
{
    if (m_presentationVisible == visible) {
        return;
    }
    MpvRendererItem* previous = activeMpvTarget();
    m_presentationVisible = visible;
    MpvRendererItem* current = activeMpvTarget();

    if (previous != current &&
        (m_mediaKind == MediaKind::Video || m_mediaKind == MediaKind::Audio) &&
        m_source.isValid() && !m_source.isEmpty()) {
        const qint64 resumePos = m_positionMs;
        const bool wasPaused = m_paused;
        clearPreviewFrame();
        if (previous != nullptr) {
            previous->stop();
        }
        if (current != nullptr) {
            applySettingsToTarget(current);
            current->setPaused(wasPaused);
            current->loadMedia(m_source, m_trimStartMs, m_trimEndMs,
                               m_mediaKind == MediaKind::Video);
            if (resumePos > m_trimStartMs) {
                current->seekMs(resumePos);
            }
        }
    } else if (visible) {
        clearPreviewFrame();
    }

    emit audioChanged();
    emit tracksChanged();
}

void PlaybackController::attachPdfController(PdfViewerController* pdfController)
{
    m_pdfController = pdfController;
}

PlaybackController::MediaKind PlaybackController::mediaKind() const
{
    return m_mediaKind;
}

QUrl PlaybackController::source() const
{
    return m_source;
}

QString PlaybackController::mediaTitle() const
{
    return m_mediaTitle;
}

bool PlaybackController::isStandbyActive() const
{
    return m_standbyActive;
}

bool PlaybackController::isBlackoutActive() const
{
    return m_blackoutActive;
}

void PlaybackController::setStandbyActive(bool active)
{
    // Leaving standby also ends a blackout.
    const bool blackout = active && m_blackoutActive;
    if (m_standbyActive == active && m_blackoutActive == blackout) {
        return;
    }
    m_standbyActive = active;
    m_blackoutActive = blackout;
    emit standbyChanged();
}

void PlaybackController::blackout()
{
    if (!m_standbyActive || !m_blackoutActive) {
        m_standbyActive = true;
        m_blackoutActive = true;
        emit standbyChanged();
    }
    pause();
}

QUrl PlaybackController::standbyImageUrl() const
{
    return m_standbyImageUrl;
}

void PlaybackController::setStandbyImageUrl(const QUrl& url)
{
    if (m_standbyImageUrl == url) {
        return;
    }
    m_standbyImageUrl = url;
    emit standbyChanged();
}

bool PlaybackController::isPaused() const
{
    return m_paused;
}

bool PlaybackController::isActivePresentation() const
{
    if (m_mediaKind == MediaKind::None) {
        return false;
    }
    if (m_mediaKind == MediaKind::Image || m_mediaKind == MediaKind::Pdf) {
        return true;
    }
    return !m_paused;
}

void PlaybackController::setPaused(bool paused)
{
    if (m_paused != paused) {
        m_paused = paused;
        emit transportChanged();
    }
    if (MpvRendererItem* target = activeMpvTarget()) {
        target->setPaused(paused);
    }
}

qint64 PlaybackController::positionMs() const
{
    return m_positionMs;
}

qint64 PlaybackController::durationMs() const
{
    return m_durationMs;
}

qint64 PlaybackController::trimStartMs() const
{
    return m_trimStartMs;
}

qint64 PlaybackController::trimEndMs() const
{
    return m_trimEndMs;
}

double PlaybackController::zoomScale() const
{
    return m_zoomScale;
}

void PlaybackController::setZoomScale(double zoomScale)
{
    const double clamped = qBound(1.0, zoomScale, 8.0);
    if (qFuzzyCompare(m_zoomScale, clamped)) {
        return;
    }
    m_zoomScale = clamped;
    syncViewportToActiveTarget();
    emit viewportChanged();
}

double PlaybackController::panX() const
{
    return m_panX;
}

void PlaybackController::setPanX(double panX)
{
    const double clamped = qBound(-0.5, panX, 0.5);
    if (qFuzzyCompare(m_panX, clamped)) {
        return;
    }
    m_panX = clamped;
    syncViewportToActiveTarget();
    emit viewportChanged();
}

double PlaybackController::panY() const
{
    return m_panY;
}

void PlaybackController::setPanY(double panY)
{
    const double clamped = qBound(-0.5, panY, 0.5);
    if (qFuzzyCompare(m_panY, clamped)) {
        return;
    }
    m_panY = clamped;
    syncViewportToActiveTarget();
    emit viewportChanged();
}

int PlaybackController::volume() const
{
    return m_volume;
}

double PlaybackController::speed() const
{
    return m_speed;
}

void PlaybackController::setSpeed(double speed)
{
    const double clamped = qBound(0.5, speed, 15.0);
    if (qFuzzyCompare(m_speed, clamped)) {
        return;
    }
    m_speed = clamped;
    if (!m_mpvItem.isNull()) {
        m_mpvItem->setSpeed(clamped);
    }
    if (!m_previewMpvItem.isNull()) {
        m_previewMpvItem->setSpeed(clamped);
    }
    emit speedChanged();
}

void PlaybackController::setVolume(int volume)
{
    const int clamped = qBound(0, volume, 100);
    if (m_volume == clamped) {
        return;
    }
    m_volume = clamped;
    if (!m_mpvItem.isNull()) {
        m_mpvItem->setVolume(clamped);
    }
    if (!m_previewMpvItem.isNull()) {
        m_previewMpvItem->setVolume(clamped);
    }
    emit audioChanged();
}

bool PlaybackController::isMuted() const
{
    return m_muted;
}

void PlaybackController::setMuted(bool muted)
{
    if (m_muted == muted) {
        return;
    }
    m_muted = muted;
    if (!m_mpvItem.isNull()) {
        m_mpvItem->setMuted(muted);
    }
    if (!m_previewMpvItem.isNull()) {
        m_previewMpvItem->setMuted(muted);
    }
    emit audioChanged();
}

QString PlaybackController::audioDevice() const
{
    return m_audioDevice;
}

QVariantList PlaybackController::audioDevices() const
{
    if (MpvRendererItem* target = activeMpvTarget(); target != nullptr) {
        return target->audioDevices();
    }
    return {};
}

QString PlaybackController::audioDeviceDescription() const
{
    if (m_audioDevice == QStringLiteral("auto")) {
        return {};
    }
    const QVariantList devices = audioDevices();
    for (const QVariant& device : devices) {
        const QVariantMap entry = device.toMap();
        if (entry.value(QStringLiteral("name")).toString() == m_audioDevice) {
            return entry.value(QStringLiteral("description")).toString();
        }
    }
    return {};
}

void PlaybackController::setAudioDevice(const QString& deviceId)
{
    const QString cleaned =
        deviceId.trimmed().isEmpty() ? QStringLiteral("auto") : deviceId.trimmed();
    if (m_audioDevice == cleaned) {
        return;
    }
    m_audioDevice = cleaned;
    if (!m_mpvItem.isNull()) {
        m_mpvItem->setAudioDevice(cleaned);
    }
    if (!m_previewMpvItem.isNull()) {
        m_previewMpvItem->setAudioDevice(cleaned);
    }
    emit audioChanged();
}

bool PlaybackController::areSubtitlesVisible() const
{
    return m_subtitlesVisible;
}

void PlaybackController::setSubtitlesVisible(bool visible)
{
    if (m_subtitlesVisible == visible) {
        return;
    }
    m_subtitlesVisible = visible;
    if (!m_mpvItem.isNull()) {
        m_mpvItem->setSubtitlesVisible(visible);
    }
    if (!m_previewMpvItem.isNull()) {
        m_previewMpvItem->setSubtitlesVisible(visible);
    }
    emit subtitlesChanged();
}

QVariantList PlaybackController::subtitleTracks() const
{
    MpvRendererItem* target = activeMpvTarget();
    return target != nullptr ? target->subtitleTracks() : QVariantList{};
}

int PlaybackController::currentSubtitleTrack() const
{
    MpvRendererItem* target = activeMpvTarget();
    return target != nullptr ? target->currentSubtitleTrack() : 0;
}

void PlaybackController::setCurrentSubtitleTrack(int trackId)
{
    if (MpvRendererItem* target = activeMpvTarget(); target != nullptr) {
        target->setCurrentSubtitleTrack(trackId);
    }
}

QString PlaybackController::formattedPosition() const
{
    return SystemUtils::formatDurationMs(m_positionMs);
}

QString PlaybackController::formattedDuration() const
{
    return SystemUtils::formatDurationMs(m_durationMs);
}

PlaybackController::MediaKind PlaybackController::detectMediaKind(const QUrl& url,
                                                                  bool fallbackToVideo)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.path();
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QStringLiteral("mp4") || ext == QStringLiteral("mkv") ||
        ext == QStringLiteral("webm") || ext == QStringLiteral("mov") ||
        ext == QStringLiteral("avi") || ext == QStringLiteral("m4v")) {
        return MediaKind::Video;
    }
    if (ext == QStringLiteral("mp3") || ext == QStringLiteral("aac") ||
        ext == QStringLiteral("ogg") || ext == QStringLiteral("flac") ||
        ext == QStringLiteral("wav") || ext == QStringLiteral("m4a") ||
        ext == QStringLiteral("opus")) {
        return MediaKind::Audio;
    }
    if (ext == QStringLiteral("png") || ext == QStringLiteral("jpg") ||
        ext == QStringLiteral("jpeg") || ext == QStringLiteral("webp") ||
        ext == QStringLiteral("bmp") || ext == QStringLiteral("gif")) {
        return MediaKind::Image;
    }
    if (ext == QStringLiteral("pdf")) {
        return MediaKind::Pdf;
    }
    return fallbackToVideo ? MediaKind::Video : MediaKind::None;
}

std::pair<qint64, qint64> PlaybackController::normalizeTrimRange(qint64 startMs, qint64 endMs)
{
    const qint64 normalizedStart = qMax<qint64>(0, startMs);
    const qint64 normalizedEnd = (endMs > 0 && endMs > normalizedStart) ? endMs : 0;
    return {normalizedStart, normalizedEnd};
}

void PlaybackController::loadMedia(const QUrl& url, const QString& title, qint64 trimStartMs,
                                   qint64 trimEndMs)
{
    if (!url.isValid() || url.isEmpty()) {
        stop();
        return;
    }
    loadMediaWithKind(url, detectMediaKind(url), title, trimStartMs, trimEndMs);
}

void PlaybackController::loadMediaWithKind(const QUrl& url, MediaKind kind, const QString& title,
                                           qint64 trimStartMs, qint64 trimEndMs)
{
    if (!url.isValid() || url.isEmpty() || kind == MediaKind::None) {
        stop();
        return;
    }

    const auto [cleanStart, cleanEnd] = normalizeTrimRange(trimStartMs, trimEndMs);
    m_mediaKind = kind;
    m_source = url;
    m_mediaTitle =
        title.trimmed().isEmpty() ? QFileInfo(url.toLocalFile()).fileName() : title.trimmed();
    m_trimStartMs = cleanStart;
    m_trimEndMs = cleanEnd;
    m_positionMs = cleanStart;
    m_durationMs = 0;
    // The previous video's last frame must not flash in the preview.
    clearPreviewFrame();

    m_zoomScale = 1.0;
    m_panX = 0.0;
    m_panY = 0.0;

    if (kind == MediaKind::Pdf) {
        if (!m_mpvItem.isNull()) {
            m_mpvItem->stop();
        }
        if (!m_previewMpvItem.isNull()) {
            m_previewMpvItem->stop();
        }
        if (!m_pdfController.isNull()) {
            const bool loaded = m_pdfController->load(url);
            Q_UNUSED(loaded)
        }
        setPaused(true);
    } else if (kind == MediaKind::Image) {
        if (!m_mpvItem.isNull()) {
            m_mpvItem->stop();
        }
        if (!m_previewMpvItem.isNull()) {
            m_previewMpvItem->stop();
        }
        if (!m_pdfController.isNull()) {
            m_pdfController->close();
        }
        setPaused(true);
    } else {
        if (!m_pdfController.isNull()) {
            m_pdfController->close();
        }
        MpvRendererItem* target = activeMpvTarget();
        if (!m_mpvItem.isNull() && target != m_mpvItem.data()) {
            m_mpvItem->stop();
        }
        if (!m_previewMpvItem.isNull() && target != m_previewMpvItem.data()) {
            m_previewMpvItem->stop();
        }
        if (target != nullptr) {
            target->resetViewport();
            target->loadMedia(url, cleanStart, cleanEnd, kind == MediaKind::Video);
        }
    }

    emit mediaChanged();
    emit trimChanged();
    emit viewportChanged();
    emit positionChanged();
    emit durationChanged();
}

void PlaybackController::play()
{
    setPaused(false);
}

void PlaybackController::pause()
{
    setPaused(true);
}

void PlaybackController::togglePause()
{
    setPaused(!m_paused);
}

void PlaybackController::stop()
{
    if (!m_mpvItem.isNull()) {
        m_mpvItem->stop();
    }
    if (!m_previewMpvItem.isNull()) {
        m_previewMpvItem->stop();
    }
    if (!m_pdfController.isNull()) {
        m_pdfController->close();
    }

    m_mediaKind = MediaKind::None;
    clearPreviewFrame();
    m_source.clear();
    m_mediaTitle.clear();
    m_paused = true;
    m_positionMs = 0;
    m_durationMs = 0;
    m_trimStartMs = 0;
    m_trimEndMs = 0;
    m_zoomScale = 1.0;
    m_panX = 0.0;
    m_panY = 0.0;

    emit mediaChanged();
    emit transportChanged();
    emit trimChanged();
    emit viewportChanged();
    emit positionChanged();
    emit durationChanged();
}

void PlaybackController::seekMs(qint64 positionMs)
{
    const qint64 clamped = qMax<qint64>(0, positionMs);
    m_positionMs = clamped;
    if (MpvRendererItem* target = activeMpvTarget(); target != nullptr) {
        target->seekMs(clamped);
    }
    emit positionChanged();
}

void PlaybackController::setFallbackDurationMs(qint64 durationMs)
{
    const qint64 clamped = qMax<qint64>(0, durationMs);
    if (m_durationMs > 0 || clamped == 0) {
        return;
    }
    m_durationMs = clamped;
    emit durationChanged();
}

void PlaybackController::setTrimRange(qint64 startMs, qint64 endMs)
{
    const auto [cleanStart, cleanEnd] = normalizeTrimRange(startMs, endMs);
    if (m_trimStartMs == cleanStart && m_trimEndMs == cleanEnd) {
        return;
    }
    m_trimStartMs = cleanStart;
    m_trimEndMs = cleanEnd;
    if (MpvRendererItem* target = activeMpvTarget(); target != nullptr) {
        target->setTrimRange(cleanStart, cleanEnd);
    }
    emit trimChanged();
}

void PlaybackController::setViewport(double zoomScale, double panX, double panY)
{
    m_zoomScale = qBound(1.0, zoomScale, 8.0);
    m_panX = qBound(-0.5, panX, 0.5);
    m_panY = qBound(-0.5, panY, 0.5);
    syncViewportToActiveTarget();
    emit viewportChanged();
}

void PlaybackController::resetViewport()
{
    setViewport(1.0, 0.0, 0.0);
}

void PlaybackController::toggleStandby()
{
    setStandbyActive(!m_standbyActive);
}

void PlaybackController::toggleSubtitles()
{
    setSubtitlesVisible(!m_subtitlesVisible);
}

void PlaybackController::syncViewportToActiveTarget()
{
    if (m_mediaKind == MediaKind::Pdf && !m_pdfController.isNull()) {
        m_pdfController->setZoomScale(m_zoomScale);
        m_pdfController->setPanX(m_panX);
        m_pdfController->setPanY(m_panY);
    } else if (MpvRendererItem* target = activeMpvTarget();
               target != nullptr &&
               (m_mediaKind == MediaKind::Video || m_mediaKind == MediaKind::Audio)) {
        target->setZoomScale(m_zoomScale);
        target->setPanX(m_panX);
        target->setPanY(m_panY);
    }
}

} // namespace kmedia
