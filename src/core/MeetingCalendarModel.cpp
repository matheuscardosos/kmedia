#include "core/MeetingCalendarModel.h"

#include "network/MeetingCatalogService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QVariantMap>

#include <algorithm>

namespace kmedia {

MeetingCalendarModel::MeetingCalendarModel(QObject* parent) : QObject(parent)
{
    m_selectedDate = QDate::currentDate();
}

QDate MeetingCalendarModel::selectedDate() const
{
    return m_selectedDate;
}

QString MeetingCalendarModel::selectedDateIso() const
{
    return m_selectedDate.toString(Qt::ISODate);
}

QDate MeetingCalendarModel::selectedWeekMonday() const
{
    return MeetingCatalogService::weekMondayForDate(m_selectedDate);
}

QString MeetingCalendarModel::selectedWeekMondayIso() const
{
    return selectedWeekMonday().toString(Qt::ISODate);
}

QString MeetingCalendarModel::selectedWeekLabel() const
{
    return MeetingCatalogService::formatWeekRange(selectedWeekMonday());
}

int MeetingCalendarModel::midweekDay() const
{
    return m_midweekDay;
}

int MeetingCalendarModel::weekendDay() const
{
    return m_weekendDay;
}

QString MeetingCalendarModel::midweekTime() const
{
    return m_midweekTime;
}

QString MeetingCalendarModel::weekendTime() const
{
    return m_weekendTime;
}

QString MeetingCalendarModel::scheduleSummary() const
{
    return QStringLiteral("%1 %2 | %3 %4")
        .arg(shortDayName(m_midweekDay), m_midweekTime, shortDayName(m_weekendDay), m_weekendTime);
}

QVariantList MeetingCalendarModel::specialEvents() const
{
    QList<SpecialWeekEvent> ordered = m_specialEvents.values();
    std::sort(ordered.begin(), ordered.end(),
              [](const SpecialWeekEvent& a, const SpecialWeekEvent& b) {
                  return a.weekMondayIso < b.weekMondayIso;
              });

    QVariantList list;
    list.reserve(ordered.size());
    for (const SpecialWeekEvent& ev : ordered) {
        QVariantMap map;
        map.insert(QStringLiteral("weekMondayIso"), ev.weekMondayIso);
        map.insert(QStringLiteral("eventType"), ev.eventType);
        map.insert(QStringLiteral("label"), ev.label);
        map.insert(QStringLiteral("memorialDateIso"), ev.memorialDateIso);
        list.append(map);
    }
    return list;
}

QString MeetingCalendarModel::selectedWeekEventType() const
{
    return m_specialEvents.value(selectedWeekMondayIso()).eventType;
}

QString MeetingCalendarModel::selectedWeekEventLabel() const
{
    return m_specialEvents.value(selectedWeekMondayIso()).label;
}

QString MeetingCalendarModel::selectedMeetingKind() const
{
    return meetingKindForDate(selectedDateIso());
}

bool MeetingCalendarModel::selectedWeekHasTempSchedule() const
{
    return m_tempSchedules.contains(selectedWeekMondayIso());
}

QString MeetingCalendarModel::eventTypeForDate(const QString& isoDate) const
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    if (!parsed.isValid()) {
        return {};
    }
    const QString mondayIso =
        MeetingCatalogService::weekMondayForDate(parsed).toString(Qt::ISODate);
    return m_specialEvents.value(mondayIso).eventType;
}

QString MeetingCalendarModel::eventLabelForDate(const QString& isoDate) const
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    if (!parsed.isValid()) {
        return {};
    }
    const QString mondayIso =
        MeetingCatalogService::weekMondayForDate(parsed).toString(Qt::ISODate);
    return m_specialEvents.value(mondayIso).label;
}

