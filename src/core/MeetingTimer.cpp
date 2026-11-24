#include "core/MeetingTimer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPageSize>
#include <QPdfWriter>
#include <QSaveFile>
#include <QTextDocument>
#include <QTime>
#include <QVariantMap>

#include <algorithm>

namespace kmedia {

namespace {
constexpr qint64 kMsPerMinute = 60000LL;
constexpr int kTickIntervalMs = 500;
constexpr int kMaxPartMinutes = 240;

const QString kStateAyfm = QStringLiteral("ayfm");
const QString kStateLac = QStringLiteral("lac");
const QString kStateMinutes = QStringLiteral("minutes");
const QString kStateTimings = QStringLiteral("timings");
const QString kStateCustom = QStringLiteral("custom");
const QString kStateStudyEnd = QStringLiteral("studyEnd");

bool hasCounsel(const QString& id, int minutes)
{
    return minutes > 0 &&
           (id == QLatin1String("bible-reading") || id.startsWith(QLatin1String("ayfm-")));
}

QString clockTime(const QDateTime& when)
{
    return when.isValid() ? when.toString(QStringLiteral("HH:mm")) : QString{};
}
} // namespace

MeetingTimer::MeetingTimer(QObject* parent)
    : QObject(parent), m_now([]() { return QDateTime::currentDateTime(); })
{
    m_tickTimer.setInterval(kTickIntervalMs);
    m_tickTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_tickTimer, &QTimer::timeout, this, &MeetingTimer::timeChanged);
}

std::vector<int> MeetingTimer::shareMinutes(int totalMinutes, int count)
{
    std::vector<int> shares;
    if (count <= 0) {
        return shares;
    }
    const int base = totalMinutes / count;
    const int remainder = totalMinutes % count;
    shares.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        shares.push_back(base + (i < remainder ? 1 : 0));
    }
    return shares;
}

std::vector<MeetingTimer::PartPlan>
MeetingTimer::defaultPlan(const QString& meetingKind, bool coWeek, int ayfmCount, int lacCount)
{
    std::vector<PartPlan> plan;
    const auto add = [&plan](const QString& id, int minutes) {
        plan.push_back({id, minutes, hasCounsel(id, minutes)});
    };

    if (meetingKind == QLatin1String("midweek")) {
        const int ayfm = qBound(1, ayfmCount, kMaxAyfmParts);
        const int lac = qBound(1, lacCount, kMaxLacParts);
        add(QStringLiteral("opening-song"), 5);
        add(QStringLiteral("introduction"), 1);
        add(QStringLiteral("treasures"), 10);
        add(QStringLiteral("gems"), 10);
        add(QStringLiteral("bible-reading"), 4);
        // The section's minutes include a minute of counsel after each part.
        const std::vector<int> ayfmShares =
            shareMinutes(kAyfmSectionMinutes - ayfm * kCounselMinutes, ayfm);
        for (int i = 0; i < ayfm; ++i) {
            add(QStringLiteral("ayfm-%1").arg(i + 1), ayfmShares.at(static_cast<size_t>(i)));
        }
        add(QStringLiteral("middle-song"), 5);
        const std::vector<int> lacShares = shareMinutes(kLacSectionMinutes, lac);
        for (int i = 0; i < lac; ++i) {
            add(QStringLiteral("lac-%1").arg(i + 1), lacShares.at(static_cast<size_t>(i)));
        }
        if (coWeek) {
            add(QStringLiteral("concluding-comments"), 3);
            add(QStringLiteral("co-service-talk"), 30);
        } else {
            add(QStringLiteral("cbs"), 30);
            add(QStringLiteral("concluding-comments"), 3);
        }
        add(QStringLiteral("closing-song"), 5);
    } else if (meetingKind == QLatin1String("weekend")) {
        add(QStringLiteral("opening-song"), 5);
        add(QStringLiteral("public-talk"), 30);
        add(QStringLiteral("middle-song"), 5);
        if (coWeek) {
            add(QStringLiteral("abbreviated-wt"), 30);
            add(QStringLiteral("co-final-talk"), 30);
        } else {
            add(QStringLiteral("wt"), 60);
        }
        add(QStringLiteral("closing-song"), 5);
    }
    return plan;
}

