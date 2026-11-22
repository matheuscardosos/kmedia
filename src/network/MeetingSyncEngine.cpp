#include "network/MeetingSyncEngine.h"

#include "core/DatabaseManager.h"
#include "core/MeetingSections.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QRegularExpression>

#include <algorithm>

namespace kmedia {

namespace {

const QString kComplete = QStringLiteral("complete");
const QString kError = QStringLiteral("error");
const QString kSyncing = QStringLiteral("syncing");
const QString kPending = QStringLiteral("pending");
const QString kAddedMedia = QStringLiteral("media");
const QString kDynamic = QStringLiteral("dynamic");

QDate mondayOf(const QDate& date)
{
    return date.addDays(-(date.dayOfWeek() - 1));
}

bool isMeetingKind(const QString& kind)
{
    return kind == QStringLiteral("midweek") || kind == QStringLiteral("weekend") ||
           kind == QStringLiteral("memorial");
}

QString resolveInFolder(const QString& folder, const QString& relativePath)
{
    if (relativePath.trimmed().isEmpty()) {
        return {};
    }
    const QString path = QDir(folder).filePath(relativePath);
    return QFileInfo::exists(path) ? QDir::cleanPath(path) : QString();
}

void resolveRowPaths(std::vector<PublicationMediaRow>& rows, const QString& folder)
{
    for (PublicationMediaRow& row : rows) {
        row.filePath = resolveInFolder(folder, row.filePath);
        row.linkedPreviewFilePath = resolveInFolder(folder, row.linkedPreviewFilePath);
    }
}

QString mediaTypeFor(const DayMediaItem& item)
{
    QString mime = item.mimeType;
    if (mime.isEmpty() && !item.filePath.isEmpty()) {
        mime = QMimeDatabase().mimeTypeForFile(item.filePath, QMimeDatabase::MatchExtension).name();
    }
    if (mime.startsWith(QStringLiteral("video"))) {
        return QStringLiteral("video");
    }
    if (mime.startsWith(QStringLiteral("audio"))) {
        return QStringLiteral("audio");
    }
    if (mime.contains(QStringLiteral("pdf"))) {
        return QStringLiteral("pdf");
    }
    return item.streamUrl.isEmpty() ? QStringLiteral("image") : QStringLiteral("video");
}

PlaylistEntryRecord entryFromItem(qint64 playlistId, const DayMediaItem& item)
{
    PlaylistEntryRecord entry;
    entry.playlistId = playlistId;
    entry.sectionId = item.sectionId;
    entry.sectionTitle = MeetingSections::translatedLabel(item.sectionId);
    entry.customTitle = item.title;
    entry.groupKey = item.groupKey;
    entry.hidden = item.hidden;
    entry.tagType = item.tagType;
    entry.tagValue = item.tagValue;
    entry.pubMediaId = item.pubMediaId;
    entry.streamUrl = item.streamUrl;
    entry.markerRangesJson = item.markerRangesJson;
    entry.isCbs = item.isCbs;
    entry.originalSection = item.sectionId;
    entry.sortOrderOriginal = item.sortOrderOriginal;
    entry.source = kDynamic;
    entry.uniqueKey = item.uniqueKey;
    entry.thumbnailPath = item.thumbnailPath;
    return entry;
}

// Adds the meeting sections a day needs (in meeting order) and the study article title.
QString updatedSectionsJson(const QString& sectionsJson, const QStringList& meetingSections,
                            const QString& documentTitle)
{
    QJsonArray sections = QJsonDocument::fromJson(sectionsJson.toUtf8()).array();
    QSet<QString> present;
    for (const QJsonValue& value : sections) {
        present.insert(value.toObject().value(QStringLiteral("id")).toString());
    }
    QJsonArray ordered;
    for (const QString& id : meetingSections) {
        if (present.contains(id)) {
            continue;
        }
        QJsonObject section;
        section.insert(QStringLiteral("id"), id);
        section.insert(QStringLiteral("color"), MeetingSections::defaultColor(id));
        ordered.append(section);
    }
    for (const QJsonValue& value : sections) {
        ordered.append(value);
    }
    for (qsizetype i = 0; i < ordered.size(); ++i) {
        QJsonObject section = ordered.at(i).toObject();
        if (section.value(QStringLiteral("id")).toString() == MeetingSections::kWatchtowerStudy &&
            !documentTitle.isEmpty()) {
            section.insert(QStringLiteral("documentTitle"), documentTitle);
            ordered.replace(i, section);
        }
    }
    return QString::fromUtf8(QJsonDocument(ordered).toJson(QJsonDocument::Compact));
}

} // namespace

MeetingSyncEngine::MeetingSyncEngine(PublicationStore* store, QObject* parent)
    : QObject(parent), m_store(store)
{
    if (m_store != nullptr) {
        connect(m_store, &PublicationStore::mediaFileReady, this,
                &MeetingSyncEngine::onMediaFileReady);
    }
}

MeetingSyncEngine::~MeetingSyncEngine() = default;

void MeetingSyncEngine::setDatabase(DatabaseManager* database)
{
    if (m_database == database) {
        return;
    }
    // Work in flight belongs to the previous profile: let it finish without writing.
    ++m_generation;
    m_queue.clear();
    m_active.clear();
    m_statuses.clear();
    m_addedMediaDays.clear();
    m_daysByFile.clear();
    m_database = database;
    emit dayStatusesChanged();
    emit busyChanged();
}

void MeetingSyncEngine::setSettings(const Settings& settings)
{
    m_settings = settings;
}

const MeetingSyncEngine::Settings& MeetingSyncEngine::settings() const
{
    return m_settings;
}

void MeetingSyncEngine::setScheduleProviders(MeetingKindProvider meetingKind, CoWeekProvider coWeek)
{
    m_meetingKind = std::move(meetingKind);
    m_coWeek = std::move(coWeek);
}

void MeetingSyncEngine::setToday(const QDate& today)
{
    m_today = today;
}

QDate MeetingSyncEngine::today() const
{
    return m_today.isValid() ? m_today : QDate::currentDate();
}

bool MeetingSyncEngine::isBusy() const
{
    return !m_queue.empty() || !m_active.isEmpty();
}

QVariantMap MeetingSyncEngine::dayStatuses() const
{
    QVariantMap statuses;
    for (auto it = m_statuses.constBegin(); it != m_statuses.constEnd(); ++it) {
        statuses.insert(it.key(), it.value());
    }
    // A finished or failed meeting keeps its own color; otherwise added media marks the day.
    for (const QString& dateIso : m_addedMediaDays) {
        const QString status = m_statuses.value(dateIso);
        if (status != kComplete && status != kError) {
            statuses.insert(dateIso, kAddedMedia);
        }
    }
    return statuses;
}

QString MeetingSyncEngine::windowStart() const
{
    const std::vector<QDate> window = lookupWindow(today(), m_settings.meteredConnection);
    return window.empty() ? QString() : window.front().toString(Qt::ISODate);
}

QString MeetingSyncEngine::windowEnd() const
{
    const std::vector<QDate> window = lookupWindow(today(), m_settings.meteredConnection);
    return window.empty() ? QString() : window.back().toString(Qt::ISODate);
}

void MeetingSyncEngine::refreshAddedMediaDays()
{
    QSet<QString> days;
    if (m_database != nullptr && m_database->isOpen()) {
        const QStringList dates = m_database->listDatesWithAddedMedia();
        days = QSet<QString>(dates.cbegin(), dates.cend());
    }
    if (days != m_addedMediaDays) {
        m_addedMediaDays = std::move(days);
        emit dayStatusesChanged();
    }
}

QString MeetingSyncEngine::statusForDate(const QString& dateIso) const
{
    return m_statuses.value(dateIso);
}

std::vector<QDate> MeetingSyncEngine::lookupWindow(const QDate& today, bool metered)
{
    // From this week's Monday: 35 days (1 on a metered connection) plus the weekday offset,
    // counting Sunday as 0 the way the meeting calendar does.
    const int weekdayOffset = today.dayOfWeek() % 7;
    const int daysAhead = (metered ? 1 : kDaysInFuture) + weekdayOffset;
    std::vector<QDate> days;
    const QDate monday = mondayOf(today);
    days.reserve(static_cast<size_t>(daysAhead));
    for (int i = 0; i < daysAhead; ++i) {
        days.push_back(monday.addDays(i));
    }
    return days;
}

int MeetingSyncEngine::midweekIssueTag(const QDate& weekMonday)
{
    // Workbooks are bimonthly: an even month reads the issue of the month before.
    const int month = weekMonday.month() % 2 == 0 ? weekMonday.month() - 1 : weekMonday.month();
    return (weekMonday.year() * 100) + month;
}

std::vector<int> MeetingSyncEngine::watchtowerIssueTags(const QDate& weekMonday)
{
    std::vector<int> tags;
    for (const int weeks : {6, 8, 10, 12}) {
        const QDate issueDate = weekMonday.addDays(-7LL * weeks);
        const int tag = (issueDate.year() * 100) + issueDate.month();
        if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
            tags.push_back(tag);
        }
    }
    return tags;
}

