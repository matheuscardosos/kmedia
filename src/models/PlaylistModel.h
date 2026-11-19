#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <vector>

namespace kmedia {

class DatabaseManager;
class PlaybackController;
class ThumbnailCache;

struct PlaylistEntryView
{
    qint64 entryId{0};
    qint64 mediaItemId{0};
    QString title;
    QString subtitle;
    QString sectionTitle;
    QString filePath;
    QString thumbnailUrl;
    QString kindLabel;
    QString formatLabel;
    QString resolutionLabel;
    int mediaKind{0};
    qint64 durationMs{0};
    qint64 trimStartMs{0};
    qint64 trimEndMs{0};
    qint64 fileSizeBytes{0};
    double zoomScale{1.0};
    double panX{0.0};
    double panY{0.0};
    int pdfPage{0};
    bool repeatable{false};
    bool fileExists{true};
    QString sourceKind{QStringLiteral("local")};
};

/**
 * Operator playlist list model with Qt row-move drag-and-drop support, SQLite persistence,
 * JWPUB/JWLPLAYLIST package import, non-destructive trims, and deterministic demo seeding.
 */
class PlaylistModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(QString playlistName READ playlistName WRITE setPlaylistName NOTIFY
                   playlistNameChanged FINAL)
    Q_PROPERTY(QString playlistSubtitle READ playlistSubtitle WRITE setPlaylistSubtitle NOTIFY
                   playlistSubtitleChanged FINAL)
    Q_PROPERTY(qint64 totalDurationMs READ totalDurationMs NOTIFY countChanged FINAL)
    Q_PROPERTY(QString formattedTotalDuration READ formattedTotalDuration NOTIFY countChanged FINAL)
    Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(QString activeThumbnailUrl READ activeThumbnailUrl NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(QString activeFileName READ activeFileName NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(QString activeFormatLabel READ activeFormatLabel NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(
        QString activeResolutionLabel READ activeResolutionLabel NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(QString activeFileSizeFormatted READ activeFileSizeFormatted NOTIFY
                   activeIndexChanged FINAL)
    Q_PROPERTY(QString activeSourceKind READ activeSourceKind NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(int activeMediaKind READ activeMediaKind NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(qint64 activeDurationMs READ activeDurationMs NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(qint64 activeTrimStartMs READ activeTrimStartMs NOTIFY activeItemEdited FINAL)
    Q_PROPERTY(qint64 activeTrimEndMs READ activeTrimEndMs NOTIFY activeItemEdited FINAL)
    Q_PROPERTY(QStringList availablePlaylistNames READ availablePlaylistNames NOTIFY
                   availablePlaylistsChanged FINAL)
    Q_PROPERTY(
        int activePlaylistIndex READ activePlaylistIndex NOTIFY availablePlaylistsChanged FINAL)

public:
    enum Roles {
        EntryIdRole = Qt::UserRole + 1,
        TitleRole,
        SubtitleRole,
        SectionTitleRole,
        FilePathRole,
        ThumbnailUrlRole,
        KindLabelRole,
        FormatLabelRole,
        ResolutionLabelRole,
        MediaKindRole,
        DurationMsRole,
        FormattedDurationRole,
        TrimStartMsRole,
        TrimEndMsRole,
        FileSizeBytesRole,
        ZoomScaleRole,
        PanXRole,
        PanYRole,
        PdfPageRole,
        RepeatableRole,
        FileExistsRole,
        SourceKindRole,
        ActiveRole,
    };
    Q_ENUM(Roles)

    explicit PlaylistModel(QObject* parent = nullptr);

    Q_DISABLE_COPY_MOVE(PlaylistModel)

    void attachDatabase(DatabaseManager* database, qint64 playlistId = 0);
    void attachPlaybackController(PlaybackController* playback);
    void attachThumbnailCache(ThumbnailCache* thumbnailCache);
    void setPackageExtractDir(const QString& extractDir);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const;
    [[nodiscard]] int activeIndex() const;
    [[nodiscard]] QString playlistName() const;
    void setPlaylistName(const QString& name);
    [[nodiscard]] QString playlistSubtitle() const;
    void setPlaylistSubtitle(const QString& subtitle);
    [[nodiscard]] qint64 totalDurationMs() const;
    [[nodiscard]] QString formattedTotalDuration() const;
    [[nodiscard]] QString activeTitle() const;
    [[nodiscard]] QString activeThumbnailUrl() const;
    [[nodiscard]] QString activeFileName() const;
    [[nodiscard]] QString activeFormatLabel() const;
    [[nodiscard]] QString activeResolutionLabel() const;
    [[nodiscard]] QString activeFileSizeFormatted() const;
    [[nodiscard]] QString activeSourceKind() const;
    [[nodiscard]] int activeMediaKind() const;
    [[nodiscard]] qint64 activeDurationMs() const;
    [[nodiscard]] qint64 activeTrimStartMs() const;
    [[nodiscard]] qint64 activeTrimEndMs() const;
    [[nodiscard]] QStringList availablePlaylistNames() const;
    [[nodiscard]] int activePlaylistIndex() const;

    [[nodiscard]] const std::vector<PlaylistEntryView>& items() const;

    Q_INVOKABLE int importUrls(const QList<QUrl>& urls,
                               const QString& defaultSection = QStringLiteral("Meeting"));
    Q_INVOKABLE bool importLocalFile(const QString& filePath,
                                     const QString& sectionTitle = QStringLiteral("Meeting"));
    Q_INVOKABLE bool appendEntry(const PlaylistEntryView& entry);
    Q_INVOKABLE bool moveItem(int fromIndex, int toIndex);
    Q_INVOKABLE bool removeAt(int index);
    Q_INVOKABLE void clear();

    Q_INVOKABLE bool activateIndex(int index);
    Q_INVOKABLE bool activateNext();
    Q_INVOKABLE bool activatePrevious();
    Q_INVOKABLE void updateActiveTrim(qint64 startMs, qint64 endMs);
    Q_INVOKABLE void resetActiveTrim();
    Q_INVOKABLE void updateActiveViewport(double zoomScale, double panX, double panY);
    Q_INVOKABLE bool switchPlaylistByIndex(int index);
    Q_INVOKABLE bool switchAdjacentPlaylist(int delta);
    Q_INVOKABLE bool createNewPlaylist(const QString& name);

    void populateDemoWorkspace(const QString& demoAssetDir);

signals:
    void countChanged();
    void activeIndexChanged();
    void playlistNameChanged();
    void playlistSubtitleChanged();
    void availablePlaylistsChanged();
    void activeItemEdited();
    void importNotice(const QString& message);

private:
    void refreshAvailablePlaylists();
    void loadFromDatabase();
    void persistReorder();
    static void enrichEntryMetadata(PlaylistEntryView& entry);
    [[nodiscard]] QString extractSubdirForPackage(const QString& archivePath) const;
    [[nodiscard]] static QString kindLabelForMediaKind(int kind);

    DatabaseManager* m_database{nullptr};
    qint64 m_playlistId{0};
    QPointer<PlaybackController> m_playback;
    ThumbnailCache* m_thumbnailCache{nullptr};
    QString m_extractDir;

    QString m_playlistName{QStringLiteral("Midweek Meeting")};
    QString m_playlistSubtitle;
    std::vector<qint64> m_playlistIds;
    QStringList m_playlistNames;
    std::vector<PlaylistEntryView> m_items;
    int m_activeIndex{-1};
};

} // namespace kmedia
