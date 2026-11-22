#include "network/WatchedFolderMedia.h"

#include "core/DatabaseManager.h"
#include "core/DatedFolderLayout.h"
#include "core/MeetingSections.h"
#include "models/PlaylistModel.h"
#include "parsers/CloudSyncWatcher.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace kmedia {

namespace {

const QString kWatched = QStringLiteral("watched");
constexpr double kUnplacedOrder = 1e9;

bool samePath(const QString& a, const QString& b)
{
    const QString left = QDir::cleanPath(QDir::fromNativeSeparators(a.trimmed()));
    const QString right = QDir::cleanPath(QDir::fromNativeSeparators(b.trimmed()));
    return !left.isEmpty() && left.compare(right, Qt::CaseInsensitive) == 0;
}

bool isKnownSection(const QString& sectionId)
{
    return MeetingSections::isMeetingSection(sectionId) ||
           MeetingSections::isCustomSection(sectionId) ||
           sectionId == MeetingSections::kImportedMedia;
}

QString mediaTypeFor(CloudSyncWatcher::MediaCategory category)
{
    switch (category) {
    case CloudSyncWatcher::MediaCategory::Video:
        return QStringLiteral("video");
    case CloudSyncWatcher::MediaCategory::Audio:
        return QStringLiteral("audio");
    case CloudSyncWatcher::MediaCategory::Pdf:
        return QStringLiteral("pdf");
    default:
        return QStringLiteral("image");
    }
}

} // namespace

WatchedFolderMedia::WatchedFolderMedia(CloudSyncWatcher* watcher, PlaylistModel* playlist,
                                       QObject* parent)
    : QObject(parent), m_watcher(watcher), m_playlist(playlist)
{
    // Files settle one by one while a cloud client syncs; each day is read once they calm down.
    m_dayTimer.setSingleShot(true);
    m_dayTimer.setInterval(300);
    connect(&m_dayTimer, &QTimer::timeout, this, &WatchedFolderMedia::flushDays);
    m_placementTimer.setSingleShot(true);
    m_placementTimer.setInterval(800);
    connect(&m_placementTimer, &QTimer::timeout, this, &WatchedFolderMedia::writePlacements);

    if (!m_watcher.isNull()) {
        connect(m_watcher, &CloudSyncWatcher::datedMediaDiscovered, this,
                [this](const QString&, const QDate& date, CloudSyncWatcher::MediaCategory,
                       const QString&) { scheduleDay(date); });
        connect(m_watcher, &CloudSyncWatcher::mediaRemoved, this,
                [this](const QString& filePath, const QString&) {
                    scheduleDay(DatedFolderLayout::dateForFile(filePath));
                });
    }
    if (!m_playlist.isNull()) {
        const auto placementsMayChange = [this]() {
            if (isActive()) {
                m_placementTimer.start();
            }
        };
        connect(m_playlist, &QAbstractItemModel::rowsMoved, this, placementsMayChange);
        connect(m_playlist, &PlaylistModel::sectionsChanged, this, placementsMayChange);
        connect(
            m_playlist, &QAbstractItemModel::dataChanged, this,
            [placementsMayChange](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
                if (roles.contains(PlaylistModel::SectionTitleRole)) {
                    placementsMayChange();
                }
            });
    }
}

void WatchedFolderMedia::setDatabase(DatabaseManager* database)
{
    m_database = database;
    apply();
}

void WatchedFolderMedia::setScheduleProviders(MeetingKindProvider meetingKind,
                                              CoWeekProvider coWeek)
{
    m_meetingKind = std::move(meetingKind);
    m_coWeek = std::move(coWeek);
}

void WatchedFolderMedia::setToday(const QDate& today)
{
    m_today = today;
}

void WatchedFolderMedia::setBlockedFolder(const QString& folder)
{
    if (m_blockedFolder == folder) {
        return;
    }
    m_blockedFolder = folder;
    apply();
}

bool WatchedFolderMedia::isEnabled() const
{
    return m_enabled;
}

void WatchedFolderMedia::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    emit settingsChanged();
    apply();
}

QString WatchedFolderMedia::folder() const
{
    return m_folder;
}

void WatchedFolderMedia::setFolder(const QString& folder)
{
    // The folder dialog hands over a file URL; typed paths come as they are.
    const QString raw = folder.trimmed().startsWith(QStringLiteral("file:"))
                            ? QUrl(folder.trimmed()).toLocalFile()
                            : folder.trimmed();
    const QString clean =
        raw.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(raw));
    if (m_folder == clean) {
        return;
    }
    m_folder = clean;
    emit settingsChanged();
    apply();
}