QString MeetingSyncEngine::extractPublicationSymbol(const QString& uniqueEnglishSymbol,
                                                    int issueTagNumber)
{
    static const QRegularExpression kNonAlphanumeric(QStringLiteral("[^a-zA-Z0-9]"));
    static const QRegularExpression kDigits(QStringLiteral("[0-9]"));
    QString symbol = uniqueEnglishSymbol.contains(kNonAlphanumeric)
                         ? uniqueEnglishSymbol
                         : QString(uniqueEnglishSymbol).remove(kDigits);
    const QString issue = QString::number(issueTagNumber);
    if (symbol == QStringLiteral("w") && issueTagNumber >= 20080101 &&
        issue.endsWith(QStringLiteral("01"))) {
        symbol = QStringLiteral("wp");
    }
    return symbol;
}

QString MeetingSyncEngine::playlistNameFor(const QString& dateIso, const QString& kind)
{
    return QStringLiteral("%1 | %2").arg(dateIso, kind.toUpper());
}

QStringList MeetingSyncEngine::languages(const QString& preferred) const
{
    QStringList list;
    for (const QString& code :
         {m_settings.languageCode, preferred, m_settings.fallbackLanguageCode}) {
        if (!code.trimmed().isEmpty() && !list.contains(code.trimmed())) {
            list.append(code.trimmed());
        }
    }
    return list;
}

