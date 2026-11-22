#include "TestZipFixtures.h"
#include "core/DatabaseManager.h"
#include "core/NetworkManager.h"
#include "media/VideoFrameExtractor.h"
#include "network/CatalogClient.h"
#include "network/MeetingSyncEngine.h"
#include "network/PublicationStore.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

namespace {

using kmedia::MeetingSyncEngine;

void writePublication(const QTemporaryDir& root, const QString& key, const QStringList& statements,
                      const QStringList& assetNames)
{
    const QByteArray dbBytes = kmedia::test_helpers::buildSqliteBytes(root.path(), statements);
    REQUIRE_FALSE(dbBytes.isEmpty());
    std::vector<std::pair<QString, QByteArray>> assets;
    for (const QString& name : assetNames) {
        assets.emplace_back(name, name.toUtf8());
    }
    const QString folder = QDir(root.path()).filePath(QStringLiteral("Publications/") + key);
    REQUIRE(QDir().mkpath(folder));
    REQUIRE(kmedia::test_helpers::writeJwpubFixture(
        QDir(folder).filePath(key + QStringLiteral(".jwpub")), root.path(), QByteArrayLiteral("{}"),
        dbBytes, key + QStringLiteral(".db"), assets));
}

QStringList mediaTables()
{
    return {
        QStringLiteral("CREATE TABLE Document (DocumentId INTEGER PRIMARY KEY, MepsDocumentId "
                       "INTEGER, Class INTEGER, Title TEXT)"),
        QStringLiteral("CREATE TABLE DatedText (DatedTextId INTEGER PRIMARY KEY, DocumentId "
                       "INTEGER, FirstDateOffset INTEGER, LastDateOffset INTEGER)"),
        QStringLiteral("CREATE TABLE Multimedia (MultimediaId INTEGER PRIMARY KEY, KeySymbol TEXT, "
                       "Track INTEGER, IssueTagNumber INTEGER, MepsLanguageIndex INTEGER, MimeType "
                       "TEXT, CategoryType INTEGER, Label TEXT, Caption TEXT, FilePath TEXT)"),
        QStringLiteral("CREATE TABLE DocumentMultimedia (DocumentMultimediaId INTEGER PRIMARY "
                       "KEY, DocumentId INTEGER, MultimediaId INTEGER, BeginParagraphOrdinal "
                       "INTEGER, EndParagraphOrdinal INTEGER)"),
        QStringLiteral("CREATE TABLE DocumentParagraph (DocumentParagraphId INTEGER PRIMARY KEY, "
                       "DocumentId INTEGER, ParagraphIndex INTEGER, ParagraphNumberLabel TEXT, "
                       "BeginPosition INTEGER, EndPosition INTEGER)"),
    };
}

void configureEngine(MeetingSyncEngine& engine, kmedia::DatabaseManager& db, const QDate& today)
{
    engine.setDatabase(&db);
    engine.setToday(today);
    MeetingSyncEngine::Settings settings;
    settings.languageCode = QStringLiteral("E");
    engine.setSettings(settings);
    engine.setScheduleProviders(
        [](const QDate& date) {
            if (date.dayOfWeek() == 3) {
                return QStringLiteral("midweek");
            }
            return date.dayOfWeek() == 7 ? QStringLiteral("weekend") : QStringLiteral("custom");
        },
        [](const QDate&) { return false; });
}

QStringList sectionIds(const std::vector<kmedia::PlaylistItemRow>& rows)
{
    QStringList ids;
    for (const auto& row : rows) {
        ids << row.entry.sectionId;
    }
    return ids;
}

} // namespace

