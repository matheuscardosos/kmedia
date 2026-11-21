#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QtGlobal>

#include <functional>
#include <optional>
#include <vector>

namespace kmedia {

struct MediaItemRecord
{
    qint64 id{0};
    QString filePath;
    QString sha256;
    QString title;
    QString mediaType;
    QString languageCode;
    qint64 durationMs{0};
    int width{0};
    int height{0};
    qint64 fileSize{0};
    QString sourceKind{QStringLiteral("local")};
};

struct PlaylistRecord
{
    qint64 id{0};
    QString name;
    QString meetingDate;
    QString meetingType{QStringLiteral("midweek")};
    bool isSpecialEvent{false};
    QString accentColor;
    QString sectionsJson;
    bool sortLocked{false};
    QString syncStatus;
    QString syncedAt;
};

struct PlaylistEntryRecord
{
    qint64 id{0};
    qint64 playlistId{0};
    qint64 mediaItemId{0};
    QString sectionTitle;
    QString sectionId;
    QString customTitle;
    int sortOrder{0};
    qint64 trimStartMs{0};
    qint64 trimEndMs{0};
    double zoomScale{1.0};
    double panX{0.0};
    double panY{0.0};
    int pdfPage{0};
    bool isRepeatable{false};
    QString groupKey;
    QString itemKind{QStringLiteral("media")};
    bool hidden{false};
    QString tagType;
    QString tagValue;
    QString pubMediaId;
    QString streamUrl;
    QString markerRangesJson;
    double playbackRate{1.0};
    bool isCbs{false};
    QString originalSection;
    int sortOrderOriginal{0};
    QString dividerColor;
    int slideIntervalMs{0};
    QString pairedAudioPath;
    QString source{QStringLiteral("additional")};
    QString uniqueKey;
};

struct PlaylistItemRow
{
    PlaylistEntryRecord entry;
    std::optional<MediaItemRecord> media;
};

struct DownloadCacheRecord
{
    QString url;
    QString etag;
    QString lastModified;
    QString localPath;
    QString contentHash;
};

class DatabaseManager
{
public:
    DatabaseManager() = default;
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    DatabaseManager(DatabaseManager&&) = delete;
    DatabaseManager& operator=(DatabaseManager&&) = delete;

    [[nodiscard]] bool open(const QString& databasePath);
    void close();

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QString databasePath() const;
    [[nodiscard]] QString connectionName() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] int schemaVersion() const;
    [[nodiscard]] QString pragmaValue(const QString& pragmaName) const;

    [[nodiscard]] bool executeTransaction(const std::function<bool(QSqlDatabase&)>& operation);

    [[nodiscard]] std::optional<QString> getSetting(const QString& key) const;
    [[nodiscard]] bool setSetting(const QString& key, const QString& value);
    [[nodiscard]] bool removeSetting(const QString& key);
    [[nodiscard]] std::vector<std::pair<QString, QString>> allSettings() const;

    [[nodiscard]] qint64 upsertMediaItem(const MediaItemRecord& item);
    [[nodiscard]] std::optional<MediaItemRecord> findMediaItemByPath(const QString& filePath) const;
    [[nodiscard]] std::optional<MediaItemRecord> findMediaItemById(qint64 id) const;
    [[nodiscard]] bool removeMediaItem(qint64 id);

    [[nodiscard]] qint64 createPlaylist(const PlaylistRecord& playlist);
    [[nodiscard]] std::optional<PlaylistRecord> findPlaylistById(qint64 id) const;
    [[nodiscard]] std::optional<PlaylistRecord>
    findPlaylistByDateAndType(const QString& meetingDate, const QString& meetingType) const;
    [[nodiscard]] PlaylistRecord findOrCreatePlaylistForDate(const QString& meetingDate,
                                                             const QString& meetingType,
                                                             const QString& displayName,
                                                             bool isSpecialEvent = false,
                                                             const QString& accentColor = {});
    [[nodiscard]] bool updatePlaylistMetadata(const PlaylistRecord& playlist);
    [[nodiscard]] std::vector<PlaylistRecord> listPlaylists() const;
    [[nodiscard]] bool setPlaylistSyncStatus(qint64 playlistId, const QString& syncStatus);
    [[nodiscard]] bool deletePlaylist(qint64 id);

    [[nodiscard]] qint64 addPlaylistEntry(const PlaylistEntryRecord& entry);
    [[nodiscard]] std::vector<PlaylistEntryRecord> listPlaylistEntries(qint64 playlistId) const;
    [[nodiscard]] std::vector<PlaylistItemRow> listPlaylistItemRows(qint64 playlistId) const;
    [[nodiscard]] bool updatePlaylistEntryFull(const PlaylistEntryRecord& entry);
    [[nodiscard]] bool deletePlaylistEntry(qint64 entryId, qint64 playlistId);
    [[nodiscard]] bool updatePlaylistEntrySection(qint64 entryId, qint64 playlistId,
                                                  const QString& sectionId);
    [[nodiscard]] bool reorderPlaylistEntries(qint64 playlistId,
                                              const std::vector<qint64>& orderedEntryIds);

    [[nodiscard]] bool upsertDownloadCache(const DownloadCacheRecord& record);
    [[nodiscard]] std::optional<DownloadCacheRecord> findDownloadCache(const QString& url) const;

private:
    [[nodiscard]] bool configurePragmas(QSqlDatabase& db);
    [[nodiscard]] bool runMigrations(QSqlDatabase& db);
    [[nodiscard]] QSqlDatabase databaseHandle() const;
    void setLastError(const QString& error);

    QString m_connectionName;
    QString m_databasePath;
    mutable QString m_lastError;
    bool m_isOpen{false};
};

} // namespace kmedia