DownloadService::Priority MeetingSyncEngine::priorityFor(const QDate& date) const
{
    const qint64 daysAway = today().daysTo(date);
    if (daysAway <= 1) {
        return DownloadService::Priority::High;
    }
    return daysAway <= 7 ? DownloadService::Priority::Normal : DownloadService::Priority::Low;
}

void MeetingSyncEngine::setStatus(const QString& dateIso, const QString& status)
{
    if (m_statuses.value(dateIso) == status) {
        return;
    }
    m_statuses.insert(dateIso, status);
    emit dayStatusChanged(dateIso, status);
    emit dayStatusesChanged();
}

bool MeetingSyncEngine::hasMissingSyncedFiles(qint64 playlistId) const
{
    for (const PlaylistItemRow& row : m_database->listPlaylistItemRows(playlistId)) {
        if (row.entry.source == kDynamic && row.entry.streamUrl.isEmpty() &&
            (!row.media.has_value() || !QFileInfo::exists(row.media->filePath))) {
            return true;
        }
    }
    return false;
}

void MeetingSyncEngine::refresh()
{
    if (m_database == nullptr || !m_database->isOpen() || !m_meetingKind) {
        return;
    }
    const QDate now = today();
    for (const QDate& date : lookupWindow(now, m_settings.meteredConnection)) {
        const QString kind = m_meetingKind(date);
        if (!isMeetingKind(kind)) {
            continue;
        }
        const QString dateIso = date.toString(Qt::ISODate);
        const bool queued = std::any_of(m_queue.begin(), m_queue.end(),
                                        [&](const JobPtr& job) { return job->dateIso == dateIso; });
        if (queued || m_active.contains(dateIso)) {
            continue;
        }

        PlaylistRecord playlist =
            m_database->findOrCreatePlaylistForDate(dateIso, kind, playlistNameFor(dateIso, kind));
        if (playlist.id <= 0) {
            continue;
        }
        const bool past = date < now;
        if (past && playlist.syncStatus == kError) {
            // A past meeting that failed is not retried.
            (void)m_database->setPlaylistSyncStatus(playlist.id, kComplete);
            playlist.syncStatus = kComplete;
        }
        setStatus(dateIso, playlist.syncStatus.isEmpty() ? kPending : playlist.syncStatus);

        const bool skipMetered = m_settings.meteredConnection && now.daysTo(date) > 1;
        const bool needsSync =
            playlist.syncStatus != kComplete || (!past && hasMissingSyncedFiles(playlist.id));
        if (needsSync && !skipMetered) {
            enqueue(date, kind, playlist.id);
        }
    }
    refreshAddedMediaDays();
    pump();
}