QString MeetingTimer::formatClock(qint64 ms)
{
    const qint64 totalSeconds = qMax<qint64>(0, ms) / 1000;
    return QStringLiteral("%1:%2")
        .arg(totalSeconds / 60, 2, 10, QLatin1Char('0'))
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

QDateTime MeetingTimer::now() const
{
    return m_now();
}

bool MeetingTimer::isMeetingDay() const
{
    return m_meetingKind == QLatin1String("midweek") || m_meetingKind == QLatin1String("weekend");
}

bool MeetingTimer::isStudyPart(const QString& id) const
{
    return id == QLatin1String("wt") || id == QLatin1String("cbs");
}

QVariantList MeetingTimer::parts() const
{
    QVariantList list;
    list.reserve(static_cast<qsizetype>(m_parts.size()));
    for (size_t i = 0; i < m_parts.size(); ++i) {
        const Part& part = m_parts.at(i);
        const int index = static_cast<int>(i);
        const bool current = index == m_currentIndex && m_running;
        qint64 durationMs = -1;
        if (current) {
            durationMs = elapsedMs();
        } else if (part.startedAt.isValid() && part.endedAt.isValid()) {
            durationMs = qMax<qint64>(0, part.startedAt.msecsTo(part.endedAt));
        }
        QString state = QStringLiteral("pending");
        if (current) {
            state = QStringLiteral("current");
        } else if (part.endedAt.isValid()) {
            state = QStringLiteral("done");
        }
        list.append(QVariantMap{
            {QStringLiteral("id"), part.id},
            {QStringLiteral("title"), part.title},
            {QStringLiteral("custom"), part.custom},
            {QStringLiteral("minutes"), part.minutes},
            {QStringLiteral("counsel"), hasCounsel(part.id, part.minutes)},
            {QStringLiteral("plannedStart"), clockTime(plannedStart(index))},
            {QStringLiteral("startedAt"), clockTime(part.startedAt)},
            {QStringLiteral("durationMs"), durationMs},
            {QStringLiteral("over"),
             durationMs > 0 && part.minutes > 0 &&
                 durationMs > static_cast<qint64>(part.minutes) * kMsPerMinute},
            {QStringLiteral("state"), state},
        });
    }
    return list;
}

int MeetingTimer::currentIndex() const
{
    return m_currentIndex;
}

QString MeetingTimer::currentPartId() const
{
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_parts.size())) {
        return {};
    }
    return m_parts.at(static_cast<size_t>(m_currentIndex)).id;
}

int MeetingTimer::nextIndex() const
{
    const int count = static_cast<int>(m_parts.size());
    for (int i = m_currentIndex + 1; i < count; ++i) {
        if (m_parts.at(static_cast<size_t>(i)).minutes > 0) {
            return i;
        }
    }
    return -1;
}

bool MeetingTimer::isRunning() const
{
    return m_running;
}

bool MeetingTimer::isPaused() const
{
    return m_paused;
}

qint64 MeetingTimer::elapsedMs() const
{
    if (m_running && !m_paused && m_segmentClock.isValid()) {
        return m_accumulatedMs + qMax<qint64>(0, m_segmentClock.elapsed());
    }
    return m_accumulatedMs;
}

qint64 MeetingTimer::targetMs() const
{
    if (m_running) {
        return m_targetMs;
    }
    const int index = nextIndex();
    return index >= 0
               ? static_cast<qint64>(m_parts.at(static_cast<size_t>(index)).minutes) * kMsPerMinute
               : 0;
}

bool MeetingTimer::isOvertime() const
{
    return m_running && m_targetMs > 0 && elapsedMs() > m_targetMs;
}

QString MeetingTimer::formattedTime() const
{
    if (!m_running) {
        return formatClock(m_countDown ? targetMs() : 0);
    }
    const qint64 elapsed = elapsedMs();
    if (m_countDown) {
        const qint64 remaining = m_targetMs - elapsed;
        return remaining >= 0 ? formatClock(remaining)
                              : QStringLiteral("-") + formatClock(-remaining);
    }
    if (isOvertime() && m_overtimeAmountOnly) {
        return QStringLiteral("+") + formatClock(elapsed - m_targetMs);
    }
    return formatClock(elapsed);
}

