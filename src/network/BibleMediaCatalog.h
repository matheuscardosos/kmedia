#pragma once

#include "network/CatalogClient.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

#include <deque>
#include <map>

namespace kmedia {

class DocumentMediaImporter;
class PublicationStore;

/**
 * The Bible recording in the media language, inspired by Meeting Media Manager: the audio
 * Bible, or the sign language Bible for sign languages. The 66 books are listed with their
 * names in that language; picked verses are added as the chapter recording trimmed to them
 * through the verse markers, with a verse tag.
 */
class BibleMediaCatalog : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged FINAL)
    Q_PROPERTY(bool signLanguage READ isSignLanguage NOTIFY booksChanged FINAL)
    // { number, name, chapters }, one entry per book of the recording.
    Q_PROPERTY(QVariantList books READ books NOTIFY booksChanged FINAL)

public:
    static constexpr int kBookCount = 66;
    static constexpr int kParallelRequests = 4;

    BibleMediaCatalog(CatalogClient* client, PublicationStore* store,
                      DocumentMediaImporter* importer, QObject* parent = nullptr);

    void setLanguage(const QString& languageCode, bool signLanguage);
    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] bool isSignLanguage() const;
    [[nodiscard]] QVariantList books() const;

    Q_INVOKABLE void load();
    // The verse numbers read in a chapter, in order.
    Q_INVOKABLE [[nodiscard]] QList<int> verses(int bookNumber, int chapter) const;
    // "John 3:16-18" in the media language.
    Q_INVOKABLE [[nodiscard]] QString reference(int bookNumber, int chapter, int firstVerse,
                                                int lastVerse) const;
    Q_INVOKABLE void addVerses(int bookNumber, int chapter, int firstVerse, int lastVerse,
                               const QString& sectionId);

    // Where the verses are read in a chapter file, from the start of the first one to the end
    // of the last one; {0, 0} when the markers do not have them.
    [[nodiscard]] static std::pair<qint64, qint64> verseRange(const CatalogMediaAsset& chapterFile,
                                                              int firstVerse, int lastVerse);

signals:
    void busyChanged();
    void booksChanged();
    void versesAdded(bool ok);

private:
    void pump();
    [[nodiscard]] std::vector<CatalogMediaAsset> chapterFiles(int bookNumber, int chapter) const;

    CatalogClient* m_client{nullptr};
    PublicationStore* m_store{nullptr};
    QPointer<DocumentMediaImporter> m_importer;
    QString m_language{QStringLiteral("E")};
    bool m_signLanguage{false};
    QString m_loadedKey;
    std::deque<int> m_queue;
    int m_inFlight{0};
    quint64 m_generation{0};
    std::map<int, JwBibleBook> m_books;
};

} // namespace kmedia
