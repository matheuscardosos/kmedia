#include "core/MeetingTimer.h"

#include <QSignalSpy>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("MeetingTimer tracks elapsed time, remaining schedule, and overtime formatting",
          "[core][timer]")
{
    kmedia::MeetingTimer timer;
    REQUIRE_FALSE(timer.isRunning());
    REQUIRE_FALSE(timer.isOvertime());
    REQUIRE(timer.plannedMinutes() == 105);
    REQUIRE(timer.elapsedMs() == 0);
    REQUIRE(timer.formattedElapsed() == QStringLiteral("00:00"));
    REQUIRE(timer.formattedRemaining() == QStringLiteral("1:45:00"));

    QSignalSpy runSpy(&timer, &kmedia::MeetingTimer::runningChanged);
    QSignalSpy timeSpy(&timer, &kmedia::MeetingTimer::timeChanged);

    timer.setPlannedMinutes(0);
    REQUIRE(timer.plannedMinutes() == 1);
    timer.setPlannedMinutes(600);
    REQUIRE(timer.plannedMinutes() == 360);
    timer.setPlannedMinutes(60);
    REQUIRE(timer.plannedMinutes() == 60);

    timer.advanceForTest(15 * 60 * 1000LL);
    REQUIRE_FALSE(timer.isOvertime());
    REQUIRE(timer.formattedElapsed() == QStringLiteral("15:00"));
    REQUIRE(timer.formattedRemaining() == QStringLiteral("45:00"));

    timer.advanceForTest(50 * 60 * 1000LL);
    REQUIRE(timer.isOvertime());
    REQUIRE(timer.formattedElapsed() == QStringLiteral("1:05:00"));
    REQUIRE(timer.formattedRemaining() == QStringLiteral("+05:00"));

    timer.toggle();
    REQUIRE(timer.isRunning());
    REQUIRE(runSpy.count() == 1);

    timer.toggle();
    REQUIRE_FALSE(timer.isRunning());
    REQUIRE(runSpy.count() == 2);

    timer.adjustPlannedMinutes(15);
    REQUIRE(timer.plannedMinutes() == 75);
    REQUIRE_FALSE(timer.isOvertime());

    timer.reset();
    REQUIRE_FALSE(timer.isRunning());
    REQUIRE(timer.elapsedMs() == 0);
    REQUIRE(timeSpy.count() >= 4);
}
