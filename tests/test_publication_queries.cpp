#include "TestZipFixtures.h"
#include "core/MeetingSections.h"
#include "parsers/JwpubParser.h"
#include "parsers/MeetingMediaBuilder.h"
#include "parsers/PublicationDatabase.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>

#include <catch2/catch_test_macros.hpp>

namespace {

using kmedia::PublicationMediaRow;

QString extractFixture(const QTemporaryDir& tempDir, const QString& name,
                       const QStringList& statements,
                       const std::vector<std::pair<QString, QByteArray>>& assets = {})
{
    const QByteArray dbBytes = kmedia::test_helpers::buildSqliteBytes(tempDir.path(), statements);
    REQUIRE_FALSE(dbBytes.isEmpty());
    const QString jwpubPath = tempDir.filePath(name + QStringLiteral(".jwpub"));
    REQUIRE(kmedia::test_helpers::writeJwpubFixture(jwpubPath, tempDir.path(),
                                                    QByteArrayLiteral("{}"), dbBytes,
                                                    name + QStringLiteral(".db"), assets));
    QString error;
    const QString dbPath = kmedia::JwpubParser::extractContents(
        jwpubPath, tempDir.filePath(QStringLiteral("Publications/") + name), &error);
    REQUIRE(error.isEmpty());
    REQUIRE_FALSE(dbPath.isEmpty());
    return dbPath;
}

QStringList workbookStatements()
{
    return {
        QStringLiteral("CREATE TABLE Document (DocumentId INTEGER PRIMARY KEY, MepsDocumentId "
                       "INTEGER, Class INTEGER, Title TEXT)"),
        QStringLiteral("INSERT INTO Document VALUES (1, 202026401, 106, 'November 23-29'), "
                       "(2, 202026402, 106, 'November 30-December 6')"),
        QStringLiteral("CREATE TABLE DatedText (DatedTextId INTEGER PRIMARY KEY, DocumentId "
                       "INTEGER, FirstDateOffset INTEGER, LastDateOffset INTEGER)"),
        QStringLiteral("INSERT INTO DatedText VALUES (1, 1, 20261123, 20261129), "
                       "(2, 2, 20261130, 20261206)"),
        QStringLiteral("CREATE TABLE Multimedia (MultimediaId INTEGER PRIMARY KEY, KeySymbol TEXT, "
                       "Track INTEGER, IssueTagNumber INTEGER, MepsLanguageIndex INTEGER, MimeType "
                       "TEXT, CategoryType INTEGER, Label TEXT, Caption TEXT, FilePath TEXT, "
                       "LinkMultimediaId INTEGER, SuppressZoom INTEGER)"),
        QStringLiteral(
            "INSERT INTO Multimedia VALUES "
            "(10, 'sjjm', 12, 0, 0, 'video/mp4', -1, '12. Opening Song', '', NULL, NULL, 0), "
            "(11, NULL, 0, 0, 0, 'image/jpeg', 8, 'Treasures', 'Picture (par. 4)', 'tgw.jpg', 12, "
            "0), "
            "(12, NULL, 0, 0, 0, 'image/jpeg', 8, '', '', 'tgw_wide.jpg', NULL, 0), "
            "(13, NULL, 0, 0, 0, 'image/jpeg', 4, 'Printed', '', 'printed.jpg', NULL, 0), "
            "(14, NULL, 0, 0, 0, 'image/jpeg', 8, 'No zoom', '', 'nozoom.jpg', NULL, 1), "
            "(15, 'sjjm', 34, 0, 0, 'video/mp4', -1, '34. Middle Song', '', NULL, NULL, 0), "
            "(16, NULL, 0, 0, 0, 'image/jpeg', 8, 'Ministry', '', 'ayfm.jpg', NULL, 0), "
            "(17, NULL, 0, 0, 0, 'image/jpeg', 8, 'Study', '', 'cbs.jpg', NULL, 0), "
            "(18, 'sjjm', 56, 0, 0, 'video/mp4', -1, '56. Closing Song', '', NULL, NULL, 0)"),
        QStringLiteral("CREATE TABLE DocumentMultimedia (DocumentMultimediaId INTEGER PRIMARY "
                       "KEY, DocumentId INTEGER, MultimediaId INTEGER, BeginParagraphOrdinal "
                       "INTEGER, EndParagraphOrdinal INTEGER)"),
        // Song videos carry unreliable ordinals (1, 2, 3); the song extracts hold the real ones.
        QStringLiteral("INSERT INTO DocumentMultimedia VALUES (1, 1, 10, 1, 1), (2, 1, 11, 4, 5), "
                       "(3, 1, 12, 4, 5), (4, 1, 13, 6, 6), (5, 1, 14, 7, 7), (6, 1, 16, 20, 20), "
                       "(7, 1, 15, 2, 2), (8, 1, 17, 40, 40), (9, 1, 18, 3, 3)"),
        QStringLiteral("CREATE TABLE DocumentParagraph (DocumentParagraphId INTEGER PRIMARY KEY, "
                       "DocumentId INTEGER, ParagraphIndex INTEGER, ParagraphNumberLabel TEXT, "
                       "BeginPosition INTEGER, EndPosition INTEGER)"),
        QStringLiteral("INSERT INTO DocumentParagraph VALUES (1, 1, 1, NULL, 10, 19), "
                       "(2, 1, 4, '4', 40, 49), (3, 1, 5, '5', 50, 59), (4, 1, 6, '6', 60, 69), "
                       "(5, 1, 7, '7', 70, 79), (6, 1, 20, '20', 200, 209), "
                       "(7, 1, 40, '18', 400, 409)"),
        QStringLiteral("CREATE TABLE Question (QuestionId INTEGER PRIMARY KEY, DocumentId INTEGER, "
                       "TargetParagraphOrdinal INTEGER, TargetParagraphNumberLabel TEXT)"),
        QStringLiteral("CREATE TABLE RefPublication (RefPublicationId INTEGER PRIMARY KEY, "
                       "UniqueEnglishSymbol TEXT, UndatedSymbol TEXT, IssueTagNumber INTEGER)"),
        QStringLiteral("INSERT INTO RefPublication VALUES (1, 'sjj', 'sjj', 0), (2, 'lmd', 'lmd', "
                       "0), (3, 'th', 'th', 0), (4, 'mwbr26', 'mwbr', 20261100)"),
        QStringLiteral("CREATE TABLE Extract (ExtractId INTEGER PRIMARY KEY, RefMepsDocumentId "
                       "INTEGER, RefPublicationId INTEGER, RefBeginParagraphOrdinal INTEGER, "
                       "RefEndParagraphOrdinal INTEGER, Link TEXT, Caption TEXT, "
                       "RefMepsDocumentClass INTEGER)"),
        QStringLiteral("INSERT INTO Extract VALUES "
                       "(1, 1102016812, 1, NULL, NULL, 'jwpub://p/E:1102016812/', 'Song 12', 31), "
                       "(2, 1102016834, 1, NULL, NULL, 'jwpub://p/T:1102016834/', 'Song 34', 31), "
                       "(3, 1102016856, 1, NULL, NULL, 'jwpub://p/E:1102016856/', 'Song 56', 31), "
                       "(4, 1102023301, 2, 3, 5, 'jwpub://p/E:1102023301/3-5', 'Lesson 3', 0), "
                       "(5, 1102001234, 3, 1, 2, 'jwpub://p/E:1102001234/1-2', 'Study 5', 0), "
                       "(6, 1102026999, 4, 1, 1, 'jwpub://p/E:1102026999/1', 'Reference', 0)"),
        QStringLiteral("CREATE TABLE DocumentExtract (DocumentExtractId INTEGER PRIMARY KEY, "
                       "DocumentId INTEGER, ExtractId INTEGER, BeginParagraphOrdinal INTEGER, "
                       "EndParagraphOrdinal INTEGER, SortPosition INTEGER)"),
        QStringLiteral("INSERT INTO DocumentExtract VALUES (1, 1, 1, 1, 1, 10), (2, 1, 2, 30, 30, "
                       "300), (3, 1, 3, 42, 42, 420), (4, 1, 4, 21, 21, 210), (5, 1, 5, 22, 22, "
                       "220), (6, 1, 6, 23, 23, 230)"),
        QStringLiteral("CREATE TABLE VideoMarker (VideoMarkerId INTEGER PRIMARY KEY, MultimediaId "
                       "INTEGER, Label TEXT, StartTimeTicks INTEGER, DurationTicks INTEGER, "
                       "EndTransitionDurationTicks INTEGER)"),
        QStringLiteral("INSERT INTO VideoMarker VALUES (1, 15, 'Verse 1', 0, 300000000, 0)"),
    };
}

QStringList watchtowerStatements()
{
    return {
        QStringLiteral("CREATE TABLE Document (DocumentId INTEGER PRIMARY KEY, MepsDocumentId "
                       "INTEGER, Class INTEGER, Title TEXT)"),
        QStringLiteral("INSERT INTO Document VALUES (1, 2026500, 13, 'Cover'), "
                       "(2, 2026501, 40, 'Article 1'), (3, 2026502, 40, 'Article 2'), "
                       "(4, 2026503, 40, 'Article 3')"),
        QStringLiteral("CREATE TABLE DatedText (DatedTextId INTEGER PRIMARY KEY, DocumentId "
                       "INTEGER, FirstDateOffset INTEGER, LastDateOffset INTEGER)"),
        QStringLiteral("INSERT INTO DatedText VALUES (1, 2, 20261102, 20261108), "
                       "(2, 3, 20261109, 20261115), (3, 4, 20261116, 20261122)"),
        QStringLiteral("CREATE TABLE Multimedia (MultimediaId INTEGER PRIMARY KEY, KeySymbol TEXT, "
                       "Track INTEGER, IssueTagNumber INTEGER, MepsLanguageIndex INTEGER, MimeType "
                       "TEXT, CategoryType INTEGER, Label TEXT, Caption TEXT, FilePath TEXT, "
                       "LinkMultimediaId INTEGER)"),
        QStringLiteral(
            "INSERT INTO Multimedia VALUES "
            "(30, 'sjjm', 20, 0, 0, 'video/mp4', -1, '20. Opening', '', NULL, NULL), "
            "(31, NULL, 0, 0, 0, 'image/jpeg', 8, 'Lead picture', 'Picture (par. 2)', 'p2.jpg', "
            "NULL), "
            "(32, 'pk', 3, 0, 0, 'video/mp4', -1, 'Video in paragraph', '', NULL, NULL), "
            "(33, 'sjjm', 21, 0, 0, 'video/mp4', -1, '21. Closing', '', NULL, NULL)"),
        QStringLiteral("CREATE TABLE DocumentMultimedia (DocumentMultimediaId INTEGER PRIMARY "
                       "KEY, DocumentId INTEGER, MultimediaId INTEGER, BeginParagraphOrdinal "
                       "INTEGER, EndParagraphOrdinal INTEGER)"),
        QStringLiteral("INSERT INTO DocumentMultimedia VALUES (1, 3, 30, 1, 1), (2, 3, 31, 2, 2), "
                       "(3, 3, 32, 3, 3), (4, 3, 33, 9, 9)"),
        QStringLiteral("CREATE TABLE DocumentParagraph (DocumentParagraphId INTEGER PRIMARY KEY, "
                       "DocumentId INTEGER, ParagraphIndex INTEGER, ParagraphNumberLabel TEXT, "
                       "BeginPosition INTEGER, EndPosition INTEGER)"),
        QStringLiteral("INSERT INTO DocumentParagraph VALUES (1, 3, 1, NULL, 5, 9), "
                       "(2, 3, 2, '1', 10, 19), (3, 3, 3, '2', 20, 29), (4, 3, 9, NULL, 90, 99)"),
        QStringLiteral("CREATE TABLE Question (QuestionId INTEGER PRIMARY KEY, DocumentId INTEGER, "
                       "TargetParagraphOrdinal INTEGER, TargetParagraphNumberLabel TEXT)"),
        QStringLiteral("INSERT INTO Question VALUES (1, 3, 2, '1'), (2, 3, 3, '2')"),
    };
}

PublicationMediaRow mediaRow(qint64 id, int paragraph, const QString& symbol = {}, int track = 0,
                             const QString& mime = QStringLiteral("image/jpeg"))
{
    PublicationMediaRow row;
    row.multimediaId = id;
    row.beginParagraphOrdinal = paragraph;
    row.endParagraphOrdinal = paragraph;
    row.keySymbol = symbol;
    row.track = track;
    row.mimeType = mime;
    row.mepsLanguageIndex = 0;
    row.filePath = QStringLiteral("/cache/Publications/mwb_E_202611/m%1.%2")
                       .arg(id)
                       .arg(mime.contains(QStringLiteral("video")) ? QStringLiteral("mp4")
                                                                   : QStringLiteral("jpg"));
    row.label = QStringLiteral("Item %1").arg(id);
    return row;
}

} // namespace