bool MeetingTimer::isCountDown() const
{
    return m_countDown;
}

void MeetingTimer::setCountDown(bool countDown)
{
    if (m_countDown == countDown) {
        return;
    }
    m_countDown = countDown;
    emit optionsChanged();
    emit timeChanged();
}

bool MeetingTimer::isOvertimeAmountOnly() const
{
    return m_overtimeAmountOnly;
}

void MeetingTimer::setOvertimeAmountOnly(bool amountOnly)
{
    if (m_overtimeAmountOnly == amountOnly) {
        return;
    }
    m_overtimeAmountOnly = amountOnly;
    emit optionsChanged();
    emit timeChanged();
}

QString MeetingTimer::meetingKind() const
{
    return m_meetingKind;
}

bool MeetingTimer::isCoWeek() const
{
    return m_coWeek;
}

QString MeetingTimer::dateIso() const
{
    return m_dateIso;
}

QDateTime MeetingTimer::meetingStart() const
{
    return isMeetingDay() ? m_meetingStart : QDateTime{};
}

int MeetingTimer::ayfmCount() const
{
    return m_ayfmCount;
}

void MeetingTimer::setAyfmCount(int count)
{
    const int clamped = qBound(1, count, kMaxAyfmParts);
    if (m_ayfmCount == clamped) {
        return;
    }
    m_ayfmCount = clamped;
    // The section's minutes are shared out again over the new number of parts.
    for (int i = 1; i <= kMaxAyfmParts; ++i) {
        m_minuteOverrides.remove(QStringLiteral("ayfm-%1").arg(i));
    }
    rebuildParts();
    notifyDayState();
}

int MeetingTimer::lacCount() const
{
    return m_lacCount;
}

void MeetingTimer::setLacCount(int count)
{
    const int clamped = qBound(1, count, kMaxLacParts);
    if (m_lacCount == clamped) {
        return;
    }
    m_lacCount = clamped;
    for (int i = 1; i <= kMaxLacParts; ++i) {
        m_minuteOverrides.remove(QStringLiteral("lac-%1").arg(i));
    }
    rebuildParts();
    notifyDayState();
}

bool MeetingTimer::hasStudyPart() const
{
    return std::any_of(m_parts.begin(), m_parts.end(),
                       [this](const Part& part) { return isStudyPart(part.id); });
}

QString MeetingTimer::studyEndTime() const
{
    return m_studyEndTime;
}

void MeetingTimer::setStudyEndTime(const QString& hhmm)
{
    const QTime time = QTime::fromString(hhmm.trimmed(), QStringLiteral("H:mm"));
    const QString normalized = time.isValid() ? time.toString(QStringLiteral("HH:mm")) : QString{};
    if (m_studyEndTime == normalized) {
        return;
    }
    m_studyEndTime = normalized;
    emit partsChanged();
    notifyDayState();
}

QDateTime MeetingTimer::studyEnd() const
{
    if (m_studyEndTime.isEmpty()) {
        return {};
    }
    const QDate date = m_meetingStart.isValid() ? m_meetingStart.date()
                                                : QDate::fromString(m_dateIso, Qt::ISODate);
    if (!date.isValid()) {
        return {};
    }
    return QDateTime(date, QTime::fromString(m_studyEndTime, QStringLiteral("HH:mm")));
}

QDateTime MeetingTimer::plannedStart(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_parts.size())) {
        return {};
    }
    if (isMeetingDay()) {
        if (!m_meetingStart.isValid()) {
            return {};
        }
        qint64 offsetMinutes = 0;
        for (int i = 0; i < index; ++i) {
            const Part& part = m_parts.at(static_cast<size_t>(i));
            offsetMinutes +=
                part.minutes + (hasCounsel(part.id, part.minutes) ? kCounselMinutes : 0);
        }
        return m_meetingStart.addSecs(offsetMinutes * 60);
    }
    // Custom parts have no schedule: they are planned from the first one that really started.
    const auto anchor = std::find_if(m_parts.begin(), m_parts.end(),
                                     [](const Part& part) { return part.startedAt.isValid(); });
    if (anchor == m_parts.end()) {
        return {};
    }
    const int anchorIndex = static_cast<int>(std::distance(m_parts.begin(), anchor));
    qint64 offsetMinutes = 0;
    for (int i = qMin(index, anchorIndex); i < qMax(index, anchorIndex); ++i) {
        offsetMinutes += m_parts.at(static_cast<size_t>(i)).minutes;
    }
    return anchor->startedAt.addSecs((index >= anchorIndex ? offsetMinutes : -offsetMinutes) * 60);
}

