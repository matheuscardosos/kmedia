#include "media/BackgroundMusicService.h"

#include "media/BackgroundMusicEngine.h"
#include "network/BackgroundMusicLibrary.h"

#include <QRandomGenerator>
#include <QStringList>
#include <QUrl>

#include <utility>

namespace kmedia {

BackgroundMusicService::BackgroundMusicService(BackgroundMusicEngine* engine,
                                               BackgroundMusicLibrary* library, QObject* parent)
    : QObject(parent), m_engine(engine), m_library(library),
      m_clock([]() { return QDateTime::currentDateTime(); })
{
    m_autoStartTimer.setInterval(30 * 1000);
    connect(&m_autoStartTimer, &QTimer::timeout, this, &BackgroundMusicService::checkAutoStart);

    if (m_engine != nullptr) {
        // A song that streams is downloaded meanwhile, so it plays from disk next time.
        connect(m_engine, &BackgroundMusicEngine::trackStarted, this, [this](int index) {
            if (m_library != nullptr && index >= 0 &&
                static_cast<std::size_t>(index) < m_queue.size()) {
                m_library->cacheSong(m_queue.at(static_cast<std::size_t>(index)));
            }
        });
        const auto clearStopTime = [this]() {
            if (m_stopTime.isValid()) {
                m_stopTime = {};
                emit stateChanged();
            }
        };
        connect(m_engine, &BackgroundMusicEngine::queueFinished, this, clearStopTime);
        connect(m_engine, &BackgroundMusicEngine::fadeOutCompleted, this, clearStopTime);
    }
}

void BackgroundMusicService::setMeetingProvider(MeetingProvider provider)
{
    m_meetingProvider = std::move(provider);
}

void BackgroundMusicService::setClock(Clock clock)
{
    m_clock = std::move(clock);
}

void BackgroundMusicService::setMediaPlaying(bool playing)
{
    m_mediaPlaying = playing;
}

bool BackgroundMusicService::isAutoStart() const
{
    return m_autoStart;
}

void BackgroundMusicService::setAutoStart(bool enabled)
{
    if (m_autoStart == enabled) {
        return;
    }
    m_autoStart = enabled;
    if (m_autoStart) {
        m_autoStartTimer.start();
    } else {
        m_autoStartTimer.stop();
    }
    emit autoStartChanged();
    checkAutoStart();
}

bool BackgroundMusicService::isLoading() const
{
    return m_loading;
}

QDateTime BackgroundMusicService::stopTime() const
{
    return m_stopTime;
}

std::optional<BackgroundMusicService::Meeting> BackgroundMusicService::upcomingMeetingToday() const
{
    if (!m_meetingProvider) {
        return std::nullopt;
    }
    const QDateTime now = m_clock();
    std::optional<Meeting> meeting = m_meetingProvider(now.date());
    if (!meeting.has_value() || !meeting->start.isValid() || meeting->start <= now) {
        return std::nullopt;
    }
    return meeting;
}

bool BackgroundMusicService::shouldAutoStart() const
{
    if (!m_autoStart || m_loading || m_mediaPlaying ||
        (m_engine != nullptr && m_engine->isPlaying())) {
        return false;
    }
    const std::optional<Meeting> meeting = upcomingMeetingToday();
    if (!meeting.has_value() || meeting->start == m_stoppedByHandFor) {
        return false;
    }
    const qint64 secondsLeft = m_clock().secsTo(meeting->start);
    return secondsLeft > kAutoStartMinimumSeconds && secondsLeft <= kAutoStartWindowSeconds;
}

void BackgroundMusicService::checkAutoStart()
{
    const bool waiting = m_autoStartRetryAt.isValid() && m_clock() < m_autoStartRetryAt;
    if (!waiting && shouldAutoStart()) {
        startMusic(false);
    }
}

void BackgroundMusicService::start()
{
    startMusic(true);
}

void BackgroundMusicService::startMusic(bool byHand)
{
    if (m_loading || m_library == nullptr || m_engine == nullptr) {
        return;
    }
    if (byHand) {
        m_stoppedByHandFor = {};
    }
    m_loading = true;
    emit stateChanged();
    m_library->requestSongs(this, [this, byHand](const std::vector<BackgroundSong>& songs) {
        m_loading = false;
        if (songs.empty()) {
            emit stateChanged();
            if (byHand) {
                emit startFailed();
            } else {
                m_autoStartRetryAt = m_clock().addSecs(kAutoStartRetrySeconds);
            }
            return;
        }
        m_autoStartRetryAt = {};
        startQueue(songs);
    });
}

void BackgroundMusicService::startQueue(const std::vector<BackgroundSong>& songs)
{
    const std::vector<BackgroundSong> shuffled =
        BackgroundMusicPlanner::weightedShuffle(songs, *QRandomGenerator::global());

    double startOffset = 0.0;
    bool loop = true;
    m_queue = shuffled;
    m_stopTime = {};
    if (const std::optional<Meeting> meeting = upcomingMeetingToday(); meeting.has_value()) {
        const double secondsLeft = static_cast<double>(m_clock().secsTo(meeting->start));
        BackgroundMusicPlanner::MeetingQueue timed =
            BackgroundMusicPlanner::meetingQueue(shuffled, meeting->songTracks, secondsLeft);
        if (!timed.songs.empty()) {
            m_queue = std::move(timed.songs);
            startOffset = timed.startOffsetSeconds;
            loop = false;
            m_stopTime = meeting->start;
        }
    }

    QList<QUrl> urls;
    QStringList titles;
    for (const BackgroundSong& song : m_queue) {
        urls.append(m_library->playbackUrl(song));
        titles.append(song.title);
    }
    m_engine->setShuffle(false);
    m_engine->setQueue(urls, titles, startOffset, loop);
    m_engine->play();
    emit stateChanged();
}

void BackgroundMusicService::stop()
{
    if (const std::optional<Meeting> meeting = upcomingMeetingToday(); meeting.has_value()) {
        m_stoppedByHandFor = meeting->start;
    }
    if (m_engine != nullptr && m_engine->isPlaying()) {
        m_engine->fadeOutAndStop();
    }
    m_stopTime = {};
    emit stateChanged();
}

void BackgroundMusicService::toggle()
{
    if (m_engine != nullptr && m_engine->isPlaying()) {
        stop();
    } else {
        start();
    }
}

} // namespace kmedia
