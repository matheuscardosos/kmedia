#include "models/MeetingDayModel.h"

#include "core/MeetingSections.h"
#include "models/PlaylistModel.h"

#include <QSet>
#include <QVariantMap>

#include <algorithm>

namespace kmedia {

namespace {

bool affectsLayout(const QList<int>& roles)
{
    if (roles.isEmpty()) {
        return true;
    }
    return std::any_of(roles.cbegin(), roles.cend(), [](int role) {
        return role == PlaylistModel::RowVisibleRole || role == PlaylistModel::SectionTitleRole ||
               role == PlaylistModel::HiddenRole || role == PlaylistModel::ItemKindRole ||
               role == PlaylistModel::EntryIdRole || role == PlaylistModel::TagTypeRole;
    });
}

QString sectionIdOf(const QVariant& section)
{
    return section.toMap().value(QStringLiteral("id")).toString();
}

} // namespace

MeetingDayModel::MeetingDayModel(PlaylistModel* source, QObject* parent)
    : QAbstractListModel(parent), m_source(source)
{
    if (m_source == nullptr) {
        return;
    }
    const auto rebuildNow = [this]() { rebuild(); };
    connect(m_source, &QAbstractItemModel::modelReset, this, rebuildNow);
    connect(m_source, &QAbstractItemModel::rowsInserted, this, rebuildNow);
    connect(m_source, &QAbstractItemModel::rowsRemoved, this, rebuildNow);
    connect(m_source, &QAbstractItemModel::rowsMoved, this, rebuildNow);
    connect(m_source, &QAbstractItemModel::layoutChanged, this, rebuildNow);
    connect(m_source, &PlaylistModel::sectionsChanged, this, rebuildNow);
    connect(m_source, &PlaylistModel::showHiddenChanged, this, rebuildNow);
    connect(m_source, &PlaylistModel::searchQueryChanged, this, rebuildNow);
    connect(m_source, &PlaylistModel::workspaceDateChanged, this, rebuildNow);
    connect(m_source, &QAbstractItemModel::dataChanged, this,
            &MeetingDayModel::onSourceDataChanged);
    rebuild();
}

int MeetingDayModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

bool MeetingDayModel::isMeetingDay() const
{
    return m_source != nullptr &&
           !MeetingSections::meetingSectionIds(m_source->currentMeetingKind(), false).isEmpty();
}

int MeetingDayModel::mediaCount() const
{
    return m_mediaCount;
}

int MeetingDayModel::sectionCount() const
{
    return m_sectionCount;
}

int MeetingDayModel::sourceRowAt(int row) const
{
    return row >= 0 && row < static_cast<int>(m_rows.size())
               ? m_rows.at(static_cast<size_t>(row)).sourceRow
               : -1;
}

int MeetingDayModel::rowForSourceRow(int sourceRow) const
{
    return sourceRow >= 0 && sourceRow < static_cast<int>(m_rowBySource.size())
               ? m_rowBySource.at(static_cast<size_t>(sourceRow))
               : -1;
}

int MeetingDayModel::headerRowOfSection(const QString& sectionId) const
{
    for (size_t i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].type == SectionHeaderRow && m_rows[i].sectionId == sectionId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

QVariant MeetingDayModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_rows.size())) {
        return {};
    }
    const Row& row = m_rows.at(static_cast<size_t>(index.row()));
    switch (role) {
    case RowTypeRole:
        return static_cast<int>(row.type);
    case SourceRowRole:
        return row.sourceRow;
    case RowSectionIdRole:
        return row.sectionId;
    case RowSectionLabelRole:
        return m_sectionInfo.value(row.sectionId).label;
    case RowSectionColorRole:
        return m_sectionInfo.value(row.sectionId).color;
    case RowSectionSubtitleRole:
        return m_sectionInfo.value(row.sectionId).documentTitle;
    case RowSectionCollapsedRole:
        return m_sectionInfo.value(row.sectionId).collapsed;
    case RowSectionIsMeetingRole:
        return MeetingSections::isMeetingSection(row.sectionId);
    case RowSectionIsCustomRole:
        return MeetingSections::isCustomSection(row.sectionId);
    case RowSectionVisibleCountRole:
        return row.visibleCount;
    case RowSectionHiddenCountRole:
        return row.hiddenCount;
    case RowSectionTotalCountRole:
        return row.totalCount;
    case RowSectionHasSongRole:
        return row.hasSong;
    case RowSectionIndexRole:
        return row.sectionIndex;
    case RowSectionCountRole:
        return m_sectionCount;
    case IsLastInSectionRole:
        return row.lastInSection;
    default:
        break;
    }
    if (row.type != MediaRow || row.sourceRow < 0 || m_source == nullptr) {
        return {};
    }
    return m_source->data(m_source->index(row.sourceRow), role);
}

