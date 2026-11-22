#pragma once

#include "network/DownloadService.h"

#include <QDir>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QThreadPool>

#include <functional>
#include <optional>
#include <vector>

namespace kmedia {

class CatalogClient;
struct CatalogMediaAsset;
struct JwVideo;

/** A publication unpacked on disk: its folder (media files) and its SQLite database. */
struct LocalPublication
{
    QString symbol;
    QString languageCode;
    int issueTagNumber{0};
    QString folder;
    QString databasePath;
};

/** What the meeting sync knows about a media file that is not inside the publication. */
struct MediaFileRequest
{
    QString keySymbol;
    int issueTagNumber{0};
    int track{0};
    qint64 mepsDocumentId{0};
    bool isAudio{false};
    QStringList languages;
};

struct ResolvedMediaFile
{
    bool ok{false};
    QString filePath;
    QString streamUrl;
    QString title;
    double durationSeconds{0.0};
    QString thumbnailPath;
};

/**
 * Keeps meeting publications and their media on disk under `<cache>/Publications`: downloads a
 * `.jwpub` once, unpacks it once on a worker thread, and shares the result with every day that
 * asks for the same issue. It also resolves media referenced by a publication but not stored in
 * it, picking the best file at or below the preferred resolution.
 */
class PublicationStore : public QObject
{
    Q_OBJECT

public:
    using PublicationCallback = std::function<void(const std::optional<LocalPublication>&)>;
    using MediaCallback = std::function<void(const ResolvedMediaFile&)>;

    static constexpr double kStreamOnlySeconds = 2100.0;

    PublicationStore(CatalogClient* catalogClient, DownloadService* downloadService,
                     QObject* parent = nullptr);
    ~PublicationStore() override;

    Q_DISABLE_COPY_MOVE(PublicationStore)

    void setCacheRoot(const QString& cacheRoot);
    [[nodiscard]] QString cacheRoot() const;
    void setMaxResolution(int maxResolution);

    [[nodiscard]] QString publicationFolder(const QString& symbol, const QString& languageCode,
                                            int issueTagNumber) const;

    void ensurePublication(const QString& symbol, int issueTagNumber, const QString& languageCode,
                           DownloadService::Priority priority, QObject* context,
                           PublicationCallback callback);
    void resolveMediaFile(const MediaFileRequest& request, DownloadService::Priority priority,
                          QObject* context, MediaCallback callback);

    /** Runs blocking work on the store's worker pool and delivers the result on this thread. */
    /** Picks a JW.org video file at or below the preferred resolution and downloads it. */
    void resolveVideo(const JwVideo& video, DownloadService::Priority priority, QObject* context,
                      MediaCallback callback);
    void runInBackground(QObject* context, std::function<void()> work, std::function<void()> done);

    [[nodiscard]] static QString publicationKey(const QString& symbol, const QString& languageCode,
                                                int issueTagNumber);
    [[nodiscard]] static int resolutionOf(const QString& label);

signals:
    void mediaFileReady(const QString& filePath);
    void mediaFileFailed(const QString& filePath);

private:
    struct PendingPublication
    {
        QString symbol;
        QString languageCode;
        int issueTagNumber{0};
        std::vector<std::pair<QPointer<QObject>, PublicationCallback>> waiters;
    };

    void finishPublication(const QString& key, const std::optional<LocalPublication>& result);
    void extractPublication(const QString& key, const QString& jwpubPath);
    void tryResolveLanguage(const MediaFileRequest& request, int languageIndex,
                            DownloadService::Priority priority, const QPointer<QObject>& context,
                            const MediaCallback& callback);
    [[nodiscard]] ResolvedMediaFile resolveAsset(const CatalogMediaAsset& asset, const QDir& folder,
                                                 DownloadService::Priority priority);
    void queueFileDownload(const QUrl& url, const QString& filePath, const QString& checksum,
                           DownloadService::Priority priority, const QString& title);
    void onDownloadFinished(const QString& taskId, bool success);

    CatalogClient* m_catalogClient{nullptr};
    DownloadService* m_downloadService{nullptr};
    QString m_cacheRoot;
    int m_maxResolution{720};
    QThreadPool m_workers;
    QHash<QString, PendingPublication> m_pendingPublications;
    QHash<QString, QString> m_publicationTasks;
    QHash<QString, QString> m_mediaTasks;
};

} // namespace kmedia