bool WatchedFolderMedia::deletePastFolders() const
{
    return m_deletePastFolders;
}

void WatchedFolderMedia::setDeletePastFolders(bool deletePast)
{
    if (m_deletePastFolders == deletePast) {
        return;
    }
    m_deletePastFolders = deletePast;
    emit settingsChanged();
    apply();
}

bool WatchedFolderMedia::isActive() const
{
    return m_enabled && !m_folder.isEmpty() && QFileInfo(m_folder).isDir() &&
           !samePath(m_folder, m_blockedFolder);
}

QDate WatchedFolderMedia::today() const
{
    return m_today.isValid() ? m_today : QDate::currentDate();
}

QString WatchedFolderMedia::dayFolderFor(const QDate& date) const
{
    return QDir(m_folder).filePath(DatedFolderLayout::dayFolderName(date));
}

QString WatchedFolderMedia::defaultSection(const QString& meetingKind, bool coWeek)
{
    if (meetingKind == QStringLiteral("weekend")) {
        return coWeek ? QString(MeetingSections::kCircuitOverseer)
                      : QString(MeetingSections::kPublicTalk);
    }
    if (meetingKind == QStringLiteral("midweek")) {
        return coWeek ? QString(MeetingSections::kCircuitOverseer)
                      : QString(MeetingSections::kLivingAsChristians);
    }
    return MeetingSections::kImportedMedia;
}

// "02 Watchtower Study - 01 Picture.jpg" was exported by KMedia or Meeting Media Manager; the
// title is what follows the numbers.
QString WatchedFolderMedia::titleFromFileName(const QString& fileName)
{
    static const QRegularExpression kExportedPrefix(QStringLiteral("^\\d+ .+? - \\d+ "));
    QString title = QFileInfo(fileName).completeBaseName();
    if (DatedFolderLayout::parseExportedFileName(fileName).has_value()) {
        title.remove(kExportedPrefix);
    }
    return title.trimmed().isEmpty() ? QFileInfo(fileName).completeBaseName() : title.trimmed();
}

void WatchedFolderMedia::apply()
{
    if (m_watcher.isNull()) {
        return;
    }
    m_watcher->clearWatchedFolders();
    if (m_enabled && !m_folder.isEmpty() && samePath(m_folder, m_blockedFolder)) {
        emit folderConflict();
    }
    if (isActive()) {
        if (m_deletePastFolders) {
            (void)DatedFolderLayout::pruneDayFolders(m_folder, today());
        }
        m_watcher->setDayFoldersOnly(true);
        (void)m_watcher->addWatchedFolder(m_folder);
    }
    reconcileAll();
}

void WatchedFolderMedia::reconcileAll()
{
    if (m_database == nullptr || !m_database->isOpen()) {
        return;
    }
    // Days that hold watched media and the day folders on disk; with watching off, the first
    // set loses its watched media.
    QSet<QDate> days;
    for (const QString& dateIso : m_database->listDatesWithSource(kWatched)) {
        days.insert(QDate::fromString(dateIso, Qt::ISODate));
    }
    if (isActive()) {
        const QStringList folders = QDir(m_folder).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& name : folders) {
            if (DatedFolderLayout::isDayFolderName(name) &&
                !DatedFolderLayout::isLongPastDayFolder(name, today())) {
                days.insert(DatedFolderLayout::dateFromDayFolderName(name));
            }
        }
    }
    std::vector<QDate> ordered(days.cbegin(), days.cend());
    std::sort(ordered.begin(), ordered.end());
    for (const QDate& date : ordered) {
        syncDay(date);
    }
}

void WatchedFolderMedia::scheduleDay(const QDate& date)
{
    if (!date.isValid()) {
        return;
    }
    m_pendingDays.insert(date);
    m_dayTimer.start();
}

void WatchedFolderMedia::flushDays()
{
    const QSet<QDate> days = std::exchange(m_pendingDays, {});
    for (const QDate& date : days) {
        syncDay(date);
    }
}

