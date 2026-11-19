#include "core/DatabaseManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DatabaseManager opens SQLite in WAL mode and runs v2 migrations", "[database][core]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString dbPath = tempDir.filePath(QStringLiteral("nested/dir/kmedia.db"));
    kmedia::DatabaseManager db;
    REQUIRE(db.open(dbPath));
    REQUIRE(db.isOpen());
    REQUIRE(QFileInfo::exists(dbPath));
    REQUIRE(db.schemaVersion() == 2);
    REQUIRE(db.pragmaValue(QStringLiteral("journal_mode")).toLower() == QStringLiteral("wal"));
    REQUIRE(db.pragmaValue(QStringLiteral("foreign_keys")) == QStringLiteral("1"));

    db.close();
    REQUIRE_FALSE(db.isOpen());

    // Reopening an existing database preserves schema version without reapplying migrations
    REQUIRE(db.open(dbPath));
    REQUIRE(db.schemaVersion() == 2);
}

TEST_CASE("DatabaseManager supports date-keyed playlists and v2 entry metadata", "[database][core]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    kmedia::DatabaseManager db;
    REQUIRE(db.open(tempDir.filePath(QStringLiteral("v2.db"))));

    const kmedia::PlaylistRecord wednesday =
        db.findOrCreatePlaylistForDate(QStringLiteral("2026-11-25"), QStringLiteral("midweek"),
                                       QStringLiteral("Nov 25 | Midweek"));
    REQUIRE(wednesday.id > 0);
    const kmedia::PlaylistRecord sameWednesday = db.findOrCreatePlaylistForDate(
        QStringLiteral("2026-11-25"), QStringLiteral("midweek"), QStringLiteral("Ignored"));
    REQUIRE(sameWednesday.id == wednesday.id);

    kmedia::PlaylistEntryRecord divider;
    divider.playlistId = wednesday.id;
    divider.sectionTitle = QStringLiteral("Treasures From God's Word");
    divider.customTitle = QStringLiteral("Opening Transition");
    divider.itemKind = QStringLiteral("divider");
    divider.dividerColor = QStringLiteral("#C96442");
    divider.groupKey = QStringLiteral("grp_treasures");
    divider.hidden = true;
    divider.tagType = QStringLiteral("para");
    divider.tagValue = QStringLiteral("Para. 3");
    divider.pubMediaId = QStringLiteral("pub-mwb_E_202611_VIDEO_1");
    divider.streamUrl = QStringLiteral("https://cdn.example/video.mp4");
    divider.playbackRate = 1.5;
    divider.isCbs = true;
    divider.originalSection = QStringLiteral("Living as Christians");
    divider.sortOrderOriginal = 4;
    divider.slideIntervalMs = 30000;
    divider.pairedAudioPath = QStringLiteral("/tmp/song.mp3");

    const qint64 divId = db.addPlaylistEntry(divider);
    REQUIRE(divId > 0);

    const auto loaded = db.listPlaylistEntries(wednesday.id);
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].itemKind == QStringLiteral("divider"));
    REQUIRE(loaded[0].dividerColor == QStringLiteral("#C96442"));
    REQUIRE(loaded[0].groupKey == QStringLiteral("grp_treasures"));
    REQUIRE(loaded[0].hidden);
    REQUIRE(loaded[0].tagType == QStringLiteral("para"));
    REQUIRE(loaded[0].tagValue == QStringLiteral("Para. 3"));
    REQUIRE(loaded[0].pubMediaId == QStringLiteral("pub-mwb_E_202611_VIDEO_1"));
    REQUIRE(loaded[0].streamUrl == QStringLiteral("https://cdn.example/video.mp4"));
    REQUIRE(loaded[0].playbackRate == 1.5);
    REQUIRE(loaded[0].isCbs);
    REQUIRE(loaded[0].originalSection == QStringLiteral("Living as Christians"));
    REQUIRE(loaded[0].sortOrderOriginal == 4);
    REQUIRE(loaded[0].slideIntervalMs == 30000);
    REQUIRE(loaded[0].pairedAudioPath == QStringLiteral("/tmp/song.mp3"));

    kmedia::PlaylistEntryRecord updated = loaded[0];
    updated.hidden = false;
    updated.tagValue = QStringLiteral("Para. 5");
    REQUIRE(db.updatePlaylistEntryFull(updated));
    const auto reloaded = db.listPlaylistEntries(wednesday.id);
    REQUIRE_FALSE(reloaded[0].hidden);
    REQUIRE(reloaded[0].tagValue == QStringLiteral("Para. 5"));
    REQUIRE(db.deletePlaylistEntry(divId, wednesday.id));
    REQUIRE(db.listPlaylistEntries(wednesday.id).empty());
}