TEST_CASE("MeetingSyncEngine computes the meeting window and publication issues", "[sync][engine]")
{
    const auto window = MeetingSyncEngine::lookupWindow(QDate(2026, 11, 25), false);
    REQUIRE(window.front() == QDate(2026, 11, 23));
    REQUIRE(window.size() == 38);
    REQUIRE(MeetingSyncEngine::lookupWindow(QDate(2026, 11, 29), false).size() == 35);
    REQUIRE(MeetingSyncEngine::lookupWindow(QDate(2026, 11, 25), true).size() == 4);

    REQUIRE(MeetingSyncEngine::midweekIssueTag(QDate(2026, 11, 23)) == 202611);
    REQUIRE(MeetingSyncEngine::midweekIssueTag(QDate(2026, 12, 7)) == 202611);
    REQUIRE(MeetingSyncEngine::midweekIssueTag(QDate(2027, 1, 4)) == 202701);
    REQUIRE(MeetingSyncEngine::watchtowerIssueTags(QDate(2026, 11, 23)) ==
            std::vector<int>{202610, 202609, 202608});

    REQUIRE(MeetingSyncEngine::extractPublicationSymbol(QStringLiteral("lmd"), 0) ==
            QStringLiteral("lmd"));
    REQUIRE(MeetingSyncEngine::extractPublicationSymbol(QStringLiteral("mwbr26"), 20261100) ==
            QStringLiteral("mwbr"));
    REQUIRE(MeetingSyncEngine::extractPublicationSymbol(QStringLiteral("w"), 20080101) ==
            QStringLiteral("wp"));
    REQUIRE(MeetingSyncEngine::extractPublicationSymbol(QStringLiteral("w"), 20080115) ==
            QStringLiteral("w"));
    REQUIRE(MeetingSyncEngine::extractPublicationSymbol(QStringLiteral("nwtsty-E"), 0) ==
            QStringLiteral("nwtsty-E"));
}

TEST_CASE("MeetingSyncEngine syncs midweek days into their own workspaces and keeps edits",
          "[sync][engine][midweek]")
{
    const QTemporaryDir root;
    REQUIRE(root.isValid());
    QStringList statements = mediaTables();
    statements << QStringLiteral("INSERT INTO Document VALUES (1, 202026401, 106, 'Nov 23-29')")
               << QStringLiteral("INSERT INTO DatedText VALUES (1, 1, 20261123, 20261129)")
               << QStringLiteral(
                      "INSERT INTO Multimedia VALUES "
                      "(10, 'sjjm', 12, 0, 0, 'video/mp4', -1, '12. Opening', '', 'song12.mp4'), "
                      "(11, NULL, 0, 0, 0, 'image/jpeg', 8, 'Treasures', '', 'tgw.jpg'), "
                      "(16, NULL, 0, 0, 0, 'image/jpeg', 8, 'Ministry', '', 'ayfm.jpg'), "
                      "(15, 'sjjm', 34, 0, 0, 'video/mp4', -1, '34. Middle', '', 'song34.mp4'), "
                      "(17, NULL, 0, 0, 0, 'image/jpeg', 8, 'Study', '', 'cbs.jpg'), "
                      "(18, 'sjjm', 56, 0, 0, 'video/mp4', -1, '56. Closing', '', 'song56.mp4')")
               << QStringLiteral("INSERT INTO DocumentMultimedia VALUES (1, 1, 10, 1, 1), "
                                 "(2, 1, 11, 4, 4), (3, 1, 16, 20, 20), (4, 1, 15, 30, 30), "
                                 "(5, 1, 17, 40, 40), (6, 1, 18, 42, 42)");
    writePublication(root, QStringLiteral("mwb_E_202611"), statements,
                     {QStringLiteral("song12.mp4"), QStringLiteral("tgw.jpg"),
                      QStringLiteral("ayfm.jpg"), QStringLiteral("song34.mp4"),
                      QStringLiteral("cbs.jpg"), QStringLiteral("song56.mp4")});

    kmedia::DatabaseManager db;
    REQUIRE(db.open(root.filePath(QStringLiteral("profile.db"))));
    kmedia::PublicationStore store(nullptr, nullptr);
    store.setCacheRoot(root.path());
    MeetingSyncEngine engine(&store);
    configureEngine(engine, db, QDate(2026, 11, 23));
    QSignalSpy idle(&engine, &MeetingSyncEngine::syncIdle);

    engine.refresh();
    REQUIRE(idle.wait(15000));
    REQUIRE(engine.statusForDate(QStringLiteral("2026-11-25")) == QStringLiteral("complete"));
    // Later weeks are not in this workbook; without a network they end as errors.
    REQUIRE(engine.statusForDate(QStringLiteral("2026-12-02")) == QStringLiteral("error"));

    const auto playlist =
        db.findPlaylistByDateAndType(QStringLiteral("2026-11-25"), QStringLiteral("midweek"));
    REQUIRE(playlist.has_value());
    REQUIRE(playlist->syncStatus == QStringLiteral("complete"));
    auto rows = db.listPlaylistItemRows(playlist->id);
    REQUIRE(rows.size() == 6);
    REQUIRE(sectionIds(rows) == QStringList{QStringLiteral("tgw"), QStringLiteral("tgw"),
                                            QStringLiteral("ayfm"), QStringLiteral("lac"),
                                            QStringLiteral("lac"), QStringLiteral("lac")});
    REQUIRE(rows[0].entry.source == QStringLiteral("dynamic"));
    REQUIRE(rows[0].entry.tagType == QStringLiteral("song"));
    REQUIRE(rows[0].entry.tagValue == QStringLiteral("12"));
    REQUIRE(rows[0].media.has_value());
    REQUIRE(QFileInfo::exists(rows[0].media->filePath));
    REQUIRE(rows[4].entry.isCbs);
    const QJsonArray sections = QJsonDocument::fromJson(playlist->sectionsJson.toUtf8()).array();
    REQUIRE(sections.size() == 3);
    REQUIRE(sections.at(0).toObject().value(QStringLiteral("id")).toString() ==
            QStringLiteral("tgw"));

    // The operator hides a picture and adds a file of their own.
    kmedia::PlaylistEntryRecord hidden = rows[1].entry;
    hidden.hidden = true;
    REQUIRE(db.updatePlaylistEntryFull(hidden));
    kmedia::MediaItemRecord ownMedia;
    ownMedia.filePath = root.filePath(QStringLiteral("own.png"));
    kmedia::PlaylistEntryRecord own;
    own.playlistId = playlist->id;
    own.mediaItemId = db.upsertMediaItem(ownMedia);
    own.sectionId = QStringLiteral("ayfm");
    own.customTitle = QStringLiteral("Own picture");
    own.sortOrder = 99;
    REQUIRE(db.addPlaylistEntry(own) > 0);

    // A re-sync neither duplicates media nor undoes the operator's changes.
    engine.resyncDay(QStringLiteral("2026-11-25"));
    REQUIRE(idle.wait(15000));
    rows = db.listPlaylistItemRows(playlist->id);
    REQUIRE(rows.size() == 7);
    REQUIRE(rows[1].entry.hidden);
    REQUIRE(rows.back().entry.customTitle == QStringLiteral("Own picture"));
    REQUIRE(rows.back().entry.source == QStringLiteral("additional"));
    REQUIRE(engine.statusForDate(QStringLiteral("2026-11-25")) == QStringLiteral("complete"));
}

