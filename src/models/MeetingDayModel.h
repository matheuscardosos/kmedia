#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QString>

#include <vector>

namespace kmedia {

class PlaylistModel;

/**
 * Presents the open day the way the meeting media page shows it: a header row per section (the
 * meeting's own sections in meeting order, then the day's other sections in their stored order,
 * only the ones holding items on a meeting day), a notice row for a section that is empty or has
 * hidden items, and the visible media rows of each section. Media rows forward every
 * PlaylistModel role, so delegates bind to the same names. Changes are applied as row inserts,
 * moves, and removals instead of a reset, so the page keeps its scroll position while a day syncs
 * or reloads.
 */
class MeetingDayModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool isMeetingDay READ isMeetingDay NOTIFY layoutRebuilt FINAL)
    Q_PROPERTY(int mediaCount READ mediaCount NOTIFY layoutRebuilt FINAL)
    Q_PROPERTY(int sectionCount READ sectionCount NOTIFY layoutRebuilt FINAL)

public:
    enum RowType {
        SectionHeaderRow = 0,
        MediaRow,
        SectionNoticeRow,
    };
    Q_ENUM(RowType)

    enum Roles {
        RowTypeRole = Qt::UserRole + 200,
        SourceRowRole,
        RowSectionIdRole,
        RowSectionLabelRole,
        RowSectionColorRole,
        RowSectionSubtitleRole,
        RowSectionCollapsedRole,
        RowSectionIsMeetingRole,
        RowSectionIsCustomRole,
        RowSectionVisibleCountRole,
        RowSectionHiddenCountRole,
        RowSectionTotalCountRole,
        RowSectionIndexRole,
        RowSectionCountRole,
        IsLastInSectionRole,
    };
    Q_ENUM(Roles)

    explicit MeetingDayModel(PlaylistModel* source, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool isMeetingDay() const;
    [[nodiscard]] int mediaCount() const;
    [[nodiscard]] int sectionCount() const;

    Q_INVOKABLE [[nodiscard]] int sourceRowAt(int row) const;
    Q_INVOKABLE [[nodiscard]] int rowForSourceRow(int sourceRow) const;
    Q_INVOKABLE [[nodiscard]] int headerRowOfSection(const QString& sectionId) const;

signals:
    void layoutRebuilt();

private:
    struct Row
    {
        RowType type{MediaRow};
        QString key;
        int sourceRow{-1};
        QString sectionId;
        int sectionIndex{0};
        int visibleCount{0};
        int hiddenCount{0};
        int totalCount{0};
        bool lastInSection{false};
    };

    struct SectionInfo
    {
        QString label;
        QString color;
        QString documentTitle;
        bool collapsed{false};
    };

    void rebuild();
    void applyRows(std::vector<Row> next);
    void onSourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
                             const QList<int>& roles);
    [[nodiscard]] std::vector<Row> computeRows(const QVariantList& sections) const;

    QPointer<PlaylistModel> m_source;
    std::vector<Row> m_rows;
    std::vector<int> m_rowBySource;
    QHash<QString, SectionInfo> m_sectionInfo;
    int m_sectionCount{0};
    int m_mediaCount{0};
};

} // namespace kmedia