void MeetingSyncEngine::resyncDay(const QString& dateIso)
{
    if (m_database == nullptr || !m_database->isOpen() || !m_meetingKind) {
        return;
    }
    const QDate date = QDate::fromString(dateIso, Qt::ISODate);
    const QString kind = date.isValid() ? m_meetingKind(date) : QString();
    if (!isMeetingKind(kind) || m_active.contains(dateIso)) {
        return;
    }
    const PlaylistRecord playlist =
        m_database->findOrCreatePlaylistForDate(dateIso, kind, playlistNameFor(dateIso, kind));
    if (playlist.id <= 0) {
        return;
    }
    std::erase_if(m_queue, [&](const JobPtr& job) { return job->dateIso == dateIso; });
    (void)m_database->setPlaylistSyncStatus(playlist.id, QString());
    enqueue(date, kind, playlist.id);
    pump();
}

void MeetingSyncEngine::resetAll()
{
    if (m_database == nullptr || !m_database->isOpen() || !m_meetingKind) {
        return;
    }
    ++m_generation;
    m_queue.clear();
    m_active.clear();
    for (const QDate& date : lookupWindow(today(), m_settings.meteredConnection)) {
        const QString kind = m_meetingKind(date);
        if (!isMeetingKind(kind)) {
            continue;
        }
        const QString dateIso = date.toString(Qt::ISODate);
        if (const auto playlist = m_database->findPlaylistByDateAndType(dateIso, kind)) {
            (void)m_database->deletePlaylistEntriesBySource(playlist->id, kDynamic);
            (void)m_database->setPlaylistSyncStatus(playlist->id, QString());
            emit dayContentChanged(dateIso);
        }
    }
    emit busyChanged();
    refresh();
}

void MeetingSyncEngine::enqueue(const QDate& date, const QString& kind, qint64 playlistId)
{
    auto job = std::make_shared<DayJob>();
    job->date = date;
    job->dateIso = date.toString(Qt::ISODate);
    job->kind = kind;
    job->coWeek = m_coWeek ? m_coWeek(date) : false;
    job->playlistId = playlistId;
    job->priority = priorityFor(date);
    job->generation = m_generation;
    // The closest meeting goes first.
    const auto position = std::find_if(m_queue.begin(), m_queue.end(),
                                       [&](const JobPtr& queued) { return queued->date > date; });
    m_queue.insert(position, job);
    setStatus(job->dateIso, kPending);
    emit busyChanged();
}

void MeetingSyncEngine::pump()
{
    while (m_active.size() < kMaxConcurrentDays && !m_queue.empty()) {
        JobPtr job = m_queue.front();
        m_queue.pop_front();
        m_active.insert(job->dateIso, job);
        setStatus(job->dateIso, kSyncing);
        startJob(job);
    }
    if (!isBusy()) {
        emit syncIdle();
    }
}

void MeetingSyncEngine::startJob(const JobPtr& job)
{
    if (m_store == nullptr) {
        failJob(job);
        return;
    }
    if (job->kind == QStringLiteral("midweek")) {
        loadMidweek(job, 0);
    } else if (job->kind == QStringLiteral("weekend")) {
        loadWeekend(job, 0, 0);
    } else {
        loadMemorial(job, 0);
    }
}

