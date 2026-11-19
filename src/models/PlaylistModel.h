#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include <vector>

namespace kmedia {

struct PlaylistSectionEntry
{
    QString name;
    QString colorHex;
    bool repeatable{false};
    bool collapsed{false};
};

class DatabaseManager;
class PlaybackController;
class ThumbnailCache;
struct JwpubExtractRef;
struct JwpubMediaItem;
struct JwpubWeekFilterOptions;

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
    QString groupKey;
    QString itemKind{QStringLiteral("media")};
    bool hidden{false};
    QString tagType;
    QString tagValue;
    QString pubMediaId;
    QString streamUrl;
    double playbackRate{1.0};
    bool isCbs{false};
    QString originalSection;
    int sortOrderOriginal{0};
    QString dividerColor;
    int slideIntervalMs{0};
    QString pairedAudioPath;
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
    Q_PROPERTY(QVariantList sections READ sections NOTIFY sectionsChanged FINAL)
    Q_PROPERTY(QString currentDateIso READ currentDateIso NOTIFY workspaceDateChanged FINAL)
    Q_PROPERTY(QString currentMeetingKind READ currentMeetingKind NOTIFY workspaceDateChanged FINAL)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged FINAL)
    Q_PROPERTY(
        QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged FINAL)
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY countChanged FINAL)

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
        GroupKeyRole,
        GroupExpandedRole,
        IsGroupStartRole,
        GroupItemCountRole,
        ItemKindRole,
        HiddenRole,
        TagTypeRole,
        TagValueRole,
        PubMediaIdRole,
        StreamUrlRole,
        PlaybackRateRole,
        IsCbsRole,
        DividerColorRole,
        SlideIntervalMsRole,
        PairedAudioPathRole,
        RowVisibleRole,
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
    int importJwpubForWeek(const QString& filePath, const JwpubWeekFilterOptions& options,
                           const QString& fallbackSection, bool isCoVisit,
                           QString* outWatchtowerSubtitle = nullptr,
                           std::vector<JwpubMediaItem>* outUnresolvedLinked = nullptr,
                           std::vector<JwpubExtractRef>* outExtracts = nullptr);
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

    [[nodiscard]] QVariantList sections() const;
    Q_INVOKABLE [[nodiscard]] QString colorForSection(const QString& sectionName) const;
    Q_INVOKABLE bool ensureSection(const QString& name, const QString& colorHex = {});
    Q_INVOKABLE bool createSection(const QString& name, const QString& colorHex = {});
    Q_INVOKABLE bool renameSection(const QString& oldName, const QString& newName);
    Q_INVOKABLE bool setSectionColor(const QString& sectionName, const QString& colorHex);
    Q_INVOKABLE bool removeSection(const QString& sectionName);
    Q_INVOKABLE bool moveSection(const QString& sectionName, int delta);
    Q_INVOKABLE bool assignItemToSection(int index, const QString& targetSection);

    [[nodiscard]] QString currentDateIso() const;
    [[nodiscard]] QString currentMeetingKind() const;
    [[nodiscard]] bool showHidden() const;
    void setShowHidden(bool showHidden);
    [[nodiscard]] QString searchQuery() const;
    void setSearchQuery(const QString& query);
    [[nodiscard]] int hiddenCount() const;

    Q_INVOKABLE bool openWorkspaceForDate(const QString& dateIso, const QString& meetingKind,
                                          const QString& displayTitle = {}, bool isCoVisit = false);
    Q_INVOKABLE bool addDivider(const QString& title, const QString& sectionTitle,
                                const QString& colorHex = QStringLiteral("#D47B2A"),
                                int afterIndex = -1);
    Q_INVOKABLE bool setItemHidden(int index, bool hidden);
    Q_INVOKABLE bool toggleItemHidden(int index);
    Q_INVOKABLE bool updateItemTag(int index, const QString& tagType, const QString& tagValue);
    Q_INVOKABLE bool setItemTitle(int index, const QString& title);
    Q_INVOKABLE bool setItemPlaybackRate(int index, double rate);
    Q_INVOKABLE bool setItemSlideInterval(int index, int intervalMs);
    Q_INVOKABLE bool pairAudioWithItem(int index, const QString& audioFilePath);
    Q_INVOKABLE bool toggleGroupExpanded(const QString& groupKey);
    Q_INVOKABLE bool setSectionRepeatable(const QString& sectionName, bool repeatable);
    Q_INVOKABLE bool toggleSectionCollapsed(const QString& sectionName);
    Q_INVOKABLE bool resetSortOrder();

    [[nodiscard]] static QString defaultColorForSection(const QString& sectionTitle);
    [[nodiscard]] static QString normalizeHexColor(const QString& raw, const QString& fallback);

    void populateDemoWorkspace(const QString& demoAssetDir);

signals:
    void countChanged();
    void activeIndexChanged();
    void playlistNameChanged();
    void playlistSubtitleChanged();
    void availablePlaylistsChanged();
    void sectionsChanged();
    void workspaceDateChanged();
    void showHiddenChanged();
    void searchQueryChanged();
    void groupStateChanged();
    void activeItemEdited();
    void importNotice(const QString& message);

private:
    void refreshAvailablePlaylists();
    void loadFromDatabase();
    void persistReorder();
    void persistEntryAt(int index) const;
    void loadSectionsForPlaylist();
    void saveSectionsForPlaylist() const;
    void syncSectionsFromItems();
    void ensureFixedMeetingSections(const QString& meetingKind, bool isCoVisit);
    [[nodiscard]] int findSectionIndex(const QString& name) const;
    [[nodiscard]] bool isRowVisible(int row) const;
    [[nodiscard]] int groupItemCount(const QString& groupKey) const;
    [[nodiscard]] bool isGroupStart(int row) const;
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
    QString m_currentDateIso;
    QString m_currentMeetingKind{QStringLiteral("midweek")};
    bool m_showHidden{false};
    QString m_searchQuery;
    QSet<QString> m_collapsedGroups;
    std::vector<qint64> m_playlistIds;
    QStringList m_playlistNames;
    std::vector<PlaylistEntryView> m_items;
    std::vector<PlaylistSectionEntry> m_sections;
    int m_activeIndex{-1};
};

} // namespace kmedia