bool MeetingTimer::isScheduleOffsetKnown() const
{
    if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_parts.size())) {
        return false;
    }
    return m_parts.at(static_cast<size_t>(m_currentIndex)).startedAt.isValid() &&
           plannedStart(m_currentIndex).isValid();
}

int MeetingTimer::scheduleOffsetMinutes() const
{
    if (!isScheduleOffsetKnown()) {
        return 0;
    }
    const QDateTime planned = plannedStart(m_currentIndex);
    const QDateTime started = m_parts.at(static_cast<size_t>(m_currentIndex)).startedAt;
    return static_cast<int>(qRound(static_cast<double>(planned.secsTo(started)) / 60.0));
}

bool MeetingTimer::hasTimings() const
{
    return !m_timings.isEmpty();
}

void MeetingTimer::setMeeting(const QString& dateIso, const QString& meetingKind, bool coWeek,
                              const QDateTime& meetingStart, const QString& dayState)
{
    const PendingMeeting meeting{dateIso, meetingKind, coWeek, meetingStart, dayState};
    if (m_running && dateIso != m_dateIso) {
        // The running part belongs to its day; the new one comes when it stops.
        m_pending = meeting;
        m_hasPending = true;
        return;
    }
    m_hasPending = false;
    applyMeeting(meeting);
}

void MeetingTimer::applyMeeting(const PendingMeeting& meeting)
{
    const bool sameDay = meeting.dateIso == m_dateIso && meeting.meetingKind == m_meetingKind &&
                         meeting.coWeek == m_coWeek;
    m_meetingStart = meeting.meetingStart;
    if (!sameDay) {
        m_dateIso = meeting.dateIso;
        m_meetingKind = meeting.meetingKind;
        m_coWeek = meeting.coWeek;
        m_currentIndex = -1;
        m_accumulatedMs = 0;
        m_targetMs = 0;
        restoreDayState(meeting.dayState);
    }
    rebuildParts();
    emit timeChanged();
}

void MeetingTimer::restoreDayState(const QString& json)
{
    m_ayfmCount = 3;
    m_lacCount = 2;
    m_studyEndTime.clear();
    m_minuteOverrides.clear();
    m_timings.clear();
    m_customParts.clear();

    const QJsonObject state = QJsonDocument::fromJson(json.toUtf8()).object();
    if (state.isEmpty()) {
        return;
    }
    m_ayfmCount = qBound(1, state.value(kStateAyfm).toInt(3), kMaxAyfmParts);
    m_lacCount = qBound(1, state.value(kStateLac).toInt(2), kMaxLacParts);
    const QTime studyEnd =
        QTime::fromString(state.value(kStateStudyEnd).toString(), QStringLiteral("HH:mm"));
    m_studyEndTime = studyEnd.isValid() ? studyEnd.toString(QStringLiteral("HH:mm")) : QString{};
    const QJsonObject minutes = state.value(kStateMinutes).toObject();
    for (auto it = minutes.begin(); it != minutes.end(); ++it) {
        m_minuteOverrides.insert(it.key(), qBound(0, it.value().toInt(), kMaxPartMinutes));
    }
    const QJsonObject timings = state.value(kStateTimings).toObject();
    for (auto it = timings.begin(); it != timings.end(); ++it) {
        const QJsonObject timing = it.value().toObject();
        m_timings.insert(
            it.key(),
            {QDateTime::fromString(timing.value(QStringLiteral("start")).toString(), Qt::ISODate),
             QDateTime::fromString(timing.value(QStringLiteral("end")).toString(), Qt::ISODate)});
    }
    const QJsonArray custom = state.value(kStateCustom).toArray();
    for (const QJsonValue& value : custom) {
        const QJsonObject object = value.toObject();
        Part part;
        part.id = object.value(QStringLiteral("id")).toString();
        part.title = object.value(QStringLiteral("title")).toString();
        part.minutes = qBound(0, object.value(QStringLiteral("minutes")).toInt(), kMaxPartMinutes);
        part.custom = true;
        if (!part.id.isEmpty()) {
            m_customParts.push_back(part);
        }
    }
}