MeetingCalendarModel::EffectiveWeekPlan
MeetingCalendarModel::effectivePlanForWeek(const QDate& weekMonday) const
{
    EffectiveWeekPlan plan;
    plan.midweekDay = m_midweekDay;
    plan.midweekTime = m_midweekTime;
    plan.weekendDay = m_weekendDay;
    plan.weekendTime = m_weekendTime;

    const QString mondayIso = weekMonday.toString(Qt::ISODate);
    const TempWeekSchedule temp = m_tempSchedules.value(mondayIso);
    if (temp.midweekDay >= 1 && temp.midweekDay <= 5) {
        plan.midweekDay = temp.midweekDay;
    }
    if (!temp.midweekTime.isEmpty()) {
        plan.midweekTime = normalizeClockTime(temp.midweekTime, plan.midweekTime);
    }
    if (temp.weekendDay >= 6 && temp.weekendDay <= 7) {
        plan.weekendDay = temp.weekendDay;
    }
    if (!temp.weekendTime.isEmpty()) {
        plan.weekendTime = normalizeClockTime(temp.weekendTime, plan.weekendTime);
    }

    const SpecialWeekEvent ev = m_specialEvents.value(mondayIso);
    if (ev.eventType == QStringLiteral("co_visit")) {
        plan.isCoVisit = true;
        if (temp.midweekDay < 1 || temp.midweekDay > 5) {
            plan.midweekDay = 2; // Circuit Overseer week moves midweek meeting to Tuesday.
        }
    } else if (ev.eventType == QStringLiteral("memorial")) {
        plan.isMemorial = true;
        QDate memDate = QDate::fromString(ev.memorialDateIso, Qt::ISODate);
        if (!memDate.isValid() || MeetingCatalogService::weekMondayForDate(memDate) != weekMonday) {
            memDate = weekMonday.addDays(plan.midweekDay - 1);
        }
        plan.memorialDate = memDate;
        plan.memorialTime = QStringLiteral("19:30");
        if (memDate.dayOfWeek() <= 5) {
            plan.midweekSuppressed = true;
        } else {
            plan.weekendSuppressed = true;
        }
    } else if (ev.eventType == QStringLiteral("circuit_assembly") ||
               ev.eventType == QStringLiteral("regional_convention")) {
        plan.midweekSuppressed = true;
        plan.weekendSuppressed = true;
    }
    return plan;
}

QDateTime MeetingCalendarModel::meetingStartForDate(const QDate& date) const
{
    if (!date.isValid()) {
        return {};
    }
    const EffectiveWeekPlan plan =
        effectivePlanForWeek(MeetingCatalogService::weekMondayForDate(date));
    QString clock;
    if (plan.isMemorial && date == plan.memorialDate) {
        clock = plan.memorialTime;
    } else if (!plan.midweekSuppressed && date.dayOfWeek() == plan.midweekDay) {
        clock = plan.midweekTime;
    } else if (!plan.weekendSuppressed && date.dayOfWeek() == plan.weekendDay) {
        clock = plan.weekendTime;
    }
    const QTime start = QTime::fromString(clock, QStringLiteral("HH:mm"));
    return start.isValid() ? QDateTime(date, start) : QDateTime{};
}

QString MeetingCalendarModel::meetingKindForDate(const QString& isoDate) const
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    const QDate date = parsed.isValid() ? parsed : m_selectedDate;
    if (!date.isValid()) {
        return QStringLiteral("custom");
    }
    const QDate weekMonday = MeetingCatalogService::weekMondayForDate(date);
    const EffectiveWeekPlan plan = effectivePlanForWeek(weekMonday);
    if (plan.isMemorial && date == plan.memorialDate) {
        return QStringLiteral("memorial");
    }
    if (!plan.midweekSuppressed && date.dayOfWeek() == plan.midweekDay) {
        return QStringLiteral("midweek");
    }
    if (!plan.weekendSuppressed && date.dayOfWeek() == plan.weekendDay) {
        return QStringLiteral("weekend");
    }
    return QStringLiteral("custom");
}

QString MeetingCalendarModel::meetingDateInWeek(const QString& isoDate, const QString& kind) const
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    const QDate base = parsed.isValid() ? parsed : m_selectedDate;
    const QDate weekMonday = MeetingCatalogService::weekMondayForDate(base);
    const EffectiveWeekPlan plan = effectivePlanForWeek(weekMonday);
    const QString k = kind.trimmed().toLower();
    if (k == QStringLiteral("midweek") || k == QStringLiteral("mw")) {
        return weekMonday.addDays(plan.midweekDay - 1).toString(Qt::ISODate);
    }
    if (k == QStringLiteral("weekend") || k == QStringLiteral("we")) {
        return weekMonday.addDays(plan.weekendDay - 1).toString(Qt::ISODate);
    }
    if (k == QStringLiteral("memorial") && plan.memorialDate.isValid()) {
        return plan.memorialDate.toString(Qt::ISODate);
    }
    return base.toString(Qt::ISODate);
}

