#include "core/MeetingTimer.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include <catch2/catch_test_macros.hpp>

namespace {

QStringList partIds(const kmedia::MeetingTimer& timer)
{
    QStringList ids;
    for (const QVariant& part : timer.parts()) {
        ids.append(part.toMap().value(QStringLiteral("id")).toString());
    }
    return ids;
}

QVariantMap partAt(const kmedia::MeetingTimer& timer, int index)
{
    return timer.parts().at(index).toMap();
}

int sumMinutes(const std::vector<kmedia::MeetingTimer::PartPlan>& plan)
{
    int total = 0;
    for (const auto& part : plan) {
        total += part.minutes + (part.counselAfter ? 1 : 0);
    }
    return total;
}

} // namespace

TEST_CASE("The meeting timer lays out the parts of each meeting", "[core][timer]")
{
    using kmedia::MeetingTimer;

    const auto midweek = MeetingTimer::defaultPlan(QStringLiteral("midweek"), false, 3, 2);
    QStringList ids;
    for (const auto& part : midweek) {
        ids.append(part.id);
    }
    REQUIRE(ids == QStringList{"opening-song", "introduction", "treasures", "gems", "bible-reading",
                               "ayfm-1", "ayfm-2", "ayfm-3", "middle-song", "lac-1", "lac-2", "cbs",
                               "concluding-comments", "closing-song"});
    // Apply Yourself shares 15 minutes with a minute of counsel after each part.
    REQUIRE(midweek.at(5).minutes == 4);
    REQUIRE(midweek.at(5).counselAfter);
    REQUIRE(midweek.at(4).counselAfter);
    REQUIRE_FALSE(midweek.at(2).counselAfter);
    REQUIRE(midweek.at(9).minutes == 8);
    REQUIRE(midweek.at(10).minutes == 7);
    REQUIRE(sumMinutes(midweek) == 104);

    const auto midweekCo = MeetingTimer::defaultPlan(QStringLiteral("midweek"), true, 4, 3);
    REQUIRE(midweekCo.at(midweekCo.size() - 2).id == QStringLiteral("co-service-talk"));
    REQUIRE(midweekCo.at(midweekCo.size() - 3).id == QStringLiteral("concluding-comments"));
    int ayfmMinutes = 0;
    int lacMinutes = 0;
    for (const auto& part : midweekCo) {
        if (part.id.startsWith(QLatin1String("ayfm-"))) {
            ayfmMinutes += part.minutes + 1;
        }
        if (part.id.startsWith(QLatin1String("lac-"))) {
            lacMinutes += part.minutes;
        }
    }
    REQUIRE(ayfmMinutes == 15);
    REQUIRE(lacMinutes == 15);

    const auto weekend = MeetingTimer::defaultPlan(QStringLiteral("weekend"), false, 3, 2);
    REQUIRE(sumMinutes(weekend) == 105);
    const auto weekendCo = MeetingTimer::defaultPlan(QStringLiteral("weekend"), true, 3, 2);
    REQUIRE(weekendCo.at(3).id == QStringLiteral("abbreviated-wt"));
    REQUIRE(weekendCo.at(4).id == QStringLiteral("co-final-talk"));

    REQUIRE(MeetingTimer::defaultPlan(QStringLiteral("custom"), false, 3, 2).empty());
    REQUIRE(MeetingTimer::shareMinutes(15, 2) == std::vector<int>{8, 7});
    REQUIRE(MeetingTimer::formatClock(65000) == QStringLiteral("01:05"));
}

