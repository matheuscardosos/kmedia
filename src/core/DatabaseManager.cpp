#include "core/DatabaseManager.h"

#include "core/MeetingSections.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <atomic>

namespace kmedia {

namespace {

std::atomic<quint64> g_connectionCounter{0};

constexpr int kLatestSchemaVersion = 3;

QString utcIsoTimestamp()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString nonNullText(const QString& value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

MediaItemRecord readMediaItemRow(const QSqlQuery& query)
{
    MediaItemRecord record;
    record.id = query.value(0).toLongLong();
    record.filePath = query.value(1).toString();
    record.sha256 = query.value(2).toString();
    record.title = query.value(3).toString();
    record.mediaType = query.value(4).toString();
    record.languageCode = query.value(5).toString();
    record.durationMs = query.value(6).toLongLong();
    record.width = query.value(7).toInt();
    record.height = query.value(8).toInt();
    record.fileSize = query.value(9).toLongLong();
    record.sourceKind = query.value(10).toString();
    return record;
}

bool applyMigrationV1(QSqlDatabase& db, QString& errorOut)
{
    static constexpr const char* kStatements[] = {
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "version INTEGER PRIMARY KEY, "
        "applied_at TEXT NOT NULL"
        ")",
        "CREATE TABLE IF NOT EXISTS settings ("
        "key TEXT PRIMARY KEY, "
        "value TEXT NOT NULL, "
        "updated_at TEXT NOT NULL"
        ")",
        "CREATE TABLE IF NOT EXISTS media_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "file_path TEXT NOT NULL UNIQUE, "
        "sha256 TEXT NOT NULL DEFAULT '', "
        "title TEXT NOT NULL DEFAULT '', "
        "media_type TEXT NOT NULL DEFAULT 'unknown', "
        "language_code TEXT NOT NULL DEFAULT '', "
        "duration_ms INTEGER NOT NULL DEFAULT 0, "
        "width INTEGER NOT NULL DEFAULT 0, "
        "height INTEGER NOT NULL DEFAULT 0, "
        "file_size INTEGER NOT NULL DEFAULT 0, "
        "source_kind TEXT NOT NULL DEFAULT 'local', "
        "created_at TEXT NOT NULL, "
        "updated_at TEXT NOT NULL"
        ")",
        "CREATE TABLE IF NOT EXISTS playlists ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "meeting_date TEXT NOT NULL DEFAULT '', "
        "meeting_type TEXT NOT NULL DEFAULT 'midweek', "
        "is_special_event INTEGER NOT NULL DEFAULT 0, "
        "accent_color TEXT NOT NULL DEFAULT '', "
        "created_at TEXT NOT NULL, "
        "updated_at TEXT NOT NULL"
        ")",
        "CREATE TABLE IF NOT EXISTS playlist_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE, "
        "media_item_id INTEGER REFERENCES media_items(id) ON DELETE SET NULL, "
        "section_title TEXT NOT NULL DEFAULT '', "
        "custom_title TEXT NOT NULL DEFAULT '', "
        "sort_order INTEGER NOT NULL, "
        "trim_start_ms INTEGER NOT NULL DEFAULT 0, "
        "trim_end_ms INTEGER NOT NULL DEFAULT 0, "
        "zoom_scale REAL NOT NULL DEFAULT 1.0, "
        "pan_x REAL NOT NULL DEFAULT 0.0, "
        "pan_y REAL NOT NULL DEFAULT 0.0, "
        "pdf_page INTEGER NOT NULL DEFAULT 0, "
        "is_repeatable INTEGER NOT NULL DEFAULT 0"
        ")",
        "CREATE TABLE IF NOT EXISTS download_cache ("
        "url TEXT PRIMARY KEY, "
        "etag TEXT NOT NULL DEFAULT '', "
        "last_modified TEXT NOT NULL DEFAULT '', "
        "local_path TEXT NOT NULL DEFAULT '', "
        "content_hash TEXT NOT NULL DEFAULT '', "
        "updated_at TEXT NOT NULL"
        ")",
        "CREATE INDEX IF NOT EXISTS idx_media_items_sha256 ON media_items(sha256)",
        "CREATE INDEX IF NOT EXISTS idx_media_items_type ON media_items(media_type)",
        "CREATE INDEX IF NOT EXISTS idx_playlists_date ON playlists(meeting_date)",
        ("CREATE INDEX IF NOT EXISTS idx_playlist_items_order "
         "ON playlist_items(playlist_id, sort_order)"),
    };

    QSqlQuery query(db);
    for (const char* sql : kStatements) {
        if (!query.exec(QString::fromLatin1(sql))) {
            errorOut = query.lastError().text();
            return false;
        }
    }

    query.prepare(QStringLiteral("INSERT OR REPLACE INTO schema_migrations (version, applied_at) "
                                 "VALUES (:version, :applied_at)"));
    query.bindValue(QStringLiteral(":version"), 1);
    query.bindValue(QStringLiteral(":applied_at"), utcIsoTimestamp());
    if (!query.exec()) {
        errorOut = query.lastError().text();
        return false;
    }

    return true;
}

bool tableHasColumn(QSqlDatabase& db, const QString& tableName, const QString& columnName)
{
    QSqlQuery infoQuery(db);
    if (!infoQuery.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tableName))) {
        return false;
    }
    while (infoQuery.next()) {
        if (infoQuery.value(1).toString() == columnName) {
            return true;
        }
    }
    return false;
}

bool applyMigrationV2(QSqlDatabase& db, QString& errorOut)
{
    struct ColumnDef
    {
        const char* table;
        const char* column;
        const char* definition;
    };

    static constexpr ColumnDef kColumns[] = {
        {"playlists", "sections_json", "TEXT NOT NULL DEFAULT ''"},
        {"playlists", "sort_locked", "INTEGER NOT NULL DEFAULT 0"},
        {"playlist_items", "group_key", "TEXT NOT NULL DEFAULT ''"},
        {"playlist_items", "item_kind", "TEXT NOT NULL DEFAULT 'media'"},
        {"playlist_items", "hidden", "INTEGER NOT NULL DEFAULT 0"},
        {"playlist_items", "tag_type", "TEXT NOT NULL DEFAULT ''"},
        {"playlist_items", "tag_value", "TEXT NOT NULL DEFAULT ''"},
        {"playlist_items", "pub_media_id", "TEXT NOT NULL DEFAULT ''"},
        {"playlist_items", "stream_url", "TEXT NOT NULL DEFAULT ''"},
        {"playlist_items", "marker_ranges_json", "TEXT NOT NULL DEFAULT ''"},
        {"playlist_items", "playback_rate", "REAL NOT NULL DEFAULT 1.0"},
        {"playlist_items", "is_cbs", "INTEGER NOT NULL DEFAULT 0"},
        {"playlist_items", "original_section", "TEXT NOT NULL DEFAULT ''"},
        {"playlist_items", "sort_order_original", "INTEGER NOT NULL DEFAULT 0"},
        {"playlist_items", "divider_color", "TEXT NOT NULL DEFAULT ''"},
        {"playlist_items", "slide_interval_ms", "INTEGER NOT NULL DEFAULT 0"},
        {"playlist_items", "paired_audio_path", "TEXT NOT NULL DEFAULT ''"},
    };

    QSqlQuery query(db);
    for (const ColumnDef& col : kColumns) {
        const QString table = QString::fromLatin1(col.table);
        const QString column = QString::fromLatin1(col.column);
        if (tableHasColumn(db, table, column)) {
            continue;
        }
        const QString sql = QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                                .arg(table, column, QString::fromLatin1(col.definition));
        if (!query.exec(sql)) {
            errorOut = query.lastError().text();
            return false;
        }
    }

    if (!query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_playlists_date_type "
                                   "ON playlists(meeting_date, meeting_type)"))) {
        errorOut = query.lastError().text();
        return false;
    }

    query.prepare(QStringLiteral("INSERT OR REPLACE INTO schema_migrations (version, applied_at) "
                                 "VALUES (:version, :applied_at)"));
    query.bindValue(QStringLiteral(":version"), 2);
    query.bindValue(QStringLiteral(":applied_at"), utcIsoTimestamp());
    if (!query.exec()) {
        errorOut = query.lastError().text();
        return false;
    }

    return true;
}