TEST_CASE("JwpubParser extracts a publication once and reuses the folder", "[publications]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString dbPath =
        extractFixture(tempDir, QStringLiteral("mwb_E_20261100"), workbookStatements(),
                       {{QStringLiteral("tgw_wide.jpg"), QByteArrayLiteral("x")}});
    const QString folder = QFileInfo(dbPath).absolutePath();
    REQUIRE(QFileInfo::exists(QDir(folder).filePath(QStringLiteral("tgw_wide.jpg"))));
    const QDateTime firstWrite = QFileInfo(dbPath).lastModified();

    // A second call with the same archive keeps the extracted files untouched.
    QThread::msleep(20);
    QString error;
    const QString again = kmedia::JwpubParser::extractContents(
        tempDir.filePath(QStringLiteral("mwb_E_20261100.jwpub")), folder, &error);
    REQUIRE(again == dbPath);
    REQUIRE(QFileInfo(dbPath).lastModified() == firstWrite);

    REQUIRE(kmedia::JwpubParser::extractContents(tempDir.filePath(QStringLiteral("missing.jwpub")),
                                                 folder, &error)
                .isEmpty());
    REQUIRE_FALSE(error.isEmpty());
}

TEST_CASE("PublicationDatabase reads the workbook week, its media, and its extracts",
          "[publications][midweek]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    kmedia::PublicationDatabase db;
    REQUIRE(db.open(extractFixture(tempDir, QStringLiteral("mwb"), workbookStatements())));

    REQUIRE(db.midweekDocumentId(QDate(2026, 11, 23)) == 1);
    REQUIRE(db.midweekDocumentId(QDate(2026, 11, 30)) == 2);
    REQUIRE(db.midweekDocumentId(QDate(2026, 12, 7)) == -1);

    kmedia::PublicationDatabase::MediaSelector selector;
    selector.documentId = 1;
    const auto rows = db.documentMultimedia(selector, false);
    std::vector<qint64> ids;
    for (const auto& row : rows) {
        ids.push_back(row.multimediaId);
    }
    // Printed art, suppressed zoom, and the linked wide crop are left out; song ordinals come
    // from the song extracts, so the meeting order holds.
    REQUIRE(ids == std::vector<qint64>{10, 11, 16, 15, 17, 18});
    REQUIRE(rows[1].filePath == QStringLiteral("tgw_wide.jpg"));
    REQUIRE(rows[1].targetParagraphNumberLabel == QStringLiteral("4-5"));
    REQUIRE(rows[3].beginParagraphOrdinal == 30);
    REQUIRE(rows[3].videoMarkers.size() == 1);
    REQUIRE(rows[5].beginParagraphOrdinal == 42);

    REQUIRE(db.documentMultimedia(selector, true).size() == 7);

    const auto extracts = db.documentExtracts(1, false);
    REQUIRE(extracts.size() == 2);
    REQUIRE(extracts[0].uniqueEnglishSymbol == QStringLiteral("lmd"));
    REQUIRE(extracts[0].refBeginParagraphOrdinal == 3);
    REQUIRE(extracts[1].uniqueEnglishSymbol == QStringLiteral("th"));
    REQUIRE(db.documentExtracts(1, true).size() == 1);

    const auto songExtracts = db.weekendSongExtracts(1);
    REQUIRE(songExtracts.size() == 2);
    REQUIRE(songExtracts[1].beginParagraphOrdinal == 30);
}