void MeetingSyncEngine::loadMidweek(const JobPtr& job, int languageIndex)
{
    const QStringList codes = languages();
    if (languageIndex >= codes.size()) {
        failJob(job);
        return;
    }
    const QDate monday = mondayOf(job->date);
    m_store->ensurePublication(
        QStringLiteral("mwb"), midweekIssueTag(monday), codes.at(languageIndex), job->priority,
        this, [this, job, languageIndex, monday](const std::optional<LocalPublication>& pub) {
            if (!pub.has_value()) {
                loadMidweek(job, languageIndex + 1);
                return;
            }
            auto found = std::make_shared<bool>(false);
            const Settings settings = m_settings;
            m_store->runInBackground(
                this,
                [job, pub, monday, settings, found]() {
                    PublicationDatabase db;
                    if (!db.open(pub->databasePath)) {
                        return;
                    }
                    PublicationDatabase::MediaSelector selector;
                    selector.documentId = db.midweekDocumentId(monday);
                    if (selector.documentId < 0) {
                        return;
                    }
                    *found = true;
                    job->rows = db.documentMultimedia(selector, settings.includePrinted);
                    resolveRowPaths(job->rows, pub->folder);
                    job->extracts = db.documentExtracts(selector.documentId, settings.excludeTh);
                },
                [this, job, found]() {
                    if (!*found) {
                        failJob(job);
                        return;
                    }
                    processExtract(job, 0, 0);
                });
        });
}

void MeetingSyncEngine::processExtract(const JobPtr& job, size_t extractIndex, int languageIndex)
{
    if (extractIndex >= job->extracts.size()) {
        std::stable_sort(job->rows.begin(), job->rows.end(),
                         [](const PublicationMediaRow& a, const PublicationMediaRow& b) {
                             return a.beginParagraphOrdinal < b.beginParagraphOrdinal;
                         });
        resolveMissingMedia(job);
        return;
    }
    const PublicationExtractRow extract = job->extracts.at(extractIndex);
    const QString symbol =
        extractPublicationSymbol(extract.uniqueEnglishSymbol, extract.issueTagNumber);
    const QStringList codes = languages(MeetingMediaBuilder::languageFromExtractLink(extract.link));
    if (symbol == QStringLiteral("it") || symbol == QStringLiteral("snnw") ||
        languageIndex >= codes.size()) {
        processExtract(job, extractIndex + 1, 0);
        return;
    }

    m_store->ensurePublication(
        symbol, extract.issueTagNumber, codes.at(languageIndex), job->priority, this,
        [this, job, extractIndex, languageIndex, extract,
         symbol](const std::optional<LocalPublication>& pub) {
            if (!pub.has_value()) {
                processExtract(job, extractIndex, languageIndex + 1);
                return;
            }
            auto rows = std::make_shared<std::vector<PublicationMediaRow>>();
            const Settings settings = m_settings;
            m_store->runInBackground(
                this,
                [pub, extract, symbol, settings, rows]() {
                    PublicationDatabase db;
                    if (!db.open(pub->databasePath)) {
                        return;
                    }
                    PublicationDatabase::MediaSelector selector;
                    selector.mepsDocumentId = extract.refMepsDocumentId;
                    if (extract.refBeginParagraphOrdinal != 0 &&
                        extract.refEndParagraphOrdinal != 0) {
                        // Lesson books start their media before the referenced point.
                        const bool lessonBook = symbol == QStringLiteral("bt") ||
                                                symbol == QStringLiteral("lff") ||
                                                symbol == QStringLiteral("lmd");
                        selector.hasParagraphRange = true;
                        selector.beginParagraphOrdinal =
                            lessonBook && extract.refBeginParagraphOrdinal < 8
                                ? 1
                                : extract.refBeginParagraphOrdinal;
                        selector.endParagraphOrdinal = extract.refEndParagraphOrdinal;
                    }
                    *rows = db.documentMultimedia(selector, settings.includePrinted);
                    resolveRowPaths(*rows, pub->folder);
                    std::erase_if(*rows, [&](const PublicationMediaRow& row) {
                        return !settings.signLanguage && symbol == QStringLiteral("lmd") &&
                               row.mimeType.contains(QStringLiteral("video"));
                    });
                    for (PublicationMediaRow& row : *rows) {
                        row.beginParagraphOrdinal = extract.beginParagraphOrdinal;
                        row.endParagraphOrdinal = extract.endParagraphOrdinal;
                        row.extractCaption = extract.caption;
                        row.extractSymbol = symbol;
                    }
                },
                [this, job, extractIndex, rows]() {
                    job->rows.insert(job->rows.end(), rows->begin(), rows->end());
                    processExtract(job, extractIndex + 1, 0);
                });
        });
}

