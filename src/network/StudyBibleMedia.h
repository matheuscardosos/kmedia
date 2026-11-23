#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace kmedia {

class DocumentMediaImporter;
class PublicationStore;

/**
 * The pictures and videos of the Study Bible (`nwtsty`), inspired by Meeting Media Manager:
 * the publication is downloaded once in the media language (or the fallback language), its
 * books and chapters with media are listed, and the picked media of a chapter is added with a
 * verse tag, the videos it only links to downloaded like synced media.
 */
class StudyBibleMedia : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged FINAL)
    // { number, name, chapters: [int] } for every book with media.
    Q_PROPERTY(QVariantList books READ books NOTIFY booksChanged FINAL)

public:
    static constexpr auto kSymbol = "nwtsty";

    StudyBibleMedia(PublicationStore* store, DocumentMediaImporter* importer,
                    QObject* parent = nullptr);

    void setLanguages(const QStringList& languages);
    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] QVariantList books() const;

    Q_INVOKABLE void open();
    // { multimediaId, title, verse, isVideo, previewUrl } for the media of a chapter.
    Q_INVOKABLE [[nodiscard]] QVariantList chapterMedia(int book, int chapter) const;
    Q_INVOKABLE void addMedia(int book, int chapter, const QList<qint64>& multimediaIds,
                              const QString& sectionId);

signals:
    void busyChanged();
    void booksChanged();
    void openFailed();
    void mediaAdded(int count);

private:
    void openInLanguage(int languageIndex);
    void setBusy(bool busy);

    PublicationStore* m_store{nullptr};
    QPointer<DocumentMediaImporter> m_importer;
    QStringList m_languages;
    QString m_openedLanguage;
    QString m_databasePath;
    QString m_folder;
    QVariantList m_books;
    quint64 m_request{0};
    bool m_busy{false};
};

} // namespace kmedia
