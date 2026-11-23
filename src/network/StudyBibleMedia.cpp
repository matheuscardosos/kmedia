#include "network/StudyBibleMedia.h"

#include "core/MeetingSections.h"
#include "network/DocumentMediaImporter.h"
#include "network/PublicationStore.h"
#include "parsers/PublicationDatabase.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>

namespace kmedia {

namespace {

std::vector<PublicationMediaRow> readChapter(const QString& databasePath, int book, int chapter)
{
    PublicationDatabase db;
    if (!db.open(databasePath)) {
        return {};
    }
    std::vector<PublicationMediaRow> rows = db.verseMultimedia(book, chapter);
    db.close();
    return rows;
}

} // namespace

StudyBibleMedia::StudyBibleMedia(PublicationStore* store, DocumentMediaImporter* importer,
                                 QObject* parent)
    : QObject(parent), m_store(store), m_importer(importer)
{
}

void StudyBibleMedia::setLanguages(const QStringList& languages)
{
    m_languages = languages;
}

bool StudyBibleMedia::isBusy() const
{
    return m_busy;
}

QVariantList StudyBibleMedia::books() const
{
    return m_books;
}

void StudyBibleMedia::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void StudyBibleMedia::open()
{
    if (m_store == nullptr || m_languages.isEmpty() || m_busy) {
        return;
    }
    if (!m_books.isEmpty() && m_openedLanguage == m_languages.first()) {
        return;
    }
    ++m_request;
    setBusy(true);
    openInLanguage(0);
}

void StudyBibleMedia::openInLanguage(int languageIndex)
{
    if (languageIndex >= m_languages.size()) {
        setBusy(false);
        emit openFailed();
        return;
    }
    const quint64 request = m_request;
    m_store->ensurePublication(
        QLatin1StringView(kSymbol), 0, m_languages.at(languageIndex),
        DownloadService::Priority::High, this,
        [this, request, languageIndex](const std::optional<LocalPublication>& publication) {
            if (request != m_request) {
                return;
            }
            if (!publication.has_value()) {
                openInLanguage(languageIndex + 1);
                return;
            }
            const QString databasePath = publication->databasePath;
            auto found = std::make_shared<std::vector<PublicationDatabase::BibleBookMedia>>();
            m_store->runInBackground(
                this,
                [databasePath, found]() {
                    PublicationDatabase db;
                    if (db.open(databasePath)) {
                        *found = db.bibleBooksWithMedia();
                        db.close();
                    }
                },
                [this, request, publication, found]() {
                    if (request != m_request) {
                        return;
                    }
                    m_openedLanguage = m_languages.first();
                    m_databasePath = publication->databasePath;
                    m_folder = publication->folder;
                    m_books.clear();
                    for (const auto& book : *found) {
                        QVariantList chapters;
                        for (const int chapter : book.chapters) {
                            chapters.append(chapter);
                        }
                        m_books.append(QVariantMap{{QStringLiteral("number"), book.book},
                                                   {QStringLiteral("name"), book.name},
                                                   {QStringLiteral("chapters"), chapters}});
                    }
                    setBusy(false);
                    emit booksChanged();
                    if (m_books.isEmpty()) {
                        emit openFailed();
                    }
                });
        });
}

QVariantList StudyBibleMedia::chapterMedia(int book, int chapter) const
{
    QVariantList media;
    if (m_databasePath.isEmpty()) {
        return media;
    }
    for (const PublicationMediaRow& row : readChapter(m_databasePath, book, chapter)) {
        const bool isVideo = row.categoryType == -1;
        const QString picture = isVideo ? row.linkedPreviewFilePath : row.filePath;
        const QString picturePath = QDir(m_folder).filePath(picture);
        media.append(QVariantMap{
            {QStringLiteral("multimediaId"), row.multimediaId},
            {QStringLiteral("title"), row.label.isEmpty() ? row.caption : row.label},
            {QStringLiteral("verse"), row.targetParagraphNumberLabel},
            {QStringLiteral("isVideo"), isVideo},
            {QStringLiteral("previewUrl"), !picture.isEmpty() && QFileInfo::exists(picturePath)
                                               ? QUrl::fromLocalFile(picturePath).toString()
                                               : QString()},
        });
    }
    return media;
}

void StudyBibleMedia::addMedia(int book, int chapter, const QList<qint64>& multimediaIds,
                               const QString& sectionId)
{
    if (m_importer.isNull() || m_databasePath.isEmpty() || multimediaIds.isEmpty()) {
        emit mediaAdded(0);
        return;
    }
    std::vector<PublicationMediaRow> picked;
    for (PublicationMediaRow& row : readChapter(m_databasePath, book, chapter)) {
        if (multimediaIds.contains(row.multimediaId)) {
            picked.push_back(std::move(row));
        }
    }
    const QString target =
        sectionId.isEmpty() ? QString(MeetingSections::kImportedMedia) : sectionId;
    m_importer->addRows(std::move(picked), m_folder, target, {}, QStringLiteral("verse"),
                        [this](int added) { emit mediaAdded(added); });
}

} // namespace kmedia
