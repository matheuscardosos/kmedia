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

TEST_CASE(
    "JwpubParser matches DatedText weeks, Class 40 Watchtower articles, extracts, and filters",
    "[parsers][jwpub][week]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QByteArray mwbDb = kmedia::test_helpers::buildSqliteBytes(
        tempDir.path(),
        {QStringLiteral("CREATE TABLE Publication (IssueTagNumber INTEGER, MepsLanguageIndex "
                        "INTEGER, UndatedSymbol TEXT, UniqueEnglishSymbol TEXT, Title TEXT, "
                        "ShortTitle TEXT, Year INTEGER, VersionNumber INTEGER)"),
         QStringLiteral("INSERT INTO Publication VALUES (20261100, 0, 'mwb', 'mwb', 'MWB Nov "
                        "2026', 'mwb26.11', 2026, 2)"),
         QStringLiteral("CREATE TABLE Document (DocumentId INTEGER PRIMARY KEY, MepsDocumentId "
                        "INTEGER, Title TEXT, Subtitle TEXT, FeatureTitle TEXT, Class INTEGER)"),
         QStringLiteral("INSERT INTO Document VALUES (1, 1001, 'Nov 16-22', '', '', 68)"),
         QStringLiteral("INSERT INTO Document VALUES (2, 1002, 'Nov 23-29', '', '', 68)"),
         QStringLiteral("CREATE TABLE DatedText (DocumentId INTEGER, FirstDateOffset INTEGER, "
                        "LastDateOffset INTEGER)"),
         QStringLiteral("INSERT INTO DatedText VALUES (1, 20261116, 20261122)"),
         QStringLiteral("INSERT INTO DatedText VALUES (2, 20261123, 20261129)"),
         QStringLiteral("CREATE TABLE DocumentParagraph (DocumentId INTEGER, ParagraphIndex "
                        "INTEGER)"),
         QStringLiteral("INSERT INTO DocumentParagraph VALUES (2, 10)"),
         QStringLiteral("CREATE TABLE Question (DocumentId INTEGER, BeginParagraphOrdinal "
                        "INTEGER, TargetParagraphNumberLabel TEXT)"),
         QStringLiteral("INSERT INTO Question VALUES (2, 4, '4-5')"),
         QStringLiteral("CREATE TABLE RefPublication (RefPublicationId INTEGER PRIMARY KEY, "
                        "UndatedSymbol TEXT, IssueTagNumber INTEGER, MepsLanguageIndex INTEGER)"),
         QStringLiteral("INSERT INTO RefPublication VALUES (1, 'sjj', 0, 0)"),
         QStringLiteral("INSERT INTO RefPublication VALUES (2, 'th', 0, 0)"),
         QStringLiteral("INSERT INTO RefPublication VALUES (3, 'lmd', 0, 0)"),
         QStringLiteral("INSERT INTO RefPublication VALUES (4, 'mwbr26', 20261100, 0)"),
         QStringLiteral("CREATE TABLE Extract (ExtractId INTEGER PRIMARY KEY, RefPublicationId "
                        "INTEGER, RefMepsDocumentId INTEGER, Caption TEXT)"),
         QStringLiteral("INSERT INTO Extract VALUES (10, 1, 126, 'Song 126')"),
         QStringLiteral("INSERT INTO Extract VALUES (11, 2, 50, 'Study Point 3')"),
         QStringLiteral("INSERT INTO Extract VALUES (12, 3, 60, 'Make Disciples Lesson 2')"),
         QStringLiteral("INSERT INTO Extract VALUES (13, 4, 70, 'Ignored Workbook Ref')"),
         QStringLiteral("CREATE TABLE DocumentExtract (DocumentId INTEGER, ExtractId INTEGER, "
                        "BeginParagraphOrdinal INTEGER, EndParagraphOrdinal INTEGER)"),
         QStringLiteral("INSERT INTO DocumentExtract VALUES (2, 10, 2, 2)"),
         QStringLiteral("INSERT INTO DocumentExtract VALUES (2, 11, 3, 3)"),
         QStringLiteral("INSERT INTO DocumentExtract VALUES (2, 12, 8, 9)"),
         QStringLiteral("INSERT INTO DocumentExtract VALUES (2, 13, 1, 1)"),
         QStringLiteral("CREATE TABLE Multimedia (MultimediaId INTEGER PRIMARY KEY, FilePath TEXT, "
                        "Label TEXT, Caption TEXT, MimeType TEXT, MajorType INTEGER, MinorType "
                        "INTEGER, CategoryType INTEGER, Track INTEGER, KeySymbol TEXT, "
                        "IssueTagNumber INTEGER, MepsLanguageIndex INTEGER, LinkMultimediaId "
                        "INTEGER, SuppressZoom INTEGER)"),
         QStringLiteral("INSERT INTO Multimedia VALUES (10, 'media/oldweek.jpg', 'Old', '', "
                        "'image/jpeg', 1, 1, 4, 0, 'mwb', 20261100, 0, 0, 0)"),
         QStringLiteral("INSERT INTO Multimedia VALUES (20, 'media/printed.jpg', 'Printed Only', "
                        "'', 'image/jpeg', 1, 1, 1, 0, 'mwb', 20261100, 0, 0, 0)"),
         QStringLiteral("INSERT INTO Multimedia VALUES (21, 'media/footnote.jpg', 'Footnote', "
                        "'', 'image/jpeg', 1, 1, 4, 0, 'mwb', 20261100, 0, 0, 0)"),
         QStringLiteral("INSERT INTO Multimedia VALUES (22, 'media/suppressed.jpg', 'Suppressed', "
                        "'', 'image/jpeg', 1, 1, 4, 0, 'mwb', 20261100, 0, 0, 1)"),
         QStringLiteral("INSERT INTO Multimedia VALUES (23, 'media/songcrop.jpg', 'Song Thumb', "
                        "'', 'image/jpeg', 1, 1, 4, 0, 'mwb', 20261100, 0, 24, 0)"),
         QStringLiteral("INSERT INTO Multimedia VALUES (24, 'media/cbs.mp4', 'CBS Clip', '', "
                        "'video/mp4', 2, 1, 9, 5, 'lmd', 0, 0, 0, 0)"),
         QStringLiteral("INSERT INTO Multimedia VALUES (25, 'media/treasures.jpg', 'Treasures', "
                        "'', 'image/jpeg', 1, 1, 4, 0, 'mwb', 20261100, 0, 0, 0)"),
         QStringLiteral(
             "CREATE TABLE DocumentMultimedia (DocumentId INTEGER, MultimediaId INTEGER, "
             "BeginParagraphOrdinal INTEGER, EndParagraphOrdinal INTEGER)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (1, 10, 2, 2)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (2, 20, 2, 2)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (2, 21, 1500, 1500)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (2, 22, 3, 3)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (2, 23, 2, 2)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (2, 24, 8, 9)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (2, 25, 4, 4)")});
    REQUIRE_FALSE(mwbDb.isEmpty());

    const QString mwbPath = tempDir.filePath(QStringLiteral("mwb_E_202611.jwpub"));
    REQUIRE(kmedia::test_helpers::writeJwpubFixture(
        mwbPath, tempDir.path(),
        QByteArrayLiteral(
            R"({"name":"mwb26.11","publication":{"symbol":"mwb","issueTagNumber":20261100,"language":0}})"),
        mwbDb, QStringLiteral("mwb_E_202611.db"), {}));

    kmedia::JwpubWeekFilterOptions opts;
    opts.targetDate = QDate(2026, 11, 25);
    opts.pubSymbolHint = QStringLiteral("mwb");
    opts.includePrinted = false;
    opts.excludeFootnotes = true;
    const kmedia::JwpubPackage filtered = kmedia::JwpubParser::parseForWeek(mwbPath, opts);
    REQUIRE(filtered.valid);
    REQUIRE(filtered.documents.size() == 1);
    REQUIRE(filtered.documents.at(0).documentId == 2);

    // Expect only cbs.mp4 (#24) and treasures.jpg (#25): #20 printed, #21 footnote, #22
    // suppressZoom, #23 linked crop, #10 other week.
    REQUIRE(filtered.mediaItems.size() == 2);
    REQUIRE(filtered.mediaItems.at(0).filePath == QStringLiteral("media/treasures.jpg"));
    REQUIRE(filtered.mediaItems.at(0).tagValue == QStringLiteral("Para. 4-5"));
    REQUIRE(filtered.mediaItems.at(1).filePath == QStringLiteral("media/cbs.mp4"));
    REQUIRE(filtered.mediaItems.at(1).isCbsCandidate);
    REQUIRE(filtered.mediaItems.at(1).groupCaption == QStringLiteral("Make Disciples Lesson 2"));
    REQUIRE(filtered.mediaItems.at(1).label == QStringLiteral("CBS Clip"));

    // Extract ignores mwbr26, resolves Song 126 for sjj.
    REQUIRE(filtered.extracts.size() == 3);
    REQUIRE(filtered.extracts.at(0).songNumber == 126);
    REQUIRE(filtered.extracts.at(1).isTh);

    // Watchtower Class=40 article selection & Memorial unlinked CategoryType=26 preservation.
    const QByteArray wtDb = kmedia::test_helpers::buildSqliteBytes(
        tempDir.path(),
        {QStringLiteral("CREATE TABLE Publication (IssueTagNumber INTEGER, MepsLanguageIndex "
                        "INTEGER, UndatedSymbol TEXT, UniqueEnglishSymbol TEXT, Title TEXT, "
                        "ShortTitle TEXT, Year INTEGER, VersionNumber INTEGER)"),
         QStringLiteral("INSERT INTO Publication VALUES (20260900, 0, 'w', 'w', 'WT Sep 2026', "
                        "'w26.09', 2026, 2)"),
         QStringLiteral("CREATE TABLE Document (DocumentId INTEGER PRIMARY KEY, MepsDocumentId "
                        "INTEGER, Title TEXT, Subtitle TEXT, FeatureTitle TEXT, Class INTEGER)"),
         QStringLiteral("INSERT INTO Document VALUES (1, 101, 'Intro', '', '', 1)"),
         QStringLiteral("INSERT INTO Document VALUES (2, 201, 'Article 1', 'First', '', 40)"),
         QStringLiteral("INSERT INTO Document VALUES (3, 202, 'Article 2', 'Second', '', 40)"),
         QStringLiteral("CREATE TABLE Multimedia (MultimediaId INTEGER PRIMARY KEY, FilePath TEXT, "
                        "Label TEXT, Caption TEXT, MimeType TEXT, MajorType INTEGER, MinorType "
                        "INTEGER, CategoryType INTEGER, Track INTEGER, KeySymbol TEXT, "
                        "IssueTagNumber INTEGER, MepsLanguageIndex INTEGER)"),
         QStringLiteral("INSERT INTO Multimedia VALUES (1, 'media/a1.jpg', 'Art 1', '', "
                        "'image/jpeg', 1, 1, 4, 0, 'w', 20260900, 0)"),
         QStringLiteral("INSERT INTO Multimedia VALUES (2, 'media/a2.jpg', 'Art 2', '', "
                        "'image/jpeg', 1, 1, 4, 0, 'w', 20260900, 0)"),
         QStringLiteral("INSERT INTO Multimedia VALUES (3, 'media/welcome.jpg', 'Welcome', '', "
                        "'image/jpeg', 1, 1, 26, 0, 'mi26', 0, 0)"),
         QStringLiteral(
             "CREATE TABLE DocumentMultimedia (DocumentId INTEGER, MultimediaId INTEGER, "
             "BeginParagraphOrdinal INTEGER, EndParagraphOrdinal INTEGER)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (2, 1, 1, 1)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (3, 2, 1, 1)")});
    REQUIRE_FALSE(wtDb.isEmpty());

    const QString wtPath = tempDir.filePath(QStringLiteral("w_E_202609.jwpub"));
    REQUIRE(kmedia::test_helpers::writeJwpubFixture(
        wtPath, tempDir.path(),
        QByteArrayLiteral(
            R"({"name":"w26.09","publication":{"symbol":"w","issueTagNumber":20260900,"language":0}})"),
        wtDb, QStringLiteral("w_E_202609.db"), {}));

    kmedia::JwpubWeekFilterOptions wtOpts;
    wtOpts.targetDate =
        QDate(2026, 11, 15); // 2nd week of Nov 2026 -> weekIdx = 1 -> Document 3 (Article 2)
    wtOpts.pubSymbolHint = QStringLiteral("w");
    const kmedia::JwpubPackage wtFiltered = kmedia::JwpubParser::parseForWeek(wtPath, wtOpts);
    REQUIRE(wtFiltered.valid);
    REQUIRE(wtFiltered.documents.size() == 1);
    REQUIRE(wtFiltered.documents.at(0).title == QStringLiteral("Article 2"));
    REQUIRE(wtFiltered.mediaItems.size() == 1);
    REQUIRE(wtFiltered.mediaItems.at(0).filePath == QStringLiteral("media/a2.jpg"));

    kmedia::JwpubWeekFilterOptions memOpts;
    memOpts.targetDate = QDate(2026, 4, 2);
    memOpts.pubSymbolHint = QStringLiteral("mi26");
    const kmedia::JwpubPackage memFiltered = kmedia::JwpubParser::parseForWeek(wtPath, memOpts);
    REQUIRE(memFiltered.mediaItems.size() >= 1);
}