TEST_CASE("PublicationDatabase finds the Watchtower study of a week only in the right issue",
          "[publications][weekend]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    kmedia::PublicationDatabase db;
    REQUIRE(db.open(extractFixture(tempDir, QStringLiteral("w"), watchtowerStatements())));

    const auto study = db.watchtowerStudyForWeek(QDate(2026, 11, 9));
    REQUIRE(study.has_value());
    REQUIRE(study->documentId == 3);
    REQUIRE(study->title == QStringLiteral("Article 2"));
    // A week outside the issue is "no match", never a guessed article.
    REQUIRE_FALSE(db.watchtowerStudyForWeek(QDate(2026, 11, 23)).has_value());

    const auto videos = db.weekendVideos(3);
    REQUIRE(videos.size() == 3);
    REQUIRE(videos[0].multimediaId == 30);
    REQUIRE(videos[0].isInNumberedParagraphs == 0);
    REQUIRE(videos[1].multimediaId == 32);
    REQUIRE(videos[1].isInNumberedParagraphs == 1);
    REQUIRE(videos[1].targetParagraphNumberLabel == QStringLiteral("2"));

    const auto pictures = db.weekendMediaWithoutVideos(3, QStringLiteral("sjjm"));
    REQUIRE(pictures.size() == 1);
    REQUIRE(pictures[0].multimediaId == 31);
    REQUIRE(pictures[0].targetParagraphNumberLabel == QStringLiteral("1"));
    REQUIRE(pictures[0].hasBeginPosition);
}