bool recordMigration(QSqlDatabase& db, int version, QString& errorOut)
{
    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO schema_migrations (version, applied_at) "
                                 "VALUES (:version, :applied_at)"));
    query.bindValue(QStringLiteral(":version"), version);
    query.bindValue(QStringLiteral(":applied_at"), utcIsoTimestamp());
    if (!query.exec()) {
        errorOut = query.lastError().text();
        return false;
    }
    return true;
}

bool execBound(QSqlQuery& query, QString& errorOut)
{
    if (!query.exec()) {
        errorOut = query.lastError().text();
        return false;
    }
    return true;
}

// Rewrites one playlist from title-based sections to stable section ids: legacy titles map to
// meeting section ids, any other title becomes a custom section that keeps its name and color.
bool migratePlaylistSectionIds(QSqlDatabase& db, qint64 playlistId, const QString& sectionsJson,
                               QString& errorOut)
{
    QHash<QString, QString> idByTitle;
    QSet<QString> emittedIds;
    QJsonArray migratedSections;

    const auto idForTitle = [&](const QString& title) {
        const QString key = title.trimmed().toLower();
        if (const auto it = idByTitle.constFind(key); it != idByTitle.constEnd()) {
            return it.value();
        }
        QString id = key.isEmpty() ? QString(MeetingSections::kImportedMedia)
                                   : MeetingSections::idForLegacyTitle(title);
        if (id.isEmpty()) {
            id = MeetingSections::createCustomId();
        }
        idByTitle.insert(key, id);
        return id;
    };
    const auto appendSection = [&](const QString& title, const QJsonObject& legacy) {
        const QString id = idForTitle(title);
        if (emittedIds.contains(id)) {
            return;
        }
        emittedIds.insert(id);
        QJsonObject section;
        section.insert(QStringLiteral("id"), id);
        if (MeetingSections::isCustomSection(id) && !title.trimmed().isEmpty() &&
            MeetingSections::idForLegacyTitle(title).isEmpty()) {
            section.insert(QStringLiteral("label"), title.trimmed());
        }
        const QString color = legacy.value(QStringLiteral("color")).toString();
        section.insert(QStringLiteral("color"),
                       color.isEmpty() ? MeetingSections::defaultColor(id) : color);
        if (legacy.value(QStringLiteral("repeatable")).toBool(false)) {
            section.insert(QStringLiteral("repeatable"), true);
        }
        if (legacy.value(QStringLiteral("collapsed")).toBool(false)) {
            section.insert(QStringLiteral("collapsed"), true);
        }
        migratedSections.append(section);
    };

    const QJsonDocument legacyDoc = QJsonDocument::fromJson(sectionsJson.toUtf8());
    for (const QJsonValue& value : legacyDoc.array()) {
        const QJsonObject legacy = value.toObject();
        if (legacy.contains(QStringLiteral("id"))) {
            emittedIds.insert(legacy.value(QStringLiteral("id")).toString());
            migratedSections.append(legacy);
            continue;
        }
        appendSection(legacy.value(QStringLiteral("name")).toString(), legacy);
    }

    const auto distinctValues = [&](const QString& column, QStringList& out) {
        QSqlQuery select(db);
        select.prepare(QStringLiteral("SELECT DISTINCT %1 FROM playlist_items "
                                      "WHERE playlist_id = :playlist_id")
                           .arg(column));
        select.bindValue(QStringLiteral(":playlist_id"), playlistId);
        if (!execBound(select, errorOut)) {
            return false;
        }
        while (select.next()) {
            out.append(select.value(0).toString());
        }
        return true;
    };

    QStringList itemTitles;
    QStringList originalTitles;
    if (!distinctValues(QStringLiteral("section_title"), itemTitles) ||
        !distinctValues(QStringLiteral("original_section"), originalTitles)) {
        return false;
    }

    QSqlQuery update(db);
    for (const QString& title : itemTitles) {
        appendSection(title, {});
        update.prepare(QStringLiteral("UPDATE playlist_items SET section_id = :section_id "
                                      "WHERE playlist_id = :playlist_id "
                                      "AND section_title = :section_title"));
        update.bindValue(QStringLiteral(":section_id"), idForTitle(title));
        update.bindValue(QStringLiteral(":playlist_id"), playlistId);
        update.bindValue(QStringLiteral(":section_title"), title);
        if (!execBound(update, errorOut)) {
            return false;
        }
    }
    for (const QString& title : originalTitles) {
        if (title.isEmpty()) {
            continue;
        }
        update.prepare(QStringLiteral("UPDATE playlist_items SET original_section = :section_id "
                                      "WHERE playlist_id = :playlist_id "
                                      "AND original_section = :section_title"));
        update.bindValue(QStringLiteral(":section_id"), idForTitle(title));
        update.bindValue(QStringLiteral(":playlist_id"), playlistId);
        update.bindValue(QStringLiteral(":section_title"), title);
        if (!execBound(update, errorOut)) {
            return false;
        }
    }

    update.prepare(
        QStringLiteral("UPDATE playlists SET sections_json = :sections_json WHERE id = :id"));
    update.bindValue(
        QStringLiteral(":sections_json"),
        QString::fromUtf8(QJsonDocument(migratedSections).toJson(QJsonDocument::Compact)));
    update.bindValue(QStringLiteral(":id"), playlistId);
    return execBound(update, errorOut);
}

