#pragma once

#include "parsers/PublicationDatabase.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

namespace kmedia {

class PlaylistModel;
struct PlaylistEntryView;
class PublicationStore;

/**
 * Adds the media of one publication document to the open day, in document order and as added
 * media: files stored in the publication directly, media it only links to resolved and
 * downloaded like synced media (the day refreshes when they arrive).
 */
class DocumentMediaImporter : public QObject
{
    Q_OBJECT

public:
    using DoneCallback = std::function<void(int added)>;

    DocumentMediaImporter(PublicationStore* store, PlaylistModel* playlist,
                          QObject* parent = nullptr);

    void setLanguages(const QStringList& languages);

    // Adds publication media rows already read, resolving the files they only link to; each
    // row's `targetParagraphNumberLabel` becomes a tag of `tagType`.
    void addRows(std::vector<PublicationMediaRow> rows, const QString& folder,
                 const QString& sectionId, const QString& sectionSubtitle, const QString& tagType,
                 DoneCallback done);
    void addDocument(const QString& databasePath, const QString& folder, qint64 documentId,
                     const QString& sectionId, const QString& sectionSubtitle, DoneCallback done);
    bool addMedia(const PlaylistEntryView& entry);

private:
    void onMediaFileReady(const QString& filePath);

    PublicationStore* m_store{nullptr};
    QPointer<PlaylistModel> m_playlist;
    QStringList m_languages;
    QStringList m_pendingFiles;
};

} // namespace kmedia
