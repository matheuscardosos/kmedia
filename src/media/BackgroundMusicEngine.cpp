#include "media/BackgroundMusicEngine.h"

#include <QDate>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QRandomGenerator>

#include <mpv/client.h>

#include <cmath>
#include <mutex>
#include <utility>

namespace kmedia {

struct BackgroundMusicEngine::WakeupBridge
{
    std::mutex mutex;
    QPointer<BackgroundMusicEngine> engine;
};

namespace {

constexpr int kFadeStepIntervalMs = 50;

void onAudioWakeup(void* ctx)
{
    auto* bridge = static_cast<BackgroundMusicEngine::WakeupBridge*>(ctx);
    if (bridge == nullptr) {
        return;
    }
    std::scoped_lock lock(bridge->mutex);
    const QPointer<BackgroundMusicEngine> target = bridge->engine;
    if (target.isNull()) {
        return;
    }
    QMetaObject::invokeMethod(target.data(), "processMpvEvents", Qt::QueuedConnection);
}

} // namespace

BackgroundMusicEngine::BackgroundMusicEngine(QObject* parent)
    : QObject(parent), m_wakeupBridge(std::make_shared<WakeupBridge>())
{
    {
        std::scoped_lock lock(m_wakeupBridge->mutex);
        m_wakeupBridge->engine = this;
    }

    m_fadeTimer.setInterval(kFadeStepIntervalMs);
    connect(&m_fadeTimer, &QTimer::timeout, this, &BackgroundMusicEngine::onFadeTick);

    m_scheduleTimer.setSingleShot(true);
    connect(&m_scheduleTimer, &QTimer::timeout, this,
            &BackgroundMusicEngine::onScheduledFadeTimeout);
}

BackgroundMusicEngine::~BackgroundMusicEngine()
{
    stopFadeTimer();
    m_scheduleTimer.stop();

    if (m_wakeupBridge != nullptr) {
        std::scoped_lock lock(m_wakeupBridge->mutex);
        m_wakeupBridge->engine = nullptr;
    }
    if (m_mpv != nullptr) {
        mpv_set_wakeup_callback(m_mpv.get(), nullptr, nullptr);
    }
}

bool BackgroundMusicEngine::initialize(bool nullAudioOutput)
{
    if (m_mpv != nullptr) {
        return true;
    }

    mpv_handle* rawHandle = mpv_create();
    if (rawHandle == nullptr) {
        emit engineError(QStringLiteral("Failed to create background audio mpv instance"));
        return false;
    }

    mpv_set_option_string(rawHandle, "vo", "null");
    mpv_set_option_string(rawHandle, "vid", "no");
    mpv_set_option_string(rawHandle, "audio-display", "no");
    mpv_set_option_string(rawHandle, "terminal", "no");
    mpv_set_option_string(rawHandle, "idle", "yes");
    mpv_set_option_string(rawHandle, "keep-open", "no");
    if (nullAudioOutput) {
        mpv_set_option_string(rawHandle, "ao", "null");
    }

    if (mpv_initialize(rawHandle) < 0) {
        mpv_terminate_destroy(rawHandle);
        emit engineError(QStringLiteral("Failed to initialize background audio mpv instance"));
        return false;
    }

    m_mpv = std::shared_ptr<mpv_handle>(rawHandle, [](mpv_handle* handle) {
        if (handle != nullptr) {
            mpv_set_wakeup_callback(handle, nullptr, nullptr);
            mpv_terminate_destroy(handle);
        }
    });

    applyMpvVolume(m_targetVolume);
    mpv_set_wakeup_callback(m_mpv.get(), &onAudioWakeup, m_wakeupBridge.get());
    return true;
}

bool BackgroundMusicEngine::isInitialized() const
{
    return m_mpv != nullptr;
}

bool BackgroundMusicEngine::isPlaying() const
{
    return m_playing;
}

bool BackgroundMusicEngine::isFading() const
{
    return m_fading;
}

bool BackgroundMusicEngine::isShuffle() const
{
    return m_shuffle;
}

void BackgroundMusicEngine::setShuffle(bool shuffle)
{
    if (m_shuffle == shuffle) {
        return;
    }
    m_shuffle = shuffle;
    emit queueChanged();
}

int BackgroundMusicEngine::targetVolume() const
{
    return m_targetVolume;
}

void BackgroundMusicEngine::setTargetVolume(int volume)
{
    const int clamped = qBound(0, volume, 100);
    if (m_targetVolume == clamped) {
        return;
    }
    m_targetVolume = clamped;
    if (!m_fading) {
        m_currentVolume = clamped;
        applyMpvVolume(clamped);
    }
    emit volumeChanged();
}

int BackgroundMusicEngine::currentVolume() const
{
    return m_currentVolume;
}

int BackgroundMusicEngine::fadeDurationMs() const
{
    return m_fadeDurationMs;
}

void BackgroundMusicEngine::setFadeDurationMs(int durationMs)
{
    const int clamped = qBound(500, durationMs, 30000);
    if (m_fadeDurationMs == clamped) {
        return;
    }
    m_fadeDurationMs = clamped;
    updateScheduleTimer();
    emit scheduleChanged();
}

int BackgroundMusicEngine::currentIndex() const
{
    return m_currentIndex;
}

int BackgroundMusicEngine::trackCount() const
{
    return static_cast<int>(m_playlist.size());
}

QList<QUrl> BackgroundMusicEngine::playlist() const
{
    return m_playlist;
}

QString BackgroundMusicEngine::currentTrackTitle() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) {
        return {};
    }
    const QUrl& url = m_playlist.at(m_currentIndex);
    return url.isLocalFile() ? QFileInfo(url.toLocalFile()).completeBaseName() : url.fileName();
}