QHash<QString, QPair<QString, QString>> MeetingCalendarModel::specialEventsMap() const
{
    QHash<QString, QPair<QString, QString>> map;
    for (auto it = m_specialEvents.constBegin(); it != m_specialEvents.constEnd(); ++it) {
        map.insert(it.key(), qMakePair(it.value().eventType, it.value().label));
    }
    return map;
}

QString MeetingCalendarModel::exportSpecialEventsJson() const
{
    QJsonArray arr;
    QList<SpecialWeekEvent> ordered = m_specialEvents.values();
    std::sort(ordered.begin(), ordered.end(),
              [](const SpecialWeekEvent& a, const SpecialWeekEvent& b) {
                  return a.weekMondayIso < b.weekMondayIso;
              });
    for (const SpecialWeekEvent& ev : ordered) {
        QJsonObject obj;
        obj.insert(QStringLiteral("weekMonday"), ev.weekMondayIso);
        obj.insert(QStringLiteral("type"), ev.eventType);
        obj.insert(QStringLiteral("label"), ev.label);
        if (!ev.memorialDateIso.isEmpty()) {
            obj.insert(QStringLiteral("memorialDate"), ev.memorialDateIso);
        }
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void MeetingCalendarModel::loadSpecialEventsJson(const QString& json)
{
    QHash<QString, SpecialWeekEvent> loaded;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isArray()) {
        for (const QJsonValue& val : doc.array()) {
            const QJsonObject obj = val.toObject();
            const QDate date =
                QDate::fromString(obj.value(QStringLiteral("weekMonday")).toString(), Qt::ISODate);
            const QString type = normalizeEventType(obj.value(QStringLiteral("type")).toString());
            if (!date.isValid() || type.isEmpty()) {
                continue;
            }
            const QString mondayIso =
                MeetingCatalogService::weekMondayForDate(date).toString(Qt::ISODate);
            const QString rawLabel = obj.value(QStringLiteral("label")).toString().trimmed();
            const QString memIso = obj.value(QStringLiteral("memorialDate")).toString().trimmed();
            loaded.insert(mondayIso,
                          SpecialWeekEvent{mondayIso, type,
                                           rawLabel.isEmpty() ? defaultEventLabel(type) : rawLabel,
                                           memIso});
        }
    }
    m_specialEvents = std::move(loaded);
    emit calendarChanged();
}

QString MeetingCalendarModel::exportTempSchedulesJson() const
{
    QJsonArray arr;
    QList<TempWeekSchedule> ordered = m_tempSchedules.values();
    std::sort(ordered.begin(), ordered.end(),
              [](const TempWeekSchedule& a, const TempWeekSchedule& b) {
                  return a.weekMondayIso < b.weekMondayIso;
              });
    for (const TempWeekSchedule& ts : ordered) {
        QJsonObject obj;
        obj.insert(QStringLiteral("weekMonday"), ts.weekMondayIso);
        obj.insert(QStringLiteral("midweekDay"), ts.midweekDay);
        obj.insert(QStringLiteral("midweekTime"), ts.midweekTime);
        obj.insert(QStringLiteral("weekendDay"), ts.weekendDay);
        obj.insert(QStringLiteral("weekendTime"), ts.weekendTime);
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void MeetingCalendarModel::loadTempSchedulesJson(const QString& json)
{
    QHash<QString, TempWeekSchedule> loaded;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isArray()) {
        for (const QJsonValue& val : doc.array()) {
            const QJsonObject obj = val.toObject();
            const QDate date =
                QDate::fromString(obj.value(QStringLiteral("weekMonday")).toString(), Qt::ISODate);
            if (!date.isValid()) {
                continue;
            }
            const QString mondayIso =
                MeetingCatalogService::weekMondayForDate(date).toString(Qt::ISODate);
            TempWeekSchedule ts;
            ts.weekMondayIso = mondayIso;
            ts.midweekDay = obj.value(QStringLiteral("midweekDay")).toInt(0);
            ts.midweekTime = obj.value(QStringLiteral("midweekTime")).toString().trimmed();
            ts.weekendDay = obj.value(QStringLiteral("weekendDay")).toInt(0);
            ts.weekendTime = obj.value(QStringLiteral("weekendTime")).toString().trimmed();
            loaded.insert(mondayIso, ts);
        }
    }
    m_tempSchedules = std::move(loaded);
    emit calendarChanged();
}

QString MeetingCalendarModel::normalizeEventType(const QString& raw)
{
    const QString cleaned = raw.trimmed().toLower();
    if (cleaned == QStringLiteral("co_visit") || cleaned == QStringLiteral("co")) {
        return QStringLiteral("co_visit");
    }
    if (cleaned == QStringLiteral("memorial")) {
        return QStringLiteral("memorial");
    }
    if (cleaned == QStringLiteral("circuit_assembly")) {
        return QStringLiteral("circuit_assembly");
    }
    if (cleaned == QStringLiteral("regional_convention")) {
        return QStringLiteral("regional_convention");
    }
    return {};
}

QString MeetingCalendarModel::defaultEventLabel(const QString& eventType)
{
    if (eventType == QStringLiteral("co_visit")) {
        return QStringLiteral("Circuit Overseer Visit");
    }
    if (eventType == QStringLiteral("memorial")) {
        return QStringLiteral("Memorial");
    }
    if (eventType == QStringLiteral("circuit_assembly")) {
        return QStringLiteral("Circuit Assembly");
    }
    if (eventType == QStringLiteral("regional_convention")) {
        return QStringLiteral("Regional Convention");
    }
    return {};
}

bool MeetingCalendarModel::isRegularStudySkippedForEvent(const QString& eventType)
{
    return eventType == QStringLiteral("memorial") ||
           eventType == QStringLiteral("circuit_assembly") ||
           eventType == QStringLiteral("regional_convention");
}

QString MeetingCalendarModel::shortDayName(int qtDayOfWeek)
{
    switch (qtDayOfWeek) {
    case 1:
        return QStringLiteral("Mon");
    case 2:
        return QStringLiteral("Tue");
    case 3:
        return QStringLiteral("Wed");
    case 4:
        return QStringLiteral("Thu");
    case 5:
        return QStringLiteral("Fri");
    case 6:
        return QStringLiteral("Sat");
    case 7:
        return QStringLiteral("Sun");
    default:
        return QStringLiteral("Wed");
    }
}

QString MeetingCalendarModel::normalizeClockTime(const QString& raw, const QString& fallback)
{
    const QString trimmed = raw.trimmed();
    const QStringList parts = trimmed.split(QLatin1Char(':'));
    if (parts.size() == 2) {
        bool okHour = false;
        bool okMinute = false;
        const int hour = parts.at(0).toInt(&okHour);
        const int minute = parts.at(1).toInt(&okMinute);
        if (okHour && okMinute && hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
            const QTime parsed(hour, minute);
            if (parsed.isValid()) {
                return parsed.toString(QStringLiteral("HH:mm"));
            }
        }
    }
    return fallback;
}

void MeetingCalendarModel::selectDate(const QString& isoDate)
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    if (!parsed.isValid()) {
        return;
    }
    m_selectedDate = parsed;
    emit calendarChanged();
    emit daySelected(m_selectedDate, selectedMeetingKind(),
                     selectedWeekEventType() == QStringLiteral("co_visit"));
}

void MeetingCalendarModel::shiftSelectedDate(int days)
{
    if (!m_selectedDate.isValid()) {
        m_selectedDate = QDate::currentDate();
    }
    const QDate next = m_selectedDate.addDays(days);
    selectDate(next.toString(Qt::ISODate));
}

void MeetingCalendarModel::openWeekForDate(const QString& isoDate, bool syncNow)
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    const QDate effective = parsed.isValid() ? parsed : m_selectedDate;
    m_selectedDate = effective;
    emit calendarChanged();
    emit daySelected(m_selectedDate, selectedMeetingKind(),
                     selectedWeekEventType() == QStringLiteral("co_visit"));
    emit weekOpenRequested(MeetingCatalogService::weekMondayForDate(effective), syncNow);
}

