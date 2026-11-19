#pragma once

#include <QDate>
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
};

/**
 * Pure Qt Core model providing monthly meeting calendar cells, congregation
 * midweek/weekend meeting days, special week overrides, and local cached media readiness counts.
 */
class MeetingCalendarModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int visibleYear READ visibleYear NOTIFY calendarChanged FINAL)
    Q_PROPERTY(int visibleMonth READ visibleMonth NOTIFY calendarChanged FINAL)
    Q_PROPERTY(QString monthTitle READ monthTitle NOTIFY calendarChanged FINAL)
    Q_PROPERTY(QString selectedDateIso READ selectedDateIso NOTIFY calendarChanged FINAL)
    Q_PROPERTY(
        QString selectedWeekMondayIso READ selectedWeekMondayIso NOTIFY calendarChanged FINAL)
    Q_PROPERTY(QString selectedWeekLabel READ selectedWeekLabel NOTIFY calendarChanged FINAL)
    Q_PROPERTY(
        QString selectedWeekEventType READ selectedWeekEventType NOTIFY calendarChanged FINAL)
    Q_PROPERTY(
        QString selectedWeekEventLabel READ selectedWeekEventLabel NOTIFY calendarChanged FINAL)
    Q_PROPERTY(int midweekDay READ midweekDay NOTIFY scheduleChanged FINAL)
    Q_PROPERTY(int weekendDay READ weekendDay NOTIFY scheduleChanged FINAL)
    Q_PROPERTY(QString midweekTime READ midweekTime NOTIFY scheduleChanged FINAL)
    Q_PROPERTY(QString weekendTime READ weekendTime NOTIFY scheduleChanged FINAL)
    Q_PROPERTY(QString scheduleSummary READ scheduleSummary NOTIFY scheduleChanged FINAL)
    Q_PROPERTY(QVariantList calendarDays READ calendarDays NOTIFY calendarChanged FINAL)
    Q_PROPERTY(QVariantList upcomingMeetings READ upcomingMeetings NOTIFY calendarChanged FINAL)
    Q_PROPERTY(QVariantList specialEvents READ specialEvents NOTIFY specialEventsChanged FINAL)

public:
    explicit MeetingCalendarModel(QObject* parent = nullptr);
    ~MeetingCalendarModel() override = default;

    Q_DISABLE_COPY_MOVE(MeetingCalendarModel)

    void setStorageRoot(const QString& storageRoot);
    void setMediaLanguage(const QString& languageCode);

    [[nodiscard]] int visibleYear() const;
    [[nodiscard]] int visibleMonth() const;
    [[nodiscard]] QString monthTitle() const;
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

    [[nodiscard]] QVariantList calendarDays() const;
    [[nodiscard]] QVariantList upcomingMeetings() const;
    [[nodiscard]] QVariantList specialEvents() const;
    [[nodiscard]] QString selectedWeekEventType() const;
    [[nodiscard]] QString selectedWeekEventLabel() const;

    [[nodiscard]] Q_INVOKABLE QString eventTypeForDate(const QString& isoDate) const;
    [[nodiscard]] Q_INVOKABLE QString eventLabelForDate(const QString& isoDate) const;
    [[nodiscard]] QHash<QString, QPair<QString, QString>> specialEventsMap() const;
    [[nodiscard]] QString exportSpecialEventsJson() const;
    void loadSpecialEventsJson(const QString& json);

    [[nodiscard]] static QString shortDayName(int qtDayOfWeek);
    [[nodiscard]] static QString normalizeClockTime(const QString& raw, const QString& fallback);
    [[nodiscard]] static QString normalizeEventType(const QString& raw);
    [[nodiscard]] static QString defaultEventLabel(const QString& eventType);
    [[nodiscard]] static bool isRegularStudySkippedForEvent(const QString& eventType);
    [[nodiscard]] static int countLocalIssueFiles(const QString& storageRoot,
                                                  const QString& languageCode,
                                                  const QString& pubSymbol, int issueTag);

public slots:
    void previousMonth();
    void nextMonth();
    void goToToday();
    void selectDate(const QString& isoDate);
    void openWeekForDate(const QString& isoDate, bool syncNow = false);
    void setScheduleDays(int midweekDay, const QString& midweekTime, int weekendDay,
                         const QString& weekendTime);
    void setWeekSpecialEvent(const QString& isoDate, const QString& eventType,
                             const QString& customLabel = {});
    void clearWeekSpecialEvent(const QString& isoDate);
    void refreshReadiness();

signals:
    void calendarChanged();
    void scheduleChanged();
    void specialEventsChanged();
    void weekOpenRequested(const QDate& weekMonday, bool syncNow);

private:
    void rebuild();

    QString m_storageRoot;
    QString m_languageCode{QStringLiteral("E")};
    int m_visibleYear{2026};
    int m_visibleMonth{1};
    QDate m_selectedDate;
    int m_midweekDay{3};
    int m_weekendDay{7};
    QString m_midweekTime{QStringLiteral("19:30")};
    QString m_weekendTime{QStringLiteral("09:30")};
    QHash<QString, SpecialWeekEvent> m_specialEvents;
    QVariantList m_calendarDays;
    QVariantList m_upcomingMeetings;
};

} // namespace kmedia
