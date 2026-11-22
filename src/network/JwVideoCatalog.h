#pragma once

#include "network/CatalogClient.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <deque>
#include <vector>

namespace kmedia {

class DocumentMediaImporter;
class PublicationStore;

/**
 * The JW.org video library in the media language, newest first: the latest videos come first,
 * then every category of Video On Demand, listed as they arrive. A picked video is downloaded at
 * or below the preferred resolution and added to the open day.
 */
class JwVideoCatalog : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged FINAL)
    Q_PROPERTY(QVariantList videos READ videos NOTIFY videosChanged FINAL)

public:
    static constexpr int kParallelRequests = 3;

    JwVideoCatalog(CatalogClient* client, PublicationStore* store, DocumentMediaImporter* importer,
                   QObject* parent = nullptr);

    void setLanguage(const QString& languageCode);

    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] QVariantList videos() const;

    Q_INVOKABLE void load();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void addVideo(const QString& key, const QString& sectionId);

signals:
    void busyChanged();
    void videosChanged();
    void videoAdded(bool ok);

private:
    struct Request
    {
        QString category;
        bool subcategoriesOnly{false};
        int depth{0};
    };

    void pump();
    void onCategory(const Request& request, bool ok, const JwVideoCategory& category);
    void publish();
    void setBusy(bool busy);

    CatalogClient* m_client{nullptr};
    PublicationStore* m_store{nullptr};
    QPointer<DocumentMediaImporter> m_importer;
    QString m_language;
    QString m_loadedLanguage;
    std::deque<Request> m_queue;
    int m_inFlight{0};
    quint64 m_generation{0};
    std::vector<JwVideo> m_videos;
    QHash<QString, size_t> m_videoIndex;
    QVariantList m_published;
    QTimer m_publishTimer;
    bool m_busy{false};
};

} // namespace kmedia
