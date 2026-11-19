#include "core/DatabaseManager.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <atomic>

namespace kmedia {

namespace {

std::atomic<quint64> g_connectionCounter{0};

constexpr int kLatestSchemaVersion = 1;

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
        "CREATE INDEX IF NOT EXISTS idx_playlist_items_order "
        "ON playlist_items(playlist_id, sort_order)",
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
                       "created_at, updated_at) "
                       "VALUES (:name, :meeting_date, :meeting_type, :is_special_event, "
                       ":accent_color, :created_at, :updated_at)"));
    query.bindValue(QStringLiteral(":name"), playlist.name.trimmed());
    query.bindValue(QStringLiteral(":meeting_date"), nonNullText(playlist.meetingDate));
    query.bindValue(QStringLiteral(":meeting_type"), nonNullText(playlist.meetingType));
    query.bindValue(QStringLiteral(":is_special_event"), playlist.isSpecialEvent ? 1 : 0);
    query.bindValue(QStringLiteral(":accent_color"), nonNullText(playlist.accentColor));
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
        "SELECT id, name, meeting_date, meeting_type, is_special_event, accent_color "
        "FROM playlists WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    PlaylistRecord record;
    record.id = query.value(0).toLongLong();
    record.name = query.value(1).toString();
    record.meetingDate = query.value(2).toString();
    record.meetingType = query.value(3).toString();
    record.isSpecialEvent = query.value(4).toInt() != 0;
    record.accentColor = query.value(5).toString();
    return record;
}

std::vector<PlaylistRecord> DatabaseManager::listPlaylists() const
{
    std::vector<PlaylistRecord> result;
    if (!m_isOpen) {
        return result;
    }
    QSqlQuery query(databaseHandle());
    if (!query.exec(QStringLiteral(
            "SELECT id, name, meeting_date, meeting_type, is_special_event, accent_color "
            "FROM playlists ORDER BY meeting_date DESC, id DESC"))) {
        m_lastError = query.lastError().text();
        return result;
    }
    while (query.next()) {
        PlaylistRecord record;
        record.id = query.value(0).toLongLong();
        record.name = query.value(1).toString();
        record.meetingDate = query.value(2).toString();
        record.meetingType = query.value(3).toString();
        record.isSpecialEvent = query.value(4).toInt() != 0;
        record.accentColor = query.value(5).toString();
        result.push_back(std::move(record));
    }
    return result;
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
        "trim_start_ms, trim_end_ms, zoom_scale, pan_x, pan_y, pdf_page, is_repeatable) "
        "VALUES "
        "(:playlist_id, :media_item_id, :section_title, :custom_title, :sort_order, "
        ":trim_start_ms, :trim_end_ms, :zoom_scale, :pan_x, :pan_y, :pdf_page, :is_repeatable)"));
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
    query.prepare(
        QStringLiteral("SELECT id, playlist_id, COALESCE(media_item_id, 0), section_title, "
                       "custom_title, sort_order, trim_start_ms, trim_end_ms, zoom_scale, "
                       "pan_x, pan_y, pdf_page, is_repeatable "
                       "FROM playlist_items WHERE playlist_id = :playlist_id "
                       "ORDER BY sort_order ASC, id ASC"));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return entries;
    }
    while (query.next()) {
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
        entries.push_back(std::move(entry));
    }
    return entries;
}

bool DatabaseManager::updatePlaylistEntrySection(qint64 entryId, qint64 playlistId,
                                                 const QString& sectionTitle)
{
    if (!m_isOpen || entryId <= 0 || playlistId <= 0) {
        return false;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(QStringLiteral("UPDATE playlist_items SET section_title = :section_title "
                                 "WHERE id = :id AND playlist_id = :playlist_id"));
    query.bindValue(QStringLiteral(":section_title"), nonNullText(sectionTitle));
    query.bindValue(QStringLiteral(":id"), entryId);
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::renamePlaylistEntrySections(qint64 playlistId, const QString& oldSectionTitle,
                                                  const QString& newSectionTitle)
{
    if (!m_isOpen || playlistId <= 0 || oldSectionTitle.trimmed().isEmpty()) {
        return false;
    }
    QSqlQuery query(databaseHandle());
    query.prepare(
        QStringLiteral("UPDATE playlist_items SET section_title = :new_section "
                       "WHERE playlist_id = :playlist_id AND section_title = :old_section"));
    query.bindValue(QStringLiteral(":new_section"), nonNullText(newSectionTitle));
    query.bindValue(QStringLiteral(":playlist_id"), playlistId);
    query.bindValue(QStringLiteral(":old_section"), oldSectionTitle);
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