TEST_CASE("The meeting timer times parts, keeps them with the day, and tracks the schedule",
          "[core][timer]")
{
    kmedia::MeetingTimer timer;
    QDateTime clock(QDate(2026, 11, 25), QTime(19, 30));
    timer.setClockForTest([&clock]() { return clock; });
    QSignalSpy daySpy(&timer, &kmedia::MeetingTimer::dayStateChanged);

    timer.setMeeting(QStringLiteral("2026-11-25"), QStringLiteral("midweek"), false,
                     QDateTime(QDate(2026, 11, 25), QTime(19, 30)));
    REQUIRE(partIds(timer).size() == 14);
    REQUIRE(partAt(timer, 0).value(QStringLiteral("plannedStart")).toString() ==
            QStringLiteral("19:30"));
    // Bible reading starts after 26 minutes; the first student part after its minute of counsel.
    REQUIRE(partAt(timer, 4).value(QStringLiteral("plannedStart")).toString() ==
            QStringLiteral("19:56"));
    REQUIRE(partAt(timer, 5).value(QStringLiteral("plannedStart")).toString() ==
            QStringLiteral("20:01"));
    REQUIRE(timer.nextIndex() == 0);
    REQUIRE_FALSE(timer.isRunning());
    REQUIRE(timer.meetingStart() == QDateTime(QDate(2026, 11, 25), QTime(19, 30)));

    // Started two minutes late.
    clock = clock.addSecs(120);
    timer.startPart(0);
    REQUIRE(timer.isRunning());
    REQUIRE(timer.currentPartId() == QStringLiteral("opening-song"));
    REQUIRE(timer.isScheduleOffsetKnown());
    REQUIRE(timer.scheduleOffsetMinutes() == 2);
    REQUIRE(timer.targetMs() == 5 * 60000LL);

    timer.advanceForTest(6 * 60000LL);
    REQUIRE(timer.isOvertime());
    // Like Meeting Media Manager, only the extra time shows by default.
    REQUIRE(timer.formattedTime() == QStringLiteral("+01:00"));
    timer.setOvertimeAmountOnly(false);
    REQUIRE(timer.formattedTime() == QStringLiteral("06:00"));
    timer.setOvertimeAmountOnly(true);
    REQUIRE(timer.formattedTime() == QStringLiteral("+01:00"));
    timer.setCountDown(true);
    REQUIRE(timer.formattedTime() == QStringLiteral("-01:00"));

    // Pausing stops the time.
    timer.togglePause();
    REQUIRE(timer.isPaused());
    const qint64 pausedAt = timer.elapsedMs();
    REQUIRE(timer.elapsedMs() == pausedAt);
    timer.togglePause();
    REQUIRE_FALSE(timer.isPaused());

    clock = clock.addSecs(6 * 60);
    timer.startNext();
    REQUIRE(timer.currentPartId() == QStringLiteral("introduction"));
    const QVariantMap song = partAt(timer, 0);
    REQUIRE(song.value(QStringLiteral("state")).toString() == QStringLiteral("done"));
    REQUIRE(song.value(QStringLiteral("over")).toBool());
    REQUIRE(song.value(QStringLiteral("startedAt")).toString() == QStringLiteral("19:32"));
    REQUIRE(timer.scheduleOffsetMinutes() == 3);
    timer.stop();
    REQUIRE_FALSE(timer.isRunning());
    REQUIRE(timer.nextIndex() == 2);
    REQUIRE(daySpy.count() > 0);
    REQUIRE(timer.hasTimings());

    // Minutes changed by hand, and fewer Apply Yourself parts sharing the section again.
    timer.setPartMinutes(2, 12);
    REQUIRE(partAt(timer, 2).value(QStringLiteral("minutes")).toInt() == 12);
    timer.setAyfmCount(2);
    REQUIRE(partIds(timer).size() == 13);
    REQUIRE(partAt(timer, 5).value(QStringLiteral("minutes")).toInt() == 7);
    timer.setStudyEndTime(QStringLiteral("21:05"));
    REQUIRE(timer.studyEndTime() == QStringLiteral("21:05"));

    // Another day starts clean, and coming back restores what was kept.
    const QString kept = timer.exportDayState();
    timer.setMeeting(QStringLiteral("2026-11-29"), QStringLiteral("weekend"), false,
                     QDateTime(QDate(2026, 11, 29), QTime(9, 30)));
    REQUIRE(partIds(timer).size() == 5);
    REQUIRE_FALSE(timer.hasTimings());
    timer.setMeeting(QStringLiteral("2026-11-25"), QStringLiteral("midweek"), false,
                     QDateTime(QDate(2026, 11, 25), QTime(19, 30)), kept);
    REQUIRE(partIds(timer).size() == 13);
    REQUIRE(partAt(timer, 2).value(QStringLiteral("minutes")).toInt() == 12);
    REQUIRE(partAt(timer, 0).value(QStringLiteral("state")).toString() == QStringLiteral("done"));
    REQUIRE(timer.studyEndTime() == QStringLiteral("21:05"));

    timer.resetTimings();
    REQUIRE_FALSE(timer.hasTimings());
}

TEST_CASE("The meeting timer counts the study down to a set end and stays on its running day",
          "[core][timer]")
{
    kmedia::MeetingTimer timer;
    QDateTime clock(QDate(2026, 11, 29), QTime(10, 20));
    timer.setClockForTest([&clock]() { return clock; });
    timer.setCountDown(true);
    timer.setMeeting(QStringLiteral("2026-11-29"), QStringLiteral("weekend"), false,
                     QDateTime(QDate(2026, 11, 29), QTime(9, 30)));
    REQUIRE(timer.hasStudyPart());

    // Running late: the study must end at 11:10, fifty minutes from now.
    timer.setStudyEndTime(QStringLiteral("11:10"));
    timer.startPart(3);
    REQUIRE(timer.currentPartId() == QStringLiteral("wt"));
    REQUIRE(timer.targetMs() == 50 * 60000LL);
    REQUIRE(timer.formattedTime() == QStringLiteral("50:00"));

    // Opening another day while a part runs waits until it stops.
    timer.setMeeting(QStringLiteral("2026-11-25"), QStringLiteral("midweek"), false,
                     QDateTime(QDate(2026, 11, 25), QTime(19, 30)));
    REQUIRE(timer.dateIso() == QStringLiteral("2026-11-29"));
    timer.stop();
    REQUIRE(timer.dateIso() == QStringLiteral("2026-11-25"));
    REQUIRE(partIds(timer).size() == 14);
}