QString MeetingTimer::exportDayState() const
{
    QJsonObject state;
    state.insert(kStateAyfm, m_ayfmCount);
    state.insert(kStateLac, m_lacCount);
    if (!m_studyEndTime.isEmpty()) {
        state.insert(kStateStudyEnd, m_studyEndTime);
    }
    QJsonObject minutes;
    for (auto it = m_minuteOverrides.cbegin(); it != m_minuteOverrides.cend(); ++it) {
        minutes.insert(it.key(), it.value());
    }
    state.insert(kStateMinutes, minutes);
    QJsonObject timings;
    for (auto it = m_timings.cbegin(); it != m_timings.cend(); ++it) {
        QJsonObject timing;
        if (it.value().first.isValid()) {
            timing.insert(QStringLiteral("start"), it.value().first.toString(Qt::ISODate));
        }
        if (it.value().second.isValid()) {
            timing.insert(QStringLiteral("end"), it.value().second.toString(Qt::ISODate));
        }
        timings.insert(it.key(), timing);
    }
    state.insert(kStateTimings, timings);
    QJsonArray custom;
    for (const Part& part : m_customParts) {
        custom.append(QJsonObject{{QStringLiteral("id"), part.id},
                                  {QStringLiteral("title"), part.title},
                                  {QStringLiteral("minutes"), part.minutes}});
    }
    state.insert(kStateCustom, custom);
    return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
}

QList<QStringList> MeetingTimer::timingReportRows(const QVariantMap& titles) const
{
    QList<QStringList> rows;
    for (size_t i = 0; i < m_parts.size(); ++i) {
        const Part& part = m_parts.at(i);
        const QString title = part.custom ? part.title : titles.value(part.id, part.id).toString();
        const qint64 plannedMs = static_cast<qint64>(part.minutes) * kMsPerMinute;
        QString took;
        QString difference;
        if (part.startedAt.isValid() && part.endedAt.isValid()) {
            const qint64 tookMs = qMax<qint64>(0, part.startedAt.msecsTo(part.endedAt));
            took = formatClock(tookMs);
            const qint64 deltaMs = tookMs - plannedMs;
            difference = (deltaMs >= 0 ? QStringLiteral("+") : QStringLiteral("-")) +
                         formatClock(deltaMs >= 0 ? deltaMs : -deltaMs);
        }
        rows.append({title, clockTime(plannedStart(static_cast<int>(i))), clockTime(part.startedAt),
                     formatClock(plannedMs), took, difference});
    }
    return rows;
}

