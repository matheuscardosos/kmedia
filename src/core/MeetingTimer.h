#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

namespace kmedia {

/**
 * Live operator meeting stopwatch and schedule variance tracker.
 * Runs a 1 s display tick timer only while active to preserve zero idle polling.
 */
class MeetingTimer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged FINAL)
    Q_PROPERTY(bool overtime READ isOvertime NOTIFY timeChanged FINAL)
    Q_PROPERTY(int plannedMinutes READ plannedMinutes WRITE setPlannedMinutes NOTIFY
                   plannedMinutesChanged FINAL)
    Q_PROPERTY(qint64 elapsedMs READ elapsedMs NOTIFY timeChanged FINAL)
    Q_PROPERTY(qint64 remainingMs READ remainingMs NOTIFY timeChanged FINAL)
    Q_PROPERTY(QString formattedElapsed READ formattedElapsed NOTIFY timeChanged FINAL)
    Q_PROPERTY(QString formattedRemaining READ formattedRemaining NOTIFY timeChanged FINAL)

public:
    explicit MeetingTimer(QObject* parent = nullptr);
    ~MeetingTimer() override = default;

    Q_DISABLE_COPY_MOVE(MeetingTimer)

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] bool isOvertime() const;
    [[nodiscard]] int plannedMinutes() const;
    void setPlannedMinutes(int minutes);

    [[nodiscard]] qint64 elapsedMs() const;
    [[nodiscard]] qint64 remainingMs() const;
    [[nodiscard]] QString formattedElapsed() const;
    [[nodiscard]] QString formattedRemaining() const;

    void advanceForTest(qint64 deltaMs);

public slots:
    void start();
    void pause();
    void toggle();
    void reset();
    void adjustPlannedMinutes(int deltaMinutes);

signals:
    void runningChanged();
    void plannedMinutesChanged();
    void timeChanged();

private:
    QTimer m_tickTimer;
    QElapsedTimer m_segmentClock;
    qint64 m_accumulatedMs{0};
    int m_plannedMinutes{105};
    bool m_running{false};
};

} // namespace kmedia