bool BackgroundMusicEngine::isScheduleEnabled() const
{
    return (m_midweekSchedule.enabled && m_midweekSchedule.startTime.isValid()) ||
           (m_weekendSchedule.enabled && m_weekendSchedule.startTime.isValid());
}

bool BackgroundMusicEngine::isOperatorOverrideActive() const
{
    return m_operatorOverrideActive;
}

QDateTime BackgroundMusicEngine::nextFadeTrigger() const
{
    return m_nextFadeTrigger;
}

void BackgroundMusicEngine::setAudioDevice(const QString& deviceId)
{
    if (m_mpv == nullptr || deviceId.trimmed().isEmpty()) {
        return;
    }
    const QByteArray utf8 = deviceId.trimmed().toUtf8();
    mpv_set_property_string(m_mpv.get(), "audio-device", utf8.constData());
}

void BackgroundMusicEngine::configureSchedules(const MeetingScheduleSpec& midweek,
                                               const MeetingScheduleSpec& weekend,
                                               int fadeDurationMs)
{
    m_midweekSchedule = midweek;
    m_weekendSchedule = weekend;
    m_fadeDurationMs = qBound(500, fadeDurationMs, 30000);
    updateScheduleTimer();
    emit scheduleChanged();
}

void BackgroundMusicEngine::resetOperatorOverride()
{
    m_operatorOverrideActive = false;
    updateScheduleTimer();
}

QDateTime BackgroundMusicEngine::computeNextFadeTrigger(const QDateTime& now,
                                                        const MeetingScheduleSpec& midweek,
                                                        const MeetingScheduleSpec& weekend,
                                                        int fadeDurationMs)
{
    if (!now.isValid()) {
        return {};
    }

    const int cleanFadeMs = qBound(500, fadeDurationMs, 30000);
    QDateTime earliest;

    const MeetingScheduleSpec specs[] = {midweek, weekend};
    for (const auto& spec : specs) {
        if (!spec.enabled || !spec.startTime.isValid() || spec.dayOfWeek < 1 ||
            spec.dayOfWeek > 7) {
            continue;
        }
        for (int dayOffset = 0; dayOffset <= 7; ++dayOffset) {
            const QDate candidateDate = now.date().addDays(dayOffset);
            if (candidateDate.dayOfWeek() != spec.dayOfWeek) {
                continue;
            }
            const QDateTime startDt(candidateDate, spec.startTime, now.timeZone());
            const QDateTime fadeTrigger = startDt.addMSecs(-cleanFadeMs);
            if (fadeTrigger >= now) {
                if (!earliest.isValid() || fadeTrigger < earliest) {
                    earliest = fadeTrigger;
                }
                break;
            }
        }
    }

    return earliest;
}