void MeetingCalendarModel::setScheduleDays(int midweekDay, const QString& midweekTime,
                                           int weekendDay, const QString& weekendTime)
{
    const int cleanMidweekDay = qBound(1, midweekDay, 5);
    const int cleanWeekendDay = qBound(6, weekendDay, 7);
    const QString cleanMidweekTime = normalizeClockTime(midweekTime, QStringLiteral("19:30"));
    const QString cleanWeekendTime = normalizeClockTime(weekendTime, QStringLiteral("09:30"));

    const bool changed = (m_midweekDay != cleanMidweekDay || m_weekendDay != cleanWeekendDay ||
                          m_midweekTime != cleanMidweekTime || m_weekendTime != cleanWeekendTime);

    m_midweekDay = cleanMidweekDay;
    m_weekendDay = cleanWeekendDay;
    m_midweekTime = cleanMidweekTime;
    m_weekendTime = cleanWeekendTime;

    if (changed) {
        emit scheduleChanged();
    }
    emit calendarChanged();
}

void MeetingCalendarModel::setWeekSpecialEvent(const QString& isoDate, const QString& eventType,
                                               const QString& customLabel,
                                               const QString& memorialDateIso)
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    const QDate effective = parsed.isValid() ? parsed : m_selectedDate;
    const QString mondayIso =
        MeetingCatalogService::weekMondayForDate(effective).toString(Qt::ISODate);
    const QString cleanType = normalizeEventType(eventType);

    if (cleanType.isEmpty()) {
        clearWeekSpecialEvent(mondayIso);
        return;
    }

    const QString trimmedLabel = customLabel.trimmed();
    const QString cleanMemIso =
        (cleanType == QStringLiteral("memorial") &&
         QDate::fromString(memorialDateIso.trimmed(), Qt::ISODate).isValid())
            ? memorialDateIso.trimmed()
            : QString();
    const SpecialWeekEvent nextEvent{
        mondayIso, cleanType, trimmedLabel.isEmpty() ? defaultEventLabel(cleanType) : trimmedLabel,
        cleanMemIso};
    const auto existing = m_specialEvents.constFind(mondayIso);
    if (existing != m_specialEvents.constEnd() && existing->eventType == nextEvent.eventType &&
        existing->label == nextEvent.label &&
        existing->memorialDateIso == nextEvent.memorialDateIso) {
        return;
    }

    m_specialEvents.insert(mondayIso, nextEvent);
    emit specialEventsChanged();
    emit calendarChanged();
}

