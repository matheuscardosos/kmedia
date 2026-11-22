#include "core/DatabaseManager.h"
#include "core/NetworkManager.h"
#include "media/BackgroundMusicEngine.h"
#include "media/BackgroundMusicPlanner.h"
#include "media/BackgroundMusicService.h"
#include "network/BackgroundMusicLibrary.h"
#include "network/CatalogClient.h"
#include "network/DownloadService.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

kmedia::BackgroundSong song(int track, double seconds)
{
    kmedia::BackgroundSong result;
    result.track = track;
    result.title = QStringLiteral("Song %1").arg(track);
    result.durationSeconds = seconds;
    return result;
}

QList<int> tracksOf(const std::vector<kmedia::BackgroundSong>& songs)
{
    QList<int> tracks;
    for (const auto& item : songs) {
        tracks.append(item.track);
    }
    return tracks;
}

} // namespace

TEST_CASE("Background music shuffle keeps every song and favors newer ones", "[music]")
{
    std::vector<kmedia::BackgroundSong> library;
    for (int track = 1; track <= 100; ++track) {
        library.push_back(song(track, 180.0));
    }

    QRandomGenerator random(42);
    const auto once = kmedia::BackgroundMusicPlanner::weightedShuffle(library, random);
    REQUIRE(once.size() == library.size());
    QList<int> sorted = tracksOf(once);
    std::sort(sorted.begin(), sorted.end());
    REQUIRE(sorted == tracksOf(library));

    // Over many shuffles the newest song opens the list far more often than the oldest.
    int newestFirst = 0;
    int oldestFirst = 0;
    for (int run = 0; run < 2000; ++run) {
        const auto shuffled = kmedia::BackgroundMusicPlanner::weightedShuffle(library, random);
        const QList<int> firstTen = tracksOf(shuffled).mid(0, 10);
        newestFirst += firstTen.contains(100) ? 1 : 0;
        oldestFirst += firstTen.contains(1) ? 1 : 0;
    }
    REQUIRE(newestFirst > oldestFirst * 3);
}

TEST_CASE("Background music before a meeting ends on time with the meeting songs last", "[music]")
{
    const std::vector<kmedia::BackgroundSong> library{song(1, 100.0), song(2, 100.0),
                                                      song(3, 100.0), song(4, 100.0)};

    // Songs 3 and 2 open and close the meeting; 250 seconds are left before the music stops.
    const auto queue = kmedia::BackgroundMusicPlanner::meetingQueue(library, {3, 2}, 250.0);
    REQUIRE(tracksOf(queue.songs) == QList<int>{1, 3, 2});
    REQUIRE(queue.startOffsetSeconds == Catch::Approx(50.0));
    REQUIRE_FALSE(queue.songs.at(0).meetingSong);
    REQUIRE(queue.songs.at(1).meetingSong);
    REQUIRE(queue.songs.at(2).meetingSong);

    // Close to the start only the end of the last meeting song fits.
    const auto late = kmedia::BackgroundMusicPlanner::meetingQueue(library, {3, 2}, 30.0);
    REQUIRE(tracksOf(late.songs) == QList<int>{2});
    REQUIRE(late.startOffsetSeconds == Catch::Approx(70.0));
}

TEST_CASE("Background music before a meeting cycles a short library and skips untimed songs",
          "[music]")
{
    std::vector<kmedia::BackgroundSong> library{song(1, 60.0), song(2, 0.0)};

    const auto queue = kmedia::BackgroundMusicPlanner::meetingQueue(library, {}, 200.0);
    REQUIRE(tracksOf(queue.songs) == QList<int>{1, 1, 1, 1});
    REQUIRE(queue.startOffsetSeconds == Catch::Approx(40.0));

    REQUIRE(kmedia::BackgroundMusicPlanner::meetingQueue(library, {}, 0.0).songs.empty());
    REQUIRE(kmedia::BackgroundMusicPlanner::meetingQueue({}, {1}, 300.0).songs.empty());
    // A meeting song without a duration cannot be timed either.
    REQUIRE(tracksOf(kmedia::BackgroundMusicPlanner::meetingQueue(library, {2}, 30.0).songs) ==
            QList<int>{1});
}

