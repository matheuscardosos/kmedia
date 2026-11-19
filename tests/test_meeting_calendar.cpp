#include "core/MeetingCalendarModel.h"
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