void MeetingSyncEngine::loadWeekend(const JobPtr& job, int languageIndex, size_t issueIndex)
{
    const QStringList codes = languages();
    const QDate monday = mondayOf(job->date);
    const std::vector<int> tags = watchtowerIssueTags(monday);
    if (languageIndex >= codes.size()) {
        failJob(job);
        return;
    }
    if (issueIndex >= tags.size()) {
        loadWeekend(job, languageIndex + 1, 0);
        return;
    }
    const int tag = tags.at(issueIndex);
    m_store->ensurePublication(
        QStringLiteral("w"), tag, codes.at(languageIndex), job->priority, this,
        [this, job, languageIndex, issueIndex, monday,
         tag](const std::optional<LocalPublication>& pub) {
            if (!pub.has_value()) {
                loadWeekend(job, languageIndex, issueIndex + 1);
                return;
            }
            auto found = std::make_shared<bool>(false);
            const Settings settings = m_settings;
            m_store->runInBackground(
                this,
                [job, pub, monday, tag, settings, found]() {
                    PublicationDatabase db;
                    if (!db.open(pub->databasePath)) {
                        return;
                    }
                    const auto study = db.watchtowerStudyForWeek(monday);
                    if (!study.has_value()) {
                        return;
                    }
                    *found = true;
                    job->documentTitle = study->title;
                    std::vector<PublicationMediaRow> pictures =
                        db.weekendMediaWithoutVideos(study->documentId, settings.songbookSymbol);
                    std::vector<PublicationMediaRow> videos = db.weekendVideos(study->documentId);
                    std::vector<PublicationMediaRow> legacySongs =
                        db.legacyWeekendSongs(study->documentId);
                    resolveRowPaths(pictures, pub->folder);
                    resolveRowPaths(videos, pub->folder);
                    resolveRowPaths(legacySongs, pub->folder);
                    std::vector<PublicationMediaRow> inParagraphs;
                    std::vector<PublicationMediaRow> outsideParagraphs;
                    for (PublicationMediaRow& video : videos) {
                        (video.targetParagraphNumberLabel.isEmpty() ? outsideParagraphs
                                                                    : inParagraphs)
                            .push_back(std::move(video));
                    }
                    WeekendMediaFilters filters;
                    filters.excludeFootnotes = settings.excludeFootnotes;
                    filters.excludeWtParagraphVideos = settings.excludeWtParagraphVideos;
                    job->rows = MeetingMediaBuilder::orderWeekendMedia(pictures, inParagraphs,
                                                                       outsideParagraphs, filters);
                    MeetingMediaBuilder::placeWeekendSongs(
                        job->rows,
                        MeetingMediaBuilder::selectWeekendSongs(tag * 100, legacySongs,
                                                                outsideParagraphs),
                        db.weekendSongExtracts(study->documentId));
                },
                [this, job, languageIndex, issueIndex, found]() {
                    if (!*found) {
                        loadWeekend(job, languageIndex, issueIndex + 1);
                        return;
                    }
                    resolveMissingMedia(job);
                });
        });
}

void MeetingSyncEngine::loadMemorial(const JobPtr& job, int languageIndex)
{
    const QStringList codes = languages();
    if (languageIndex >= codes.size()) {
        failJob(job);
        return;
    }
    const QString symbol =
        QStringLiteral("mi%1").arg(job->date.year() % 100, 2, 10, QLatin1Char('0'));
    m_store->ensurePublication(
        symbol, 0, codes.at(languageIndex), job->priority, this,
        [this, job, languageIndex](const std::optional<LocalPublication>& pub) {
            if (!pub.has_value()) {
                loadMemorial(job, languageIndex + 1);
                return;
            }
            m_store->runInBackground(
                this,
                [job, pub]() {
                    PublicationDatabase db;
                    if (db.open(pub->databasePath)) {
                        job->rows = db.memorialMedia();
                        resolveRowPaths(job->rows, pub->folder);
                    }
                },
                [this, job]() { resolveMissingMedia(job); });
        });
}