TEST_CASE("Songbook library keeps one file per song and plays the cached copy when complete",
          "[music]")
{
    std::vector<kmedia::CatalogMediaAsset> assets;
    const auto asset = [](int track, qint64 size, const QString& url) {
        kmedia::CatalogMediaAsset item;
        item.track = track;
        item.fileSize = size;
        item.url = url;
        item.title = QStringLiteral("Song %1").arg(track);
        item.durationSeconds = 120.0;
        return item;
    };
    assets.push_back(asset(2, 900, QStringLiteral("https://cdn.example/sjj_E_002_r720P.mp4")));
    assets.push_back(asset(2, 300, QStringLiteral("https://cdn.example/sjj_E_002_r240P.mp4")));
    assets.push_back(asset(1, 500, QStringLiteral("https://cdn.example/sjj_E_001_r240P.mp4")));
    assets.push_back(asset(0, 100, QStringLiteral("https://cdn.example/sjj_E_000.mp4")));

    const auto songs = kmedia::BackgroundMusicLibrary::songsFromAssets(assets);
    REQUIRE(tracksOf(songs) == QList<int>{1, 2});
    REQUIRE(songs.at(1).remoteUrl == QStringLiteral("https://cdn.example/sjj_E_002_r240P.mp4"));
    REQUIRE(kmedia::BackgroundMusicLibrary::songbookSymbol(true) == QStringLiteral("sjj"));
    REQUIRE(kmedia::BackgroundMusicLibrary::songbookFormat(false) == QStringLiteral("MP3"));

    const QTemporaryDir cache;
    REQUIRE(cache.isValid());
    kmedia::BackgroundMusicLibrary library(nullptr, nullptr);
    library.setCacheRoot(cache.path());
    library.setLanguage(QStringLiteral("e"), true);
    const kmedia::BackgroundSong& first = songs.at(0);
    REQUIRE(library.localPath(first) ==
            QDir(cache.path()).filePath(QStringLiteral("Music/sjj_E/sjj_E_001_r240P.mp4")));
    REQUIRE(library.playbackUrl(first) == QUrl(first.remoteUrl));

    // A partial file still streams; the complete one plays from disk.
    REQUIRE(QDir().mkpath(QFileInfo(library.localPath(first)).absolutePath()));
    QFile partial(library.localPath(first));
    REQUIRE(partial.open(QIODevice::WriteOnly));
    partial.write(QByteArray(200, 'x'));
    partial.close();
    REQUIRE_FALSE(library.playbackUrl(first).isLocalFile());
    REQUIRE(partial.open(QIODevice::Append));
    partial.write(QByteArray(300, 'x'));
    partial.close();
    REQUIRE(library.playbackUrl(first) == QUrl::fromLocalFile(library.localPath(first)));

    // Without a catalog the songbook cannot be fetched, and the answer says so.
    QObject context;
    bool answered = false;
    library.requestSongs(&context, [&answered](const std::vector<kmedia::BackgroundSong>& found) {
        answered = found.empty();
    });
    REQUIRE(answered);
}

TEST_CASE("Background music starts by itself in the window before a meeting", "[music]")
{
    kmedia::BackgroundMusicEngine engine;
    kmedia::BackgroundMusicLibrary library(nullptr, nullptr);
    kmedia::BackgroundMusicService service(&engine, &library);

    QDateTime now(QDate(2026, 11, 25), QTime(18, 30));
    QDateTime meetingStart(QDate(2026, 11, 25), QTime(19, 30));
    bool mediaPlaying = false;
    service.setClock([&now]() { return now; });
    service.setMeetingProvider([&meetingStart](const QDate& date)
                                   -> std::optional<kmedia::BackgroundMusicService::Meeting> {
        if (date != meetingStart.date()) {
            return std::nullopt;
        }
        return kmedia::BackgroundMusicService::Meeting{meetingStart, {76, 151}};
    });

    REQUIRE_FALSE(service.shouldAutoStart());
    QSignalSpy failures(&service, &kmedia::BackgroundMusicService::startFailed);
    QSignalSpy states(&service, &kmedia::BackgroundMusicService::stateChanged);
    service.setAutoStart(true);
    // An hour before the meeting it tries to start; the songbook cannot be fetched here, and
    // an automatic start that fails stays quiet and waits five minutes before trying again.
    REQUIRE(states.count() == 2);
    REQUIRE(failures.isEmpty());
    REQUIRE(service.shouldAutoStart());
    REQUIRE_FALSE(service.isLoading());
    service.checkAutoStart();
    REQUIRE(states.count() == 2);
    now = now.addSecs(kmedia::BackgroundMusicService::kAutoStartRetrySeconds);
    service.checkAutoStart();
    REQUIRE(states.count() == 4);

    now = QDateTime(QDate(2026, 11, 25), QTime(18, 0));
    REQUIRE_FALSE(service.shouldAutoStart()); // more than 75 minutes before
    now = QDateTime(QDate(2026, 11, 25), QTime(19, 29));
    REQUIRE_FALSE(service.shouldAutoStart()); // too close to the start
    now = QDateTime(QDate(2026, 11, 25), QTime(19, 45));
    REQUIRE_FALSE(service.shouldAutoStart()); // the meeting already began
    now = QDateTime(QDate(2026, 11, 26), QTime(18, 45));
    REQUIRE_FALSE(service.shouldAutoStart()); // no meeting that day

    now = QDateTime(QDate(2026, 11, 25), QTime(19, 0));
    service.setMediaPlaying(true);
    REQUIRE_FALSE(service.shouldAutoStart());
    service.setMediaPlaying(mediaPlaying);
    REQUIRE(service.shouldAutoStart());

    // Stopped by hand, it waits for a hand start before this meeting.
    service.stop();
    REQUIRE_FALSE(service.shouldAutoStart());
    service.start();
    REQUIRE(failures.count() == 1); // a hand start says so when it fails
    REQUIRE(service.shouldAutoStart());

    service.setAutoStart(false);
    REQUIRE_FALSE(service.shouldAutoStart());
}