TEST_CASE("MeetingMediaBuilder assigns midweek sections by paragraph in any language",
          "[builder][midweek]")
{
    using kmedia::MeetingMediaBuilder;
    std::vector<PublicationMediaRow> rows = {
        mediaRow(10, 1, QStringLiteral("sjjm"), 12, QStringLiteral("video/mp4")),
        mediaRow(11, 4),
        mediaRow(16, 20),
        mediaRow(20, 21),
        mediaRow(21, 21),
        mediaRow(15, 30, QStringLiteral("sjjm"), 34, QStringLiteral("video/mp4")),
        mediaRow(17, 40),
        mediaRow(18, 42, QStringLiteral("sjjm"), 56, QStringLiteral("video/mp4")),
    };
    rows[0].label = QStringLiteral("12. Opening Song");
    rows[1].caption = QStringLiteral("Picture (par. 4)");
    rows[1].targetParagraphNumberLabel = QStringLiteral("4");
    rows[3].extractCaption = QStringLiteral("Lesson 3");
    rows[4].extractCaption = QStringLiteral("Lesson 3");

    kmedia::MeetingMediaContext context;
    context.date = QDate(2026, 11, 25);
    context.isMidweek = true;
    const auto items = MeetingMediaBuilder::buildDay(rows, context);
    REQUIRE(items.size() == 8);

    QStringList sections;
    for (const auto& item : items) {
        sections << item.sectionId;
    }
    REQUIRE(sections == QStringList{QStringLiteral("tgw"), QStringLiteral("tgw"),
                                    QStringLiteral("ayfm"), QStringLiteral("ayfm"),
                                    QStringLiteral("ayfm"), QStringLiteral("lac"),
                                    QStringLiteral("lac"), QStringLiteral("lac")});
    REQUIRE(items[0].title == QStringLiteral("Opening Song"));
    REQUIRE(items[0].tagType == QStringLiteral("song"));
    REQUIRE(items[0].tagValue == QStringLiteral("12"));
    REQUIRE(items[1].tagType == QStringLiteral("para"));
    REQUIRE(items[1].tagValue == QStringLiteral("4"));
    REQUIRE(items[3].groupKey == QStringLiteral("Lesson 3"));
    REQUIRE(items[4].groupKey == QStringLiteral("Lesson 3"));
    REQUIRE(items[6].isCbs);
    REQUIRE_FALSE(items[5].isCbs);
    REQUIRE(items[0].uniqueKey.startsWith(QStringLiteral("20261125-")));
    REQUIRE(items[5].markerRangesJson.isEmpty());

    // Circuit Overseer week: the closing song and the study media are hidden.
    context.isCoWeek = true;
    const auto coItems = MeetingMediaBuilder::buildDay(rows, context);
    REQUIRE(coItems.back().hidden);
    REQUIRE(coItems[6].hidden);
    REQUIRE_FALSE(coItems[5].hidden);
}