bool applyMigrationV3(QSqlDatabase& db, QString& errorOut)
{
    struct ColumnDef
    {
        const char* table;
        const char* column;
        const char* definition;
    };
    static constexpr ColumnDef kColumns[] = {
        {"playlists", "sync_status", "TEXT NOT NULL DEFAULT ''"},
        {"playlists", "synced_at", "TEXT NOT NULL DEFAULT ''"},
        {"playlist_items", "section_id", "TEXT NOT NULL DEFAULT ''"},
        {"playlist_items", "source", "TEXT NOT NULL DEFAULT 'additional'"},
        {"playlist_items", "unique_key", "TEXT NOT NULL DEFAULT ''"},
    };

    QSqlQuery query(db);
    for (const ColumnDef& col : kColumns) {
        const QString table = QString::fromLatin1(col.table);
        const QString column = QString::fromLatin1(col.column);
        if (tableHasColumn(db, table, column)) {
            continue;
        }
        if (!query.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                            .arg(table, column, QString::fromLatin1(col.definition)))) {
            errorOut = query.lastError().text();
            return false;
        }
    }

    // Sections stored before V3 could also live in the legacy settings key.
    QSqlQuery playlists(db);
    if (!playlists.exec(QStringLiteral(
            "SELECT p.id, CASE WHEN p.sections_json <> '' THEN p.sections_json "
            "ELSE COALESCE((SELECT s.value FROM settings s "
            "WHERE s.key = 'playlist/sections_' || p.id), '') END FROM playlists p"))) {
        errorOut = playlists.lastError().text();
        return false;
    }
    std::vector<std::pair<qint64, QString>> playlistSections;
    while (playlists.next()) {
        playlistSections.emplace_back(playlists.value(0).toLongLong(),
                                      playlists.value(1).toString());
    }
    for (const auto& [playlistId, sectionsJson] : playlistSections) {
        if (!migratePlaylistSectionIds(db, playlistId, sectionsJson, errorOut)) {
            return false;
        }
    }

    // Media written by the previous weekly sync is marked as synced so the next sync replaces it.
    static constexpr const char* kStatements[] = {
        ("UPDATE playlist_items SET source = 'dynamic' WHERE stream_url <> '' OR media_item_id IN "
         "(SELECT id FROM media_items WHERE source_kind IN ('jwpub', 'catalog', 'stream') "
         "OR file_path LIKE '%/catalog/%')"),
        "DELETE FROM settings WHERE key LIKE 'playlist/sections_%'",
        ("CREATE INDEX IF NOT EXISTS idx_playlist_items_unique "
         "ON playlist_items(playlist_id, unique_key)"),
    };
    for (const char* sql : kStatements) {
        if (!query.exec(QString::fromLatin1(sql))) {
            errorOut = query.lastError().text();
            return false;
        }
    }
    return recordMigration(db, 3, errorOut);
}

PlaylistRecord readPlaylistRow(const QSqlQuery& query)
{
    PlaylistRecord record;
    record.id = query.value(0).toLongLong();
    record.name = query.value(1).toString();
    record.meetingDate = query.value(2).toString();
    record.meetingType = query.value(3).toString();
    record.isSpecialEvent = query.value(4).toInt() != 0;
    record.accentColor = query.value(5).toString();
    record.sectionsJson = query.value(6).toString();
    record.sortLocked = query.value(7).toInt() != 0;
    record.syncStatus = query.value(8).toString();
    record.syncedAt = query.value(9).toString();
    return record;
}

// Column order shared by every playlist_items read; `pi` is the playlist_items alias.
constexpr auto kPlaylistEntryColumns =
    "pi.id, pi.playlist_id, COALESCE(pi.media_item_id, 0), pi.section_title, pi.custom_title, "
    "pi.sort_order, pi.trim_start_ms, pi.trim_end_ms, pi.zoom_scale, pi.pan_x, pi.pan_y, "
    "pi.pdf_page, pi.is_repeatable, pi.group_key, pi.item_kind, pi.hidden, pi.tag_type, "
    "pi.tag_value, pi.pub_media_id, pi.stream_url, pi.marker_ranges_json, pi.playback_rate, "
    "pi.is_cbs, pi.original_section, pi.sort_order_original, pi.divider_color, "
    "pi.slide_interval_ms, pi.paired_audio_path, pi.section_id, pi.source, pi.unique_key";
constexpr int kPlaylistEntryColumnCount = 31;