TEST_CASE("MeetingSyncEngine finds the weekend study in the issue that holds the week",
          "[sync][engine][weekend]")
{
    const QTemporaryDir root;
    REQUIRE(root.isValid());
    QStringList statements = mediaTables();
    statements << QStringLiteral("INSERT INTO Document VALUES (1, 2026500, 13, 'Cover'), "
                                 "(2, 2026501, 40, 'Earlier Article'), "
                                 "(3, 2026502, 40, 'Keep Your Joy')")
               << QStringLiteral("INSERT INTO DatedText VALUES (1, 2, 20261116, 20261122), "
                                 "(2, 3, 20261123, 20261129)")
               << QStringLiteral(
                      "INSERT INTO Multimedia VALUES "
                      "(30, 'sjjm', 20, 0, 0, 'video/mp4', -1, '20. Opening', '', 'song20.mp4'), "
                      "(31, NULL, 0, 0, 0, 'image/jpeg', 8, 'Lead', '', 'lead.jpg'), "
                      "(33, 'sjjm', 21, 0, 0, 'video/mp4', -1, '21. Closing', '', 'song21.mp4')")
               << QStringLiteral("INSERT INTO DocumentMultimedia VALUES (1, 3, 30, 1, 1), "
                                 "(2, 3, 31, 2, 2), (3, 3, 33, 9, 9)")
               << QStringLiteral("INSERT INTO DocumentParagraph VALUES (1, 3, 1, NULL, 5, 9), "
                                 "(2, 3, 2, '1', 10, 19), (3, 3, 9, NULL, 90, 99)");
    // The 6-week candidate (202610) holds the week; the other candidates are absent.
    writePublication(
        root, QStringLiteral("w_E_202610"), statements,
        {QStringLiteral("song20.mp4"), QStringLiteral("lead.jpg"), QStringLiteral("song21.mp4")});

    kmedia::DatabaseManager db;
    REQUIRE(db.open(root.filePath(QStringLiteral("profile.db"))));
    kmedia::PublicationStore store(nullptr, nullptr);
    store.setCacheRoot(root.path());
    MeetingSyncEngine engine(&store);
    configureEngine(engine, db, QDate(2026, 11, 23));
    QSignalSpy idle(&engine, &MeetingSyncEngine::syncIdle);

    engine.refresh();
    REQUIRE(idle.wait(15000));
    REQUIRE(engine.statusForDate(QStringLiteral("2026-11-29")) == QStringLiteral("complete"));
    const auto playlist =
        db.findPlaylistByDateAndType(QStringLiteral("2026-11-29"), QStringLiteral("weekend"));
    REQUIRE(playlist.has_value());
    const auto rows = db.listPlaylistItemRows(playlist->id);
    REQUIRE(rows.size() == 3);
    for (const auto& row : rows) {
        REQUIRE(row.entry.sectionId == QStringLiteral("wt"));
    }
    REQUIRE(rows.front().entry.tagValue == QStringLiteral("20"));
    REQUIRE(rows.back().entry.tagValue == QStringLiteral("21"));
    REQUIRE(playlist->sectionsJson.contains(QStringLiteral("Keep Your Joy")));
}