TEST_CASE("MeetingMediaBuilder orders weekend media around the study songs", "[builder][weekend]")
{
    using kmedia::MeetingMediaBuilder;
    PublicationMediaRow picture = mediaRow(31, 2);
    picture.beginPosition = 10;
    picture.hasBeginPosition = true;
    picture.targetParagraphNumberLabel = QStringLiteral("1");
    PublicationMediaRow inParagraph =
        mediaRow(32, 3, QStringLiteral("pk"), 3, QStringLiteral("video/mp4"));
    inParagraph.beginPosition = 20;
    inParagraph.hasBeginPosition = true;
    inParagraph.isInNumberedParagraphs = 1;
    inParagraph.targetParagraphNumberLabel = QStringLiteral("2");
    PublicationMediaRow opening =
        mediaRow(30, 1, QStringLiteral("sjjm"), 20, QStringLiteral("video/mp4"));
    opening.beginPosition = 5;
    opening.hasBeginPosition = true;
    opening.isInNumberedParagraphs = 0;
    PublicationMediaRow closing =
        mediaRow(33, 9, QStringLiteral("sjjm"), 21, QStringLiteral("video/mp4"));
    closing.beginPosition = 90;
    closing.hasBeginPosition = true;
    closing.isInNumberedParagraphs = 0;

    kmedia::WeekendMediaFilters filters;
    filters.excludeFootnotes = true;
    auto media = MeetingMediaBuilder::orderWeekendMedia({picture}, {inParagraph},
                                                        {opening, closing}, filters);
    // Songs outside the numbered paragraphs count as footnotes and are filtered here...
    REQUIRE(media.size() == 2);

    const auto songs = MeetingMediaBuilder::selectWeekendSongs(20261100, {}, {closing, opening});
    REQUIRE(songs.size() == 2);
    REQUIRE(songs[0].multimediaId == 30);

    // ...and come back as the opening and closing songs of the study.
    MeetingMediaBuilder::placeWeekendSongs(media, songs,
                                           {{1, QStringLiteral("jwpub://p/T:1102016820/"), 1},
                                            {2, QStringLiteral("jwpub://p/E:1102016821/"), 9}});
    REQUIRE(media.size() == 4);
    REQUIRE(media.front().multimediaId == 30);
    REQUIRE(media.front().beginParagraphOrdinal == 0);
    REQUIRE(media.front().languageOverride == QStringLiteral("T"));
    REQUIRE(media.back().multimediaId == 33);
    REQUIRE(media.back().beginParagraphOrdinal == kmedia::PublicationDatabase::kLastSongOrdinal);

    kmedia::MeetingMediaContext context;
    context.date = QDate(2026, 11, 29);
    context.isWeekend = true;
    for (const auto& item : MeetingMediaBuilder::buildDay(media, context)) {
        REQUIRE(item.sectionId == QStringLiteral("wt"));
    }
    REQUIRE(MeetingMediaBuilder::selectWeekendSongs(20221200, {opening}, {closing}).size() == 1);
}

