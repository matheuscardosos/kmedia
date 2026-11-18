#include "TestZipFixtures.h"
#include "parsers/JwlPlaylistParser.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("JwlPlaylistParser converts JW 100ns ticks into trim milliseconds accurately",
          "[parsers][jwlplaylist]")
{
    REQUIRE(kmedia::JwlPlaylistParser::ticksToMs(0) == 0);
    REQUIRE(kmedia::JwlPlaylistParser::ticksToMs(25'000'000) == 2500);

    // 60s total duration, start at 5s, trim 10s off the end -> [5000 ms, 50000 ms].
    const auto [startMs, endMs] = kmedia::JwlPlaylistParser::computeTrimRangeMs(
        600'000'000LL, 0, 50'000'000LL, 100'000'000LL);
    REQUIRE(startMs == 5000);
    REQUIRE(endMs == 50000);

    // Fallback to itemDurationTicks when BaseDurationTicks is missing/zero.
    const auto [start2, end2] =
        kmedia::JwlPlaylistParser::computeTrimRangeMs(0, 300'000'000LL, 20'000'000LL, 50'000'000LL);
    REQUIRE(start2 == 2000);
    REQUIRE(end2 == 25000);
}

TEST_CASE("JwlPlaylistParser extracts ordered playlist entries, metadata, and rejects Zip-Slip",
          "[parsers][jwlplaylist][security]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QByteArray dbBytes = kmedia::test_helpers::buildSqliteBytes(
        tempDir.path(),
        {QStringLiteral("CREATE TABLE Tag (TagId INTEGER PRIMARY KEY, Name TEXT)"),
         QStringLiteral("INSERT INTO Tag VALUES (1, 'Weekend Meeting')"),
         QStringLiteral("CREATE TABLE PlaylistItem ("
                        "PlaylistItemId INTEGER PRIMARY KEY, Label TEXT, "
                        "StartTrimOffsetTicks INTEGER, EndTrimOffsetTicks INTEGER, "
                        "Accuracy INTEGER, EndAction INTEGER, ThumbnailFilePath TEXT)"),
         QStringLiteral("INSERT INTO PlaylistItem VALUES "
                        "(1, 'Closing Photo', 0, 0, 1, 3, 'thumbs/hash1')"),
         QStringLiteral("INSERT INTO PlaylistItem VALUES "
                        "(2, 'Watchtower Video', 30000000, 20000000, 1, 0, 'thumbs/hash2')"),
         QStringLiteral("CREATE TABLE IndependentMedia ("
                        "IndependentMediaId INTEGER PRIMARY KEY, OriginalFilename TEXT, "
                        "FilePath TEXT, MimeType TEXT, Hash TEXT)"),
         QStringLiteral("INSERT INTO IndependentMedia VALUES "
                        "(10, 'closing.jpg', 'media/closing.jpg', 'image/jpeg', 'sha-a')"),
         QStringLiteral("INSERT INTO IndependentMedia VALUES "
                        "(20, 'intro.mp4', 'media/intro.mp4', 'video/mp4', 'sha-b')"),
         QStringLiteral(
             "CREATE TABLE PlaylistItemIndependentMediaMap ("
             "PlaylistItemId INTEGER, IndependentMediaId INTEGER, DurationTicks INTEGER)"),
         QStringLiteral("INSERT INTO PlaylistItemIndependentMediaMap VALUES (1, 10, 0)"),
         QStringLiteral("INSERT INTO PlaylistItemIndependentMediaMap VALUES (2, 20, 150000000)"),
         QStringLiteral(
             "CREATE TABLE Location ("
             "LocationId INTEGER PRIMARY KEY, BookNumber INTEGER, ChapterNumber INTEGER, "
             "DocumentId INTEGER, Track INTEGER, IssueTagNumber INTEGER, "
             "KeySymbol TEXT, MepsLanguage INTEGER, Type INTEGER, Title TEXT)"),
         QStringLiteral(
             "INSERT INTO Location VALUES (5, 0, 0, 505, 2, 20261100, 'w', 0, 1, 'Study Article')"),
         QStringLiteral("CREATE TABLE PlaylistItemLocationMap ("
                        "PlaylistItemId INTEGER, LocationId INTEGER, BaseDurationTicks INTEGER)"),
         QStringLiteral("INSERT INTO PlaylistItemLocationMap VALUES (2, 5, 200000000)"),
         QStringLiteral("CREATE TABLE TagMap (TagMapId INTEGER PRIMARY KEY, TagId INTEGER, "
                        "PlaylistItemId INTEGER, Position INTEGER)"),
         QStringLiteral("INSERT INTO TagMap VALUES (1, 1, 2, 0)"),
         QStringLiteral("INSERT INTO TagMap VALUES (2, 1, 1, 1)"),
         QStringLiteral("CREATE TABLE PlaylistItemMarker (PlaylistItemId INTEGER, Label TEXT)"),
         QStringLiteral("INSERT INTO PlaylistItemMarker VALUES (2, 'John 3:16')")});
    REQUIRE_FALSE(dbBytes.isEmpty());

    const QByteArray manifestJson = QByteArrayLiteral(
        R"({"name":"Convention Replay","userDataBackup":{"databaseName":"userData.db","hash":"h1"}})");

    const QString jwlPath = tempDir.filePath(QStringLiteral("weekend.jwlplaylist"));
    REQUIRE(kmedia::test_helpers::writeZipFile(
        jwlPath, {{QStringLiteral("manifest.json"), manifestJson},
                  {QStringLiteral("userData.db"), dbBytes},
                  {QStringLiteral("media/closing.jpg"), QByteArrayLiteral("IMG")},
                  {QStringLiteral("media/intro.mp4"), QByteArrayLiteral("VID")},
                  {QStringLiteral("thumbs/hash1"), QByteArrayLiteral("TH1")},
                  {QStringLiteral("thumbs/hash2"), QByteArrayLiteral("TH2")}}));

    const kmedia::JwlPlaylistPackage pkg = kmedia::JwlPlaylistParser::parse(jwlPath);
    REQUIRE(pkg.valid);
    REQUIRE(pkg.playlistName == QStringLiteral("Convention Replay"));
    REQUIRE(pkg.entries.size() == 2);

    // Position 0 is PlaylistItemId=2 ("Watchtower Video") per TagMap.
    const auto& first = pkg.entries.at(0);
    REQUIRE(first.playlistItemId == 2);
    REQUIRE(first.position == 0);
    REQUIRE(first.label == QStringLiteral("Watchtower Video"));
    REQUIRE(first.mediaArchivePath == QStringLiteral("media/intro.mp4"));
    REQUIRE(first.mimeType == QStringLiteral("video/mp4"));
    REQUIRE(first.durationMs == 20000);
    REQUIRE(first.trimStartMs == 3000);
    REQUIRE(first.trimEndMs == 18000);
    REQUIRE(first.keySymbol == QStringLiteral("w"));
    REQUIRE(first.markerLabels == QStringList{QStringLiteral("John 3:16")});

    // Position 1 is PlaylistItemId=1 ("Closing Photo") with repeat loop EndAction==3.
    const auto& second = pkg.entries.at(1);
    REQUIRE(second.playlistItemId == 1);
    REQUIRE(second.repeatLoop);

    const QString extractDir = tempDir.filePath(QStringLiteral("assets"));
    std::vector<QString> extracted;
    REQUIRE(kmedia::JwlPlaylistParser::extractAssets(jwlPath, extractDir, {}, &extracted));
    REQUIRE(extracted.size() == 4);
    REQUIRE(QFileInfo::exists(QDir(extractDir).filePath(QStringLiteral("media/intro.mp4"))));
    REQUIRE_FALSE(QFileInfo::exists(QDir(extractDir).filePath(QStringLiteral("userData.db"))));

    // Malicious Zip-Slip entry must fail cleanly before touching disk.
    const QString maliciousPath = tempDir.filePath(QStringLiteral("bad.jwlplaylist"));
    REQUIRE(kmedia::test_helpers::writeZipFile(
        maliciousPath, {{QStringLiteral("manifest.json"), manifestJson},
                        {QStringLiteral("userData.db"), dbBytes},
                        {QStringLiteral("../evil.sh"), QByteArrayLiteral("echo nope")}}));
    const kmedia::JwlPlaylistPackage rejected = kmedia::JwlPlaylistParser::parse(maliciousPath);
    REQUIRE_FALSE(rejected.valid);
    REQUIRE(rejected.errorMessage.contains(QStringLiteral("Unsafe zip entry path")));
}
