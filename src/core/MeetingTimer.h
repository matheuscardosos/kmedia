#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <functional>
#include <vector>

namespace kmedia {

/**
 * The meeting timer, inspired by Meeting Media Manager: it times each part of the meeting in
 * order. The parts and their minutes come from the meeting type (midweek or weekend, with the
 * circuit overseer's week), one minute of counsel follows each student part, and the minutes of
 * Apply Yourself and Living as Christians are shared out over the number of parts. Every part's
 * minutes can be changed by hand; nothing is read from the workbook.
 *
 * Elapsed time runs on a monotonic clock, so a change of the system time does not affect it. The
 * real start and end of each part, the changed minutes, and the custom parts belong to the day
 * and are handed out as JSON to be kept with it.
 */
class MeetingTimer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList parts READ parts NOTIFY partsChanged FINAL)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY partsChanged FINAL)
    Q_PROPERTY(QString currentPartId READ currentPartId NOTIFY partsChanged FINAL)
    Q_PROPERTY(int nextIndex READ nextIndex NOTIFY partsChanged FINAL)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged FINAL)
    Q_PROPERTY(bool paused READ isPaused NOTIFY runningChanged FINAL)
    Q_PROPERTY(qint64 elapsedMs READ elapsedMs NOTIFY timeChanged FINAL)
    Q_PROPERTY(qint64 targetMs READ targetMs NOTIFY timeChanged FINAL)
    Q_PROPERTY(bool overtime READ isOvertime NOTIFY timeChanged FINAL)
    Q_PROPERTY(QString formattedTime READ formattedTime NOTIFY timeChanged FINAL)
    Q_PROPERTY(bool countDown READ isCountDown WRITE setCountDown NOTIFY optionsChanged FINAL)
    Q_PROPERTY(bool overtimeAmountOnly READ isOvertimeAmountOnly WRITE setOvertimeAmountOnly NOTIFY
                   optionsChanged FINAL)
    Q_PROPERTY(QString meetingKind READ meetingKind NOTIFY partsChanged FINAL)
    Q_PROPERTY(bool coWeek READ isCoWeek NOTIFY partsChanged FINAL)
    Q_PROPERTY(QString dateIso READ dateIso NOTIFY partsChanged FINAL)
    // When the open day's meeting starts; invalid on a day without one.
    Q_PROPERTY(QDateTime meetingStart READ meetingStart NOTIFY partsChanged FINAL)
    Q_PROPERTY(int ayfmCount READ ayfmCount WRITE setAyfmCount NOTIFY partsChanged FINAL)
    Q_PROPERTY(int lacCount READ lacCount WRITE setLacCount NOTIFY partsChanged FINAL)
    Q_PROPERTY(bool hasStudyPart READ hasStudyPart NOTIFY partsChanged FINAL)
    Q_PROPERTY(
        QString studyEndTime READ studyEndTime WRITE setStudyEndTime NOTIFY partsChanged FINAL)
    Q_PROPERTY(bool scheduleOffsetKnown READ isScheduleOffsetKnown NOTIFY partsChanged FINAL)
    Q_PROPERTY(int scheduleOffsetMinutes READ scheduleOffsetMinutes NOTIFY partsChanged FINAL)
    Q_PROPERTY(bool hasTimings READ hasTimings NOTIFY partsChanged FINAL)

