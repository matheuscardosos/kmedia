#include "network/DocumentMediaImporter.h"

#include "models/PlaylistModel.h"
#include "network/PublicationStore.h"
#include "parsers/PublicationDatabase.h"

#include <QDir>
#include <QFileInfo>

#include <memory>
#include <utility>

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

bool isLinkedOnly(const PublicationMediaRow& row)
{
    return row.filePath.isEmpty() && (!row.keySymbol.isEmpty() || row.mepsDocumentId != 0);
}

} // namespace

DocumentMediaImporter::DocumentMediaImporter(PublicationStore* store, PlaylistModel* playlist,
                                             QObject* parent)
    : QObject(parent), m_store(store), m_playlist(playlist)
{
    if (m_store != nullptr) {
        connect(m_store, &PublicationStore::mediaFileReady, this,
                &DocumentMediaImporter::onMediaFileReady);
    }
}

void DocumentMediaImporter::setLanguages(const QStringList& languages)
{
    m_languages = languages;
}

void DocumentMediaImporter::addDocument(const QString& databasePath, const QString& folder,
                                        qint64 documentId, const QString& sectionId,
                                        const QString& sectionSubtitle, DoneCallback done)
{
    PublicationDatabase db;
    if (m_playlist.isNull() || !db.open(databasePath)) {
        done(0);
        return;
    }
    PublicationDatabase::MediaSelector selector;
    selector.documentId = documentId;
    auto rows = std::make_shared<std::vector<PublicationMediaRow>>(
        PublicationDatabase::dedupeLinkedMultimedia(db.documentMultimedia(selector, false)));
    db.close();
    for (PublicationMediaRow& row : *rows) {
        row.filePath = inFolder(folder, row.filePath);
        row.linkedPreviewFilePath = inFolder(folder, row.linkedPreviewFilePath);
    }

    // Everything is added in document order once every linked file is known.
    const auto finish = [this, rows, sectionId, sectionSubtitle, done]() {
        if (m_playlist.isNull()) {
            done(0);
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
            entry.sectionId = sectionId;
            entry.filePath = row.filePath;
            entry.streamUrl = row.streamUrl;
            entry.thumbnailPath = row.linkedPreviewFilePath;
            entry.durationMs = static_cast<qint64>(row.durationSeconds * 1000.0);
            if (!row.targetParagraphNumberLabel.isEmpty()) {
                entry.tagType = QStringLiteral("para");
                entry.tagValue = row.targetParagraphNumberLabel;
            }
            entry.sourceKind = QStringLiteral("jwpub");
            entry.source = QStringLiteral("additional");
            if (m_playlist->appendEntry(entry)) {
                ++added;
            }
        }
        if (added > 0 && !sectionSubtitle.isEmpty()) {
            (void)m_playlist->setSectionSubtitle(sectionId, sectionSubtitle);
        }
        done(added);
    };

    auto pending = std::make_shared<int>(
        static_cast<int>(std::count_if(rows->cbegin(), rows->cend(), isLinkedOnly)));
    if (*pending == 0 || m_store == nullptr) {
        finish();
        return;
    }
    for (size_t i = 0; i < rows->size(); ++i) {
        const PublicationMediaRow& row = rows->at(i);
        if (!isLinkedOnly(row)) {
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

void DocumentMediaImporter::onMediaFileReady(const QString& filePath)
{
    // A linked video or its picture arrived: the open day shows it once it reloads.
    if (m_pendingFiles.removeAll(QDir::cleanPath(filePath)) > 0 && !m_playlist.isNull()) {
        m_playlist->reloadFromDatabase();
    }
}

} // namespace kmedia