TEST_CASE("MeetingMediaBuilder reads paragraph references from captions", "[builder][tags]")
{
    using kmedia::MeetingMediaBuilder;
    REQUIRE(MeetingMediaBuilder::paragraphNumbers(QStringLiteral("7"), {}) == QStringLiteral("7"));
    REQUIRE(MeetingMediaBuilder::paragraphNumbers({}, QStringLiteral("Opening picture (par. 4)")) ==
            QStringLiteral("4"));
    REQUIRE(MeetingMediaBuilder::paragraphNumbers(QStringLiteral("11"),
                                                  QStringLiteral("Image for paragraphs 11-12.")) ==
            QStringLiteral("11-12"));
    // Bible references are not paragraphs.
    REQUIRE(MeetingMediaBuilder::paragraphNumbers(QStringLiteral("3"),
                                                  QStringLiteral("Jesus heals (Matthew 9:2)"))
                .isEmpty());
    REQUIRE(MeetingMediaBuilder::paragraphNumbers(QStringLiteral("2"),
                                                  QStringLiteral("A family at home"))
                .isEmpty());

    PublicationMediaRow song =
        mediaRow(10, 1, QStringLiteral("sjjm"), 12, QStringLiteral("video/mp4"));
    REQUIRE(MeetingMediaBuilder::pubMediaId(song) == QStringLiteral("sjjm_0_12"));
    PublicationMediaRow picture = mediaRow(11, 4);
    picture.mepsDocumentId = 202026401;
    REQUIRE(MeetingMediaBuilder::pubMediaId(picture) == QStringLiteral("202026401_0"));
    REQUIRE(MeetingMediaBuilder::languageFromExtractLink(
                QStringLiteral("jwpub://p/E:1102023301/3-5")) == QStringLiteral("E"));
    REQUIRE(MeetingMediaBuilder::languageFromExtractLink(QStringLiteral("no link")).isEmpty());
}

