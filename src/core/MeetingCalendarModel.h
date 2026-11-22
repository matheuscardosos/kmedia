#pragma once

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QTime>
#include <QVariantList>

namespace kmedia {

struct SpecialWeekEvent
{
    QString weekMondayIso;
    QString eventType;
    QString label;
    QString memorialDateIso;
};

struct TempWeekSchedule
{
    QString weekMondayIso;
    int midweekDay{0};
    QString midweekTime;
    int weekendDay{0};
    QString weekendTime;
};

/**
 * The congregation's meeting schedule: midweek and weekend days and times, the selected day,
 * special weeks (Circuit Overseer visit with its Tuesday shift, Memorial, assemblies), and
 * temporary schedules for a single week.
 */
class MeetingCalendarModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString selectedDateIso READ selectedDateIso NOTIFY calendarChanged FINAL)
    Q_PROPERTY(QString selectedMeetingKind READ selectedMeetingKind NOTIFY calendarChanged FINAL)
    Q_PROPERTY(
        QString selectedWeekMondayIso READ selectedWeekMondayIso NOTIFY calendarChanged FINAL)
    Q_PROPERTY(QString selectedWeekLabel READ selectedWeekLabel NOTIFY calendarChanged FINAL)
    Q_PROPERTY(
        QString selectedWeekEventType READ selectedWeekEventType NOTIFY calendarChanged FINAL)
    Q_PROPERTY(
        QString selectedWeekEventLabel READ selectedWeekEventLabel NOTIFY calendarChanged FINAL)
    Q_PROPERTY(bool selectedWeekHasTempSchedule READ selectedWeekHasTempSchedule NOTIFY
                   calendarChanged FINAL)
    Q_PROPERTY(int midweekDay READ midweekDay NOTIFY scheduleChanged FINAL)
    Q_PROPERTY(int weekendDay READ weekendDay NOTIFY scheduleChanged FINAL)
    Q_PROPERTY(QString midweekTime READ midweekTime NOTIFY scheduleChanged FINAL)
    Q_PROPERTY(QString weekendTime READ weekendTime NOTIFY scheduleChanged FINAL)
    Q_PROPERTY(QString scheduleSummary READ scheduleSummary NOTIFY scheduleChanged FINAL)
    Q_PROPERTY(QVariantList specialEvents READ specialEvents NOTIFY specialEventsChanged FINAL)

public:
    explicit MeetingCalendarModel(QObject* parent = nullptr);
    ~MeetingCalendarModel() override = default;

    Q_DISABLE_COPY_MOVE(MeetingCalendarModel)

    [[nodiscard]] QDate selectedDate() const;
    [[nodiscard]] QString selectedDateIso() const;
    [[nodiscard]] QDate selectedWeekMonday() const;
    [[nodiscard]] QString selectedWeekMondayIso() const;
    [[nodiscard]] QString selectedWeekLabel() const;

    [[nodiscard]] int midweekDay() const;
    [[nodiscard]] int weekendDay() const;
    [[nodiscard]] QString midweekTime() const;
    [[nodiscard]] QString weekendTime() const;
    [[nodiscard]] QString scheduleSummary() const;

    [[nodiscard]] QVariantList specialEvents() const;
    [[nodiscard]] QString selectedWeekEventType() const;
    [[nodiscard]] QString selectedWeekEventLabel() const;
    [[nodiscard]] QString selectedMeetingKind() const;
    [[nodiscard]] bool selectedWeekHasTempSchedule() const;

    [[nodiscard]] Q_INVOKABLE QString eventTypeForDate(const QString& isoDate) const;
    [[nodiscard]] Q_INVOKABLE QString eventLabelForDate(const QString& isoDate) const;
    [[nodiscard]] Q_INVOKABLE QString meetingKindForDate(const QString& isoDate) const;
    // When the meeting of `date` starts, honoring temporary schedules and special weeks;
    // invalid when that day has no meeting.
    [[nodiscard]] QDateTime meetingStartForDate(const QDate& date) const;
    [[nodiscard]] Q_INVOKABLE QString meetingDateInWeek(const QString& isoDate,
                                                        const QString& kind) const;
    [[nodiscard]] QHash<QString, QPair<QString, QString>> specialEventsMap() const;
    [[nodiscard]] QString exportSpecialEventsJson() const;
    void loadSpecialEventsJson(const QString& json);
    [[nodiscard]] QString exportTempSchedulesJson() const;
    void loadTempSchedulesJson(const QString& json);

    [[nodiscard]] static QString shortDayName(int qtDayOfWeek);
    [[nodiscard]] static QString normalizeClockTime(const QString& raw, const QString& fallback);
    [[nodiscard]] static QString normalizeEventType(const QString& raw);
    [[nodiscard]] static QString defaultEventLabel(const QString& eventType);
    [[nodiscard]] static bool isRegularStudySkippedForEvent(const QString& eventType);

public slots:
    void selectDate(const QString& isoDate);
    void shiftSelectedDate(int days);
    void openWeekForDate(const QString& isoDate, bool syncNow = false);
    void setScheduleDays(int midweekDay, const QString& midweekTime, int weekendDay,
                         const QString& weekendTime);
    void setWeekSpecialEvent(const QString& isoDate, const QString& eventType,
                             const QString& customLabel = {}, const QString& memorialDateIso = {});
    void clearWeekSpecialEvent(const QString& isoDate);
    void setTempWeekSchedule(const QString& isoDate, int midweekDay, const QString& midweekTime,
                             int weekendDay, const QString& weekendTime);
    void clearTempWeekSchedule(const QString& isoDate);

signals:
    void calendarChanged();
    void scheduleChanged();
    void specialEventsChanged();
    void tempSchedulesChanged();
    void daySelected(const QDate& date, const QString& meetingKind, bool isCoVisit);
    void weekOpenRequested(const QDate& weekMonday, bool syncNow);

private:
    struct EffectiveWeekPlan
    {
        int midweekDay{3};
        QString midweekTime;
        int weekendDay{7};
        QString weekendTime;
        bool midweekSuppressed{false};
        bool weekendSuppressed{false};
        bool isCoVisit{false};
        bool isMemorial{false};
        QDate memorialDate;
        QString memorialTime;
    };

    [[nodiscard]] EffectiveWeekPlan effectivePlanForWeek(const QDate& weekMonday) const;

    QDate m_selectedDate;
    int m_midweekDay{3};
    int m_weekendDay{7};
    QString m_midweekTime{QStringLiteral("19:30")};
    QString m_weekendTime{QStringLiteral("09:30")};
    QHash<QString, SpecialWeekEvent> m_specialEvents;
    QHash<QString, TempWeekSchedule> m_tempSchedules;
};

} // namespace kmedia