bool MeetingTimer::saveTimingReport(const QUrl& file, const QVariantMap& titles,
                                    const QStringList& headers, const QString& heading) const
{
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (path.isEmpty()) {
        return false;
    }
    const QList<QStringList> rows = timingReportRows(titles);

    if (path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
        const auto csvField = [](QString value) {
            if (value.contains(QLatin1Char(',')) || value.contains(QLatin1Char('"')) ||
                value.contains(QLatin1Char('\n'))) {
                value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
                return QStringLiteral("\"%1\"").arg(value);
            }
            return value;
        };
        QString csv;
        QStringList headerFields;
        for (const QString& header : headers) {
            headerFields.append(csvField(header));
        }
        csv += headerFields.join(QLatin1Char(',')) + QStringLiteral("\r\n");
        for (const QStringList& row : rows) {
            QStringList fields;
            for (const QString& value : row) {
                fields.append(csvField(value));
            }
            csv += fields.join(QLatin1Char(',')) + QStringLiteral("\r\n");
        }
        QSaveFile out(path);
        if (!out.open(QIODevice::WriteOnly)) {
            return false;
        }
        // A byte order mark, so spreadsheet programs read the accents right.
        out.write("\xEF\xBB\xBF");
        out.write(csv.toUtf8());
        return out.commit();
    }

    QString html = QStringLiteral("<h2>%1</h2><table width=\"100%\" cellspacing=\"0\" "
                                  "cellpadding=\"4\" border=\"1\"><tr>")
                       .arg(heading.toHtmlEscaped());
    for (const QString& header : headers) {
        html += QStringLiteral("<th align=\"left\">%1</th>").arg(header.toHtmlEscaped());
    }
    html += QStringLiteral("</tr>");
    for (const QStringList& row : rows) {
        html += QStringLiteral("<tr>");
        for (const QString& value : row) {
            html += QStringLiteral("<td>%1</td>").arg(value.toHtmlEscaped());
        }
        html += QStringLiteral("</tr>");
    }
    html += QStringLiteral("</table>");

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(96);
    writer.setTitle(heading);
    QTextDocument document;
    document.setHtml(html);
    document.setPageSize(QSizeF(writer.width(), writer.height()));
    document.print(&writer);
    return QFile::exists(path);
}

void MeetingTimer::notifyDayState()
{
    if (!m_dateIso.isEmpty()) {
        emit dayStateChanged(m_dateIso, exportDayState());
    }
}

void MeetingTimer::rebuildParts()
{
    const QString currentId = currentPartId();
    m_parts.clear();
    if (isMeetingDay()) {
        for (const PartPlan& plan : defaultPlan(m_meetingKind, m_coWeek, m_ayfmCount, m_lacCount)) {
            Part part;
            part.id = plan.id;
            part.minutes = m_minuteOverrides.value(plan.id, plan.minutes);
            m_parts.push_back(part);
        }
    } else {
        m_parts = m_customParts;
    }
    for (Part& part : m_parts) {
        const auto timing = m_timings.constFind(part.id);
        if (timing != m_timings.cend()) {
            part.startedAt = timing->first;
            part.endedAt = timing->second;
        }
    }
    m_currentIndex = -1;
    for (size_t i = 0; i < m_parts.size(); ++i) {
        if (m_parts.at(i).id == currentId) {
            m_currentIndex = static_cast<int>(i);
        }
    }
    emit partsChanged();
}

void MeetingTimer::startPart(int index)
{
    if (index < 0 || index >= static_cast<int>(m_parts.size())) {
        return;
    }
    if (m_running) {
        stop();
    }
    Part& part = m_parts.at(static_cast<size_t>(index));
    const QDateTime startedAt = now();
    part.startedAt = startedAt;
    part.endedAt = {};
    m_timings.insert(part.id, {startedAt, {}});

    m_currentIndex = index;
    m_accumulatedMs = 0;
    m_targetMs = static_cast<qint64>(part.minutes) * kMsPerMinute;
    // Counting down, the study may have to end at a set time.
    const QDateTime end = studyEnd();
    if (m_countDown && isStudyPart(part.id) && end.isValid()) {
        m_targetMs = qMin(m_targetMs, qMax<qint64>(0, startedAt.msecsTo(end)));
    }
    m_segmentClock.start();
    m_running = true;
    m_paused = false;
    m_tickTimer.start();
    emit partsChanged();
    emit runningChanged();
    emit timeChanged();
    notifyDayState();
}

void MeetingTimer::startNext()
{
    const int index = nextIndex();
    if (index >= 0) {
        startPart(index);
    }
}

void MeetingTimer::togglePause()
{
    if (!m_running) {
        return;
    }
    if (m_paused) {
        m_segmentClock.start();
        m_paused = false;
        m_tickTimer.start();
    } else {
        m_accumulatedMs += qMax<qint64>(0, m_segmentClock.elapsed());
        m_paused = true;
        m_tickTimer.stop();
    }
    emit runningChanged();
    emit timeChanged();
}