void BackgroundMusicEngine::setPlaylist(const QList<QUrl>& tracks)
{
    m_playlist = tracks;
    m_currentIndex = m_playlist.isEmpty() ? -1 : 0;
    emit queueChanged();
}

void BackgroundMusicEngine::addTrack(const QUrl& track)
{
    if (!track.isValid() || track.isEmpty()) {
        return;
    }
    m_playlist.append(track);
    if (m_currentIndex < 0) {
        m_currentIndex = 0;
    }
    emit queueChanged();
}

void BackgroundMusicEngine::clearPlaylist()
{
    stop();
    m_playlist.clear();
    m_currentIndex = -1;
    emit queueChanged();
}

void BackgroundMusicEngine::play()
{
    if (m_playlist.isEmpty()) {
        return;
    }
    if (m_mpv == nullptr && !initialize(false)) {
        return;
    }

    stopFadeTimer();
    m_currentVolume = m_targetVolume;
    applyMpvVolume(m_currentVolume);
    emit volumeChanged();

    m_operatorOverrideActive = false;

    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) {
        m_currentIndex = 0;
    }

    if (m_playing) {
        int flag = 0;
        mpv_set_property(m_mpv.get(), "pause", MPV_FORMAT_FLAG, &flag);
        return;
    }

    loadTrackAtIndex(m_currentIndex);
}

void BackgroundMusicEngine::pause()
{
    stopFadeTimer();
    m_operatorOverrideActive = true;
    if (m_mpv != nullptr) {
        int flag = 1;
        mpv_set_property(m_mpv.get(), "pause", MPV_FORMAT_FLAG, &flag);
    }
    if (m_playing) {
        m_playing = false;
        emit playbackStateChanged();
    }
}

void BackgroundMusicEngine::stop()
{
    stopFadeTimer();
    if (m_mpv != nullptr) {
        const char* cmd[] = {"stop", nullptr};
        mpv_command_async(m_mpv.get(), 0, cmd);
    }
    if (m_playing) {
        m_playing = false;
        emit playbackStateChanged();
    }
    m_currentVolume = m_targetVolume;
    applyMpvVolume(m_currentVolume);
    emit volumeChanged();
}

void BackgroundMusicEngine::next()
{
    if (m_playlist.isEmpty()) {
        return;
    }
    if (m_shuffle && m_playlist.size() > 1) {
        int candidate = m_currentIndex;
        while (candidate == m_currentIndex) {
            candidate = static_cast<int>(
                QRandomGenerator::global()->bounded(static_cast<quint32>(m_playlist.size())));
        }
        m_currentIndex = candidate;
    } else {
        m_currentIndex = (m_currentIndex + 1) % static_cast<int>(m_playlist.size());
    }
    emit queueChanged();
    if (m_playing && m_mpv != nullptr) {
        loadTrackAtIndex(m_currentIndex);
    }
}

void BackgroundMusicEngine::previous()
{
    if (m_playlist.isEmpty()) {
        return;
    }
    m_currentIndex =
        (m_currentIndex <= 0) ? static_cast<int>(m_playlist.size()) - 1 : m_currentIndex - 1;
    emit queueChanged();
    if (m_playing && m_mpv != nullptr) {
        loadTrackAtIndex(m_currentIndex);
    }
}