TEST_CASE("The meeting timer times custom parts on days without a meeting", "[core][timer]")
{
    kmedia::MeetingTimer timer;
    QDateTime clock(QDate(2026, 12, 5), QTime(10, 0));
    timer.setClockForTest([&clock]() { return clock; });
    timer.setMeeting(QStringLiteral("2026-12-05"), QStringLiteral("custom"), false, {});
    REQUIRE(timer.parts().isEmpty());

    timer.addCustomPart(QStringLiteral("Talk"), 20);
    timer.addCustomPart(QStringLiteral("Interview"), 10);
    REQUIRE(partIds(timer) == QStringList{"custom-1", "custom-2"});
    timer.renameCustomPart(1, QStringLiteral("Experience"));
    REQUIRE(partAt(timer, 1).value(QStringLiteral("title")).toString() ==
            QStringLiteral("Experience"));
    REQUIRE(partAt(timer, 1).value(QStringLiteral("plannedStart")).toString().isEmpty());

    // Planned from the first part that really started.
    timer.startPart(0);
    REQUIRE(partAt(timer, 1).value(QStringLiteral("plannedStart")).toString() ==
            QStringLiteral("10:20"));
    timer.stop();

    timer.removeCustomPart(0);
    REQUIRE(partIds(timer) == QStringList{"custom-2"});
    // Meeting days have their own parts.
    timer.setMeeting(QStringLiteral("2026-12-06"), QStringLiteral("weekend"), false, {});
    timer.addCustomPart(QStringLiteral("Extra"), 5);
    REQUIRE(partIds(timer).size() == 5);
}

TEST_CASE("The meeting timer saves a timing report as CSV and PDF", "[core][timer]")
{
    kmedia::MeetingTimer timer;
    QDateTime clock(QDate(2026, 11, 29), QTime(9, 31));
    timer.setClockForTest([&clock]() { return clock; });
    timer.setMeeting(QStringLiteral("2026-11-29"), QStringLiteral("weekend"), false,
                     QDateTime(QDate(2026, 11, 29), QTime(9, 30)));
    timer.startPart(0);
    timer.advanceForTest(6 * 60000LL + 5000);
    timer.stop();

    const QVariantMap titles{{QStringLiteral("opening-song"), QStringLiteral("Song, and prayer")},
                             {QStringLiteral("public-talk"), QStringLiteral("Public talk")}};
    const QList<QStringList> rows = timer.timingReportRows(titles);
    REQUIRE(rows.size() == 5);
    REQUIRE(rows.at(0) ==
            QStringList{"Song, and prayer", "09:30", "09:31", "05:00", "06:05", "+01:05"});
    REQUIRE(rows.at(1).at(0) == QStringLiteral("Public talk"));
    REQUIRE(rows.at(1).at(4).isEmpty());

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QStringList headers{"Part", "Planned start", "Started", "Planned", "Took", "Difference"};
    const QString csvPath = dir.filePath(QStringLiteral("report.csv"));
    REQUIRE(timer.saveTimingReport(QUrl::fromLocalFile(csvPath), titles, headers,
                                   QStringLiteral("Weekend meeting")));
    QFile csv(csvPath);
    REQUIRE(csv.open(QIODevice::ReadOnly));
    const QByteArray bytes = csv.readAll();
    REQUIRE(bytes.startsWith("\xEF\xBB\xBF"
                             "Part,Planned start,"));
    const QString text = QString::fromUtf8(bytes);
    REQUIRE(text.contains(QStringLiteral("\"Song, and prayer\",09:30,09:31,05:00,06:05,+01:05")));

    const QString pdfPath = dir.filePath(QStringLiteral("report.pdf"));
    REQUIRE(timer.saveTimingReport(QUrl::fromLocalFile(pdfPath), titles, headers,
                                   QStringLiteral("Weekend meeting")));
    QFile pdf(pdfPath);
    REQUIRE(pdf.open(QIODevice::ReadOnly));
    REQUIRE(pdf.read(4) == QByteArray("%PDF"));
}