void MeetingTimer::stop()
{
    if (!m_running) {
        return;
    }
    if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_parts.size())) {
        Part& part = m_parts.at(static_cast<size_t>(m_currentIndex));
        // The part lasted what the monotonic clock measured, pauses left out.
        const qint64 lasted = elapsedMs();
        part.endedAt = part.startedAt.isValid() ? part.startedAt.addMSecs(lasted) : now();
        m_timings.insert(part.id, {part.startedAt, part.endedAt});
    }
    m_tickTimer.stop();
    m_running = false;
    m_paused = false;
    m_accumulatedMs = 0;
    m_targetMs = 0;
    emit partsChanged();
    emit runningChanged();
    emit timeChanged();
    notifyDayState();
    if (m_hasPending) {
        m_hasPending = false;
        applyMeeting(m_pending);
    }
}

void MeetingTimer::setPartMinutes(int index, int minutes)
{
    if (index < 0 || index >= static_cast<int>(m_parts.size())) {
        return;
    }
    const int clamped = qBound(0, minutes, kMaxPartMinutes);
    Part& part = m_parts.at(static_cast<size_t>(index));
    if (part.minutes == clamped) {
        return;
    }
    part.minutes = clamped;
    if (part.custom) {
        for (Part& custom : m_customParts) {
            if (custom.id == part.id) {
                custom.minutes = clamped;
            }
        }
    } else {
        m_minuteOverrides.insert(part.id, clamped);
    }
    if (index == m_currentIndex && m_running) {
        m_targetMs = static_cast<qint64>(clamped) * kMsPerMinute;
        emit timeChanged();
    }
    emit partsChanged();
    notifyDayState();
}

void MeetingTimer::adjustPartMinutes(int index, int deltaMinutes)
{
    if (index < 0 || index >= static_cast<int>(m_parts.size())) {
        return;
    }
    setPartMinutes(index, m_parts.at(static_cast<size_t>(index)).minutes + deltaMinutes);
}

void MeetingTimer::resetTimings()
{
    if (m_running) {
        stop();
    }
    m_timings.clear();
    m_currentIndex = -1;
    rebuildParts();
    emit timeChanged();
    notifyDayState();
}

void MeetingTimer::addCustomPart(const QString& title, int minutes)
{
    if (isMeetingDay()) {
        return;
    }
    int number = 0;
    for (const Part& part : m_customParts) {
        number = qMax(number, part.id.section(QLatin1Char('-'), 1).toInt());
    }
    Part part;
    part.id = QStringLiteral("custom-%1").arg(number + 1);
    part.title = title.trimmed();
    part.minutes = qBound(0, minutes, kMaxPartMinutes);
    part.custom = true;
    m_customParts.push_back(part);
    rebuildParts();
    notifyDayState();
}

void MeetingTimer::removeCustomPart(int index)
{
    if (index < 0 || index >= static_cast<int>(m_parts.size()) ||
        !m_parts.at(static_cast<size_t>(index)).custom) {
        return;
    }
    if (index == m_currentIndex && m_running) {
        stop();
    }
    const QString id = m_parts.at(static_cast<size_t>(index)).id;
    m_customParts.erase(std::remove_if(m_customParts.begin(), m_customParts.end(),
                                       [&id](const Part& part) { return part.id == id; }),
                        m_customParts.end());
    m_timings.remove(id);
    rebuildParts();
    notifyDayState();
}

void MeetingTimer::renameCustomPart(int index, const QString& title)
{
    if (index < 0 || index >= static_cast<int>(m_parts.size()) ||
        !m_parts.at(static_cast<size_t>(index)).custom) {
        return;
    }
    const QString id = m_parts.at(static_cast<size_t>(index)).id;
    for (Part& part : m_customParts) {
        if (part.id == id) {
            part.title = title.trimmed();
        }
    }
    m_parts.at(static_cast<size_t>(index)).title = title.trimmed();
    emit partsChanged();
    notifyDayState();
}

void MeetingTimer::setClockForTest(std::function<QDateTime()> now)
{
    m_now = std::move(now);
}

void MeetingTimer::advanceForTest(qint64 deltaMs)
{
    if (deltaMs <= 0) {
        return;
    }
    m_accumulatedMs += deltaMs;
    emit timeChanged();
}

} // namespace kmedia