void BackgroundMusicEngine::fadeOutAndStop(int durationMs)
{
    if (!m_playing || m_currentVolume <= 0) {
        stop();
        emit fadeOutCompleted();
        return;
    }

    m_activeFadeDurationMs = (durationMs > 0) ? qBound(100, durationMs, 30000) : m_fadeDurationMs;
    m_fadeStartVolume = m_currentVolume;
    m_fadeElapsedMs = 0;

    if (!m_fading) {
        m_fading = true;
        emit fadeStateChanged();
    }
    m_fadeTimer.start();
}

void BackgroundMusicEngine::processMpvEvents()
{
    if (m_mpv == nullptr) {
        return;
    }

    while (true) {
        const mpv_event* event = mpv_wait_event(m_mpv.get(), 0.0);
        if (event == nullptr || event->event_id == MPV_EVENT_NONE) {
            break;
        }
        if (event->event_id == MPV_EVENT_END_FILE) {
            const auto* endData = static_cast<const mpv_event_end_file*>(event->data);
            if (endData != nullptr && endData->reason == MPV_END_FILE_REASON_EOF && m_playing) {
                next();
            }
        }
    }
}

void BackgroundMusicEngine::onFadeTick()
{
    m_fadeElapsedMs += kFadeStepIntervalMs;
    const double progress = qBound(
        0.0, static_cast<double>(m_fadeElapsedMs) / static_cast<double>(m_activeFadeDurationMs),
        1.0);
    const int steppedVol = static_cast<int>(std::lround(m_fadeStartVolume * (1.0 - progress)));

    if (m_currentVolume != steppedVol) {
        m_currentVolume = steppedVol;
        applyMpvVolume(steppedVol);
        emit volumeChanged();
    }

    if (progress >= 1.0) {
        stop();
        emit fadeOutCompleted();
    }
}

void BackgroundMusicEngine::onScheduledFadeTimeout()
{
    if (!m_operatorOverrideActive && m_playing) {
        emit scheduledFadeTriggered();
        fadeOutAndStop(m_fadeDurationMs);
    }
    updateScheduleTimer();
}

void BackgroundMusicEngine::loadTrackAtIndex(int index)
{
    if (m_mpv == nullptr || index < 0 || index >= m_playlist.size()) {
        return;
    }

    const QUrl& url = m_playlist.at(index);
    const QByteArray pathUtf8 =
        url.isLocalFile() ? url.toLocalFile().toUtf8() : url.toString().toUtf8();
    int unpause = 0;
    mpv_set_property(m_mpv.get(), "pause", MPV_FORMAT_FLAG, &unpause);

    const char* cmd[] = {"loadfile", pathUtf8.constData(), nullptr};
    if (mpv_command_async(m_mpv.get(), 0, cmd) >= 0) {
        if (!m_playing) {
            m_playing = true;
            emit playbackStateChanged();
        }
    }
}

void BackgroundMusicEngine::applyMpvVolume(int volume)
{
    if (m_mpv == nullptr) {
        return;
    }
    double vol = static_cast<double>(qBound(0, volume, 100));
    mpv_set_property_async(m_mpv.get(), 0, "volume", MPV_FORMAT_DOUBLE, &vol);
}

void BackgroundMusicEngine::updateScheduleTimer()
{
    m_scheduleTimer.stop();
    m_nextFadeTrigger = computeNextFadeTrigger(QDateTime::currentDateTime(), m_midweekSchedule,
                                               m_weekendSchedule, m_fadeDurationMs);
    if (!m_nextFadeTrigger.isValid()) {
        return;
    }

    const qint64 delayMs = QDateTime::currentDateTime().msecsTo(m_nextFadeTrigger);
    if (delayMs > 0) {
        const qint64 maxSingleShotMs = 24LL * 60 * 60 * 1000;
        m_scheduleTimer.start(static_cast<int>(qMin(delayMs, maxSingleShotMs)));
    }
}

void BackgroundMusicEngine::stopFadeTimer()
{
    m_fadeTimer.stop();
    if (m_fading) {
        m_fading = false;
        emit fadeStateChanged();
    }
}

} // namespace kmedia
