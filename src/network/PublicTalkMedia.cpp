#include "network/PublicTalkMedia.h"

#include "core/MeetingSections.h"
#include "models/PlaylistModel.h"
#include "network/PublicationStore.h"
#include "parsers/JwpubParser.h"
#include "parsers/PublicationDatabase.h"
#include "parsers/S34Parser.h"

#include <QDir>
#include <QFileInfo>
#include <QVariantMap>

#include <algorithm>
#include <memory>

namespace kmedia {

namespace {

QString inFolder(const QString& folder, const QString& relativePath)
{
    if (relativePath.trimmed().isEmpty()) {
        return {};
    }
    const QString path = QDir(folder).filePath(relativePath);
    return QFileInfo::exists(path) ? QDir::cleanPath(path) : QString();
}

} // namespace

PublicTalkMedia::PublicTalkMedia(PublicationStore* store, PlaylistModel* playlist, QObject* parent)
    : QObject(parent), m_store(store), m_playlist(playlist)
{
    if (m_store != nullptr) {
        connect(m_store, &PublicationStore::mediaFileReady, this,
                &PublicTalkMedia::onMediaFileReady);
    }
}

void PublicTalkMedia::setFolder(const QString& folder)
{
    if (m_folder == folder) {
        return;
    }
    m_folder = folder;
    loadTalks();
}

void PublicTalkMedia::setLanguages(const QStringList& languages)
{
    m_languages = languages;
}

bool PublicTalkMedia::isAvailable() const
{
    return !m_databasePath.isEmpty();
}

bool PublicTalkMedia::isBusy() const
{
    return m_busy;
}

QString PublicTalkMedia::label() const
{
    return m_label;
}

QVariantList PublicTalkMedia::talks() const
{
    return m_talks;
}

void PublicTalkMedia::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void PublicTalkMedia::loadTalks()
{
    m_talks.clear();
    m_label.clear();
    m_databasePath.clear();
    const QStringList databases =
        m_folder.isEmpty()
            ? QStringList()
            : QDir(m_folder).entryList({QStringLiteral("*.db")}, QDir::Files, QDir::Name);
    PublicationDatabase db;
    if (!databases.isEmpty() && db.open(QDir(m_folder).filePath(databases.first()))) {
        m_databasePath = QDir(m_folder).filePath(databases.first());
        const PublicationSummary summary = db.summary();
        const QString symbol = summary.symbol.isEmpty() ? QStringLiteral("S-34mp") : summary.symbol;
        m_label = summary.year > 0 ? QStringLiteral("%1 %2 v%3")
                                         .arg(symbol)
                                         .arg(summary.year)
                                         .arg(summary.versionNumber)
                                   : symbol;

        struct Talk
        {
            int number{0};
            PublicationDocumentEntry document;
        };
        std::vector<Talk> talks;
        for (const PublicationDocumentEntry& document : db.documentsWithMedia()) {
            talks.push_back(Talk{S34Parser::extractTalkNumber(document.title), document});
        }
        // Numbered talks in order; outlines without a number follow.
        std::stable_sort(talks.begin(), talks.end(), [](const Talk& a, const Talk& b) {
            if ((a.number > 0) != (b.number > 0)) {
                return a.number > 0;
            }
            return a.number < b.number;
        });
        for (const Talk& talk : talks) {
            QVariantMap entry;
            entry.insert(QStringLiteral("documentId"), talk.document.documentId);
            entry.insert(QStringLiteral("number"), talk.number);
            entry.insert(QStringLiteral("title"), talk.document.title);
            m_talks.append(entry);
        }
    }
    emit talksChanged();
}

void PublicTalkMedia::importFile(const QUrl& fileUrl)
{
    const QString source = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    if (m_folder.isEmpty() || m_store == nullptr || m_busy || !QFileInfo(source).isFile()) {
        emit importFailed();
        return;
    }
    setBusy(true);
    const QString folder = m_folder;
    auto database = std::make_shared<QString>();
    m_store->runInBackground(
        this,
        [source, folder, database]() {
            // A new S-34mp replaces the previous one completely.
            QDir(folder).removeRecursively();
            (void)QDir().mkpath(folder);
            *database = JwpubParser::extractContents(source, folder);
        },
        [this, database]() {
            setBusy(false);
            loadTalks();
            if (database->isEmpty() || m_talks.isEmpty()) {
                emit importFailed();
            }
        });
}

void PublicTalkMedia::addTalk(qint64 documentId, const QString& sectionId)
{
    PublicationDatabase db;
    if (m_playlist.isNull() || m_databasePath.isEmpty() || !db.open(m_databasePath)) {
        emit talkAdded(0);
        return;
    }
    PublicationDatabase::MediaSelector selector;
    selector.documentId = documentId;
    auto rows = std::make_shared<std::vector<PublicationMediaRow>>(
        PublicationDatabase::dedupeLinkedMultimedia(db.documentMultimedia(selector, false)));
    db.close();
    for (PublicationMediaRow& row : *rows) {
        row.filePath = inFolder(m_folder, row.filePath);
        row.linkedPreviewFilePath = inFolder(m_folder, row.linkedPreviewFilePath);
    }

    QString talkTitle;
    for (const QVariant& value : std::as_const(m_talks)) {
        const QVariantMap talk = value.toMap();
        if (talk.value(QStringLiteral("documentId")).toLongLong() == documentId) {
            talkTitle = talk.value(QStringLiteral("title")).toString();
        }
    }
    const QString target = sectionId.isEmpty() ? QString(MeetingSections::kPublicTalk) : sectionId;

    // Media is added in outline order once every linked file is known.
    const auto finish = [this, rows, target, talkTitle]() {
        if (m_playlist.isNull()) {
            return;
        }
        int added = 0;
        for (const PublicationMediaRow& row : *rows) {
            if (row.filePath.isEmpty() && row.streamUrl.isEmpty()) {
                continue;
            }
            PlaylistEntryView entry;
            entry.title = !row.label.isEmpty()     ? row.label
                          : !row.caption.isEmpty() ? row.caption
                                                   : QFileInfo(row.filePath).completeBaseName();
            entry.sectionId = target;
            entry.filePath = row.filePath;
            entry.streamUrl = row.streamUrl;
            entry.thumbnailPath = row.linkedPreviewFilePath;
            entry.durationMs = static_cast<qint64>(row.durationSeconds * 1000.0);
            entry.sourceKind = QStringLiteral("jwpub");
            entry.source = QStringLiteral("additional");
            if (m_playlist->appendEntry(entry)) {
                ++added;
            }
        }
        if (added > 0 && !talkTitle.isEmpty()) {
            (void)m_playlist->setSectionSubtitle(target, talkTitle);
        }
        emit talkAdded(added);
    };

    auto pending = std::make_shared<int>(0);
    for (const PublicationMediaRow& row : *rows) {
        if (row.filePath.isEmpty() && (!row.keySymbol.isEmpty() || row.mepsDocumentId != 0)) {
            ++*pending;
        }
    }
    if (*pending == 0 || m_store == nullptr) {
        finish();
        return;
    }
    for (size_t i = 0; i < rows->size(); ++i) {
        const PublicationMediaRow& row = rows->at(i);
        if (!row.filePath.isEmpty() || (row.keySymbol.isEmpty() && row.mepsDocumentId == 0)) {
            continue;
        }
        MediaFileRequest request;
        request.keySymbol = row.keySymbol;
        request.issueTagNumber = row.issueTagNumber;
        request.track = row.track;
        request.mepsDocumentId = row.mepsDocumentId;
        request.isAudio = row.mimeType.contains(QStringLiteral("audio"));
        request.languages = m_languages;
        m_store->resolveMediaFile(
            request, DownloadService::Priority::High, this,
            [this, rows, i, pending, finish](const ResolvedMediaFile& resolved) {
                PublicationMediaRow& media = rows->at(i);
                if (resolved.ok) {
                    media.filePath = resolved.filePath;
                    media.streamUrl = resolved.streamUrl;
                    media.durationSeconds = resolved.durationSeconds;
                    if (media.linkedPreviewFilePath.isEmpty()) {
                        media.linkedPreviewFilePath = resolved.thumbnailPath;
                    }
                    if (media.label.isEmpty()) {
                        media.label = resolved.title;
                    }
                    for (const QString& path : {resolved.filePath, resolved.thumbnailPath}) {
                        if (!path.isEmpty() && !QFileInfo::exists(path)) {
                            m_pendingFiles.append(QDir::cleanPath(path));
                        }
                    }
                }
                if (--*pending == 0) {
                    finish();
                }
            });
    }
}

void PublicTalkMedia::onMediaFileReady(const QString& filePath)
{
    // A linked video or its picture arrived: the open day shows it once it reloads.
    if (m_pendingFiles.removeAll(QDir::cleanPath(filePath)) > 0 && !m_playlist.isNull()) {
        m_playlist->reloadFromDatabase();
    }
}

} // namespace kmedia