QHash<int, QByteArray> MeetingDayModel::roleNames() const
{
    QHash<int, QByteArray> names =
        m_source != nullptr ? m_source->roleNames() : QHash<int, QByteArray>{};
    names.insert(RowTypeRole, "rowType");
    names.insert(SourceRowRole, "sourceRow");
    names.insert(RowSectionIdRole, "rowSectionId");
    names.insert(RowSectionLabelRole, "rowSectionLabel");
    names.insert(RowSectionColorRole, "rowSectionColor");
    names.insert(RowSectionSubtitleRole, "rowSectionSubtitle");
    names.insert(RowSectionCollapsedRole, "rowSectionCollapsed");
    names.insert(RowSectionIsMeetingRole, "rowSectionIsMeeting");
    names.insert(RowSectionIsCustomRole, "rowSectionIsCustom");
    names.insert(RowSectionVisibleCountRole, "rowSectionVisibleCount");
    names.insert(RowSectionHiddenCountRole, "rowSectionHiddenCount");
    names.insert(RowSectionTotalCountRole, "rowSectionTotalCount");
    names.insert(RowSectionHasSongRole, "rowSectionHasSong");
    names.insert(RowSectionIndexRole, "rowSectionIndex");
    names.insert(RowSectionCountRole, "rowSectionCount");
    names.insert(IsLastInSectionRole, "isLastInSection");
    return names;
}

std::vector<MeetingDayModel::Row> MeetingDayModel::computeRows(const QVariantList& sections) const
{
    std::vector<Row> rows;
    if (m_source == nullptr) {
        return rows;
    }

    struct SectionItems
    {
        std::vector<int> visible;
        int hidden{0};
        int total{0};
        bool hasSong{false};
    };
    QHash<QString, SectionItems> bySection;
    const bool showHidden = m_source->showHidden();
    for (int sourceRow = 0; sourceRow < m_source->rowCount(); ++sourceRow) {
        const QModelIndex index = m_source->index(sourceRow);
        SectionItems& items =
            bySection[m_source->data(index, PlaylistModel::SectionTitleRole).toString()];
        ++items.total;
        items.hasSong =
            items.hasSong ||
            m_source->data(index, PlaylistModel::TagTypeRole).toString() == QStringLiteral("song");
        if (!showHidden && m_source->data(index, PlaylistModel::HiddenRole).toBool()) {
            ++items.hidden;
        }
        if (m_source->data(index, PlaylistModel::RowVisibleRole).toBool()) {
            items.visible.push_back(sourceRow);
        }
    }

    // The meeting's own sections come first, in meeting order and even when empty; the other
    // sections follow in their stored order, on a meeting day only the ones holding items.
    const bool meetingDay = isMeetingDay();
    QStringList ordered;
    QSet<QString> known;
    for (const QVariant& section : sections) {
        known.insert(sectionIdOf(section));
    }
    for (const QString& id : MeetingSections::meetingSectionIds(m_source->currentMeetingKind(),
                                                                m_source->currentIsCoWeek())) {
        if (known.contains(id)) {
            ordered << id;
        }
    }
    for (const QVariant& section : sections) {
        const QString id = sectionIdOf(section);
        if (!ordered.contains(id) && (!meetingDay || bySection.value(id).total > 0)) {
            ordered << id;
        }
    }

    const bool searching = !m_source->searchQuery().trimmed().isEmpty();
    int sectionIndex = 0;
    for (const QString& id : ordered) {
        const SectionItems items = bySection.value(id);
        const SectionInfo info = m_sectionInfo.value(id);
        const auto makeRow = [&](RowType type, const QString& key, int sourceRow) {
            return Row{type,         key,          sourceRow,
                       id,           sectionIndex, static_cast<int>(items.visible.size()),
                       items.hidden, items.total,  items.hasSong,
                       false};
        };
        rows.push_back(makeRow(SectionHeaderRow, QStringLiteral("h:") + id, -1));
        if (!info.collapsed && !searching && (items.total == 0 || items.hidden > 0)) {
            rows.push_back(makeRow(SectionNoticeRow, QStringLiteral("n:") + id, -1));
        }
        for (const int sourceRow : items.visible) {
            const qint64 entryId =
                m_source->data(m_source->index(sourceRow), PlaylistModel::EntryIdRole).toLongLong();
            rows.push_back(makeRow(MediaRow,
                                   entryId > 0 ? QStringLiteral("m:%1").arg(entryId)
                                               : QStringLiteral("r:%1").arg(sourceRow),
                                   sourceRow));
        }
        rows.back().lastInSection = true;
        ++sectionIndex;
    }
    return rows;
}

