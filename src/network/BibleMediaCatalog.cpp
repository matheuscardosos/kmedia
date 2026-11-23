#include "network/BibleMediaCatalog.h"

#include "core/MeetingSections.h"
#include "models/PlaylistModel.h"
#include "network/DocumentMediaImporter.h"
#include "network/PublicationStore.h"

#include <QVariantMap>

#include <algorithm>

namespace kmedia {

BibleMediaCatalog::BibleMediaCatalog(CatalogClient* client, PublicationStore* store,
                                     DocumentMediaImporter* importer, QObject* parent)
    : QObject(parent), m_client(client), m_store(store), m_importer(importer)
{
}

void BibleMediaCatalog::setLanguage(const QString& languageCode, bool signLanguage)
{
    m_language = languageCode.trimmed().isEmpty() ? QStringLiteral("E") : languageCode.trimmed();
    m_signLanguage = signLanguage;
}

bool BibleMediaCatalog::isBusy() const
{
    return m_inFlight > 0 || !m_queue.empty();
}

bool BibleMediaCatalog::isSignLanguage() const
{
    return m_signLanguage;
}

QVariantList BibleMediaCatalog::books() const
{
    QVariantList list;
    for (const auto& [number, book] : m_books) {
        int chapters = 0;
        for (const CatalogMediaAsset& file : book.chapters) {
            chapters = std::max(chapters, file.track);
        }
        list.append(QVariantMap{{QStringLiteral("number"), number},
                                {QStringLiteral("name"), book.name},
                                {QStringLiteral("chapters"), chapters}});
    }
    return list;
}

void BibleMediaCatalog::load()
{
    const QString key =
        m_language + (m_signLanguage ? QStringLiteral("/MP4") : QStringLiteral("/MP3"));
    if (m_client == nullptr || isBusy() || (key == m_loadedKey && !m_books.empty())) {
        return;
    }
    ++m_generation;
    m_loadedKey = key;
    m_books.clear();
    m_queue.clear();
    m_inFlight = 0;
    for (int book = 1; book <= kBookCount; ++book) {
        m_queue.push_back(book);
    }
    emit booksChanged();
    emit busyChanged();
    pump();
}

void BibleMediaCatalog::pump()
{
    const QString format = m_signLanguage ? QStringLiteral("MP4") : QStringLiteral("MP3");
    while (m_inFlight < kParallelRequests && !m_queue.empty()) {
        const int bookNumber = m_queue.front();
        m_queue.pop_front();
        ++m_inFlight;
        const quint64 generation = m_generation;
        m_client->requestBibleBook(
            CatalogClient::bibleBookUrl(m_language, bookNumber, format), m_language, this,
            [this, generation, bookNumber](bool ok, const JwBibleBook& book) {
                if (generation != m_generation) {
                    return;
                }
                --m_inFlight;
                // Books not recorded in this language are left out of the list.
                if (ok) {
                    JwBibleBook stored = book;
                    stored.number = bookNumber;
                    m_books[bookNumber] = std::move(stored);
                    emit booksChanged();
                }
                pump();
                if (!isBusy()) {
                    emit busyChanged();
                }
            });
    }
}

std::vector<CatalogMediaAsset> BibleMediaCatalog::chapterFiles(int bookNumber, int chapter) const
{
    std::vector<CatalogMediaAsset> files;
    const auto found = m_books.find(bookNumber);
    if (found == m_books.end()) {
        return files;
    }
    for (const CatalogMediaAsset& file : found->second.chapters) {
        if (file.track == chapter) {
            files.push_back(file);
        }
    }
    return files;
}

QList<int> BibleMediaCatalog::verses(int bookNumber, int chapter) const
{
    QList<int> numbers;
    for (const CatalogMediaAsset& file : chapterFiles(bookNumber, chapter)) {
        for (const BibleVerseMarker& marker : file.verseMarkers) {
            if (!numbers.contains(marker.verse)) {
                numbers.append(marker.verse);
            }
        }
    }
    std::sort(numbers.begin(), numbers.end());
    return numbers;
}

QString BibleMediaCatalog::reference(int bookNumber, int chapter, int firstVerse,
                                     int lastVerse) const
{
    const auto found = m_books.find(bookNumber);
    const QString name = found != m_books.end() ? found->second.name : QString::number(bookNumber);
    if (firstVerse <= 0) {
        return QStringLiteral("%1 %2").arg(name).arg(chapter);
    }
    const QString verses = lastVerse > firstVerse
                               ? QStringLiteral("%1-%2").arg(firstVerse).arg(lastVerse)
                               : QString::number(firstVerse);
    return QStringLiteral("%1 %2:%3").arg(name).arg(chapter).arg(verses);
}

std::pair<qint64, qint64> BibleMediaCatalog::verseRange(const CatalogMediaAsset& chapterFile,
                                                        int firstVerse, int lastVerse)
{
    const auto markerOf = [&chapterFile](int verse) -> const BibleVerseMarker* {
        for (const BibleVerseMarker& marker : chapterFile.verseMarkers) {
            if (marker.verse == verse) {
                return &marker;
            }
        }
        return nullptr;
    };
    const BibleVerseMarker* first = markerOf(std::min(firstVerse, lastVerse));
    const BibleVerseMarker* last = markerOf(std::max(firstVerse, lastVerse));
    if (first == nullptr || last == nullptr) {
        return {0, 0};
    }
    return {first->startMs, last->startMs + last->durationMs};
}

void BibleMediaCatalog::addVerses(int bookNumber, int chapter, int firstVerse, int lastVerse,
                                  const QString& sectionId)
{
    const std::vector<CatalogMediaAsset> files = chapterFiles(bookNumber, chapter);
    if (files.empty() || m_store == nullptr || m_importer.isNull()) {
        emit versesAdded(false);
        return;
    }
    const ResolvedMediaFile resolved =
        m_store->resolveBibleChapter(files, DownloadService::Priority::High);
    // Without verses (a chapter with no markers) the whole chapter is added.
    const bool wholeChapter = firstVerse <= 0;
    const auto [startMs, endMs] = wholeChapter ? std::pair<qint64, qint64>{0, 0}
                                               : verseRange(files.front(), firstVerse, lastVerse);
    if (!resolved.ok || (!wholeChapter && endMs <= startMs)) {
        emit versesAdded(false);
        return;
    }

    PlaylistEntryView entry;
    entry.title = reference(bookNumber, chapter, firstVerse, lastVerse);
    entry.sectionId = sectionId.isEmpty() ? QString(MeetingSections::kImportedMedia) : sectionId;
    entry.filePath = resolved.filePath;
    entry.streamUrl = resolved.streamUrl;
    entry.thumbnailPath = resolved.thumbnailPath;
    entry.durationMs = static_cast<qint64>(resolved.durationSeconds * 1000.0);
    entry.trimStartMs = startMs;
    entry.trimEndMs = endMs;
    entry.tagType = QStringLiteral("verse");
    const int low = std::min(firstVerse, lastVerse);
    const int high = std::max(firstVerse, lastVerse);
    if (wholeChapter) {
        entry.tagValue = QString::number(chapter);
    } else {
        entry.tagValue = high > low ? QStringLiteral("%1:%2-%3").arg(chapter).arg(low).arg(high)
                                    : QStringLiteral("%1:%2").arg(chapter).arg(low);
    }
    entry.sourceKind = QStringLiteral("catalog");
    entry.source = QStringLiteral("additional");
    emit versesAdded(m_importer->addMedia(entry));
}

} // namespace kmedia