TEST_CASE("PublicationDatabase lists Study Bible media by book, chapter, and verse",
          "[parsers][bible]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString dbPath = extractFixture(
        tempDir, QStringLiteral("nwtsty_E"),
        {
            QStringLiteral("CREATE TABLE BibleBook (BibleBookId INTEGER PRIMARY KEY, "
                           "BookDisplayTitle TEXT)"),
            QStringLiteral("INSERT INTO BibleBook VALUES (40, 'Matthew'), (43, 'John')"),
            QStringLiteral("CREATE TABLE BibleChapter (BibleChapterId INTEGER PRIMARY KEY, "
                           "BookNumber INTEGER, ChapterNumber INTEGER, FirstVerseId INTEGER, "
                           "LastVerseId INTEGER)"),
            QStringLiteral("INSERT INTO BibleChapter VALUES (1, 40, 4, 100, 125), "
                           "(2, 43, 11, 200, 257)"),
            QStringLiteral(
                "CREATE TABLE BibleVerse (BibleVerseId INTEGER PRIMARY KEY, Label TEXT)"),
            QStringLiteral(
                "INSERT INTO BibleVerse VALUES (100, '<span class=\"cl\">4</span>'), "
                "(104, '<span class=\"vl\">5</span>'), (120, '<span class=\"vl\">21</span>'), "
                "(243, '<span class=\"vl\">44</span>')"),
            QStringLiteral(
                "CREATE TABLE Multimedia (MultimediaId INTEGER PRIMARY KEY, KeySymbol "
                "TEXT, Track INTEGER, IssueTagNumber INTEGER, MepsLanguageIndex INTEGER, "
                "MepsDocumentId INTEGER, MimeType TEXT, CategoryType INTEGER, Label TEXT, "
                "Caption TEXT, FilePath TEXT, LinkMultimediaId INTEGER)"),
            QStringLiteral(
                "INSERT INTO Multimedia VALUES "
                "(1, NULL, NULL, 0, 0, NULL, 'image/jpeg', 8, 'Wilderness', '', 'wild.jpg', NULL), "
                "(2, NULL, 1, 0, 0, 1001072080, 'video/mp4', -1, 'Battlement', '', "
                "'battlement_r240P.mp4', 3), "
                "(3, NULL, NULL, 0, 0, NULL, 'image/jpeg', 17, '', '', 'battlement.jpg', NULL), "
                "(4, NULL, NULL, 0, 0, NULL, 'image/jpeg', 9, 'Cover', '', 'cover.jpg', NULL), "
                "(5, NULL, NULL, 0, 0, NULL, 'image/jpeg', 8, 'Lazarus', '', 'lazarus.jpg', NULL)"),
            QStringLiteral("CREATE TABLE VerseMultimediaMap (BibleVerseId INTEGER, MultimediaId "
                           "INTEGER)"),
            // The video belongs to two verses and comes back once, at the first one.
            QStringLiteral("INSERT INTO VerseMultimediaMap VALUES (100, 1), (104, 2), (120, 2), "
                           "(100, 4), (243, 5)"),
        });

    kmedia::PublicationDatabase db;
    REQUIRE(db.open(dbPath));
    const auto books = db.bibleBooksWithMedia();
    REQUIRE(books.size() == 2);
    REQUIRE(books[0].book == 40);
    REQUIRE(books[0].name == QStringLiteral("Matthew"));
    REQUIRE(books[0].chapters == QList<int>{4});
    REQUIRE(books[1].chapters == QList<int>{11});

    const auto rows = db.verseMultimedia(40, 4);
    REQUIRE(rows.size() == 2);
    REQUIRE(rows[0].label == QStringLiteral("Wilderness"));
    REQUIRE(rows[0].targetParagraphNumberLabel == QStringLiteral("4:4"));
    REQUIRE(rows[1].categoryType == -1);
    REQUIRE(rows[1].mepsDocumentId == 1001072080);
    REQUIRE(rows[1].linkedPreviewFilePath == QStringLiteral("battlement.jpg"));
    REQUIRE(rows[1].targetParagraphNumberLabel == QStringLiteral("4:5"));
    REQUIRE(db.verseMultimedia(43, 11).front().targetParagraphNumberLabel ==
            QStringLiteral("11:44"));
    REQUIRE(db.verseMultimedia(43, 12).empty());
}

