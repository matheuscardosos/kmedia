#include "network/PublicationMediaPicker.h"

#include "core/MeetingSections.h"
#include "core/SystemUtils.h"
#include "network/DocumentMediaImporter.h"
#include "network/PublicationStore.h"
#include "parsers/JwpubParser.h"
#include "parsers/PublicationDatabase.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QVariantMap>

#include <memory>

namespace kmedia {

PublicationMediaPicker::PublicationMediaPicker(PublicationStore* store,
                                               DocumentMediaImporter* importer, QObject* parent)
    : QObject(parent), m_store(store), m_importer(importer)
{
}

void PublicationMediaPicker::setLanguages(const QStringList& languages)
{
    m_languages = languages;
}

bool PublicationMediaPicker::isBusy() const
{
    return m_busy;
}

QString PublicationMediaPicker::title() const
{
    return m_title;
}

QVariantList PublicationMediaPicker::documents() const
{
    return m_documents;
}

void PublicationMediaPicker::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

// "2026-11", "202611", and "20261100" all name the November 2026 issue; a blank issue is 0.
int PublicationMediaPicker::issueTagFrom(const QString& issue)
{
    QString digits = issue;
    digits.remove(QRegularExpression(QStringLiteral("[^0-9]")));
    if (digits.size() == 8 && digits.endsWith(QStringLiteral("00"))) {
        digits.chop(2);
    }
    return digits.size() == 6 ? digits.toInt() : 0;
}

void PublicationMediaPicker::clear()
{
    ++m_request;
    m_databasePath.clear();
    m_folder.clear();
    m_title.clear();
    m_documents.clear();
    setBusy(false);
    emit documentsChanged();
}

void PublicationMediaPicker::load(const QString& databasePath, const QString& folder)
{
    m_databasePath.clear();
    m_folder.clear();
    m_title.clear();
    m_documents.clear();
    PublicationDatabase db;
    if (!databasePath.isEmpty() && db.open(databasePath)) {
        m_databasePath = databasePath;
        m_folder = folder;
        const PublicationSummary summary = db.summary();
        m_title = summary.title.isEmpty() ? summary.symbol : summary.title;
        for (const PublicationDocumentEntry& document : db.documentsWithMedia()) {
            QVariantMap entry;
            entry.insert(QStringLiteral("documentId"), document.documentId);
            entry.insert(QStringLiteral("title"), document.title);
            m_documents.append(entry);
        }
    }
    setBusy(false);
    emit documentsChanged();
    if (m_documents.isEmpty()) {
        emit openFailed();
    }
}

void PublicationMediaPicker::openFile(const QUrl& fileUrl)
{
    const QString source = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    if (m_store == nullptr || m_store->cacheRoot().isEmpty() || !QFileInfo(source).isFile()) {
        emit openFailed();
        return;
    }
    // Each file gets its own folder; unpacking is skipped when the same file was opened before.
    const QString folder =
        QDir(m_store->cacheRoot())
            .filePath(QStringLiteral("Publications/Local/") +
                      SystemUtils::sanitizeFileName(QFileInfo(source).completeBaseName()));
    const quint64 request = ++m_request;
    setBusy(true);
    auto database = std::make_shared<QString>();
    m_store->runInBackground(
        this,
        [source, folder, database]() {
            (void)QDir().mkpath(folder);
            *database = JwpubParser::extractContents(source, folder);
        },
        [this, request, database, folder]() {
            if (request == m_request) {
                load(*database, folder);
            }
        });
}

void PublicationMediaPicker::openPublication(const QString& symbol, const QString& issue)
{
    const QString cleanSymbol = symbol.trimmed().toLower();
    if (m_store == nullptr || cleanSymbol.isEmpty() || m_languages.isEmpty()) {
        emit openFailed();
        return;
    }
    ++m_request;
    setBusy(true);
    openInLanguage(cleanSymbol, issueTagFrom(issue), 0);
}

void PublicationMediaPicker::openInLanguage(const QString& symbol, int issueTag, int languageIndex)
{
    if (languageIndex >= m_languages.size()) {
        load({}, {});
        return;
    }
    const quint64 request = m_request;
    m_store->ensurePublication(symbol, issueTag, m_languages.at(languageIndex),
                               DownloadService::Priority::High, this,
                               [this, request, symbol, issueTag,
                                languageIndex](const std::optional<LocalPublication>& publication) {
                                   if (request != m_request) {
                                       return;
                                   }
                                   if (!publication.has_value()) {
                                       openInLanguage(symbol, issueTag, languageIndex + 1);
                                       return;
                                   }
                                   load(publication->databasePath, publication->folder);
                               });
}

void PublicationMediaPicker::addDocument(qint64 documentId, const QString& sectionId)
{
    if (m_importer.isNull() || m_databasePath.isEmpty()) {
        emit documentAdded(0);
        return;
    }
    const QPointer<PublicationMediaPicker> self(this);
    m_importer->addDocument(m_databasePath, m_folder, documentId,
                            sectionId.isEmpty() ? QString(MeetingSections::kImportedMedia)
                                                : sectionId,
                            {}, [self](int added) {
                                if (!self.isNull()) {
                                    emit self->documentAdded(added);
                                }
                            });
}

} // namespace kmedia
