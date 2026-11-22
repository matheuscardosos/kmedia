#include "core/MeetingCalendarModel.h"
#include "models/PlaylistModel.h"
#include "network/MeetingCatalogService.h"

#include <QSignalSpy>
#include <QVariantMap>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("MeetingCalendarModel selects days and opens weeks", "[core][calendar]")
{
    kmedia::MeetingCalendarModel model;
    model.setScheduleDays(3, QStringLiteral("19:30"), 7, QStringLiteral("09:30"));
    QSignalSpy daySpy(&model, &kmedia::MeetingCalendarModel::daySelected);

    model.selectDate(QStringLiteral("2026-01-15"));
    REQUIRE(daySpy.count() == 1);
    REQUIRE(model.selectedWeekMonday() == QDate(2026, 1, 12));
    REQUIRE(model.selectedMeetingKind() == QStringLiteral("custom"));

    model.shiftSelectedDate(-1);
    REQUIRE(model.selectedDateIso() == QStringLiteral("2026-01-14"));
    REQUIRE(model.selectedMeetingKind() == QStringLiteral("midweek"));

    QSignalSpy openSpy(&model, &kmedia::MeetingCalendarModel::weekOpenRequested);
    model.openWeekForDate(QStringLiteral("2026-05-08"), true);
    REQUIRE(openSpy.count() == 1);
    REQUIRE(openSpy.at(0).at(0).toDate() == QDate(2026, 5, 4));
    REQUIRE(openSpy.at(0).at(1).toBool());
}

TEST_CASE("MeetingCalendarModel validates congregation schedule bounds and time normalization",
          "[core][calendar]")
{
    kmedia::MeetingCalendarModel model;
    QSignalSpy scheduleSpy(&model, &kmedia::MeetingCalendarModel::scheduleChanged);

    model.setScheduleDays(99, QStringLiteral("invalid"), 1, QStringLiteral("8:5"));
    REQUIRE(scheduleSpy.count() == 1);
    REQUIRE(model.midweekDay() == 5);
    REQUIRE(model.weekendDay() == 6);
    REQUIRE(model.midweekTime() == QStringLiteral("19:30"));
    REQUIRE(model.weekendTime() == QStringLiteral("08:05"));
    REQUIRE(model.scheduleSummary() == QStringLiteral("Fri 19:30 | Sat 08:05"));
}

