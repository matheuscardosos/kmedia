#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>

namespace kmedia {

class PlaylistModel;
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

    void addDocument(const QString& databasePath, const QString& folder, qint64 documentId,
                     const QString& sectionId, const QString& sectionSubtitle, DoneCallback done);

private:
    void onMediaFileReady(const QString& filePath);

    PublicationStore* m_store{nullptr};
    QPointer<PlaylistModel> m_playlist;
    QStringList m_languages;
    QStringList m_pendingFiles;
};

} // namespace kmedia