void MeetingCalendarModel::clearWeekSpecialEvent(const QString& isoDate)
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    const QDate effective = parsed.isValid() ? parsed : m_selectedDate;
    const QString mondayIso =
        MeetingCatalogService::weekMondayForDate(effective).toString(Qt::ISODate);
    if (m_specialEvents.remove(mondayIso)) {
        emit specialEventsChanged();
        emit calendarChanged();
    }
}

void MeetingCalendarModel::setTempWeekSchedule(const QString& isoDate, int midweekDay,
                                               const QString& midweekTime, int weekendDay,
                                               const QString& weekendTime)
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    const QDate effective = parsed.isValid() ? parsed : m_selectedDate;
    const QString mondayIso =
        MeetingCatalogService::weekMondayForDate(effective).toString(Qt::ISODate);
    TempWeekSchedule ts;
    ts.weekMondayIso = mondayIso;
    ts.midweekDay = (midweekDay >= 1 && midweekDay <= 5) ? midweekDay : 0;
    ts.midweekTime = midweekTime.trimmed().isEmpty()
                         ? QString()
                         : normalizeClockTime(midweekTime, m_midweekTime);
    ts.weekendDay = (weekendDay >= 6 && weekendDay <= 7) ? weekendDay : 0;
    ts.weekendTime = weekendTime.trimmed().isEmpty()
                         ? QString()
                         : normalizeClockTime(weekendTime, m_weekendTime);
    m_tempSchedules.insert(mondayIso, ts);
    emit tempSchedulesChanged();
    emit calendarChanged();
}

void MeetingCalendarModel::clearTempWeekSchedule(const QString& isoDate)
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    const QDate effective = parsed.isValid() ? parsed : m_selectedDate;
    const QString mondayIso =
        MeetingCatalogService::weekMondayForDate(effective).toString(Qt::ISODate);
    if (m_tempSchedules.remove(mondayIso)) {
        emit tempSchedulesChanged();
        emit calendarChanged();
    }
}

} // namespace kmedia