TEST_CASE("MeetingCalendarModel handles CO visit and special week overrides",
          "[core][calendar][events]")
{
    kmedia::MeetingCalendarModel model;
    QSignalSpy eventsSpy(&model, &kmedia::MeetingCalendarModel::specialEventsChanged);

    model.selectDate(QStringLiteral("2026-06-10"));
    model.setWeekSpecialEvent(QStringLiteral("2026-06-10"), QStringLiteral("co_visit"));
    REQUIRE(eventsSpy.count() == 1);
    REQUIRE(model.selectedWeekEventType() == QStringLiteral("co_visit"));
    REQUIRE(model.selectedWeekEventLabel() == QStringLiteral("Circuit Overseer Visit"));
    REQUIRE_FALSE(
        kmedia::MeetingCalendarModel::isRegularStudySkippedForEvent(QStringLiteral("co_visit")));
    REQUIRE(
        kmedia::MeetingCalendarModel::isRegularStudySkippedForEvent(QStringLiteral("memorial")));
    REQUIRE(kmedia::MeetingCalendarModel::isRegularStudySkippedForEvent(
        QStringLiteral("circuit_assembly")));

    model.setWeekSpecialEvent(QStringLiteral("2026-06-24"), QStringLiteral("memorial"),
                              QStringLiteral("Annual Memorial"));
    const QString exported = model.exportSpecialEventsJson();

    kmedia::MeetingCalendarModel restored;
    restored.loadSpecialEventsJson(exported);
    restored.selectDate(QStringLiteral("2026-06-24"));
    REQUIRE(restored.selectedWeekEventType() == QStringLiteral("memorial"));
    REQUIRE(restored.selectedWeekEventLabel() == QStringLiteral("Annual Memorial"));

    // A day in a Circuit Overseer week gets the Circuit Overseer section after the meeting's own.
    kmedia::PlaylistModel playlist;
    REQUIRE(playlist.openWorkspaceForDate(
        QStringLiteral("2026-06-10"), QStringLiteral("midweek"), {},
        restored.eventTypeForDate(QStringLiteral("2026-06-10")) == QStringLiteral("co_visit")));
    const QVariantList sections = playlist.sections();
    REQUIRE(sections.size() == 4);
    REQUIRE(sections.at(3).toMap().value(QStringLiteral("id")).toString() ==
            QStringLiteral("circuit-overseer"));
    REQUIRE(sections.at(3).toMap().value(QStringLiteral("name")).toString() ==
            QStringLiteral("Circuit Overseer"));
    REQUIRE(playlist.colorForSection(QStringLiteral("circuit-overseer")) ==
            QStringLiteral("#945EB5"));

    restored.clearWeekSpecialEvent(QStringLiteral("2026-06-24"));
    REQUIRE(restored.selectedWeekEventType().isEmpty());

    // CO visit shifts midweek meeting to Tuesday; Memorial Mon-Fri suppresses regular midweek.
    restored.setScheduleDays(3, QStringLiteral("19:30"), 7, QStringLiteral("09:30"));
    restored.setWeekSpecialEvent(QStringLiteral("2026-11-18"), QStringLiteral("co_visit"));
    REQUIRE(restored.meetingKindForDate(QStringLiteral("2026-11-17")) == QStringLiteral("midweek"));
    REQUIRE(restored.meetingKindForDate(QStringLiteral("2026-11-18")) == QStringLiteral("custom"));

    restored.setWeekSpecialEvent(QStringLiteral("2026-04-02"), QStringLiteral("memorial"),
                                 QStringLiteral("Memorial"), QStringLiteral("2026-04-02"));
    REQUIRE(restored.meetingKindForDate(QStringLiteral("2026-04-02")) ==
            QStringLiteral("memorial"));
    REQUIRE(restored.meetingKindForDate(QStringLiteral("2026-04-01")) == QStringLiteral("custom"));
    REQUIRE(restored.meetingKindForDate(QStringLiteral("2026-04-05")) == QStringLiteral("weekend"));

    restored.setTempWeekSchedule(QStringLiteral("2026-12-09"), 4, QStringLiteral("20:00"), 6,
                                 QStringLiteral("17:00"));
    REQUIRE(restored.meetingKindForDate(QStringLiteral("2026-12-10")) == QStringLiteral("midweek"));
    REQUIRE(restored.meetingKindForDate(QStringLiteral("2026-12-12")) == QStringLiteral("weekend"));

    // Meeting start times follow the same overrides; days without a meeting have none.
    REQUIRE(restored.meetingStartForDate(QDate(2026, 12, 10)) ==
            QDateTime(QDate(2026, 12, 10), QTime(20, 0)));
    REQUIRE(restored.meetingStartForDate(QDate(2026, 12, 12)) ==
            QDateTime(QDate(2026, 12, 12), QTime(17, 0)));
    REQUIRE(restored.meetingStartForDate(QDate(2026, 11, 17)) ==
            QDateTime(QDate(2026, 11, 17), QTime(19, 30)));
    REQUIRE(restored.meetingStartForDate(QDate(2026, 11, 22)) ==
            QDateTime(QDate(2026, 11, 22), QTime(9, 30)));
    REQUIRE_FALSE(restored.meetingStartForDate(QDate(2026, 11, 18)).isValid());
    REQUIRE_FALSE(restored.meetingStartForDate(QDate(2026, 11, 16)).isValid());
    const QString exportedTemp = restored.exportTempSchedulesJson();
    kmedia::MeetingCalendarModel reloadedTemp;
    reloadedTemp.loadTempSchedulesJson(exportedTemp);
    REQUIRE(reloadedTemp.meetingKindForDate(QStringLiteral("2026-12-10")) ==
            QStringLiteral("midweek"));
}