TEST_CASE("MeetingSyncEngine syncs this week's meetings from JW.org", "[.network][sync]")
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
    store.setMaxResolution(240);
    MeetingSyncEngine engine(&store);
    engine.setDatabase(&db);
    MeetingSyncEngine::Settings settings;
    settings.languageCode = language;
    settings.meteredConnection = true;
    engine.setSettings(settings);
    engine.setScheduleProviders(
        [](const QDate& date) {
            if (date.dayOfWeek() == 3) {
                return QStringLiteral("midweek");
            }
            return date.dayOfWeek() == 7 ? QStringLiteral("weekend") : QStringLiteral("custom");
        },
        [](const QDate&) { return false; });

    QSignalSpy idle(&engine, &MeetingSyncEngine::syncIdle);
    engine.refresh();
    REQUIRE(idle.wait(10 * 60 * 1000));

    // The public talk song is added by number, like the operator does before the meeting.
    QSignalSpy songs(&engine, &MeetingSyncEngine::songAdded);
    engine.addSong(QStringLiteral("2026-11-22"), 151, QStringLiteral("pt"));
    REQUIRE(songs.wait(60 * 1000));
    QSignalSpy counts(&downloads, &kmedia::DownloadService::countsChanged);
    while (downloads.activeCount() + downloads.queuedCount() > 0) {
        REQUIRE(counts.wait(10 * 60 * 1000));
    }

    for (const auto& playlist : db.listPlaylists()) {
        const auto rows = db.listPlaylistItemRows(playlist.id);
        int missing = 0;
        QStringList lines;
        for (const auto& row : rows) {
            const bool hasFile = row.media.has_value() && QFileInfo::exists(row.media->filePath);
            if (!hasFile && row.entry.streamUrl.isEmpty()) {
                ++missing;
            }
            const bool video =
                row.media.has_value() && row.media->filePath.endsWith(QStringLiteral(".mp4"));
            if (video) {
                // Synced videos bring their preview picture along, or show a frame of their own.
                INFO(row.media->filePath.toStdString());
                CHECK((QFileInfo::exists(row.entry.thumbnailPath) ||
                       !kmedia::VideoFrameExtractor::extract(row.media->filePath).isNull()));
            }
            lines << QStringLiteral("  [%1] %2 %3%4 %5%6")
                         .arg(row.entry.sectionId, row.entry.tagType, row.entry.tagValue,
                              row.entry.hidden ? QStringLiteral(" (hidden)") : QString(),
                              video ? QStringLiteral("(video) ") : QString(),
                              row.entry.customTitle.left(60));
        }
        WARN(QStringLiteral("%1 %2: %3, %4 items, %5 missing\n%6")
                 .arg(playlist.meetingDate, playlist.meetingType, playlist.syncStatus)
                 .arg(rows.size())
                 .arg(missing)
                 .arg(lines.join(QLatin1Char('\n')))
                 .toStdString());
        CHECK(playlist.syncStatus == QStringLiteral("complete"));
        if (playlist.meetingDate == QStringLiteral("2026-11-22")) {
            const bool hasSong = std::any_of(rows.cbegin(), rows.cend(), [](const auto& row) {
                return row.entry.sectionId == QStringLiteral("pt") &&
                       row.entry.tagValue == QStringLiteral("151") &&
                       row.entry.source == QStringLiteral("additional");
            });
            CHECK(hasSong);
        }
        CHECK(missing == 0);
        CHECK_FALSE(rows.empty());
    }
}

