#include "TestZipFixtures.h"
#include "parsers/JwpubParser.h"
#include "parsers/S34Parser.h"
#include "parsers/ZipArchiveReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ZipArchiveReader validates relative entry paths and prevents Zip-Slip",
          "[parsers][zip][security]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    REQUIRE(kmedia::ZipArchiveReader::isSafeRelativeEntryName(QStringLiteral("media/video.mp4")));
    REQUIRE(kmedia::ZipArchiveReader::isSafeRelativeEntryName(QStringLiteral("manifest.json")));
    REQUIRE_FALSE(kmedia::ZipArchiveReader::isSafeRelativeEntryName(QString()));
    REQUIRE_FALSE(
        kmedia::ZipArchiveReader::isSafeRelativeEntryName(QStringLiteral("../outside.txt")));
    REQUIRE_FALSE(
        kmedia::ZipArchiveReader::isSafeRelativeEntryName(QStringLiteral("a/../../outside.txt")));
    REQUIRE_FALSE(
        kmedia::ZipArchiveReader::isSafeRelativeEntryName(QStringLiteral("a\\..\\outside.txt")));
    REQUIRE_FALSE(kmedia::ZipArchiveReader::isSafeRelativeEntryName(QStringLiteral("/etc/passwd")));
    REQUIRE_FALSE(
        kmedia::ZipArchiveReader::isSafeRelativeEntryName(QStringLiteral("C:/Windows/evil.dll")));

    const QString evilZipPath = tempDir.filePath(QStringLiteral("evil.jwpub"));
    REQUIRE(kmedia::test_helpers::writeZipFile(
        evilZipPath, {{QStringLiteral("manifest.json"), QByteArrayLiteral("{}")},
                      {QStringLiteral("../../pwned.txt"), QByteArrayLiteral("bad")}}));

    const kmedia::JwpubPackage parsed = kmedia::JwpubParser::parse(evilZipPath);
    REQUIRE_FALSE(parsed.valid);
    REQUIRE(parsed.errorMessage.contains(QStringLiteral("Unsafe zip entry path")));
    REQUIRE_FALSE(QFileInfo::exists(QDir(tempDir.path()).filePath(QStringLiteral("../pwned.txt"))));
}