PlaylistEntryRecord readPlaylistEntryRow(const QSqlQuery& query)
{
    PlaylistEntryRecord entry;
    entry.id = query.value(0).toLongLong();
    entry.playlistId = query.value(1).toLongLong();
    entry.mediaItemId = query.value(2).toLongLong();
    entry.sectionTitle = query.value(3).toString();
    entry.customTitle = query.value(4).toString();
    entry.sortOrder = query.value(5).toInt();
    entry.trimStartMs = query.value(6).toLongLong();
    entry.trimEndMs = query.value(7).toLongLong();
    entry.zoomScale = query.value(8).toDouble();
    entry.panX = query.value(9).toDouble();
    entry.panY = query.value(10).toDouble();
    entry.pdfPage = query.value(11).toInt();
    entry.isRepeatable = query.value(12).toInt() != 0;
    entry.groupKey = query.value(13).toString();
    entry.itemKind =
        query.value(14).toString().isEmpty() ? QStringLiteral("media") : query.value(14).toString();
    entry.hidden = query.value(15).toInt() != 0;
    entry.tagType = query.value(16).toString();
    entry.tagValue = query.value(17).toString();
    entry.pubMediaId = query.value(18).toString();
    entry.streamUrl = query.value(19).toString();
    entry.markerRangesJson = query.value(20).toString();
    entry.playbackRate = query.value(21).toDouble() > 0.0 ? query.value(21).toDouble() : 1.0;
    entry.isCbs = query.value(22).toInt() != 0;
    entry.originalSection = query.value(23).toString();
    entry.sortOrderOriginal = query.value(24).toInt();
    entry.dividerColor = query.value(25).toString();
    entry.slideIntervalMs = query.value(26).toInt();
    entry.pairedAudioPath = query.value(27).toString();
    entry.sectionId = query.value(28).toString();
    entry.source = query.value(29).toString().isEmpty() ? QStringLiteral("additional")
                                                        : query.value(29).toString();
    entry.uniqueKey = query.value(30).toString();
    return entry;
}

} // namespace

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::open(const QString& databasePath)
{
    close();

    if (databasePath.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Database path cannot be empty"));
        return false;
    }

    const QString cleanedPath = QDir::cleanPath(databasePath);
    const QFileInfo info(cleanedPath);
    if (!QDir().mkpath(info.absolutePath())) {
        setLastError(QStringLiteral("Failed to create directory for database"));
        return false;
    }

    m_connectionName = QStringLiteral("kmedia_sqlite_%1")
                           .arg(g_connectionCounter.fetch_add(1, std::memory_order_relaxed) + 1);
    m_databasePath = cleanedPath;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        db.setDatabaseName(m_databasePath);
        if (!db.open()) {
            setLastError(db.lastError().text());
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(m_connectionName);
            m_connectionName.clear();
            m_databasePath.clear();
            return false;
        }

        if (!configurePragmas(db) || !runMigrations(db)) {
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(m_connectionName);
            m_connectionName.clear();
            m_databasePath.clear();
            return false;
        }
    }

    m_isOpen = true;
    m_lastError.clear();
    return true;
}

void DatabaseManager::close()
{
    if (m_connectionName.isEmpty()) {
        m_isOpen = false;
        return;
    }

    const QString connName = m_connectionName;
    {
        QSqlDatabase db = QSqlDatabase::database(connName, false);
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);

    m_connectionName.clear();
    m_databasePath.clear();
    m_isOpen = false;
}

bool DatabaseManager::isOpen() const
{
    return m_isOpen;
}

QString DatabaseManager::databasePath() const
{
    return m_databasePath;
}

QString DatabaseManager::connectionName() const
{
    return m_connectionName;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

int DatabaseManager::schemaVersion() const
{
    if (!m_isOpen) {
        return 0;
    }
    QSqlQuery query(databaseHandle());
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_migrations")) ||
        !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

QString DatabaseManager::pragmaValue(const QString& pragmaName) const
{
    if (!m_isOpen || pragmaName.trimmed().isEmpty()) {
        return {};
    }
    for (const QChar ch : pragmaName) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('_')) {
            return {};
        }
    }
    QSqlQuery query(databaseHandle());
    if (!query.exec(QStringLiteral("PRAGMA %1").arg(pragmaName)) || !query.next()) {
        return {};
    }
    return query.value(0).toString();
}

bool DatabaseManager::executeTransaction(const std::function<bool(QSqlDatabase&)>& operation)
{
    if (!m_isOpen) {
        setLastError(QStringLiteral("Database is not open"));
        return false;
    }

    QSqlDatabase db = databaseHandle();
    if (!db.transaction()) {
        setLastError(db.lastError().text());
        return false;
    }

    bool ok = false;
    try {
        ok = operation(db);
    } catch (...) {
        db.rollback();
        setLastError(QStringLiteral("Unhandled exception inside database transaction"));
        return false;
    }

    if (!ok) {
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        setLastError(db.lastError().text());
        db.rollback();
        return false;
    }
    return true;
}

