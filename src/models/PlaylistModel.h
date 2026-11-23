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
    QString id;
    QString label;
    QString colorHex;
    bool repeatable{false};
    bool collapsed{false};
    QString documentTitle;
};

class DatabaseManager;
class PlaybackController;
class ThumbnailCache;
struct JwpubExtractRef;
struct JwpubMediaItem;

struct PlaylistEntryView
{
    qint64 entryId{0};
    qint64 mediaItemId{0};
    QString title;
    QString subtitle;
    QString sectionId;
    QString filePath;
    QString thumbnailUrl;
    QString thumbnailPath;
    QString thumbnailKey;
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
    // Clips of a sign language video: [{ label, startMs, durationMs }].
    QString markerRangesJson;
    QString streamUrl;
    double playbackRate{1.0};
    bool isCbs{false};
    QString originalSection;
    int sortOrderOriginal{0};
    QString dividerColor;
    int slideIntervalMs{0};
    QString pairedAudioPath;
    QString source{QStringLiteral("additional")};
    QString uniqueKey;
    int width{0};
    int height{0};
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
    Q_PROPERTY(
        QString activeThumbnailUrl READ activeThumbnailUrl NOTIFY activeThumbnailChanged FINAL)
    Q_PROPERTY(QString activeFileName READ activeFileName NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(QString activeFormatLabel READ activeFormatLabel NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(
        QString activeResolutionLabel READ activeResolutionLabel NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(QString activeFileSizeFormatted READ activeFileSizeFormatted NOTIFY
                   activeIndexChanged FINAL)
    Q_PROPERTY(QString activeSourceKind READ activeSourceKind NOTIFY activeIndexChanged FINAL)
    Q_PROPERTY(bool activeIsSynced READ activeIsSynced NOTIFY activeIndexChanged FINAL)
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
    Q_PROPERTY(bool currentIsCoWeek READ currentIsCoWeek NOTIFY workspaceDateChanged FINAL)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged FINAL)
    Q_PROPERTY(
        QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged FINAL)
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY countChanged FINAL)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged FINAL)

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
        SectionLabelRole,
        SourceRole,
        SelectedRole,
        ClipsRole,
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
    [[nodiscard]] bool activeIsSynced() const;
    [[nodiscard]] int activeMediaKind() const;
    [[nodiscard]] qint64 activeDurationMs() const;
    [[nodiscard]] qint64 activeTrimStartMs() const;
    [[nodiscard]] qint64 activeTrimEndMs() const;
    [[nodiscard]] QStringList availablePlaylistNames() const;
    [[nodiscard]] int activePlaylistIndex() const;

    [[nodiscard]] const std::vector<PlaylistEntryView>& items() const;

    Q_INVOKABLE int importUrls(const QList<QUrl>& urls, const QString& sectionId = {});
    Q_INVOKABLE bool importLocalFile(const QString& filePath, const QString& sectionId = {});
    // Adds a PDF as one document even when it has several pages.
    Q_INVOKABLE bool importPdfDocument(const QString& filePath, const QString& sectionId = {});
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
    Q_INVOKABLE [[nodiscard]] QString colorForSection(const QString& sectionId) const;
    Q_INVOKABLE [[nodiscard]] QString sectionLabel(const QString& sectionId) const;
    Q_INVOKABLE bool ensureSection(const QString& sectionId, const QString& colorHex = {});
    Q_INVOKABLE QString createSection(const QString& label, const QString& colorHex = {});
    Q_INVOKABLE bool renameSection(const QString& sectionId, const QString& newLabel);
    Q_INVOKABLE bool setSectionColor(const QString& sectionId, const QString& colorHex);
    Q_INVOKABLE bool setSectionSubtitle(const QString& sectionId, const QString& subtitle);
    Q_INVOKABLE bool removeSection(const QString& sectionId);
    Q_INVOKABLE bool moveSection(const QString& sectionId, int delta);
    Q_INVOKABLE bool assignItemToSection(int index, const QString& sectionId);

    [[nodiscard]] QString currentDateIso() const;
    [[nodiscard]] QString currentMeetingKind() const;
    [[nodiscard]] bool currentIsCoWeek() const;
    [[nodiscard]] bool showHidden() const;
    void setShowHidden(bool showHidden);
    [[nodiscard]] QString searchQuery() const;
    void setSearchQuery(const QString& query);
    [[nodiscard]] int hiddenCount() const;

