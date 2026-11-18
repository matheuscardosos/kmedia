#include "media/BackgroundMusicEngine.h"

#include <QSignalSpy>
#include <QTimeZone>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BackgroundMusicEngine computes next scheduled fade triggers accurately",
          "[background-music][media]")
{
    kmedia::MeetingScheduleSpec midweek;
    midweek.enabled = true;
    midweek.dayOfWeek = Qt::Wednesday;
    midweek.startTime = QTime(19, 30, 0);

    kmedia::MeetingScheduleSpec weekend;
    weekend.enabled = true;
    weekend.dayOfWeek = Qt::Sunday;
    weekend.startTime = QTime(9, 30, 0);

    // 2026-11-18 is a Wednesday at 18:00:00
    const QDateTime wedEvening(QDate(2026, 11, 18), QTime(18, 0, 0), QTimeZone::utc());
    const QDateTime trigger =
        kmedia::BackgroundMusicEngine::computeNextFadeTrigger(wedEvening, midweek, weekend, 5000);
    REQUIRE(trigger.isValid());
    REQUIRE(trigger == QDateTime(QDate(2026, 11, 18), QTime(19, 29, 55), QTimeZone::utc()));

    // At 19:29:58 (after today's fade point already passed), the next trigger must be Sunday
    const QDateTime pastTrigger(QDate(2026, 11, 18), QTime(19, 29, 58), QTimeZone::utc());
    const QDateTime nextTrigger =
        kmedia::BackgroundMusicEngine::computeNextFadeTrigger(pastTrigger, midweek, weekend, 5000);
    REQUIRE(nextTrigger.isValid());
    REQUIRE(nextTrigger == QDateTime(QDate(2026, 11, 22), QTime(9, 29, 55), QTimeZone::utc()));

    midweek.enabled = false;
    weekend.enabled = false;
    REQUIRE_FALSE(
        kmedia::BackgroundMusicEngine::computeNextFadeTrigger(wedEvening, midweek, weekend, 5000)
            .isValid());
}

TEST_CASE("BackgroundMusicEngine manages queues and performs headless fade-out",
          "[background-music][mpv][media]")
{
    kmedia::BackgroundMusicEngine engine;
    REQUIRE(engine.initialize(true));
    REQUIRE(engine.isInitialized());

    engine.setTargetVolume(80);
    REQUIRE(engine.targetVolume() == 80);

    const QUrl track1 = QUrl::fromLocalFile(QStringLiteral("/tmp/kingdom-song-1.mp3"));
    const QUrl track2 = QUrl::fromLocalFile(QStringLiteral("/tmp/kingdom-song-2.mp3"));
    engine.setPlaylist({track1, track2});
    REQUIRE(engine.trackCount() == 2);
    REQUIRE(engine.currentIndex() == 0);
    REQUIRE(engine.currentTrackTitle() == QStringLiteral("kingdom-song-1"));

    engine.next();
    REQUIRE(engine.currentIndex() == 1);
    engine.previous();
    REQUIRE(engine.currentIndex() == 0);

    QSignalSpy fadeDoneSpy(&engine, &kmedia::BackgroundMusicEngine::fadeOutCompleted);
    engine.fadeOutAndStop(200);
    REQUIRE(fadeDoneSpy.count() == 1);
    REQUIRE_FALSE(engine.isPlaying());
    REQUIRE(engine.currentVolume() == 80);
}