TEST_CASE(
    "JwpubParser inspects manifest, SQLite metadata, markers, and selectively extracts assets",
    "[parsers][jwpub]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QByteArray dbBytes = kmedia::test_helpers::buildSqliteBytes(
        tempDir.path(),
        {QStringLiteral("CREATE TABLE Publication ("
                        "IssueTagNumber INTEGER, MepsLanguageIndex INTEGER, UndatedSymbol TEXT, "
                        "UniqueEnglishSymbol TEXT, Title TEXT, ShortTitle TEXT, Year INTEGER, "
                        "VersionNumber INTEGER)"),
         QStringLiteral("INSERT INTO Publication VALUES "
                        "(20261100, 0, 'mwb', 'mwb', 'Meeting Workbook - November 2026', "
                        "'mwb26.11', 2026, 2)"),
         QStringLiteral("CREATE TABLE Document ("
                        "DocumentId INTEGER PRIMARY KEY, MepsDocumentId INTEGER, Title TEXT, "
                        "FeatureTitle TEXT, Class INTEGER)"),
         QStringLiteral("INSERT INTO Document VALUES (10, 202026401, 'Treasures From God Word', "
                        "'Treasures', 68)"),
         QStringLiteral("CREATE TABLE Multimedia ("
                        "MultimediaId INTEGER PRIMARY KEY, DocumentId INTEGER, FilePath TEXT, "
                        "Label TEXT, Caption TEXT, MimeType TEXT, MajorType INTEGER, "
                        "MinorType INTEGER, CategoryType INTEGER, Track INTEGER, "
                        "KeySymbol TEXT, IssueTagNumber INTEGER, MepsLanguageIndex INTEGER)"),
         QStringLiteral("INSERT INTO Multimedia VALUES "
                        "(100, 10, 'media/lesson1.jpg', 'Illustration 1', 'Noah builds the ark', "
                        "'image/jpeg', 1, 1, 1, 0, 'mwb', 20261100, 0)"),
         QStringLiteral("INSERT INTO Multimedia VALUES "
                        "(101, 10, 'media/intro.mp4', 'Intro Video', '', "
                        "'video/mp4', 2, 1, 0, 1, 'mwb', 20261100, 0)"),
         QStringLiteral("CREATE TABLE DocumentMultimedia ("
                        "DocumentId INTEGER, MultimediaId INTEGER, BeginParagraphOrdinal INTEGER, "
                        "EndParagraphOrdinal INTEGER)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (10, 100, 2, 4)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (10, 101, 5, 5)"),
         QStringLiteral("CREATE TABLE VideoMarker ("
                        "VideoMarkerId INTEGER PRIMARY KEY, MultimediaId INTEGER, Label TEXT, "
                        "StartTimeTicks INTEGER, DurationTicks INTEGER, "
                        "EndTransitionDurationTicks INTEGER)"),
         QStringLiteral(
             "INSERT INTO VideoMarker VALUES (1, 101, 'Part 1', 10000000, 50000000, 0)")});
    REQUIRE_FALSE(dbBytes.isEmpty());

    const QByteArray manifestJson = QByteArrayLiteral(
        R"({"name":"mwb26.11","hash":"abc123","publication":{"symbol":"mwb","issueTagNumber":20261100,"language":0,"shortTitle":"MWB Nov 2026"}})");

    const QString jwpubPath = tempDir.filePath(QStringLiteral("mwb_E_202611.jwpub"));
    const QByteArray fakeJpg = QByteArrayLiteral("JPEG-BYTES-1");
    const QByteArray fakeMp4 = QByteArrayLiteral("MP4-BYTES-2");
    REQUIRE(kmedia::test_helpers::writeJwpubFixture(
        jwpubPath, tempDir.path(), manifestJson, dbBytes, QStringLiteral("mwb_E_202611.db"),
        {{QStringLiteral("media/lesson1.jpg"), fakeJpg},
         {QStringLiteral("media/intro.mp4"), fakeMp4}}));

    const kmedia::JwpubPackage pkg = kmedia::JwpubParser::parse(jwpubPath);
    REQUIRE(pkg.valid);
    REQUIRE(pkg.manifest.name == QStringLiteral("mwb26.11"));
    REQUIRE(pkg.publication.undatedSymbol == QStringLiteral("mwb"));
    REQUIRE(pkg.publication.issueTagNumber == 20261100);
    REQUIRE(pkg.publication.year == 2026);
    REQUIRE(pkg.publication.versionNumber == 2);
    REQUIRE(pkg.documents.size() == 1);
    REQUIRE(pkg.documents.at(0).mepsDocumentId == 202026401);
    REQUIRE(pkg.mediaItems.size() == 2);
    REQUIRE(pkg.mediaItems.at(0).filePath == QStringLiteral("media/lesson1.jpg"));
    REQUIRE(pkg.mediaItems.at(0).beginParagraphOrdinal == 2);
    REQUIRE(pkg.mediaItems.at(1).videoMarkers.size() == 1);
    REQUIRE(pkg.mediaItems.at(1).videoMarkers.at(0).label == QStringLiteral("Part 1"));

    // Extract ONLY one requested file and confirm the other stays unextracted.
    const QString outDir = tempDir.filePath(QStringLiteral("extracted"));
    std::vector<QString> extracted;
    QString err;
    REQUIRE(kmedia::JwpubParser::extractMediaAssets(
        jwpubPath, outDir, {QStringLiteral("media/lesson1.jpg")}, &extracted, &err));
    REQUIRE(extracted.size() == 1);
    REQUIRE(QFileInfo::exists(QDir(outDir).filePath(QStringLiteral("media/lesson1.jpg"))));
    REQUIRE_FALSE(QFileInfo::exists(QDir(outDir).filePath(QStringLiteral("media/intro.mp4"))));
}