TEST_CASE("MeetingSyncEngine marks days that hold added media", "[sync][engine][calendar]")
{
    const QTemporaryDir root;
    REQUIRE(root.isValid());
    kmedia::DatabaseManager db;
    REQUIRE(db.open(root.filePath(QStringLiteral("profile.db"))));

    const auto addItem = [&db](const QString& dateIso, const QString& kind, const QString& source) {
        const kmedia::PlaylistRecord playlist =
            db.findOrCreatePlaylistForDate(dateIso, kind, dateIso);
        kmedia::PlaylistEntryRecord entry;
        entry.playlistId = playlist.id;
        entry.customTitle = QStringLiteral("Item");
        entry.streamUrl = QStringLiteral("https://example.org/item.mp4");
        entry.source = source;
        REQUIRE(db.addPlaylistEntry(entry) > 0);
        return playlist.id;
    };
    addItem(QStringLiteral("2026-11-20"), QStringLiteral("custom"), QStringLiteral("additional"));
    const qint64 midweek = addItem(QStringLiteral("2026-11-18"), QStringLiteral("midweek"),
                                   QStringLiteral("additional"));
    addItem(QStringLiteral("2026-11-22"), QStringLiteral("weekend"), QStringLiteral("dynamic"));
    REQUIRE(db.setPlaylistSyncStatus(midweek, QStringLiteral("complete")));

    kmedia::PublicationStore store(nullptr, nullptr);
    MeetingSyncEngine engine(&store);
    engine.setToday(QDate(2026, 11, 18));
    engine.setDatabase(&db);
    engine.setScheduleProviders(
        [](const QDate& date) {
            if (date.dayOfWeek() == 3) {
                return QStringLiteral("midweek");
            }
            return date.dayOfWeek() == 7 ? QStringLiteral("weekend") : QStringLiteral("custom");
        },
        [](const QDate&) { return false; });
    engine.refresh();

    const QVariantMap statuses = engine.dayStatuses();
    REQUIRE(statuses.value(QStringLiteral("2026-11-20")).toString() == QStringLiteral("media"));
    // A finished meeting keeps its own color even when media was added to it.
    REQUIRE(statuses.value(QStringLiteral("2026-11-18")).toString() == QStringLiteral("complete"));
    REQUIRE(statuses.value(QStringLiteral("2026-11-22")).toString() != QStringLiteral("media"));
    REQUIRE(engine.windowStart() == QStringLiteral("2026-11-16"));
    REQUIRE(engine.windowEnd() >= QStringLiteral("2026-12-21"));

    // Previous and next skip the days without a meeting or media, inside the window.
    REQUIRE(engine.dayWithContent(QStringLiteral("2026-11-18"), 0) == QStringLiteral("2026-11-18"));
    REQUIRE(engine.dayWithContent(QStringLiteral("2026-11-19"), 0) == QStringLiteral("2026-11-20"));
    REQUIRE(engine.dayWithContent(QStringLiteral("2026-11-18"), 1) == QStringLiteral("2026-11-20"));
    REQUIRE(engine.dayWithContent(QStringLiteral("2026-11-20"), 1) == QStringLiteral("2026-11-22"));
    REQUIRE(engine.dayWithContent(QStringLiteral("2026-11-20"), -1) ==
            QStringLiteral("2026-11-18"));
    REQUIRE(engine.dayWithContent(QStringLiteral("2026-11-18"), -1).isEmpty());
    REQUIRE(engine.dayWithContent(QStringLiteral("2026-11-01"), 0) == QStringLiteral("2026-11-18"));
}
