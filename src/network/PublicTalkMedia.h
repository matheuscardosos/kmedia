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
 * The congregation's S-34mp public talk media: the publication is unpacked once into the profile
 * and remembered, its talks are listed by number, and a picked talk adds its media to the open
 * day in outline order.
 */
class PublicTalkMedia : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ isAvailable NOTIFY talksChanged FINAL)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged FINAL)
    Q_PROPERTY(QString label READ label NOTIFY talksChanged FINAL)
    Q_PROPERTY(QVariantList talks READ talks NOTIFY talksChanged FINAL)

public:
    PublicTalkMedia(PublicationStore* store, DocumentMediaImporter* importer,
                    QObject* parent = nullptr);

    void setFolder(const QString& folder);

    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] QString label() const;
    [[nodiscard]] QVariantList talks() const;

    Q_INVOKABLE void importFile(const QUrl& fileUrl);
    Q_INVOKABLE void addTalk(qint64 documentId, const QString& sectionId);

signals:
    void talksChanged();
    void busyChanged();
    void importFailed();
    void talkAdded(int count);

private:
    void setBusy(bool busy);
    void loadTalks();

    PublicationStore* m_store{nullptr};
    QPointer<DocumentMediaImporter> m_importer;
    QString m_folder;
    QString m_databasePath;
    QString m_label;
    QVariantList m_talks;
    bool m_busy{false};
};

} // namespace kmedia
