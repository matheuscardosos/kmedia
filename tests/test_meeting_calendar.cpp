#include "core/MeetingCalendarModel.h"
#include "models/PlaylistModel.h"
#include "network/MeetingCatalogService.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariantMap>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("MeetingCalendarModel builds Monday-first 42-day grid and wraps months cleanly",
          "[core][calendar]")
{
    kmedia::MeetingCalendarModel model;
    REQUIRE(model.calendarDays().size() == 42);
    REQUIRE(model.upcomingMeetings().size() == 8);

    model.selectDate(QStringLiteral("2026-01-15"));
    REQUIRE(model.visibleYear() == 2026);
    REQUIRE(model.visibleMonth() == 1);
    REQUIRE(model.selectedWeekMonday() == QDate(2026, 1, 12));

    model.previousMonth();
    REQUIRE(model.visibleYear() == 2025);
    REQUIRE(model.visibleMonth() == 12);

    model.nextMonth();
    REQUIRE(model.visibleYear() == 2026);
    REQUIRE(model.visibleMonth() == 1);
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

TEST_CASE("MeetingCalendarModel detects cached issue readiness and emits weekOpenRequested",
          "[core][calendar]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    kmedia::MeetingCalendarModel model;
    model.setStorageRoot(tempDir.path());
    model.setMediaLanguage(QStringLiteral("T"));
    model.setScheduleDays(3, QStringLiteral("19:30"), 7, QStringLiteral("09:30"));
    model.selectDate(QStringLiteral("2026-05-06")); // Wednesday

    const int mwbTag = kmedia::MeetingCatalogService::midweekIssueTagForDate(QDate(2026, 5, 6));
    const QString mwbDir = tempDir.filePath(QStringLiteral("catalog/T/mwb_%1").arg(mwbTag));
    REQUIRE(QDir().mkpath(mwbDir));

    // Partial and zero-byte files must not count as cached media.
    QFile partFile(mwbDir + QStringLiteral("/mwb_T.jwpub.kmedia.part"));
    REQUIRE(partFile.open(QIODevice::WriteOnly));
    partFile.write("partial");
    partFile.close();

    REQUIRE(kmedia::MeetingCalendarModel::countLocalIssueFiles(tempDir.path(), QStringLiteral("T"),
                                                               QStringLiteral("mwb"), mwbTag) == 0);

    // Completed non-empty file marks the Wednesday cell and upcoming card ready.
    QFile doneFile(mwbDir + QStringLiteral("/mwb_T.jwpub"));
    REQUIRE(doneFile.open(QIODevice::WriteOnly));
    doneFile.write("zip-bytes");
    doneFile.close();

    model.refreshReadiness();

    bool foundReadyWed = false;
    for (const QVariant& item : model.calendarDays()) {
        const QVariantMap cell = item.toMap();
        if (cell.value(QStringLiteral("dateIso")).toString() == QStringLiteral("2026-05-06")) {
            foundReadyWed =
                cell.value(QStringLiteral("mediaReady")).toBool() &&
                cell.value(QStringLiteral("cachedCount")).toInt() == 1 &&
                cell.value(QStringLiteral("meetingKind")).toString() == QStringLiteral("midweek");
        }
    }
    REQUIRE(foundReadyWed);

    QSignalSpy openSpy(&model, &kmedia::MeetingCalendarModel::weekOpenRequested);
    model.openWeekForDate(QStringLiteral("2026-05-08"), true);
    REQUIRE(openSpy.count() == 1);
    REQUIRE(openSpy.at(0).at(0).toDate() == QDate(2026, 5, 4));
    REQUIRE(openSpy.at(0).at(1).toBool() == true);
}

TEST_CASE(
    "MeetingCalendarModel and MeetingCatalogService handle CO visit and special week overrides",
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

    // Verify Circuit Overseer section injection and Memorial auto-sync skip.
    kmedia::PlaylistModel playlist;
    kmedia::MeetingCatalogService catalog(nullptr, nullptr, &playlist);
    catalog.setSpecialWeekEvents(restored.specialEventsMap());

    catalog.setTargetWeekDate(QDate(2026, 6, 10));
    REQUIRE(catalog.weekEventType() == QStringLiteral("co_visit"));
    catalog.syncCurrentWeekToPlaylist();
    REQUIRE(playlist.sections().size() == 1);
    REQUIRE(playlist.sections().at(0).toMap().value(QStringLiteral("id")).toString() ==
            QStringLiteral("circuit-overseer"));
    REQUIRE(playlist.sections().at(0).toMap().value(QStringLiteral("name")).toString() ==
            QStringLiteral("Circuit Overseer"));
    REQUIRE(playlist.colorForSection(QStringLiteral("circuit-overseer")) ==
            QStringLiteral("#945EB5"));

    catalog.setTargetWeekDate(QDate(2026, 6, 24));
    catalog.syncCurrentWeekToPlaylist();
    REQUIRE(catalog.statusMessage().contains(QStringLiteral("skipped regular weekly auto-sync")));

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
    const QString exportedTemp = restored.exportTempSchedulesJson();
    kmedia::MeetingCalendarModel reloadedTemp;
    reloadedTemp.loadTempSchedulesJson(exportedTemp);
    REQUIRE(reloadedTemp.meetingKindForDate(QStringLiteral("2026-12-10")) ==
            QStringLiteral("midweek"));
}
