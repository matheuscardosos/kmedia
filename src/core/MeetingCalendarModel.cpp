#include "core/MeetingCalendarModel.h"

#include "core/SystemUtils.h"
#include "network/MeetingCatalogService.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QVariantMap>

namespace kmedia {

namespace {
constexpr int kCalendarGridCells = 42;
constexpr int kUpcomingWeeksCount = 4;
} // namespace

MeetingCalendarModel::MeetingCalendarModel(QObject* parent) : QObject(parent)
{
    const QDate today = QDate::currentDate();
    m_visibleYear = today.year();
    m_visibleMonth = today.month();
    m_selectedDate = today;
    rebuild();
}

void MeetingCalendarModel::setStorageRoot(const QString& storageRoot)
{
    const QString clean = storageRoot.trimmed();
    if (m_storageRoot == clean) {
        return;
    }
    m_storageRoot = clean;
    rebuild();
}

void MeetingCalendarModel::setMediaLanguage(const QString& languageCode)
{
    const QString clean =
        languageCode.trimmed().isEmpty() ? QStringLiteral("E") : languageCode.trimmed();
    if (m_languageCode == clean) {
        return;
    }
    m_languageCode = clean;
    rebuild();
}

int MeetingCalendarModel::visibleYear() const
{
    return m_visibleYear;
}

int MeetingCalendarModel::visibleMonth() const
{
    return m_visibleMonth;
}

QString MeetingCalendarModel::monthTitle() const
{
    const QDate first(m_visibleYear, m_visibleMonth, 1);
    return QLocale::c().toString(first, QStringLiteral("MMMM yyyy"));
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

QVariantList MeetingCalendarModel::calendarDays() const
{
    return m_calendarDays;
}

QVariantList MeetingCalendarModel::upcomingMeetings() const
{
    return m_upcomingMeetings;
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

int MeetingCalendarModel::countLocalIssueFiles(const QString& storageRoot,
                                               const QString& languageCode,
                                               const QString& pubSymbol, int issueTag)
{
    if (storageRoot.trimmed().isEmpty() || pubSymbol.trimmed().isEmpty() || issueTag <= 0) {
        return 0;
    }

    const QString lang = SystemUtils::sanitizeFileName(
        languageCode.trimmed().isEmpty() ? QStringLiteral("E") : languageCode.trimmed());
    const QString pub = SystemUtils::sanitizeFileName(pubSymbol.trimmed().toLower());
    const QString dirPath = QStringLiteral("%1/catalog/%2/%3_%4")
                                .arg(storageRoot.trimmed(), lang, pub, QString::number(issueTag));
    const QDir dir(dirPath);
    if (!dir.exists()) {
        return 0;
    }

    int count = 0;
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entries) {
        if (info.size() > 0 && !info.fileName().endsWith(QStringLiteral(".kmedia.part"))) {
            ++count;
        }
    }
    return count;
}

void MeetingCalendarModel::previousMonth()
{
    const QDate prev = QDate(m_visibleYear, m_visibleMonth, 1).addMonths(-1);
    m_visibleYear = prev.year();
    m_visibleMonth = prev.month();
    rebuild();
}

void MeetingCalendarModel::nextMonth()
{
    const QDate next = QDate(m_visibleYear, m_visibleMonth, 1).addMonths(1);
    m_visibleYear = next.year();
    m_visibleMonth = next.month();
    rebuild();
}

void MeetingCalendarModel::goToToday()
{
    const QDate today = QDate::currentDate();
    m_visibleYear = today.year();
    m_visibleMonth = today.month();
    m_selectedDate = today;
    rebuild();
}

void MeetingCalendarModel::selectDate(const QString& isoDate)
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    if (!parsed.isValid()) {
        return;
    }
    m_selectedDate = parsed;
    m_visibleYear = parsed.year();
    m_visibleMonth = parsed.month();
    rebuild();
}

void MeetingCalendarModel::openWeekForDate(const QString& isoDate, bool syncNow)
{
    const QDate parsed = QDate::fromString(isoDate.trimmed(), Qt::ISODate);
    const QDate effective = parsed.isValid() ? parsed : m_selectedDate;
    m_selectedDate = effective;
    m_visibleYear = effective.year();
    m_visibleMonth = effective.month();
    rebuild();
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
    rebuild();
}

void MeetingCalendarModel::refreshReadiness()
{
    rebuild();
}

void MeetingCalendarModel::rebuild()
{
    const QDate today = QDate::currentDate();
    const QDate firstOfMonth(m_visibleYear, m_visibleMonth, 1);
    const QDate gridStart = firstOfMonth.addDays(-(firstOfMonth.dayOfWeek() - 1));
    const QDate activeWeekMonday = selectedWeekMonday();

    m_calendarDays.clear();
    m_calendarDays.reserve(kCalendarGridCells);

    for (int i = 0; i < kCalendarGridCells; ++i) {
        const QDate date = gridStart.addDays(i);
        const int dow = date.dayOfWeek();
        const QDate weekMonday = MeetingCatalogService::weekMondayForDate(date);

        QString meetingKind;
        QString meetingTime;
        int issueTag = 0;
        int cachedCount = 0;

        if (dow == m_midweekDay) {
            meetingKind = QStringLiteral("midweek");
            meetingTime = m_midweekTime;
            issueTag = MeetingCatalogService::midweekIssueTagForDate(date);
            cachedCount = countLocalIssueFiles(m_storageRoot, m_languageCode, QStringLiteral("mwb"),
                                               issueTag);
        } else if (dow == m_weekendDay) {
            meetingKind = QStringLiteral("weekend");
            meetingTime = m_weekendTime;
            issueTag = MeetingCatalogService::watchtowerIssueTagForDate(date);
            cachedCount =
                countLocalIssueFiles(m_storageRoot, m_languageCode, QStringLiteral("w"), issueTag);
        }

        QVariantMap cell;
        cell.insert(QStringLiteral("dateIso"), date.toString(Qt::ISODate));
        cell.insert(QStringLiteral("dayNumber"), date.day());
        cell.insert(QStringLiteral("inCurrentMonth"), date.month() == m_visibleMonth);
        cell.insert(QStringLiteral("isToday"), date == today);
        cell.insert(QStringLiteral("isSelected"), date == m_selectedDate);
        cell.insert(QStringLiteral("isSelectedWeek"), weekMonday == activeWeekMonday);
        cell.insert(QStringLiteral("meetingKind"), meetingKind);
        cell.insert(QStringLiteral("meetingTime"), meetingTime);
        cell.insert(QStringLiteral("issueTag"), issueTag);
        cell.insert(QStringLiteral("cachedCount"), cachedCount);
        cell.insert(QStringLiteral("mediaReady"), cachedCount > 0);
        m_calendarDays.append(cell);
    }

    m_upcomingMeetings.clear();
    const QDate baseMonday = MeetingCatalogService::weekMondayForDate(m_selectedDate);
    for (int w = 0; w < kUpcomingWeeksCount; ++w) {
        const QDate weekMonday = baseMonday.addDays(w * 7);
        const QDate midDate = weekMonday.addDays(m_midweekDay - 1);
        const QDate endDate = weekMonday.addDays(m_weekendDay - 1);

        const int mwbTag = MeetingCatalogService::midweekIssueTagForDate(midDate);
        const int wTag = MeetingCatalogService::watchtowerIssueTagForDate(endDate);
        const int mwbFiles =
            countLocalIssueFiles(m_storageRoot, m_languageCode, QStringLiteral("mwb"), mwbTag);
        const int wFiles =
            countLocalIssueFiles(m_storageRoot, m_languageCode, QStringLiteral("w"), wTag);

        QVariantMap midCard;
        midCard.insert(QStringLiteral("dateIso"), midDate.toString(Qt::ISODate));
        midCard.insert(QStringLiteral("weekMondayIso"), weekMonday.toString(Qt::ISODate));
        midCard.insert(QStringLiteral("dayBadge"), shortDayName(m_midweekDay));
        midCard.insert(QStringLiteral("meetingKind"), QStringLiteral("midweek"));
        midCard.insert(QStringLiteral("meetingTime"), m_midweekTime);
        midCard.insert(QStringLiteral("issueLabel"), QStringLiteral("MWB %1").arg(mwbTag));
        midCard.insert(QStringLiteral("cachedCount"), mwbFiles);
        midCard.insert(QStringLiteral("mediaReady"), mwbFiles > 0);
        m_upcomingMeetings.append(midCard);

        QVariantMap endCard;
        endCard.insert(QStringLiteral("dateIso"), endDate.toString(Qt::ISODate));
        endCard.insert(QStringLiteral("weekMondayIso"), weekMonday.toString(Qt::ISODate));
        endCard.insert(QStringLiteral("dayBadge"), shortDayName(m_weekendDay));
        endCard.insert(QStringLiteral("meetingKind"), QStringLiteral("weekend"));
        endCard.insert(QStringLiteral("meetingTime"), m_weekendTime);
        endCard.insert(QStringLiteral("issueLabel"), QStringLiteral("W %1").arg(wTag));
        endCard.insert(QStringLiteral("cachedCount"), wFiles);
        endCard.insert(QStringLiteral("mediaReady"), wFiles > 0);
        m_upcomingMeetings.append(endCard);
    }

    emit calendarChanged();
}

} // namespace kmedia