void MeetingSyncEngine::resolveMissingMedia(const JobPtr& job)
{
    std::vector<size_t> missing;
    for (size_t i = 0; i < job->rows.size(); ++i) {
        const PublicationMediaRow& row = job->rows[i];
        if (row.filePath.isEmpty() && row.streamUrl.isEmpty() &&
            (!row.keySymbol.isEmpty() || row.mepsDocumentId != 0)) {
            missing.push_back(i);
        }
    }
    if (missing.empty()) {
        finishJob(job);
        return;
    }

    job->pendingResolutions = static_cast<int>(missing.size());
    for (const size_t index : missing) {
        const PublicationMediaRow& row = job->rows[index];
        MediaFileRequest request;
        // Sign languages publish their songs under their own songbook symbol.
        request.keySymbol = row.keySymbol == QStringLiteral("sjjm") && m_settings.signLanguage
                                ? m_settings.songbookSymbol
                                : row.keySymbol;
        request.issueTagNumber = row.issueTagNumber;
        request.track = row.track;
        request.mepsDocumentId = row.mepsDocumentId;
        request.isAudio = row.mimeType.contains(QStringLiteral("audio"));
        request.languages = languages(row.languageOverride);
        m_store->resolveMediaFile(request, job->priority, this,
                                  [this, job, index](const ResolvedMediaFile& resolved) {
                                      PublicationMediaRow& target = job->rows[index];
                                      if (resolved.ok) {
                                          target.filePath = resolved.filePath;
                                          target.streamUrl = resolved.streamUrl;
                                          target.durationSeconds = resolved.durationSeconds;
                                          if (target.linkedPreviewFilePath.isEmpty()) {
                                              target.linkedPreviewFilePath = resolved.thumbnailPath;
                                          }
                                          if (target.label.isEmpty()) {
                                              target.label = resolved.title;
                                          }
                                      } else {
                                          ++job->failedMedia;
                                      }
                                      if (--job->pendingResolutions == 0) {
                                          finishJob(job);
                                      }
                                  });
    }
}

void MeetingSyncEngine::finishJob(const JobPtr& job)
{
    if (job->finished) {
        return;
    }
    job->finished = true;
    if (job->generation != m_generation || m_database == nullptr || !m_database->isOpen()) {
        releaseJob(job);
        return;
    }

    // Media that could not be found anywhere is left out; the day is retried later.
    std::erase_if(job->rows, [](const PublicationMediaRow& row) {
        return row.filePath.isEmpty() && row.streamUrl.isEmpty();
    });
    MeetingMediaContext context;
    context.date = job->date;
    context.isMidweek = job->kind == QStringLiteral("midweek");
    context.isWeekend = job->kind == QStringLiteral("weekend");
    context.isCoWeek = job->coWeek;
    context.excludeCbsVideos = m_settings.excludeCbsVideos;
    std::vector<DayMediaItem> items = MeetingMediaBuilder::buildDay(job->rows, context);
    if (job->kind == QStringLiteral("memorial")) {
        for (DayMediaItem& item : items) {
            item.sectionId = MeetingSections::kImportedMedia;
        }
    }
    writeDay(job, items);

    const bool failed = job->failedMedia > 0 && job->date >= today();
    (void)m_database->setPlaylistSyncStatus(job->playlistId, failed ? kError : kComplete);
    setStatus(job->dateIso, failed ? kError : kComplete);
    emit dayContentChanged(job->dateIso);
    releaseJob(job);
}

void MeetingSyncEngine::failJob(const JobPtr& job)
{
    if (job->finished) {
        return;
    }
    job->finished = true;
    if (job->generation == m_generation && m_database != nullptr && m_database->isOpen()) {
        const QString status = job->date < today() ? kComplete : kError;
        (void)m_database->setPlaylistSyncStatus(job->playlistId, status);
        setStatus(job->dateIso, status);
    }
    releaseJob(job);
}

void MeetingSyncEngine::releaseJob(const JobPtr& job)
{
    if (job->generation == m_generation) {
        m_active.remove(job->dateIso);
    }
    emit busyChanged();
    pump();
}