public:
    static constexpr int kMaxAyfmParts = 5;
    static constexpr int kMaxLacParts = 3;
    static constexpr int kAyfmSectionMinutes = 15;
    static constexpr int kLacSectionMinutes = 15;
    static constexpr int kCounselMinutes = 1;

    struct PartPlan
    {
        QString id;
        int minutes{0};
        bool counselAfter{false};
    };

    explicit MeetingTimer(QObject* parent = nullptr);
    ~MeetingTimer() override = default;

    Q_DISABLE_COPY_MOVE(MeetingTimer)

    // The parts of a meeting in order with their default minutes; empty for a day without one.
    [[nodiscard]] static std::vector<PartPlan> defaultPlan(const QString& meetingKind, bool coWeek,
                                                           int ayfmCount, int lacCount);
    // A section's minutes shared out over its parts, the first ones getting the remainder.
    [[nodiscard]] static std::vector<int> shareMinutes(int totalMinutes, int count);
    // mm:ss, never negative.
    [[nodiscard]] static QString formatClock(qint64 ms);

    [[nodiscard]] QVariantList parts() const;
    [[nodiscard]] int currentIndex() const;
    [[nodiscard]] QString currentPartId() const;
    [[nodiscard]] int nextIndex() const;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] bool isPaused() const;
    [[nodiscard]] qint64 elapsedMs() const;
    // How long the current part may take: its minutes, or less when the study must end at a time.
    [[nodiscard]] qint64 targetMs() const;
    [[nodiscard]] bool isOvertime() const;
    [[nodiscard]] QString formattedTime() const;

    [[nodiscard]] bool isCountDown() const;
    void setCountDown(bool countDown);
    [[nodiscard]] bool isOvertimeAmountOnly() const;
    void setOvertimeAmountOnly(bool amountOnly);

    [[nodiscard]] QString meetingKind() const;
    [[nodiscard]] bool isCoWeek() const;
    [[nodiscard]] QString dateIso() const;
    [[nodiscard]] QDateTime meetingStart() const;
    [[nodiscard]] int ayfmCount() const;
    void setAyfmCount(int count);
    [[nodiscard]] int lacCount() const;
    void setLacCount(int count);
    [[nodiscard]] bool hasStudyPart() const;
    [[nodiscard]] QString studyEndTime() const;
    void setStudyEndTime(const QString& hhmm);
    [[nodiscard]] bool isScheduleOffsetKnown() const;
    // Minutes the meeting is behind (positive) or ahead of (negative) its schedule, judged by when
    // the current part really started.
    [[nodiscard]] int scheduleOffsetMinutes() const;
    [[nodiscard]] bool hasTimings() const;

    // The day the timer follows. While a part runs, the timer stays on its day and takes the new
    // one when it stops. `dayState` is what exportDayState() gave for that day, if anything.
    void setMeeting(const QString& dateIso, const QString& meetingKind, bool coWeek,
                    const QDateTime& meetingStart, const QString& dayState = {});
    [[nodiscard]] QString exportDayState() const;

    // The day's timing report, one row per part: title, planned start, real start, planned
    // length, real length, and the difference. `titles` maps part ids to their shown titles.
    [[nodiscard]] QList<QStringList> timingReportRows(const QVariantMap& titles) const;
    // Saves the report as CSV when the file ends in .csv, as PDF otherwise. `headers` are the six
    // column titles and `heading` the line above the table.
    Q_INVOKABLE bool saveTimingReport(const QUrl& file, const QVariantMap& titles,
                                      const QStringList& headers, const QString& heading) const;

    void setClockForTest(std::function<QDateTime()> now);
    void advanceForTest(qint64 deltaMs);

public slots:
    void startPart(int index);
    void startNext();
    void togglePause();
    void stop();
    void setPartMinutes(int index, int minutes);
    void adjustPartMinutes(int index, int deltaMinutes);
    void resetTimings();
    void addCustomPart(const QString& title, int minutes);
    void removeCustomPart(int index);
    void renameCustomPart(int index, const QString& title);

signals:
    void partsChanged();
    void runningChanged();
    void timeChanged();
    void optionsChanged();
    // The day's timings, minutes, or custom parts changed: keep `dayState` with that day.
    void dayStateChanged(const QString& dateIso, const QString& dayState);

private:
    struct Part
    {
        QString id;
        QString title;
        int minutes{0};
        bool counselAfter{false};
        bool custom{false};
        QDateTime startedAt;
        QDateTime endedAt;
    };

    struct PendingMeeting
    {
        QString dateIso;
        QString meetingKind;
        bool coWeek{false};
        QDateTime meetingStart;
        QString dayState;
    };

    void applyMeeting(const PendingMeeting& meeting);
    void rebuildParts();
    void restoreDayState(const QString& json);
    void notifyDayState();
    [[nodiscard]] QDateTime now() const;
    [[nodiscard]] QDateTime plannedStart(int index) const;
    [[nodiscard]] QDateTime studyEnd() const;
    [[nodiscard]] bool isStudyPart(const QString& id) const;
    [[nodiscard]] bool isMeetingDay() const;

    QTimer m_tickTimer;
    QElapsedTimer m_segmentClock;
    qint64 m_accumulatedMs{0};
    qint64 m_targetMs{0};
    bool m_running{false};
    bool m_paused{false};
    int m_currentIndex{-1};
    bool m_countDown{false};
    bool m_overtimeAmountOnly{true};

    QString m_dateIso;
    QString m_meetingKind;
    bool m_coWeek{false};
    QDateTime m_meetingStart;
    int m_ayfmCount{3};
    int m_lacCount{2};
    QString m_studyEndTime;
    std::vector<Part> m_parts;
    // Minutes changed by hand, by part id; custom parts keep theirs in m_customParts.
    QHash<QString, int> m_minuteOverrides;
    QHash<QString, QPair<QDateTime, QDateTime>> m_timings;
    std::vector<Part> m_customParts;
    bool m_hasPending{false};
    PendingMeeting m_pending;
    std::function<QDateTime()> m_now;
};

} // namespace kmedia
