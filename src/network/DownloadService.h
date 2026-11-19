#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

#include <memory>
#include <optional>
#include <vector>

class QFile;
class QNetworkReply;
class QTimer;

namespace kmedia {

class DatabaseManager;
class NetworkManager;

/**
 * Bounded concurrent downloader with HTTP ETag/Last-Modified validation,
 * chunked disk streaming via `.part` temporary files, SHA-256 verification,
 * pause/resume/cancel controls, and exponential retry backoff.
 */
class DownloadService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int activeCount READ activeCount NOTIFY countsChanged FINAL)
    Q_PROPERTY(int queuedCount READ queuedCount NOTIFY countsChanged FINAL)
    Q_PROPERTY(int maxConcurrent READ maxConcurrent WRITE setMaxConcurrent NOTIFY
                   maxConcurrentChanged FINAL)
    Q_PROPERTY(QVariantList tasks READ tasksVariant NOTIFY tasksChanged FINAL)
    Q_PROPERTY(int finishedCount READ finishedCount NOTIFY tasksChanged FINAL)

public:
    enum class Status {
        Queued = 0,
        Downloading,
        Paused,
        Completed,
        CachedNotModified,
        Cancelled,
        Failed,
    };
    Q_ENUM(Status)

    enum class Priority {
        High = 0,
        Normal = 1,
        Low = 2,
    };
    Q_ENUM(Priority)

    struct TaskInfo
    {
        QString id;
        QString displayTitle;
        QUrl url;
        QString destinationPath;
        QString expectedSha256;
        QString actualSha256;
        QString etag;
        QString lastModified;
        QString errorMessage;
        Priority priority{Priority::Normal};
        Status status{Status::Queued};
        qint64 bytesReceived{0};
        qint64 bytesTotal{0};
        int retryCount{0};
    };

    explicit DownloadService(NetworkManager* networkManager = nullptr,
                             DatabaseManager* databaseManager = nullptr, QObject* parent = nullptr);
    ~DownloadService() override;

    Q_DISABLE_COPY_MOVE(DownloadService)

    void setDatabaseManager(DatabaseManager* databaseManager);

    [[nodiscard]] int activeCount() const;
    [[nodiscard]] int queuedCount() const;
    [[nodiscard]] int maxConcurrent() const;
    void setMaxConcurrent(int maxConcurrent);

    void setMaxRetries(int maxRetries);
    void setRetryBaseDelayMs(int baseDelayMs);

    QString enqueue(const QUrl& url, const QString& destinationPath,
                    const QString& expectedSha256 = {}, Priority priority = Priority::Normal,
                    const QString& customId = {}, const QString& displayTitle = {});

    Q_INVOKABLE bool pause(const QString& taskId);
    Q_INVOKABLE bool resume(const QString& taskId);
    Q_INVOKABLE bool cancel(const QString& taskId);
    Q_INVOKABLE bool retryTask(const QString& taskId);
    Q_INVOKABLE void pauseAll();
    Q_INVOKABLE void cancelAll();
    Q_INVOKABLE void clearFinished();

    [[nodiscard]] int finishedCount() const;
    [[nodiscard]] QVariantList tasksVariant() const;
    [[nodiscard]] std::optional<TaskInfo> taskInfo(const QString& taskId) const;
    [[nodiscard]] std::vector<TaskInfo> allTasks() const;

signals:
    void countsChanged();
    void tasksChanged();
    void maxConcurrentChanged();
    void taskAdded(const QString& taskId);
    void taskStateChanged(const QString& taskId, kmedia::DownloadService::Status status);
    void taskProgress(const QString& taskId, qint64 bytesReceived, qint64 bytesTotal);
    void taskFinished(const QString& taskId, bool success);

private:
    struct InternalTask;

    void dispatchNext();
    void startTask(const std::shared_ptr<InternalTask>& task);
    void finalizeReply(const std::shared_ptr<InternalTask>& task);
    void cleanupActiveHandles(const std::shared_ptr<InternalTask>& task, bool removePartFile);
    void scheduleRetry(const std::shared_ptr<InternalTask>& task, const QString& reason);
    void updateStatus(const std::shared_ptr<InternalTask>& task, Status nextStatus);
    [[nodiscard]] std::shared_ptr<InternalTask> pickNextQueuedTask() const;

    std::unique_ptr<NetworkManager> m_ownedNetwork;
    NetworkManager* m_network{nullptr};
    DatabaseManager* m_database{nullptr};
    int m_maxRetries{3};
    int m_retryBaseDelayMs{500};

    QHash<QString, std::shared_ptr<InternalTask>> m_tasks;
    std::vector<QString> m_insertionOrder;
};

} // namespace kmedia