void WatchedFolderMedia::syncDay(const QDate& date)
{
    if (m_database == nullptr || !m_database->isOpen() || !date.isValid()) {
        return;
    }
    const QString dateIso = date.toString(Qt::ISODate);
    QString kind = m_meetingKind ? m_meetingKind(date) : QString();
    if (kind.isEmpty()) {
        kind = QStringLiteral("custom");
    }
    const bool coWeek = m_coWeek && m_coWeek(date);
    const QString dayFolder = isActive() ? dayFolderFor(date) : QString();

    // The media files of the day folder, by clean path.
    QHash<QString, QFileInfo> files;
    if (!dayFolder.isEmpty()) {
        const QFileInfoList entries =
            QDir(dayFolder).entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& info : entries) {
            const auto category = CloudSyncWatcher::classifyMediaFile(info.absoluteFilePath());
            if (category == CloudSyncWatcher::MediaCategory::Unknown ||
                category == CloudSyncWatcher::MediaCategory::Jwpub ||
                category == CloudSyncWatcher::MediaCategory::JwlPlaylist ||
                info.fileName().startsWith(QLatin1Char('.'))) {
                continue;
            }
            files.insert(QDir::cleanPath(info.absoluteFilePath()), info);
        }
    }

    std::optional<PlaylistRecord> playlist = m_database->findPlaylistByDateAndType(dateIso, kind);
    if (!playlist.has_value()) {
        if (files.isEmpty()) {
            return;
        }
        playlist = m_database->findOrCreatePlaylistForDate(
            dateIso, kind, QStringLiteral("%1 | %2").arg(dateIso, kind.toUpper()));
        if (playlist->id <= 0) {
            return;
        }
    }

    const std::vector<PlaylistItemRow> rows = m_database->listPlaylistItemRows(playlist->id);
    const QHash<QString, DatedFilePlacement> placements =
        dayFolder.isEmpty() ? QHash<QString, DatedFilePlacement>{}
                            : DatedFolderLayout::readSectionOrder(dayFolder);
    const QStringList meetingIds = MeetingSections::meetingSectionIds(kind, coWeek);

    // Where a file goes: the folder's placements, then an exported file name, then the default.
    const auto placementFor = [&](const QString& fileName, double* order, bool* explicitPlacement) {
        *order = kUnplacedOrder;
        *explicitPlacement = false;
        const DatedFilePlacement placement = placements.value(fileName);
        if (isKnownSection(placement.sectionId)) {
            *explicitPlacement = true;
            if (placement.hasOrder) {
                *order = placement.order;
            }
            return placement.sectionId;
        }
        if (const QString legacy = DatedFolderLayout::legacySectionId(fileName);
            isKnownSection(legacy)) {
            return legacy;
        }
        if (const auto parts = DatedFolderLayout::parseExportedFileName(fileName);
            parts.has_value()) {
            *order = parts->order();
            for (const QString& id : meetingIds) {
                if (MeetingSections::translatedLabel(id).compare(parts->sectionLabel,
                                                                 Qt::CaseInsensitive) == 0) {
                    return id;
                }
            }
            if (const QString known = MeetingSections::idForLegacyTitle(parts->sectionLabel);
                meetingIds.contains(known)) {
                return known;
            }
        }
        return defaultSection(kind, coWeek);
    };

    bool changed = false;
    (void)m_database->executeTransaction([&](QSqlDatabase&) {
        QSet<QString> known;
        for (const PlaylistItemRow& row : rows) {
            if (row.entry.source != kWatched) {
                continue;
            }
            const QString path = QDir::cleanPath(row.entry.uniqueKey);
            if (!files.contains(path)) {
                // Gone from the folder, gone from the day.
                changed = m_database->deletePlaylistEntry(row.entry.id, playlist->id) || changed;
                continue;
            }
            known.insert(path);
            // A placement written on another computer moves the item here too.
            double order = kUnplacedOrder;
            bool explicitPlacement = false;
            const QString section =
                placementFor(files.value(path).fileName(), &order, &explicitPlacement);
            if (explicitPlacement && section != row.entry.sectionId) {
                changed =
                    m_database->updatePlaylistEntrySection(row.entry.id, playlist->id, section) ||
                    changed;
            }
        }

        struct NewFile
        {
            QString path;
            QFileInfo info;
            QString section;
            double order{kUnplacedOrder};
        };
        std::vector<NewFile> added;
        for (auto it = files.cbegin(); it != files.cend(); ++it) {
            if (known.contains(it.key())) {
                continue;
            }
            NewFile file{it.key(), it.value(), {}, kUnplacedOrder};
            bool explicitPlacement = false;
            file.section = placementFor(it.value().fileName(), &file.order, &explicitPlacement);
            added.push_back(std::move(file));
        }
        std::sort(added.begin(), added.end(), [](const NewFile& a, const NewFile& b) {
            if (a.order != b.order) {
                return a.order < b.order;
            }
            return a.info.fileName().localeAwareCompare(b.info.fileName()) < 0;
        });

        int sortOrder = rows.empty() ? 0 : rows.back().entry.sortOrder + 1;
        for (const NewFile& file : added) {
            MediaItemRecord media;
            media.filePath = file.path;
            media.title = titleFromFileName(file.info.fileName());
            media.mediaType = mediaTypeFor(CloudSyncWatcher::classifyMediaFile(file.path));
            media.fileSize = file.info.size();
            media.sourceKind = kWatched;
            PlaylistEntryRecord entry;
            entry.playlistId = playlist->id;
            entry.mediaItemId = m_database->upsertMediaItem(media);
            entry.sectionId = file.section;
            entry.sectionTitle = MeetingSections::translatedLabel(file.section);
            entry.originalSection = file.section;
            entry.customTitle = media.title;
            entry.sortOrder = sortOrder;
            entry.sortOrderOriginal = sortOrder;
            entry.source = kWatched;
            entry.uniqueKey = file.path;
            if (entry.mediaItemId > 0 && m_database->addPlaylistEntry(entry) > 0) {
                ++sortOrder;
                changed = true;
            }
        }
        return true;
    });
    if (changed) {
        emit dayChanged(dateIso);
    }
}