TEST_CASE("DatabaseManager supports media, playlist, cascade, and transaction rollback",
          "[database][core]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    kmedia::DatabaseManager db;
    REQUIRE(db.open(tempDir.filePath(QStringLiteral("store.db"))));

    kmedia::MediaItemRecord media;
    media.filePath = tempDir.filePath(QStringLiteral("videos/talk.mp4"));
    media.sha256 = QStringLiteral("abc123");
    media.title = QStringLiteral("Weekend Video");
    media.mediaType = QStringLiteral("video");
    media.languageCode = QStringLiteral("E");
    media.durationMs = 125000;
    media.width = 1920;
    media.height = 1080;
    media.fileSize = 4096;

    const qint64 mediaId = db.upsertMediaItem(media);
    REQUIRE(mediaId > 0);

    media.title = QStringLiteral("Updated Weekend Video");
    const qint64 sameMediaId = db.upsertMediaItem(media);
    REQUIRE(sameMediaId == mediaId);

    const auto loadedMedia = db.findMediaItemById(mediaId);
    REQUIRE(loadedMedia.has_value());
    REQUIRE(loadedMedia->title == QStringLiteral("Updated Weekend Video"));

    kmedia::PlaylistRecord playlist;
    playlist.name = QStringLiteral("Midweek Meeting");
    playlist.meetingDate = QStringLiteral("2026-11-18");
    const qint64 playlistId = db.createPlaylist(playlist);
    REQUIRE(playlistId > 0);

    kmedia::PlaylistEntryRecord firstEntry;
    firstEntry.playlistId = playlistId;
    firstEntry.mediaItemId = mediaId;
    firstEntry.sectionTitle = QStringLiteral("Treasures");
    firstEntry.sortOrder = 0;
    firstEntry.trimStartMs = 1000;
    firstEntry.trimEndMs = 60000;

    kmedia::PlaylistEntryRecord secondEntry;
    secondEntry.playlistId = playlistId;
    secondEntry.sectionTitle = QStringLiteral("Living");
    secondEntry.customTitle = QStringLiteral("Song 1");
    secondEntry.sortOrder = 1;

    const qint64 entry1 = db.addPlaylistEntry(firstEntry);
    const qint64 entry2 = db.addPlaylistEntry(secondEntry);
    REQUIRE(entry1 > 0);
    REQUIRE(entry2 > 0);

    REQUIRE(db.reorderPlaylistEntries(playlistId, {entry2, entry1}));
    const auto entries = db.listPlaylistEntries(playlistId);
    REQUIRE(entries.size() == 2);
    REQUIRE(entries[0].id == entry2);
    REQUIRE(entries[1].id == entry1);
    REQUIRE(entries[1].trimStartMs == 1000);

    // Transaction rollback must discard partial writes
    REQUIRE_FALSE(db.executeTransaction([&](QSqlDatabase& handle) {
        QSqlQuery q(handle);
        q.prepare(
            QStringLiteral("INSERT INTO settings (key, value, updated_at) VALUES (:k, :v, 'now')"));
        q.bindValue(QStringLiteral(":k"), QStringLiteral("rollback_probe"));
        q.bindValue(QStringLiteral(":v"), QStringLiteral("dirty"));
        REQUIRE(q.exec());
        return false;
    }));
    REQUIRE_FALSE(db.getSetting(QStringLiteral("rollback_probe")).has_value());

    REQUIRE(db.deletePlaylist(playlistId));
    REQUIRE(db.listPlaylistEntries(playlistId).empty());

    kmedia::DownloadCacheRecord cacheRec;
    cacheRec.url = QStringLiteral("https://example.org/catalog.json");
    cacheRec.etag = QStringLiteral("\"v1\"");
    REQUIRE(db.upsertDownloadCache(cacheRec));
    const auto loadedCache = db.findDownloadCache(cacheRec.url);
    REQUIRE(loadedCache.has_value());
    REQUIRE(loadedCache->etag == QStringLiteral("\"v1\""));
    REQUIRE(loadedCache->lastModified.isEmpty());
}
