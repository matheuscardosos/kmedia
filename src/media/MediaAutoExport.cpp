#include "media/MediaAutoExport.h"

#include "core/DatabaseManager.h"
#include "core/DatedFolderLayout.h"
#include "core/MeetingSections.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QUrl>

#include <memory>

namespace kmedia {

MediaAutoExport::MediaAutoExport(QObject* parent)
    : QObject(parent), m_ffmpegAvailable(!PlaylistExporter::findFfmpegExecutable().isEmpty())
{
    m_pool.setMaxThreadCount(kParallelDays);
}

MediaAutoExport::~MediaAutoExport()
{
    m_pool.waitForDone();
}

void MediaAutoExport::setDatabase(DatabaseManager* database)
{
    m_database = database;
}

void MediaAutoExport::setScheduleProviders(MeetingKindProvider meetingKind, CoWeekProvider coWeek)
{
    m_meetingKind = std::move(meetingKind);
    m_coWeek = std::move(coWeek);
}

void MediaAutoExport::setConversionCacheDir(const QString& cacheDir)
{
    m_cacheDir = cacheDir;
}

void MediaAutoExport::setSongLabel(const QString& songLabel)
{
    m_songLabel = songLabel;
}

void MediaAutoExport::setToday(const QDate& today)
{
    m_today = today;
}

bool MediaAutoExport::isEnabled() const
{
    return m_enabled;
}

void MediaAutoExport::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit settingsChanged();
    }
}

QString MediaAutoExport::folder() const
{
    return m_folder;
}

void MediaAutoExport::setFolder(const QString& folder)
{
    // The folder dialog hands over a file URL; typed paths come as they are.
    const QString raw = folder.trimmed().startsWith(QStringLiteral("file:"))
                            ? QUrl(folder.trimmed()).toLocalFile()
                            : folder.trimmed();
    const QString clean =
        raw.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(raw));
    if (m_folder != clean) {
        m_folder = clean;
        emit settingsChanged();
    }
}

bool MediaAutoExport::convertToMp4() const
{
    return m_convertToMp4;
}

void MediaAutoExport::setConvertToMp4(bool convert)
{
    if (m_convertToMp4 != convert) {
        m_convertToMp4 = convert;
        emit settingsChanged();
    }
}

bool MediaAutoExport::isFfmpegAvailable() const
{
    return m_ffmpegAvailable;
}

bool MediaAutoExport::isActive() const
{
    return m_enabled && !m_folder.isEmpty() && m_database != nullptr && m_database->isOpen();
}

QDate MediaAutoExport::today() const
{
    return m_today.isValid() ? m_today : QDate::currentDate();
}

// The day's sections in the order of the meeting page, each with its media in day order.
std::vector<ExportSection> MediaAutoExport::sectionsForDay(const QDate& date) const
{
    std::vector<ExportSection> sections;
    if (m_database == nullptr || !m_database->isOpen() || !date.isValid()) {
        return sections;
    }
    QString kind = m_meetingKind ? m_meetingKind(date) : QString();
    if (kind.isEmpty()) {
        kind = QStringLiteral("custom");
    }
    const bool coWeek = m_coWeek && m_coWeek(date);
    const auto playlist = m_database->findPlaylistByDateAndType(date.toString(Qt::ISODate), kind);
    if (!playlist.has_value()) {
        return sections;
    }

    QStringList order = MeetingSections::meetingSectionIds(kind, coWeek);
    QHash<QString, QString> customLabels;
    for (const QJsonValue& value :
         QJsonDocument::fromJson(playlist->sectionsJson.toUtf8()).array()) {
        const QJsonObject section = value.toObject();
        const QString id = section.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue;
        }
        if (!order.contains(id)) {
            order << id;
        }
        const QString label = section.value(QStringLiteral("label")).toString();
        if (!label.isEmpty()) {
            customLabels.insert(id, label);
        }
    }

    QHash<QString, std::vector<ExportMediaItem>> itemsBySection;
    for (const PlaylistItemRow& row : m_database->listPlaylistItemRows(playlist->id)) {
        if (row.entry.itemKind == QStringLiteral("divider") || !row.media.has_value() ||
            row.media->filePath.isEmpty()) {
            continue;
        }
        const QString sectionId = row.entry.sectionId.isEmpty()
                                      ? QString(MeetingSections::kImportedMedia)
                                      : row.entry.sectionId;
        if (!order.contains(sectionId)) {
            order << sectionId;
        }
        ExportMediaItem item;
        item.filePath = row.media->filePath;
        item.title = row.entry.customTitle.isEmpty() ? row.media->title : row.entry.customTitle;
        if (row.entry.tagType == QStringLiteral("song")) {
            item.songNumber = row.entry.tagValue;
        }
        item.hidden = row.entry.hidden;
        itemsBySection[sectionId].push_back(std::move(item));
    }
    for (const QString& id : order) {
        auto items = itemsBySection.take(id);
        if (items.empty()) {
            continue;
        }
        sections.push_back(ExportSection{
            customLabels.value(id, MeetingSections::translatedLabel(id)), std::move(items)});
    }
    return sections;
}

void MediaAutoExport::exportDays(const std::vector<QDate>& days)
{
    if (!isActive()) {
        return;
    }
    // Folders of weeks that are over are not needed by anyone anymore.
    (void)DatedFolderLayout::pruneDayFolders(m_folder, today());
    for (const QDate& date : days) {
        if (date >= today().addDays(-(today().dayOfWeek() - 1)) && !sectionsForDay(date).empty()) {
            exportDay(date.toString(Qt::ISODate));
        }
    }
}

void MediaAutoExport::exportDay(const QString& dateIso)
{
    if (!isActive() || !QDate::fromString(dateIso, Qt::ISODate).isValid()) {
        return;
    }
    if (m_running.contains(dateIso)) {
        // Exported again once the running export is done.
        m_dirty.insert(dateIso);
        return;
    }
    start(dateIso);
}

void MediaAutoExport::start(const QString& dateIso)
{
    const QDate date = QDate::fromString(dateIso, Qt::ISODate);
    std::vector<ExportSection> sections = sectionsForDay(date);
    DayExportOptions options;
    options.date = date;
    options.exportRoot = m_folder;
    options.songLabel = m_songLabel;
    options.convertFilesToMp4 = m_convertToMp4;
    options.conversionCacheDir = m_cacheDir;

    m_running.insert(dateIso);
    auto result = std::make_shared<DayExportResult>();
    m_pool.start([this, dateIso, sections = std::move(sections), options, result]() {
        *result = PlaylistExporter::exportDay(sections, options);
        QMetaObject::invokeMethod(
            this, [this, dateIso, result]() { finish(dateIso, *result); }, Qt::QueuedConnection);
    });
}

void MediaAutoExport::finish(const QString& dateIso, const DayExportResult& result)
{
    m_running.remove(dateIso);
    emit dayExported(dateIso, result.copiedCount,
                     static_cast<int>(result.conversionFailures.size()));
    if (m_dirty.remove(dateIso)) {
        start(dateIso);
    }
}

void MediaAutoExport::waitForIdle()
{
    while (!m_running.isEmpty()) {
        m_pool.waitForDone();
        QCoreApplication::processEvents();
    }
}

} // namespace kmedia