TEST_CASE("S34Parser groups public talks cleanly and selectively extracts talk media",
          "[parsers][s34][jwpub]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    REQUIRE(kmedia::S34Parser::extractTalkNumber(
                QStringLiteral("No. 1 - How Well Do You Know God?")) == 1);
    REQUIRE(kmedia::S34Parser::extractTalkNumber(QStringLiteral("124. Always Remain Loyal")) ==
            124);
    REQUIRE(kmedia::S34Parser::extractTalkNumber(QStringLiteral("Special Outline")) == 0);

    const QByteArray dbBytes = kmedia::test_helpers::buildSqliteBytes(
        tempDir.path(),
        {QStringLiteral("CREATE TABLE Publication (Year INTEGER, VersionNumber INTEGER, "
                        "UndatedSymbol TEXT, IssueTagNumber INTEGER, MepsLanguageIndex INTEGER)"),
         QStringLiteral("INSERT INTO Publication VALUES (2026, 5, 'S-34mp', 0, 0)"),
         QStringLiteral("CREATE TABLE Document (DocumentId INTEGER PRIMARY KEY, MepsDocumentId "
                        "INTEGER, Title TEXT)"),
         QStringLiteral("INSERT INTO Document VALUES (1, 1001, '12. Respect for Authority')"),
         QStringLiteral("INSERT INTO Document VALUES (2, 1002, 'No. 3 - Serving With the Unified "
                        "Organization')"),
         QStringLiteral("INSERT INTO Document VALUES (3, 1003, '99. Talk Without Media')"),
         QStringLiteral("CREATE TABLE Multimedia (MultimediaId INTEGER PRIMARY KEY, FilePath TEXT, "
                        "Label TEXT, MimeType TEXT)"),
         QStringLiteral("INSERT INTO Multimedia VALUES (10, 'media/talk12.jpg', 'Authority Slide', "
                        "'image/jpeg')"),
         QStringLiteral(
             "INSERT INTO Multimedia VALUES (20, 'media/talk3.jpg', 'Unity Slide', 'image/jpeg')"),
         QStringLiteral(
             "CREATE TABLE DocumentMultimedia (DocumentId INTEGER, MultimediaId INTEGER)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (1, 10)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (2, 20)")});
    REQUIRE_FALSE(dbBytes.isEmpty());

    const QString s34Path = tempDir.filePath(QStringLiteral("S-34mp_E.jwpub"));
    REQUIRE(kmedia::test_helpers::writeJwpubFixture(
        s34Path, tempDir.path(), QByteArrayLiteral(R"({"name":"S-34mp"})"), dbBytes,
        QStringLiteral("S-34mp_E.db"),
        {{QStringLiteral("media/talk12.jpg"), QByteArrayLiteral("T12")},
         {QStringLiteral("media/talk3.jpg"), QByteArrayLiteral("T3")}}));

    const kmedia::S34Package filtered = kmedia::S34Parser::parse(s34Path, false);
    REQUIRE(filtered.valid);
    REQUIRE(filtered.publication.year == 2026);
    REQUIRE(filtered.publication.versionNumber == 5);
    REQUIRE(filtered.talks.size() == 2);
    REQUIRE(filtered.talks.at(0).talkNumber == 3);
    REQUIRE(filtered.talks.at(1).talkNumber == 12);

    const kmedia::S34Package allTalks = kmedia::S34Parser::parse(s34Path, true);
    REQUIRE(allTalks.talks.size() == 3);

    const QString outDir = tempDir.filePath(QStringLiteral("s34-talk3"));
    std::vector<QString> extracted;
    REQUIRE(kmedia::S34Parser::extractTalkMedia(filtered, filtered.talks.at(0).documentId, outDir,
                                                &extracted));
    REQUIRE(extracted.size() == 1);
    REQUIRE(QFileInfo::exists(QDir(outDir).filePath(QStringLiteral("media/talk3.jpg"))));
    REQUIRE_FALSE(QFileInfo::exists(QDir(outDir).filePath(QStringLiteral("media/talk12.jpg"))));
}