void WatchedFolderMedia::prepareDayFolders(const std::vector<QDate>& meetingDays)
{
    if (!isActive()) {
        return;
    }
    if (m_deletePastFolders) {
        (void)DatedFolderLayout::pruneDayFolders(m_folder, today());
    }
    // Upcoming meeting days get their folder, so media can be dropped in ahead of time.
    for (const QDate& date : meetingDays) {
        if (date >= today()) {
            (void)QDir().mkpath(dayFolderFor(date));
        }
    }
}

void WatchedFolderMedia::writePlacements()
{
    if (!isActive() || m_playlist.isNull()) {
        return;
    }
    const QDate date = QDate::fromString(m_playlist->currentDateIso(), Qt::ISODate);
    const QString dayFolder = date.isValid() ? dayFolderFor(date) : QString();
    if (dayFolder.isEmpty() || !QFileInfo(dayFolder).isDir()) {
        return;
    }

    // Orders follow the meeting page: section by section, then item by item.
    const QString kind = m_playlist->currentMeetingKind();
    QStringList sectionOrder =
        MeetingSections::meetingSectionIds(kind, m_playlist->currentIsCoWeek());
    for (const QVariant& section : m_playlist->sections()) {
        const QString id = section.toMap().value(QStringLiteral("id")).toString();
        if (!sectionOrder.contains(id)) {
            sectionOrder << id;
        }
    }
    QHash<QString, int> itemsPerSection;
    QHash<QString, DatedFilePlacement> placements;
    for (int row = 0; row < m_playlist->rowCount(); ++row) {
        const QModelIndex index = m_playlist->index(row, 0);
        const QString section = m_playlist->data(index, PlaylistModel::SectionTitleRole).toString();
        const int position = ++itemsPerSection[section];
        if (m_playlist->data(index, PlaylistModel::SourceRole).toString() != kWatched) {
            continue;
        }
        const QFileInfo file(m_playlist->data(index, PlaylistModel::FilePathRole).toString());
        if (QDir::cleanPath(file.absolutePath())
                .compare(QDir::cleanPath(dayFolder), Qt::CaseInsensitive) != 0) {
            continue;
        }
        const int sectionIndex = static_cast<int>(sectionOrder.indexOf(section)) + 1;
        placements.insert(file.fileName(),
                          DatedFilePlacement{section, sectionIndex * 1000.0 + position, true});
    }
    if (placements.isEmpty()) {
        return;
    }
    // Writing only real changes keeps cloud clients from syncing the file after every reload.
    const QHash<QString, DatedFilePlacement> current =
        DatedFolderLayout::readSectionOrder(dayFolder);
    for (auto it = placements.cbegin(); it != placements.cend(); ++it) {
        const DatedFilePlacement existing = current.value(it.key());
        if (existing.sectionId != it.value().sectionId || !existing.hasOrder ||
            std::abs(existing.order - it.value().order) >= 0.5) {
            (void)DatedFolderLayout::mergeSectionOrder(dayFolder, placements);
            return;
        }
    }
}

} // namespace kmedia