    /** Reloads the open day from the database, keeping the active item when it still exists. */
    Q_INVOKABLE void reloadFromDatabase();
    Q_INVOKABLE bool openWorkspaceForDate(const QString& dateIso, const QString& meetingKind,
                                          const QString& displayTitle = {}, bool isCoVisit = false);
    Q_INVOKABLE bool addDivider(const QString& title, const QString& sectionId,
                                const QString& colorHex = QStringLiteral("#D47B2A"),
                                int afterIndex = -1);
    Q_INVOKABLE bool setItemHidden(int index, bool hidden);
    Q_INVOKABLE bool toggleItemHidden(int index);
    Q_INVOKABLE bool updateItemTag(int index, const QString& tagType, const QString& tagValue);
    Q_INVOKABLE bool setItemTitle(int index, const QString& title);
    Q_INVOKABLE bool setItemPlaybackRate(int index, double rate);
    Q_INVOKABLE bool setItemSlideInterval(int index, int intervalMs);
    // Plays only part of an item (a clip); 0 and 0 play all of it.
    Q_INVOKABLE bool setItemTrim(int index, qint64 startMs, qint64 endMs);
    Q_INVOKABLE bool pairAudioWithItem(int index, const QString& audioFilePath);
    Q_INVOKABLE bool toggleGroupExpanded(const QString& groupKey);
    Q_INVOKABLE bool setSectionRepeatable(const QString& sectionId, bool repeatable);
    Q_INVOKABLE bool toggleSectionCollapsed(const QString& sectionId);
    Q_INVOKABLE bool resetSortOrder();

    // Several items can be selected at once. The selection follows the items by entry, so it
    // survives moves and reloads, and dividers are never part of it.
    [[nodiscard]] int selectionCount() const;
    [[nodiscard]] Q_INVOKABLE bool isRowSelected(int index) const;
    [[nodiscard]] QList<int> selectedRows() const;
    Q_INVOKABLE void toggleRowSelected(int index);
    Q_INVOKABLE void selectRows(const QList<int>& indexes);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE int setSelectedHidden(bool hidden);
    Q_INVOKABLE int assignSelectedToSection(const QString& sectionId);
    // Deletes added media and hides synced and shared folder media, like the Delete key.
    Q_INVOKABLE int removeSelected();
    // Moves the selection as one block, in its order, next to the item at `targetIndex`.
    Q_INVOKABLE bool moveSelectedTo(int targetIndex);

    [[nodiscard]] static QString defaultColorForSection(const QString& sectionId);
    [[nodiscard]] static QString normalizeHexColor(const QString& raw, const QString& fallback);

    void populateDemoWorkspace(const QString& demoAssetDir);

signals:
    void countChanged();
    void activeIndexChanged();
    void playlistNameChanged();
    void playlistSubtitleChanged();
    void availablePlaylistsChanged();
    void sectionsChanged();
    void publicationFileRequested(const QUrl& fileUrl, const QString& sectionId);
    // A PDF with several pages asks which pages to add before anything is added.
    void pdfPagesRequested(const QUrl& fileUrl, const QString& sectionId);
    void workspaceDateChanged();
    void showHiddenChanged();
    void selectionChanged();
    void searchQueryChanged();
    void groupStateChanged();
    void activeItemEdited();
    void activeThumbnailChanged();
    void importNotice(const QString& message);

private:
    void refreshAvailablePlaylists();
    void loadFromDatabase();
    void persistReorder();
    void setSelectedIds(const QSet<qint64>& ids);
    void pruneSelection();
    [[nodiscard]] int rowOfEntry(qint64 entryId) const;
    void persistEntryAt(int index) const;
    void loadSectionsForPlaylist();
    void saveSectionsForPlaylist() const;
    void syncSectionsFromItems();
    void ensureFixedMeetingSections(const QString& meetingKind, bool isCoVisit);
    [[nodiscard]] int findSectionIndex(const QString& sectionId) const;
    [[nodiscard]] bool isRowVisible(int row) const;
    [[nodiscard]] int groupItemCount(const QString& groupKey) const;
    [[nodiscard]] bool isGroupStart(int row) const;
    static void enrichEntryMetadata(PlaylistEntryView& entry);
    void applyThumbnail(PlaylistEntryView& entry);
    void onThumbnailReady(const QString& key, const QString& cachedPath);
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
    bool m_currentIsCoWeek{false};
    bool m_showHidden{false};
    bool m_acceptPdfDocument{false};
    QSet<qint64> m_selectedIds;
    QString m_searchQuery;
    QSet<QString> m_collapsedGroups;
    std::vector<qint64> m_playlistIds;
    QStringList m_playlistNames;
    std::vector<PlaylistEntryView> m_items;
    std::vector<PlaylistSectionEntry> m_sections;
    int m_activeIndex{-1};
};

} // namespace kmedia