TEST_CASE("alignSongOrdinals preserves valid ordinals between songs and cleans extract HTML",
          "[publications][builder]")
{
    REQUIRE(kmedia::MeetingSections::cleanExtractCaption(
                QStringLiteral("<span class=\"eloc\">w25.11 pp. 2-7</span> "
                               "<span class=\"etitle\">Walk &amp; Serve</span>")) ==
            QStringLiteral("w25.11 pp. 2-7 - Walk & Serve"));
    REQUIRE(kmedia::MeetingSections::cleanExtractCaption(QStringLiteral("Lesson 3")) ==
            QStringLiteral("Lesson 3"));

    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString dbPath = extractFixture(
        tempDir, QStringLiteral("mwb_ord"),
        {
            QStringLiteral("CREATE TABLE Document (DocumentId INTEGER PRIMARY KEY, MepsDocumentId "
                           "INTEGER, Class INTEGER, Title TEXT)"),
            QStringLiteral("INSERT INTO Document VALUES (1, 2026404, 106, 'Nov 23')"),
            QStringLiteral("CREATE TABLE Multimedia (MultimediaId INTEGER PRIMARY KEY, KeySymbol "
                           "TEXT, Track INTEGER, IssueTagNumber INTEGER, MepsLanguageIndex "
                           "INTEGER, MimeType TEXT, CategoryType INTEGER, Label TEXT, Caption "
                           "TEXT, FilePath TEXT, LinkMultimediaId INTEGER)"),
            QStringLiteral(
                "INSERT INTO Multimedia VALUES "
                "(10, 'sjjm', 130, 0, 0, 'video/mp4', -1, '130. Song 1', '', 's130.mp4', NULL), "
                "(11, NULL, 0, 0, 0, 'image/jpeg', 8, 'Treasures Art', '', 'art1.jpg', NULL), "
                "(12, 'sjjm', 95, 0, 0, 'video/mp4', -1, '95. Song 2', '', 's95.mp4', NULL), "
                "(13, NULL, 0, 0, 0, 'image/jpeg', 8, 'Living Art', '', 'art2.jpg', NULL), "
                "(14, 'sjjm', 100, 0, 0, 'video/mp4', -1, '100. Song 3', '', 's100.mp4', NULL)"),
            QStringLiteral("CREATE TABLE DocumentMultimedia (DocumentMultimediaId INTEGER PRIMARY "
                           "KEY, DocumentId INTEGER, MultimediaId INTEGER, BeginParagraphOrdinal "
                           "INTEGER, EndParagraphOrdinal INTEGER)"),
            // Song rows carry early/stale ordinals (1, 15, 50) while non-song art sits at 9 and 33,
            // matching extract paragraph ordinals at 17 and 42.
            QStringLiteral("INSERT INTO DocumentMultimedia VALUES "
                           "(1, 1, 10, 1, 1), (2, 1, 11, 9, 9), (3, 1, 12, 15, 15), "
                           "(4, 1, 13, 33, 33), (5, 1, 14, 50, 50)"),
            QStringLiteral(
                "CREATE TABLE DocumentParagraph (DocumentParagraphId INTEGER PRIMARY KEY, "
                "DocumentId INTEGER, ParagraphIndex INTEGER, ParagraphNumberLabel TEXT, "
                "BeginPosition INTEGER, EndPosition INTEGER)"),
            QStringLiteral("CREATE TABLE RefPublication (RefPublicationId INTEGER PRIMARY KEY, "
                           "UniqueEnglishSymbol TEXT, UndatedSymbol TEXT, IssueTagNumber INTEGER)"),
            QStringLiteral("INSERT INTO RefPublication VALUES (1, 'sjjm', 'sjj', 0)"),
            QStringLiteral("CREATE TABLE Extract (ExtractId INTEGER PRIMARY KEY, RefMepsDocumentId "
                           "INTEGER, RefPublicationId INTEGER, RefBeginParagraphOrdinal INTEGER, "
                           "RefEndParagraphOrdinal INTEGER, Link TEXT, Caption TEXT, "
                           "RefMepsDocumentClass INTEGER)"),
            QStringLiteral("INSERT INTO Extract VALUES "
                           "(1, 101, 1, 1, 2, 'jwpub://p/E:101/1', 'Song 1', 31), "
                           "(2, 102, 1, 1, 2, 'jwpub://p/E:102/1', 'Song 2', 31), "
                           "(3, 103, 1, 1, 2, 'jwpub://p/E:103/1', 'Song 3', 31)"),
            QStringLiteral("CREATE TABLE DocumentExtract (DocumentExtractId INTEGER PRIMARY KEY, "
                           "DocumentId INTEGER, ExtractId INTEGER, BeginParagraphOrdinal INTEGER, "
                           "EndParagraphOrdinal INTEGER, SortPosition INTEGER)"),
            QStringLiteral("INSERT INTO DocumentExtract VALUES "
                           "(1, 1, 1, 3, 3, 10), "
                           "(2, 1, 2, 26, 26, 200), "
                           "(3, 1, 3, 47, 47, 400)"),
        });

    kmedia::PublicationDatabase db;
    REQUIRE(db.open(dbPath));
    kmedia::PublicationDatabase::MediaSelector selector;
    selector.documentId = 1;
    const auto rows = db.documentMultimedia(selector, true);
    REQUIRE(rows.size() == 5);
    // Songs adopt extract ordinals (3, 26, 47) while art keeps its exact paragraph ordinals (9, 33)
    // instead of shifting to 4 and 27.
    REQUIRE(rows[0].beginParagraphOrdinal == 3);
    REQUIRE(rows[1].beginParagraphOrdinal == 9);
    REQUIRE(rows[2].beginParagraphOrdinal == 26);
    REQUIRE(rows[3].beginParagraphOrdinal == 33);
    REQUIRE(rows[4].beginParagraphOrdinal == 47);

    std::vector<PublicationMediaRow> htmlRows = {
        mediaRow(90, 42),
    };
    htmlRows[0].extractCaption = QStringLiteral(
        "<span class=\"eloc\">w25.11 p. 20</span> <span class=\"etitle\">Truth</span>");
    kmedia::MeetingMediaContext ctx;
    ctx.date = QDate(2026, 11, 25);
    ctx.isMidweek = true;
    const auto items = kmedia::MeetingMediaBuilder::buildDay(htmlRows, ctx);
    REQUIRE(items.size() == 1);
    REQUIRE(items[0].groupKey == QStringLiteral("w25.11 p. 20 - Truth"));
}
