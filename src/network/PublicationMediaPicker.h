#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

namespace kmedia {

class DocumentMediaImporter;
class PublicationStore;

/**
 * Picks the media of one document of a publication: a publication from JW.org by symbol and
 * issue, or a `.jwpub` added from the computer, is opened once on a worker thread, its documents
 * with media are listed, and a picked document adds its media to the open day.
 */
class PublicationMediaPicker : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged FINAL)
    Q_PROPERTY(QString title READ title NOTIFY documentsChanged FINAL)
    Q_PROPERTY(QVariantList documents READ documents NOTIFY documentsChanged FINAL)

public:
    PublicationMediaPicker(PublicationStore* store, DocumentMediaImporter* importer,
                           QObject* parent = nullptr);

    void setLanguages(const QStringList& languages);

    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QVariantList documents() const;

    Q_INVOKABLE void openFile(const QUrl& fileUrl);
    Q_INVOKABLE void openPublication(const QString& symbol, const QString& issue);
    Q_INVOKABLE void addDocument(qint64 documentId, const QString& sectionId);
    Q_INVOKABLE void clear();

    [[nodiscard]] static int issueTagFrom(const QString& issue);

signals:
    void busyChanged();
    void documentsChanged();
    void openFailed();
    void documentAdded(int count);

private:
    void setBusy(bool busy);
    void load(const QString& databasePath, const QString& folder);
    void openInLanguage(const QString& symbol, int issueTag, int languageIndex);

    PublicationStore* m_store{nullptr};
    QPointer<DocumentMediaImporter> m_importer;
    QStringList m_languages;
    QString m_databasePath;
    QString m_folder;
    QString m_title;
    QVariantList m_documents;
    bool m_busy{false};
    quint64 m_request{0};
};

} // namespace kmedia
