#include "network/PublicTalkMedia.h"

#include "core/MeetingSections.h"
#include "network/DocumentMediaImporter.h"
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

PublicTalkMedia::PublicTalkMedia(PublicationStore* store, DocumentMediaImporter* importer,
                                 QObject* parent)
    : QObject(parent), m_store(store), m_importer(importer)
{
}

void PublicTalkMedia::setFolder(const QString& folder)
{
    if (m_folder == folder) {
        return;
    }
    m_folder = folder;
    loadTalks();
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
    if (m_importer.isNull() || m_databasePath.isEmpty()) {
        emit talkAdded(0);
        return;
    }
    QString talkTitle;
    for (const QVariant& value : std::as_const(m_talks)) {
        const QVariantMap talk = value.toMap();
        if (talk.value(QStringLiteral("documentId")).toLongLong() == documentId) {
            talkTitle = talk.value(QStringLiteral("title")).toString();
        }
    }
    const QString target = sectionId.isEmpty() ? QString(MeetingSections::kPublicTalk) : sectionId;
    const QPointer<PublicTalkMedia> self(this);
    m_importer->addDocument(m_databasePath, m_folder, documentId, target, talkTitle,
                            [self](int added) {
                                if (!self.isNull()) {
                                    emit self->talkAdded(added);
                                }
                            });
}

} // namespace kmedia
