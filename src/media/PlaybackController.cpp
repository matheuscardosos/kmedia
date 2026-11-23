#include "media/PlaybackController.h"

#include "core/SystemUtils.h"
#include "media/MpvRendererItem.h"
#include "media/PdfViewerController.h"

#include <QFileInfo>

namespace kmedia {

PlaybackController::PlaybackController(QObject* parent) : QObject(parent) {}

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

    connect(m_mpvItem.data(), &MpvRendererItem::pausedChanged, this, [this]() {
        if (m_mpvItem.isNull()) {
            return;
        }
        const bool pausedNow = m_mpvItem->isPaused();
        if (m_paused != pausedNow) {
            m_paused = pausedNow;
            emit transportChanged();
        }
    });

    connect(m_mpvItem.data(), &MpvRendererItem::positionChanged, this, [this]() {
        if (m_mpvItem.isNull()) {
            return;
        }
        m_positionMs = m_mpvItem->positionMs();
        emit positionChanged();
    });

    connect(m_mpvItem.data(), &MpvRendererItem::durationChanged, this, [this]() {
        if (m_mpvItem.isNull()) {
            return;
        }
        m_durationMs = m_mpvItem->durationMs();
        emit durationChanged();
    });

    connect(m_mpvItem.data(), &MpvRendererItem::tracksChanged, this, [this]() {
        if (m_mpvItem.isNull()) {
            return;
        }
        const bool subVisible = m_mpvItem->areSubtitlesVisible();
        if (m_subtitlesVisible != subVisible) {
            m_subtitlesVisible = subVisible;
            emit subtitlesChanged();
        }
        emit tracksChanged();
    });

    connect(m_mpvItem.data(), &MpvRendererItem::audioDevicesChanged, this,
            &PlaybackController::audioChanged);
    connect(m_mpvItem.data(), &MpvRendererItem::playbackFinished, this,
            &PlaybackController::playbackFinished);
    connect(m_mpvItem.data(), &MpvRendererItem::playbackError, this,
            &PlaybackController::playbackError);

    m_mpvItem->setVolume(m_volume);
    m_mpvItem->setSpeed(m_speed);
    m_mpvItem->setMuted(m_muted);
    m_mpvItem->setAudioDevice(m_audioDevice);
    m_mpvItem->setSubtitlesVisible(m_subtitlesVisible);
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
    if (!m_mpvItem.isNull()) {
        m_mpvItem->setPaused(paused);
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
    emit audioChanged();
}

QString PlaybackController::audioDevice() const
{
    return m_audioDevice;
}

QVariantList PlaybackController::audioDevices() const
{
    return m_mpvItem.isNull() ? QVariantList{} : m_mpvItem->audioDevices();
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
    emit subtitlesChanged();
}

QVariantList PlaybackController::subtitleTracks() const
{
    return m_mpvItem.isNull() ? QVariantList{} : m_mpvItem->subtitleTracks();
}

int PlaybackController::currentSubtitleTrack() const
{
    return m_mpvItem.isNull() ? 0 : m_mpvItem->currentSubtitleTrack();
}

void PlaybackController::setCurrentSubtitleTrack(int trackId)
{
    if (!m_mpvItem.isNull()) {
        m_mpvItem->setCurrentSubtitleTrack(trackId);
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

    m_zoomScale = 1.0;
    m_panX = 0.0;
    m_panY = 0.0;

    if (kind == MediaKind::Pdf) {
        if (!m_mpvItem.isNull()) {
            m_mpvItem->stop();
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
        if (!m_pdfController.isNull()) {
            m_pdfController->close();
        }
        setPaused(true);
    } else {
        if (!m_pdfController.isNull()) {
            m_pdfController->close();
        }
        if (!m_mpvItem.isNull()) {
            m_mpvItem->resetViewport();
            m_mpvItem->loadMedia(url, cleanStart, cleanEnd);
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
    if (!m_pdfController.isNull()) {
        m_pdfController->close();
    }

    m_mediaKind = MediaKind::None;
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
    if (!m_mpvItem.isNull()) {
        m_mpvItem->seekMs(clamped);
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
    if (!m_mpvItem.isNull()) {
        m_mpvItem->setTrimRange(cleanStart, cleanEnd);
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
    } else if (!m_mpvItem.isNull() &&
               (m_mediaKind == MediaKind::Video || m_mediaKind == MediaKind::Audio)) {
        m_mpvItem->setZoomScale(m_zoomScale);
        m_mpvItem->setPanX(m_panX);
        m_mpvItem->setPanY(m_panY);
    }
}

} // namespace kmedia