std::optional<QString> DatabaseManager::getSetting(const QString& key) const
{
    if (!m_isOpen || key.isEmpty()) {
        return std::nullopt;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = :key"));
    query.bindValue(QStringLiteral(":key"), key);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return query.value(0).toString();
}

bool DatabaseManager::setSetting(const QString& key, const QString& value)
{
    if (!m_isOpen || key.isEmpty()) {
        return false;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("INSERT INTO settings (key, value, updated_at) "
                                 "VALUES (:key, :value, :updated_at) "
                                 "ON CONFLICT(key) DO UPDATE SET "
                                 "value = excluded.value, updated_at = excluded.updated_at"));
    query.bindValue(QStringLiteral(":key"), nonNullText(key));
    query.bindValue(QStringLiteral(":value"), nonNullText(value));
    query.bindValue(QStringLiteral(":updated_at"), utcIsoTimestamp());
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::removeSetting(const QString& key)
{
    if (!m_isOpen || key.isEmpty()) {
        return false;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("DELETE FROM settings WHERE key = :key"));
    query.bindValue(QStringLiteral(":key"), key);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

std::vector<std::pair<QString, QString>> DatabaseManager::allSettings() const
{
    std::vector<std::pair<QString, QString>> rows;
    if (!m_isOpen) {
        return rows;
    }
    QSqlQuery query(databaseHandle());
    if (!query.exec(QStringLiteral("SELECT key, value FROM settings ORDER BY key ASC"))) {
        m_lastError = query.lastError().text();
        return rows;
    }
    while (query.next()) {
        rows.emplace_back(query.value(0).toString(), query.value(1).toString());
    }
    return rows;
}

qint64 DatabaseManager::upsertMediaItem(const MediaItemRecord& item)
{
    if (!m_isOpen || item.filePath.isEmpty()) {
        return 0;
    }

    const QString now = utcIsoTimestamp();
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral(
        "INSERT INTO media_items ("
        "file_path, sha256, title, media_type, language_code, "
        "duration_ms, width, height, file_size, source_kind, created_at, updated_at"
        ") VALUES ("
        ":file_path, :sha256, :title, :media_type, :language_code, "
        ":duration_ms, :width, :height, :file_size, :source_kind, :created_at, :updated_at"
        ") ON CONFLICT(file_path) DO UPDATE SET "
        "sha256 = excluded.sha256, "
        "title = excluded.title, "
        "media_type = excluded.media_type, "
        "language_code = excluded.language_code, "
        "duration_ms = excluded.duration_ms, "
        "width = excluded.width, "
        "height = excluded.height, "
        "file_size = excluded.file_size, "
        "source_kind = excluded.source_kind, "
        "updated_at = excluded.updated_at"));
    query.bindValue(QStringLiteral(":file_path"), QDir::cleanPath(item.filePath));
    query.bindValue(QStringLiteral(":sha256"), nonNullText(item.sha256));
    query.bindValue(QStringLiteral(":title"), nonNullText(item.title));
    query.bindValue(QStringLiteral(":media_type"), nonNullText(item.mediaType));
    query.bindValue(QStringLiteral(":language_code"), nonNullText(item.languageCode));
    query.bindValue(QStringLiteral(":duration_ms"), item.durationMs);
    query.bindValue(QStringLiteral(":width"), item.width);
    query.bindValue(QStringLiteral(":height"), item.height);
    query.bindValue(QStringLiteral(":file_size"), item.fileSize);
    query.bindValue(QStringLiteral(":source_kind"), nonNullText(item.sourceKind));
    query.bindValue(QStringLiteral(":created_at"), now);
    query.bindValue(QStringLiteral(":updated_at"), now);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return 0;
    }

    const auto existing = findMediaItemByPath(QDir::cleanPath(item.filePath));
    return existing ? existing->id : 0;
}

std::optional<MediaItemRecord> DatabaseManager::findMediaItemByPath(const QString& filePath) const
{
    if (!m_isOpen || filePath.isEmpty()) {
        return std::nullopt;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("SELECT id, file_path, sha256, title, media_type, language_code, "
                                 "duration_ms, width, height, file_size, source_kind "
                                 "FROM media_items WHERE file_path = :file_path"));
    query.bindValue(QStringLiteral(":file_path"), QDir::cleanPath(filePath));
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return readMediaItemRow(query);
}

std::optional<MediaItemRecord> DatabaseManager::findMediaItemById(qint64 id) const
{
    if (!m_isOpen || id <= 0) {
        return std::nullopt;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("SELECT id, file_path, sha256, title, media_type, language_code, "
                                 "duration_ms, width, height, file_size, source_kind "
                                 "FROM media_items WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return readMediaItemRow(query);
}

bool DatabaseManager::removeMediaItem(qint64 id)
{
    if (!m_isOpen || id <= 0) {
        return false;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("DELETE FROM media_items WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return query.numRowsAffected() > 0;
}

qint64 DatabaseManager::createPlaylist(const PlaylistRecord& playlist)
{
    if (!m_isOpen || playlist.name.trimmed().isEmpty()) {
        return 0;
    }
    const QString now = utcIsoTimestamp();
    QSqlQuery query(databaseHandle());
    query.prepare(
        QStringLiteral("INSERT INTO playlists "
                       "(name, meeting_date, meeting_type, is_special_event, accent_color, "
                       "sections_json, sort_locked, sync_status, synced_at, created_at, "
                       "updated_at) "
                       "VALUES (:name, :meeting_date, :meeting_type, :is_special_event, "
                       ":accent_color, :sections_json, :sort_locked, :sync_status, :synced_at, "
                       ":created_at, :updated_at)"));
    query.bindValue(QStringLiteral(":name"), playlist.name.trimmed());
    query.bindValue(QStringLiteral(":meeting_date"), nonNullText(playlist.meetingDate));
    query.bindValue(QStringLiteral(":meeting_type"), nonNullText(playlist.meetingType));
    query.bindValue(QStringLiteral(":is_special_event"), playlist.isSpecialEvent ? 1 : 0);
    query.bindValue(QStringLiteral(":accent_color"), nonNullText(playlist.accentColor));
    query.bindValue(QStringLiteral(":sections_json"), nonNullText(playlist.sectionsJson));
    query.bindValue(QStringLiteral(":sort_locked"), playlist.sortLocked ? 1 : 0);
    query.bindValue(QStringLiteral(":sync_status"), nonNullText(playlist.syncStatus));
    query.bindValue(QStringLiteral(":synced_at"), nonNullText(playlist.syncedAt));
    query.bindValue(QStringLiteral(":created_at"), now);
    query.bindValue(QStringLiteral(":updated_at"), now);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return 0;
    }
    return query.lastInsertId().toLongLong();
}

std::optional<PlaylistRecord> DatabaseManager::findPlaylistById(qint64 id) const
{
    if (!m_isOpen || id <= 0) {
        return std::nullopt;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral(
        "SELECT id, name, meeting_date, meeting_type, is_special_event, accent_color, "
        "sections_json, sort_locked, sync_status, synced_at "
        "FROM playlists WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return readPlaylistRow(query);
}

std::optional<PlaylistRecord>
DatabaseManager::findPlaylistByDateAndType(const QString& meetingDate,
                                           const QString& meetingType) const
{
    if (!m_isOpen || meetingDate.trimmed().isEmpty()) {
        return std::nullopt;
    }
    QSqlQuery query(databaseHandle());
    if (meetingType.trimmed().isEmpty()) {
        query.prepare(QStringLiteral(
            "SELECT id, name, meeting_date, meeting_type, is_special_event, accent_color, "
            "sections_json, sort_locked, sync_status, synced_at "
            "FROM playlists WHERE meeting_date = :meeting_date ORDER BY id DESC LIMIT 1"));
    } else {
        query.prepare(QStringLiteral(
            "SELECT id, name, meeting_date, meeting_type, is_special_event, accent_color, "
            "sections_json, sort_locked, sync_status, synced_at "
            "FROM playlists WHERE meeting_date = :meeting_date AND meeting_type = :meeting_type "
            "ORDER BY id DESC LIMIT 1"));
        query.bindValue(QStringLiteral(":meeting_type"), meetingType.trimmed());
    }
    query.bindValue(QStringLiteral(":meeting_date"), meetingDate.trimmed());
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return readPlaylistRow(query);
}

PlaylistRecord DatabaseManager::findOrCreatePlaylistForDate(const QString& meetingDate,
                                                            const QString& meetingType,
                                                            const QString& displayName,
                                                            bool isSpecialEvent,
                                                            const QString& accentColor)
{
    const QString cleanDate = meetingDate.trimmed();
    const QString cleanType =
        meetingType.trimmed().isEmpty() ? QStringLiteral("custom") : meetingType.trimmed();
    if (const auto existing = findPlaylistByDateAndType(cleanDate, cleanType);
        existing.has_value()) {
        return *existing;
    }

    PlaylistRecord created;
    created.name = displayName.trimmed().isEmpty()
                       ? QStringLiteral("%1 (%2)").arg(cleanDate, cleanType)
                       : displayName.trimmed();
    created.meetingDate = cleanDate;
    created.meetingType = cleanType;
    created.isSpecialEvent = isSpecialEvent;
    created.accentColor = accentColor;
    created.id = createPlaylist(created);
    return created;
}

bool DatabaseManager::updatePlaylistMetadata(const PlaylistRecord& playlist)
{
    if (!m_isOpen || playlist.id <= 0) {
        return false;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral(
        "UPDATE playlists SET "
        "name = :name, meeting_date = :meeting_date, meeting_type = :meeting_type, "
        "is_special_event = :is_special_event, accent_color = :accent_color, "
        "sections_json = :sections_json, sort_locked = :sort_locked, "
        "sync_status = :sync_status, synced_at = :synced_at, updated_at = :updated_at "
        "WHERE id = :id"));
    query.bindValue(QStringLiteral(":name"), playlist.name.trimmed());
    query.bindValue(QStringLiteral(":meeting_date"), nonNullText(playlist.meetingDate));
    query.bindValue(QStringLiteral(":meeting_type"), nonNullText(playlist.meetingType));
    query.bindValue(QStringLiteral(":is_special_event"), playlist.isSpecialEvent ? 1 : 0);
    query.bindValue(QStringLiteral(":accent_color"), nonNullText(playlist.accentColor));
    query.bindValue(QStringLiteral(":sections_json"), nonNullText(playlist.sectionsJson));
    query.bindValue(QStringLiteral(":sort_locked"), playlist.sortLocked ? 1 : 0);
    query.bindValue(QStringLiteral(":sync_status"), nonNullText(playlist.syncStatus));
    query.bindValue(QStringLiteral(":synced_at"), nonNullText(playlist.syncedAt));
    query.bindValue(QStringLiteral(":updated_at"), utcIsoTimestamp());
    query.bindValue(QStringLiteral(":id"), playlist.id);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

std::vector<PlaylistRecord> DatabaseManager::listPlaylists() const
{
    std::vector<PlaylistRecord> result;
    if (!m_isOpen) {
        return result;
    }
    QSqlQuery query(databaseHandle());
    if (!query.exec(QStringLiteral(
            "SELECT id, name, meeting_date, meeting_type, is_special_event, accent_color, "
            "sections_json, sort_locked, sync_status, synced_at "
            "FROM playlists ORDER BY meeting_date DESC, id DESC"))) {
        m_lastError = query.lastError().text();
        return result;
    }
    while (query.next()) {
        result.push_back(readPlaylistRow(query));
    }
    return result;
}

bool DatabaseManager::setPlaylistSyncStatus(qint64 playlistId, const QString& syncStatus)
{
    if (!m_isOpen || playlistId <= 0) {
        return false;
    }
    const QString now = utcIsoTimestamp();
    QSqlQuery query(databaseHandle());
    query.prepare(
        QStringLiteral("UPDATE playlists SET sync_status = :sync_status, "
                       "synced_at = :synced_at, updated_at = :updated_at WHERE id = :id"));
    query.bindValue(QStringLiteral(":sync_status"), nonNullText(syncStatus));
    query.bindValue(QStringLiteral(":synced_at"),
                    nonNullText(syncStatus.isEmpty() ? QString() : now));
    query.bindValue(QStringLiteral(":updated_at"), now);
    query.bindValue(QStringLiteral(":id"), playlistId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::deletePlaylist(qint64 id)
{
    if (!m_isOpen || id <= 0) {
        return false;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("DELETE FROM playlists WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return query.numRowsAffected() > 0;
}

qint64 DatabaseManager::addPlaylistEntry(const PlaylistEntryRecord& entry)
{
    if (!m_isOpen || entry.playlistId <= 0) {
        return 0;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral(
        "INSERT INTO playlist_items "
        "(playlist_id, media_item_id, section_title, custom_title, sort_order, "
        "trim_start_ms, trim_end_ms, zoom_scale, pan_x, pan_y, pdf_page, is_repeatable, "
        "group_key, item_kind, hidden, tag_type, tag_value, pub_media_id, stream_url, "
        "marker_ranges_json, playback_rate, is_cbs, original_section, sort_order_original, "
        "divider_color, slide_interval_ms, paired_audio_path, section_id, source, unique_key) "
        "VALUES "
        "(:playlist_id, :media_item_id, :section_title, :custom_title, :sort_order, "
        ":trim_start_ms, :trim_end_ms, :zoom_scale, :pan_x, :pan_y, :pdf_page, :is_repeatable, "
        ":group_key, :item_kind, :hidden, :tag_type, :tag_value, :pub_media_id, :stream_url, "
        ":marker_ranges_json, :playback_rate, :is_cbs, :original_section, :sort_order_original, "
        ":divider_color, :slide_interval_ms, :paired_audio_path, :section_id, :source, "
        ":unique_key)"));
    query.bindValue(QStringLiteral(":playlist_id"), entry.playlistId);
    query.bindValue(QStringLiteral(":media_item_id"),
                    entry.mediaItemId > 0 ? QVariant(entry.mediaItemId) : QVariant());
    query.bindValue(QStringLiteral(":section_title"), nonNullText(entry.sectionTitle));
    query.bindValue(QStringLiteral(":custom_title"), nonNullText(entry.customTitle));
    query.bindValue(QStringLiteral(":sort_order"), entry.sortOrder);
    query.bindValue(QStringLiteral(":trim_start_ms"), entry.trimStartMs);
    query.bindValue(QStringLiteral(":trim_end_ms"), entry.trimEndMs);
    query.bindValue(QStringLiteral(":zoom_scale"), entry.zoomScale);
    query.bindValue(QStringLiteral(":pan_x"), entry.panX);
    query.bindValue(QStringLiteral(":pan_y"), entry.panY);
    query.bindValue(QStringLiteral(":pdf_page"), entry.pdfPage);
    query.bindValue(QStringLiteral(":is_repeatable"), entry.isRepeatable ? 1 : 0);
    query.bindValue(QStringLiteral(":group_key"), nonNullText(entry.groupKey));
    query.bindValue(QStringLiteral(":item_kind"),
                    entry.itemKind.trimmed().isEmpty() ? QStringLiteral("media") : entry.itemKind);
    query.bindValue(QStringLiteral(":hidden"), entry.hidden ? 1 : 0);
    query.bindValue(QStringLiteral(":tag_type"), nonNullText(entry.tagType));
    query.bindValue(QStringLiteral(":tag_value"), nonNullText(entry.tagValue));
    query.bindValue(QStringLiteral(":pub_media_id"), nonNullText(entry.pubMediaId));
    query.bindValue(QStringLiteral(":stream_url"), nonNullText(entry.streamUrl));
    query.bindValue(QStringLiteral(":marker_ranges_json"), nonNullText(entry.markerRangesJson));
    query.bindValue(QStringLiteral(":playback_rate"),
                    entry.playbackRate > 0.0 ? entry.playbackRate : 1.0);
    query.bindValue(QStringLiteral(":is_cbs"), entry.isCbs ? 1 : 0);
    query.bindValue(QStringLiteral(":original_section"), nonNullText(entry.originalSection));
    query.bindValue(QStringLiteral(":sort_order_original"), entry.sortOrderOriginal);
    query.bindValue(QStringLiteral(":divider_color"), nonNullText(entry.dividerColor));
    query.bindValue(QStringLiteral(":slide_interval_ms"), entry.slideIntervalMs);
    query.bindValue(QStringLiteral(":paired_audio_path"), nonNullText(entry.pairedAudioPath));
    query.bindValue(QStringLiteral(":section_id"), nonNullText(entry.sectionId));
    query.bindValue(QStringLiteral(":source"),
                    entry.source.isEmpty() ? QStringLiteral("additional") : entry.source);
    query.bindValue(QStringLiteral(":unique_key"), nonNullText(entry.uniqueKey));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return 0;
    }
    return query.lastInsertId().toLongLong();
}

std::vector<PlaylistEntryRecord> DatabaseManager::listPlaylistEntries(qint64 playlistId) const
{
    std::vector<PlaylistEntryRecord> entries;
    if (!m_isOpen || playlistId <= 0) {
        return entries;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("SELECT %1 FROM playlist_items pi "
                                 "WHERE pi.playlist_id = :playlist_id "
                                 "ORDER BY pi.sort_order ASC, pi.id ASC")
                      .arg(QLatin1StringView(kPlaylistEntryColumns)));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return entries;
    }
    while (query.next()) {
        entries.push_back(readPlaylistEntryRow(query));
    }
    return entries;
}

std::vector<PlaylistItemRow> DatabaseManager::listPlaylistItemRows(qint64 playlistId) const
{
    std::vector<PlaylistItemRow> rows;
    if (!m_isOpen || playlistId <= 0) {
        return rows;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("SELECT %1, m.id, m.file_path, m.sha256, m.title, m.media_type, "
                                 "m.language_code, m.duration_ms, m.width, m.height, m.file_size, "
                                 "m.source_kind FROM playlist_items pi "
                                 "LEFT JOIN media_items m ON m.id = pi.media_item_id "
                                 "WHERE pi.playlist_id = :playlist_id "
                                 "ORDER BY pi.sort_order ASC, pi.id ASC")
                      .arg(QLatin1StringView(kPlaylistEntryColumns)));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return rows;
    }
    while (query.next()) {
        PlaylistItemRow row;
        row.entry = readPlaylistEntryRow(query);
        constexpr int col = kPlaylistEntryColumnCount;
        if (!query.value(col).isNull()) {
            MediaItemRecord media;
            media.id = query.value(col).toLongLong();
            media.filePath = query.value(col + 1).toString();
            media.sha256 = query.value(col + 2).toString();
            media.title = query.value(col + 3).toString();
            media.mediaType = query.value(col + 4).toString();
            media.languageCode = query.value(col + 5).toString();
            media.durationMs = query.value(col + 6).toLongLong();
            media.width = query.value(col + 7).toInt();
            media.height = query.value(col + 8).toInt();
            media.fileSize = query.value(col + 9).toLongLong();
            media.sourceKind = query.value(col + 10).toString();
            row.media = media;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

bool DatabaseManager::updatePlaylistEntryFull(const PlaylistEntryRecord& entry)
{
    if (!m_isOpen || entry.id <= 0 || entry.playlistId <= 0) {
        return false;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral(
        "UPDATE playlist_items SET "
        "section_title = :section_title, custom_title = :custom_title, "
        "trim_start_ms = :trim_start_ms, trim_end_ms = :trim_end_ms, "
        "zoom_scale = :zoom_scale, pan_x = :pan_x, pan_y = :pan_y, pdf_page = :pdf_page, "
        "is_repeatable = :is_repeatable, group_key = :group_key, item_kind = :item_kind, "
        "hidden = :hidden, tag_type = :tag_type, tag_value = :tag_value, "
        "pub_media_id = :pub_media_id, stream_url = :stream_url, "
        "marker_ranges_json = :marker_ranges_json, playback_rate = :playback_rate, "
        "is_cbs = :is_cbs, original_section = :original_section, "
        "sort_order_original = :sort_order_original, divider_color = :divider_color, "
        "slide_interval_ms = :slide_interval_ms, paired_audio_path = :paired_audio_path, "
        "section_id = :section_id, source = :source, unique_key = :unique_key "
        "WHERE id = :id AND playlist_id = :playlist_id"));
    query.bindValue(QStringLiteral(":section_title"), nonNullText(entry.sectionTitle));
    query.bindValue(QStringLiteral(":custom_title"), nonNullText(entry.customTitle));
    query.bindValue(QStringLiteral(":trim_start_ms"), entry.trimStartMs);
    query.bindValue(QStringLiteral(":trim_end_ms"), entry.trimEndMs);
    query.bindValue(QStringLiteral(":zoom_scale"), entry.zoomScale);
    query.bindValue(QStringLiteral(":pan_x"), entry.panX);
    query.bindValue(QStringLiteral(":pan_y"), entry.panY);
    query.bindValue(QStringLiteral(":pdf_page"), entry.pdfPage);
    query.bindValue(QStringLiteral(":is_repeatable"), entry.isRepeatable ? 1 : 0);
    query.bindValue(QStringLiteral(":group_key"), nonNullText(entry.groupKey));
    query.bindValue(QStringLiteral(":item_kind"),
                    entry.itemKind.trimmed().isEmpty() ? QStringLiteral("media") : entry.itemKind);
    query.bindValue(QStringLiteral(":hidden"), entry.hidden ? 1 : 0);
    query.bindValue(QStringLiteral(":tag_type"), nonNullText(entry.tagType));
    query.bindValue(QStringLiteral(":tag_value"), nonNullText(entry.tagValue));
    query.bindValue(QStringLiteral(":pub_media_id"), nonNullText(entry.pubMediaId));
    query.bindValue(QStringLiteral(":stream_url"), nonNullText(entry.streamUrl));
    query.bindValue(QStringLiteral(":marker_ranges_json"), nonNullText(entry.markerRangesJson));
    query.bindValue(QStringLiteral(":playback_rate"),
                    entry.playbackRate > 0.0 ? entry.playbackRate : 1.0);
    query.bindValue(QStringLiteral(":is_cbs"), entry.isCbs ? 1 : 0);
    query.bindValue(QStringLiteral(":original_section"), nonNullText(entry.originalSection));
    query.bindValue(QStringLiteral(":sort_order_original"), entry.sortOrderOriginal);
    query.bindValue(QStringLiteral(":divider_color"), nonNullText(entry.dividerColor));
    query.bindValue(QStringLiteral(":slide_interval_ms"), entry.slideIntervalMs);
    query.bindValue(QStringLiteral(":paired_audio_path"), nonNullText(entry.pairedAudioPath));
    query.bindValue(QStringLiteral(":section_id"), nonNullText(entry.sectionId));
    query.bindValue(QStringLiteral(":source"),
                    entry.source.isEmpty() ? QStringLiteral("additional") : entry.source);
    query.bindValue(QStringLiteral(":unique_key"), nonNullText(entry.uniqueKey));
    query.bindValue(QStringLiteral(":id"), entry.id);
    query.bindValue(QStringLiteral(":playlist_id"), entry.playlistId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::deletePlaylistEntry(qint64 entryId, qint64 playlistId)
{
    if (!m_isOpen || entryId <= 0 || playlistId <= 0) {
        return false;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(
        QStringLiteral("DELETE FROM playlist_items WHERE id = :id AND playlist_id = :playlist_id"));
    query.bindValue(QStringLiteral(":id"), entryId);
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::updatePlaylistEntrySection(qint64 entryId, qint64 playlistId,
                                                 const QString& sectionId)
{
    if (!m_isOpen || entryId <= 0 || playlistId <= 0) {
        return false;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("UPDATE playlist_items SET section_id = :section_id "
                                 "WHERE id = :id AND playlist_id = :playlist_id"));
    query.bindValue(QStringLiteral(":section_id"), nonNullText(sectionId));
    query.bindValue(QStringLiteral(":id"), entryId);
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::reorderPlaylistEntries(qint64 playlistId,
                                             const std::vector<qint64>& orderedEntryIds)
{
    return executeTransaction([&](QSqlDatabase& db) {
        QSqlQuery query(db);
        query.prepare(QStringLiteral("UPDATE playlist_items SET sort_order = :sort_order "
                                     "WHERE id = :id AND playlist_id = :playlist_id"));
        for (int index = 0; index < static_cast<int>(orderedEntryIds.size()); ++index) {
            query.bindValue(QStringLiteral(":sort_order"), index);
            query.bindValue(QStringLiteral(":id"), orderedEntryIds[static_cast<size_t>(index)]);
            query.bindValue(QStringLiteral(":playlist_id"), playlistId);
            if (!query.exec()) {
                setLastError(query.lastError().text());
                return false;
            }
        }
        return true;
    });
}

bool DatabaseManager::upsertDownloadCache(const DownloadCacheRecord& record)
{
    if (!m_isOpen || record.url.isEmpty()) {
        return false;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral(
        "INSERT INTO download_cache "
        "(url, etag, last_modified, local_path, content_hash, updated_at) "
        "VALUES (:url, :etag, :last_modified, :local_path, :content_hash, :updated_at) "
        "ON CONFLICT(url) DO UPDATE SET "
        "etag = excluded.etag, "
        "last_modified = excluded.last_modified, "
        "local_path = excluded.local_path, "
        "content_hash = excluded.content_hash, "
        "updated_at = excluded.updated_at"));
    query.bindValue(QStringLiteral(":url"), nonNullText(record.url));
    query.bindValue(QStringLiteral(":etag"), nonNullText(record.etag));
    query.bindValue(QStringLiteral(":last_modified"), nonNullText(record.lastModified));
    query.bindValue(QStringLiteral(":local_path"), nonNullText(record.localPath));
    query.bindValue(QStringLiteral(":content_hash"), nonNullText(record.contentHash));
    query.bindValue(QStringLiteral(":updated_at"), utcIsoTimestamp());
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

std::optional<DownloadCacheRecord> DatabaseManager::findDownloadCache(const QString& url) const
{
    if (!m_isOpen || url.isEmpty()) {
        return std::nullopt;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("SELECT url, etag, last_modified, local_path, content_hash "
                                 "FROM download_cache WHERE url = :url"));
    query.bindValue(QStringLiteral(":url"), url);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    DownloadCacheRecord record;
    record.url = query.value(0).toString();
    record.etag = query.value(1).toString();
    record.lastModified = query.value(2).toString();
    record.localPath = query.value(3).toString();
    record.contentHash = query.value(4).toString();
    return record;
}

bool DatabaseManager::configurePragmas(QSqlDatabase& db)
{
    static constexpr const char* kPragmas[] = {
        "PRAGMA journal_mode = WAL",  "PRAGMA synchronous = NORMAL", "PRAGMA foreign_keys = ON",
        "PRAGMA busy_timeout = 5000", "PRAGMA temp_store = MEMORY",  "PRAGMA mmap_size = 67108864",
    };

    QSqlQuery query(db);
    for (const char* sql : kPragmas) {
        if (!query.exec(QString::fromLatin1(sql))) {
            setLastError(query.lastError().text());
            return false;
        }
    }
    return true;
}

bool DatabaseManager::runMigrations(QSqlDatabase& db)
{
    QSqlQuery checkQuery(db);
    int currentVersion = 0;
    if (checkQuery.exec(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type='table' AND name='schema_migrations'")) &&
        checkQuery.next()) {
        QSqlQuery versionQuery(db);
        if (versionQuery.exec(
                QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_migrations")) &&
            versionQuery.next()) {
            currentVersion = versionQuery.value(0).toInt();
        }
    }

    if (currentVersion >= kLatestSchemaVersion) {
        return true;
    }

    if (!db.transaction()) {
        setLastError(db.lastError().text());
        return false;
    }

    QString migrationError;
    bool ok = true;
    if (currentVersion < 1) {
        ok = applyMigrationV1(db, migrationError);
    }
    if (ok && currentVersion < 2) {
        ok = applyMigrationV2(db, migrationError);
    }
    if (ok && currentVersion < 3) {
        ok = applyMigrationV3(db, migrationError);
    }

    if (!ok) {
        db.rollback();
        setLastError(migrationError);
        return false;
    }

    if (!db.commit()) {
        setLastError(db.lastError().text());
        db.rollback();
        return false;
    }
    return true;
}

QSqlDatabase DatabaseManager::databaseHandle() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

void DatabaseManager::setLastError(const QString& error)
{
    m_lastError = error;
}

} // namespace kmedia