void MeetingSyncEngine::writeDay(const JobPtr& job, const std::vector<DayMediaItem>& items)
{
    const qint64 playlistId = job->playlistId;
    const std::vector<PlaylistItemRow> existing = m_database->listPlaylistItemRows(playlistId);

    (void)m_database->executeTransaction([&](QSqlDatabase&) {
        QHash<QString, size_t> existingByKey;
        for (size_t i = 0; i < existing.size(); ++i) {
            if (existing[i].entry.source == kDynamic && !existing[i].entry.uniqueKey.isEmpty()) {
                existingByKey.insert(existing[i].entry.uniqueKey, i);
            }
        }

        // Existing rows keep the order and the edits the operator made; new synced items are
        // placed right after the synced item that precedes them in meeting order.
        std::vector<qint64> order;
        for (const PlaylistItemRow& row : existing) {
            order.push_back(row.entry.id);
        }
        QSet<QString> seenKeys;
        qsizetype insertAfter = -1;
        for (const DayMediaItem& item : items) {
            seenKeys.insert(item.uniqueKey);
            qint64 mediaItemId = 0;
            if (!item.filePath.isEmpty()) {
                MediaItemRecord media;
                media.filePath = item.filePath;
                media.title = item.title;
                media.mediaType = mediaTypeFor(item);
                media.durationMs = item.durationMs;
                media.sourceKind = QStringLiteral("jwpub");
                mediaItemId = m_database->upsertMediaItem(media);
            }

            if (const auto match = existingByKey.constFind(item.uniqueKey);
                match != existingByKey.constEnd()) {
                PlaylistEntryRecord entry = existing[match.value()].entry;
                entry.mediaItemId = mediaItemId != 0 ? mediaItemId : entry.mediaItemId;
                entry.streamUrl = item.streamUrl;
                entry.pubMediaId = item.pubMediaId;
                entry.tagType = item.tagType;
                entry.tagValue = item.tagValue;
                entry.groupKey = item.groupKey;
                entry.isCbs = item.isCbs;
                entry.markerRangesJson = item.markerRangesJson;
                entry.originalSection = item.sectionId;
                entry.sortOrderOriginal = item.sortOrderOriginal;
                entry.thumbnailPath = item.thumbnailPath;
                (void)m_database->updatePlaylistEntryFull(entry);
                const auto position = std::find(order.begin(), order.end(), entry.id);
                insertAfter = position == order.end()
                                  ? static_cast<qsizetype>(order.size()) - 1
                                  : static_cast<qsizetype>(position - order.begin());
                continue;
            }

            PlaylistEntryRecord entry = entryFromItem(playlistId, item);
            entry.mediaItemId = mediaItemId;
            const qint64 entryId = m_database->addPlaylistEntry(entry);
            if (entryId <= 0) {
                continue;
            }
            order.insert(order.begin() + (insertAfter + 1), entryId);
            ++insertAfter;
        }

        for (const PlaylistItemRow& row : existing) {
            if (row.entry.source == kDynamic && !seenKeys.contains(row.entry.uniqueKey)) {
                (void)m_database->deletePlaylistEntry(row.entry.id, playlistId);
                std::erase(order, row.entry.id);
            }
        }
        (void)m_database->reorderPlaylistEntries(playlistId, order);

        if (auto playlist = m_database->findPlaylistById(playlistId)) {
            playlist->sectionsJson = updatedSectionsJson(
                playlist->sectionsJson, MeetingSections::meetingSectionIds(job->kind, job->coWeek),
                job->documentTitle);
            (void)m_database->updatePlaylistMetadata(*playlist);
        }
        return true;
    });

    for (const DayMediaItem& item : items) {
        for (const QString& path : {item.filePath, item.thumbnailPath}) {
            if (!path.isEmpty()) {
                m_daysByFile[QDir::cleanPath(path)].insert(job->dateIso);
            }
        }
    }
}

void MeetingSyncEngine::onMediaFileReady(const QString& filePath)
{
    const QSet<QString> days = m_daysByFile.value(QDir::cleanPath(filePath));
    for (const QString& dateIso : days) {
        emit dayContentChanged(dateIso);
    }
}

} // namespace kmedia