TEST_CASE("Background music starts a timed queue late and stops after its last song",
          "[music][media]")
{
    const QTemporaryDir root;
    REQUIRE(root.isValid());

    // 32 seconds of silence as 8 kHz mono 8-bit PCM.
    constexpr quint32 kRate = 8000;
    constexpr quint32 kSamples = kRate * 32;
    QByteArray wav;
    const auto put32 = [&wav](quint32 value) {
        for (int shift = 0; shift < 32; shift += 8) {
            wav.append(static_cast<char>((value >> shift) & 0xFFu));
        }
    };
    const auto put16 = [&wav](quint16 value) {
        wav.append(static_cast<char>(value & 0xFFu));
        wav.append(static_cast<char>((value >> 8) & 0xFFu));
    };
    wav.append("RIFF");
    put32(36 + kSamples);
    wav.append("WAVEfmt ");
    put32(16);
    put16(1);
    put16(1);
    put32(kRate);
    put32(kRate);
    put16(1);
    put16(8);
    wav.append("data");
    put32(kSamples);
    wav.append(QByteArray(static_cast<qsizetype>(kSamples), static_cast<char>(0x80)));
    const QString path = root.filePath(QStringLiteral("silence.wav"));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(wav);
    file.close();

    kmedia::BackgroundMusicEngine engine;
    REQUIRE(engine.initialize(true));
    QSignalSpy started(&engine, &kmedia::BackgroundMusicEngine::trackStarted);
    QSignalSpy finished(&engine, &kmedia::BackgroundMusicEngine::queueFinished);
    engine.setQueue({QUrl::fromLocalFile(path)}, {QStringLiteral("Silence")}, 30.0, false);
    REQUIRE(engine.currentTrackTitle() == QStringLiteral("Silence"));
    REQUIRE_FALSE(engine.isLooping());

    QElapsedTimer clock;
    clock.start();
    engine.play();
    REQUIRE(started.count() == 1);
    // Starting 30 seconds in, the 32 second song ends in about two seconds, and the queue stops.
    REQUIRE(finished.wait(15 * 1000));
    REQUIRE(clock.elapsed() < 15 * 1000);
    REQUIRE_FALSE(engine.isPlaying());
}

TEST_CASE("Songbook library loads the songbook from JW.org", "[.network][music]")
{
    const QTemporaryDir root;
    REQUIRE(root.isValid());
    const QString language = qEnvironmentVariable("KMEDIA_NETWORK_LANG", QStringLiteral("E"));

    kmedia::DatabaseManager db;
    REQUIRE(db.open(root.filePath(QStringLiteral("profile.db"))));
    kmedia::NetworkManager network;
    kmedia::CatalogClient catalog(&network, &db);
    kmedia::DownloadService downloads(&network, &db);
    kmedia::BackgroundMusicLibrary library(&catalog, &downloads);
    library.setCacheRoot(root.path());
    library.setLanguage(language, false);

    QObject context;
    bool answered = false;
    std::vector<kmedia::BackgroundSong> songs;
    library.requestSongs(&context, [&](const std::vector<kmedia::BackgroundSong>& found) {
        songs = found;
        answered = true;
    });
    REQUIRE(QTest::qWaitFor([&answered]() { return answered; }, 60 * 1000));
    REQUIRE(songs.size() > 100);
    REQUIRE(songs.front().track == 1);
    REQUIRE(std::all_of(songs.begin(), songs.end(), [](const kmedia::BackgroundSong& song) {
        return song.durationSeconds > 0.0 && song.remoteUrl.endsWith(QStringLiteral(".mp3"));
    }));

    // A song plays from JW.org until its download lands in the cache.
    const kmedia::BackgroundSong& first = songs.front();
    REQUIRE_FALSE(library.playbackUrl(first).isLocalFile());
    QSignalSpy finished(&downloads, &kmedia::DownloadService::taskFinished);
    library.cacheSong(first);
    REQUIRE(finished.wait(2 * 60 * 1000));
    REQUIRE(finished.first().at(1).toBool());
    REQUIRE(library.playbackUrl(first).isLocalFile());
}
