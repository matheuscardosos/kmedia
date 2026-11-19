#include "core/MeetingTimer.h"

#include "core/SystemUtils.h"

namespace kmedia {

namespace {
constexpr qint64 kMsPerMinute = 60000LL;
constexpr int kMinPlannedMinutes = 1;
constexpr int kMaxPlannedMinutes = 360;
constexpr int kTickIntervalMs = 1000;
} // namespace

MeetingTimer::MeetingTimer(QObject* parent) : QObject(parent)
{
    m_tickTimer.setInterval(kTickIntervalMs);
    m_tickTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_tickTimer, &QTimer::timeout, this, &MeetingTimer::timeChanged);
}

bool MeetingTimer::isRunning() const
{
    return m_running;
}

bool MeetingTimer::isOvertime() const
{
    return remainingMs() < 0;
}

int MeetingTimer::plannedMinutes() const
{
    return m_plannedMinutes;
}

void MeetingTimer::setPlannedMinutes(int minutes)
{
    const int clamped = qBound(kMinPlannedMinutes, minutes, kMaxPlannedMinutes);
    if (m_plannedMinutes == clamped) {
        return;
    }
    m_plannedMinutes = clamped;
    emit plannedMinutesChanged();
    emit timeChanged();
}

qint64 MeetingTimer::elapsedMs() const
{
    if (m_running && m_segmentClock.isValid()) {
        return m_accumulatedMs + qMax<qint64>(0, m_segmentClock.elapsed());
    }
    return m_accumulatedMs;
}

qint64 MeetingTimer::remainingMs() const
{
    const qint64 plannedMs = static_cast<qint64>(m_plannedMinutes) * kMsPerMinute;
    return plannedMs - elapsedMs();
}

QString MeetingTimer::formattedElapsed() const
{
    return SystemUtils::formatDurationMs(elapsedMs());
}

QString MeetingTimer::formattedRemaining() const
{
    const qint64 rem = remainingMs();
    if (rem < 0) {
        return QStringLiteral("+") + SystemUtils::formatDurationMs(-rem);
    }
    return SystemUtils::formatDurationMs(rem);
}

void MeetingTimer::advanceForTest(qint64 deltaMs)
{
    if (deltaMs <= 0) {
        return;
    }
    m_accumulatedMs += deltaMs;
    emit timeChanged();
}

void MeetingTimer::start()
{
    if (m_running) {
        return;
    }
    m_segmentClock.start();
    m_running = true;
    m_tickTimer.start();
    emit runningChanged();
    emit timeChanged();
}

void MeetingTimer::pause()
{
    if (!m_running) {
        return;
    }
    if (m_segmentClock.isValid()) {
        m_accumulatedMs += qMax<qint64>(0, m_segmentClock.elapsed());
    }
    m_tickTimer.stop();
    m_running = false;
    emit timeChanged();
    emit runningChanged();
}

void MeetingTimer::toggle()
{
    if (m_running) {
        pause();
    } else {
        start();
    }
}

void MeetingTimer::reset()
{
    m_tickTimer.stop();
    const bool wasRunning = m_running;
    m_running = false;
    m_accumulatedMs = 0;
    if (wasRunning) {
        emit runningChanged();
    }
    emit timeChanged();
}

void MeetingTimer::adjustPlannedMinutes(int deltaMinutes)
{
    setPlannedMinutes(m_plannedMinutes + deltaMinutes);
}

} // namespace kmedia