void MeetingDayModel::rebuild()
{
    const QVariantList sections = m_source != nullptr ? m_source->sections() : QVariantList{};
    QHash<QString, SectionInfo> info;
    for (const QVariant& value : sections) {
        const QVariantMap section = value.toMap();
        info.insert(section.value(QStringLiteral("id")).toString(),
                    SectionInfo{section.value(QStringLiteral("name")).toString(),
                                section.value(QStringLiteral("color")).toString(),
                                section.value(QStringLiteral("documentTitle")).toString(),
                                section.value(QStringLiteral("collapsed")).toBool()});
    }
    m_sectionInfo = std::move(info);

    std::vector<Row> next = computeRows(sections);
    m_sectionCount = static_cast<int>(std::count_if(
        next.cbegin(), next.cend(), [](const Row& row) { return row.type == SectionHeaderRow; }));
    m_mediaCount = static_cast<int>(std::count_if(
        next.cbegin(), next.cend(), [](const Row& row) { return row.type == MediaRow; }));
    applyRows(std::move(next));

    m_rowBySource.assign(m_source != nullptr ? static_cast<size_t>(m_source->rowCount()) : 0U, -1);
    for (size_t i = 0; i < m_rows.size(); ++i) {
        const int sourceRow = m_rows[i].sourceRow;
        if (sourceRow >= 0 && sourceRow < static_cast<int>(m_rowBySource.size())) {
            m_rowBySource[static_cast<size_t>(sourceRow)] = static_cast<int>(i);
        }
    }
    // Labels, counts, and the forwarded item data may all have changed in place.
    if (!m_rows.empty()) {
        emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1));
    }
    emit layoutRebuilt();
}

void MeetingDayModel::applyRows(std::vector<Row> next)
{
    QHash<QString, int> nextIndex;
    nextIndex.reserve(static_cast<qsizetype>(next.size()));
    for (size_t i = 0; i < next.size(); ++i) {
        nextIndex.insert(next[i].key, static_cast<int>(i));
    }
    if (nextIndex.size() != static_cast<qsizetype>(next.size())) {
        beginResetModel();
        m_rows = std::move(next);
        endResetModel();
        return;
    }

    // Rows that stay point at their new source rows before anything moves, so the view never
    // reads another item's data halfway through the update.
    for (Row& row : m_rows) {
        const auto found = nextIndex.constFind(row.key);
        row.sourceRow =
            found != nextIndex.cend() ? next[static_cast<size_t>(*found)].sourceRow : -1;
    }

    for (int last = static_cast<int>(m_rows.size()) - 1; last >= 0;) {
        if (nextIndex.contains(m_rows[static_cast<size_t>(last)].key)) {
            --last;
            continue;
        }
        int first = last;
        while (first > 0 && !nextIndex.contains(m_rows[static_cast<size_t>(first - 1)].key)) {
            --first;
        }
        beginRemoveRows({}, first, last);
        m_rows.erase(m_rows.begin() + first, m_rows.begin() + last + 1);
        endRemoveRows();
        last = first - 1;
    }

    QSet<QString> present;
    for (const Row& row : m_rows) {
        present.insert(row.key);
    }
    for (int j = 0; j < static_cast<int>(next.size()); ++j) {
        const size_t target = static_cast<size_t>(j);
        if (target < m_rows.size() && m_rows[target].key == next[target].key) {
            m_rows[target] = next[target];
            continue;
        }
        if (present.contains(next[target].key)) {
            int from = j + 1;
            while (m_rows[static_cast<size_t>(from)].key != next[target].key) {
                ++from;
            }
            if (!beginMoveRows({}, from, from, {}, j)) {
                beginResetModel();
                m_rows = std::move(next);
                endResetModel();
                return;
            }
            m_rows.erase(m_rows.begin() + from);
            m_rows.insert(m_rows.begin() + j, next[target]);
            endMoveRows();
            continue;
        }
        int end = j;
        while (end + 1 < static_cast<int>(next.size()) &&
               !present.contains(next[static_cast<size_t>(end + 1)].key)) {
            ++end;
        }
        beginInsertRows({}, j, end);
        m_rows.insert(m_rows.begin() + j, next.begin() + j, next.begin() + end + 1);
        endInsertRows();
        j = end;
    }
}

void MeetingDayModel::onSourceDataChanged(const QModelIndex& topLeft,
                                          const QModelIndex& bottomRight, const QList<int>& roles)
{
    if (affectsLayout(roles)) {
        rebuild();
        return;
    }
    int first = -1;
    int last = -1;
    for (int sourceRow = topLeft.row(); sourceRow <= bottomRight.row(); ++sourceRow) {
        const int row = rowForSourceRow(sourceRow);
        if (row < 0) {
            continue;
        }
        first = first < 0 ? row : std::min(first, row);
        last = std::max(last, row);
    }
    if (first >= 0) {
        emit dataChanged(index(first), index(last), roles);
    }
}

} // namespace kmedia
