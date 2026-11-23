#include "core/DatabaseManager.h"
#include "core/NetworkManager.h"
#include "network/BibleMediaCatalog.h"
#include "network/CatalogClient.h"
#include "network/DownloadService.h"
#include "network/PublicationStore.h"
#include "network/StudyBibleMedia.h"
#include "network/YeartextProvider.h"

#include <QDate>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Bible book recordings come with their name and verse markers", "[bible]")
{
    const QByteArray json = R"({
        "pubName": "Jo&nbsp;\u00e3o", "booknum": 43, "pub": "nwt", "issue": "",
        "files": {"T": {"MP3": [
            {"title": "Cap\u00edtulo 3", "track": 3, "booknum": 43, "duration": 300.5,
             "filesize": 4000, "file": {"url": "https://cdn.example/nwt_43_Joh_T_03.mp3",
                                        "checksum": "abc"},
             "markers": {"markers": [
                 {"verseNumber": 16, "startTime": "00:02:10.500", "duration": "00:00:07.250"},
                 {"verseNumber": 17, "startTime": "00:02:17.750", "duration": "00:00:05.000"},
                 {"verseNumber": 18, "startTime": "00:02:22.750", "duration": "00:00:09.100"}]}},
            {"title": "Introdu\u00e7\u00e3o", "track": 0, "file": {"url": "https://cdn.example/intro.mp3"}}
        ]}}})";

    const kmedia::JwBibleBook book =
        kmedia::CatalogClient::parseBibleBookJson(json, QStringLiteral("T"));
    REQUIRE(book.number == 43);
    REQUIRE(book.name == QStringLiteral("Jo \u00e3o"));
    REQUIRE(book.chapters.size() == 1);
    const kmedia::CatalogMediaAsset& chapter = book.chapters.front();
    REQUIRE(chapter.track == 3);
    REQUIRE(chapter.verseMarkers.size() == 3);
    REQUIRE(chapter.verseMarkers.front().startMs == 130500);
    REQUIRE(chapter.verseMarkers.front().durationMs == 7250);

    // From the start of the first verse to the end of the last, in either order.
    REQUIRE(kmedia::BibleMediaCatalog::verseRange(chapter, 16, 18) ==
            std::pair<qint64, qint64>{130500, 151850});
    REQUIRE(kmedia::BibleMediaCatalog::verseRange(chapter, 17, 17) ==
            std::pair<qint64, qint64>{137750, 142750});
    REQUIRE(kmedia::BibleMediaCatalog::verseRange(chapter, 18, 16) ==
            std::pair<qint64, qint64>{130500, 151850});
    REQUIRE(kmedia::BibleMediaCatalog::verseRange(chapter, 16, 40) ==
            std::pair<qint64, qint64>{0, 0});

    const QUrlQuery query(
        kmedia::CatalogClient::bibleBookUrl(QStringLiteral("ASL"), 43, QStringLiteral("MP4")));
    REQUIRE(query.queryItemValue(QStringLiteral("pub")) == QStringLiteral("nwt"));
    REQUIRE(query.queryItemValue(QStringLiteral("booknum")) == QStringLiteral("43"));
    REQUIRE(query.queryItemValue(QStringLiteral("fileformat")) == QStringLiteral("MP4"));
}

TEST_CASE("BibleMediaCatalog lists the Bible books from JW.org", "[.network][bible]")
{
    const QTemporaryDir root;
    REQUIRE(root.isValid());
    const QString language = qEnvironmentVariable("KMEDIA_NETWORK_LANG", QStringLiteral("E"));
    kmedia::DatabaseManager db;
    REQUIRE(db.open(root.filePath(QStringLiteral("profile.db"))));
    kmedia::NetworkManager network;
    kmedia::CatalogClient catalog(&network, &db);
    kmedia::BibleMediaCatalog bible(&catalog, nullptr, nullptr);
    bible.setLanguage(language, false);

    bible.load();
    REQUIRE(bible.isBusy());
    REQUIRE(QTest::qWaitFor([&bible]() { return !bible.isBusy(); }, 3 * 60 * 1000));
    REQUIRE(bible.books().size() == kmedia::BibleMediaCatalog::kBookCount);
    const QList<int> verses = bible.verses(43, 3);
    REQUIRE(verses.size() == 36);
    REQUIRE(verses.front() == 1);
    REQUIRE(bible.reference(43, 3, 16, 18).endsWith(QStringLiteral(" 3:16-18")));
}

TEST_CASE("StudyBibleMedia opens the Study Bible from JW.org", "[.network][bible]")
{
    const QTemporaryDir root;
    REQUIRE(root.isValid());
    const QString language = qEnvironmentVariable("KMEDIA_NETWORK_LANG", QStringLiteral("E"));
    kmedia::DatabaseManager db;
    REQUIRE(db.open(root.filePath(QStringLiteral("profile.db"))));
    kmedia::NetworkManager network;
    kmedia::CatalogClient catalog(&network, &db);
    kmedia::DownloadService downloads(&network, &db);
    kmedia::PublicationStore store(&catalog, &downloads);
    store.setCacheRoot(root.path());
    kmedia::StudyBibleMedia studyBible(&store, nullptr);
    studyBible.setLanguages({language});

    QSignalSpy books(&studyBible, &kmedia::StudyBibleMedia::booksChanged);
    studyBible.open();
    REQUIRE(books.wait(10 * 60 * 1000));
    REQUIRE(studyBible.books().size() >= 20);
    const QVariantList media = studyBible.chapterMedia(40, 4);
    REQUIRE(media.size() >= 5);
    const auto hasVideo = std::any_of(media.begin(), media.end(), [](const QVariant& item) {
        return item.toMap().value(QStringLiteral("isVideo")).toBool();
    });
    REQUIRE(hasVideo);
    REQUIRE(media.front()
                .toMap()
                .value(QStringLiteral("verse"))
                .toString()
                .startsWith(QStringLiteral("4:")));
}

TEST_CASE("YeartextProvider fetches this year's yeartext from JW.org", "[.network][yeartext]")
{
    const QTemporaryDir root;
    REQUIRE(root.isValid());
    const QString language = qEnvironmentVariable("KMEDIA_NETWORK_LANG", QStringLiteral("E"));
    kmedia::DatabaseManager db;
    REQUIRE(db.open(root.filePath(QStringLiteral("profile.db"))));
    kmedia::NetworkManager network;
    kmedia::CatalogClient catalog(&network, &db);
    kmedia::YeartextProvider yeartext(&catalog);

    QSignalSpy changed(&yeartext, &kmedia::YeartextProvider::textChanged);
    yeartext.setLanguages({language}, false, QDate::currentDate().year());
    REQUIRE(changed.wait(60 * 1000));
    REQUIRE(yeartext.text().contains(QLatin1Char('\n')));

    // Sign languages show none.
    yeartext.setLanguages({language}, true, QDate::currentDate().year());
    REQUIRE(yeartext.text().isEmpty());
}
