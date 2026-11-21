#include "network/DownloadService.h"

#include "core/DatabaseManager.h"
#include "core/NetworkManager.h"
#include "core/SystemUtils.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QUuid>
#include <QVariantMap>

#include <algorithm>
#include <limits>

namespace kmedia {

namespace {

enum class AbortIntent {
    None = 0,
    Pause,
    Cancel,
};

QString partFilePathFor(const QString& destinationPath)
{
    return destinationPath + QStringLiteral(".kmedia.part");
}

bool replaceTargetAtomically(const QString& tempPath, const QString& destinationPath,
                             QString* error)
{
    const QString cleanDest = QDir::cleanPath(destinationPath);
    if (!QDir().mkpath(QFileInfo(cleanDest).absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to create destination directory for %1").arg(cleanDest);
        }
        return false;
    }

    if (QFile::exists(cleanDest) && !QFile::remove(cleanDest)) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to overwrite existing file %1").arg(cleanDest);
        }
        return false;
    }

    if (!QFile::rename(tempPath, cleanDest)) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed moving completed download into %1").arg(cleanDest);
        }
        return false;
    }
    return true;
}

QString statusLabelFor(DownloadService::Status status)
{
    switch (status) {
    case DownloadService::Status::Queued:
        return QStringLiteral("Queued");
    case DownloadService::Status::Downloading:
        return QStringLiteral("Downloading");
    case DownloadService::Status::Paused:
        return QStringLiteral("Paused");
    case DownloadService::Status::Completed:
        return QStringLiteral("Completed");
    case DownloadService::Status::CachedNotModified:
        return QStringLiteral("Cached");
    case DownloadService::Status::Cancelled:
        return QStringLiteral("Cancelled");
    case DownloadService::Status::Failed:
        return QStringLiteral("Failed");
    }
    return QStringLiteral("Queued");
}

bool isTerminalStatus(DownloadService::Status status)
{
    return status == DownloadService::Status::Completed ||
           status == DownloadService::Status::CachedNotModified ||
           status == DownloadService::Status::Cancelled ||
           status == DownloadService::Status::Failed;
}

} // namespace

struct DownloadService::InternalTask
{
    TaskInfo info;
    QPointer<QNetworkReply> reply;
    std::unique_ptr<QFile> partFile;
    QCryptographicHash hasher{QCryptographicHash::Sha256};
    QElapsedTimer progressNotifyClock;
    AbortIntent abortIntent{AbortIntent::None};
    bool resumedFromPartial{false};
    qint64 resumeOffset{0};
    bool responseInitialized{false};
    int statusCode{0};
};

DownloadService::DownloadService(NetworkManager* networkManager, DatabaseManager* databaseManager,
                                 QObject* parent)
    : QObject(parent), m_network(networkManager), m_database(databaseManager)
{
    if (m_network == nullptr) {
        m_ownedNetwork = std::make_unique<NetworkManager>();
        m_network = m_ownedNetwork.get();
    }
}

DownloadService::~DownloadService()
{
    cancelAll();
}

void DownloadService::setDatabaseManager(DatabaseManager* databaseManager)
{
    m_database = databaseManager;
}

int DownloadService::activeCount() const
{
    int count = 0;
    for (const auto& task : m_tasks) {
        if (task->info.status == Status::Downloading) {
            ++count;
        }
    }
    return count;
}

int DownloadService::queuedCount() const
{
    int count = 0;
    for (const auto& task : m_tasks) {
        if (task->info.status == Status::Queued) {
            ++count;
        }
    }
    return count;
}

int DownloadService::maxConcurrent() const
{
    return m_network->maxConcurrentRequests();
}

void DownloadService::setMaxConcurrent(int maxConcurrent)
{
    const int before = m_network->maxConcurrentRequests();
    m_network->setMaxConcurrentRequests(maxConcurrent);
    if (m_network->maxConcurrentRequests() != before) {
        emit maxConcurrentChanged();
        dispatchNext();
    }
}

void DownloadService::setMaxRetries(int maxRetries)
{
    m_maxRetries = qBound(0, maxRetries, 8);
}

void DownloadService::setRetryBaseDelayMs(int baseDelayMs)
{
    m_retryBaseDelayMs = qBound(10, baseDelayMs, 60000);
}

QString DownloadService::enqueue(const QUrl& url, const QString& destinationPath,
                                 const QString& expectedSha256, Priority priority,
                                 const QString& customId, const QString& displayTitle)
{
    const QString id = customId.trimmed().isEmpty()
                           ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                           : customId.trimmed();

    auto task = std::make_shared<InternalTask>();
    task->info.id = id;
    task->info.displayTitle = displayTitle.trimmed();
    task->info.url = url;
    task->info.destinationPath = destinationPath;
    task->info.expectedSha256 = expectedSha256.trimmed().toLower();
    task->info.priority = priority;
    task->info.status = Status::Queued;
    task->progressNotifyClock.start();

    if (!url.isValid() || destinationPath.trimmed().isEmpty()) {
        task->info.status = Status::Failed;
        task->info.errorMessage = QStringLiteral("Invalid download URL or destination path");
        m_tasks.insert(id, task);
        m_insertionOrder.push_back(id);
        emit taskAdded(id);
        emit taskStateChanged(id, Status::Failed);
        emit tasksChanged();
        emit taskFinished(id, false);
        return id;
    }

    m_tasks.insert(id, task);
    m_insertionOrder.push_back(id);
    emit taskAdded(id);
    emit taskStateChanged(id, Status::Queued);
    emit countsChanged();
    emit tasksChanged();

    dispatchNext();
    return id;
}

bool DownloadService::pause(const QString& taskId)
{
    const auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) {
        return false;
    }

    const std::shared_ptr<InternalTask>& task = it.value();
    if (task->info.status == Status::Queued) {
        updateStatus(task, Status::Paused);
        return true;
    }
    if (task->info.status != Status::Downloading) {
        return false;
    }

    task->abortIntent = AbortIntent::Pause;
    if (!task->reply.isNull()) {
        task->reply->abort();
    } else {
        cleanupActiveHandles(task, false);
        updateStatus(task, Status::Paused);
        dispatchNext();
    }
    return true;
}

bool DownloadService::resume(const QString& taskId)
{
    const auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) {
        return false;
    }

    const std::shared_ptr<InternalTask>& task = it.value();
    if (task->info.status != Status::Paused) {
        return false;
    }

    task->abortIntent = AbortIntent::None;
    task->info.errorMessage.clear();
    updateStatus(task, Status::Queued);
    dispatchNext();
    return true;
}

bool DownloadService::cancel(const QString& taskId)
{
    const auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) {
        return false;
    }

    const std::shared_ptr<InternalTask>& task = it.value();
    if (task->info.status == Status::Completed || task->info.status == Status::CachedNotModified ||
        task->info.status == Status::Cancelled) {
        return false;
    }

    if (task->info.status == Status::Downloading && !task->reply.isNull()) {
        task->abortIntent = AbortIntent::Cancel;
        task->reply->abort();
        return true;
    }

    cleanupActiveHandles(task, true);
    updateStatus(task, Status::Cancelled);
    emit taskFinished(taskId, false);
    dispatchNext();
    return true;
}

bool DownloadService::retryTask(const QString& taskId)
{
    const auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) {
        return false;
    }
    const std::shared_ptr<InternalTask>& task = it.value();
    if (task->info.status != Status::Failed && task->info.status != Status::Cancelled) {
        return false;
    }
    task->info.retryCount = 0;
    task->info.errorMessage.clear();
    task->info.bytesReceived = 0;
    task->abortIntent = AbortIntent::None;
    updateStatus(task, Status::Queued);
    dispatchNext();
    return true;
}

void DownloadService::pauseAll()
{
    for (const QString& id : m_insertionOrder) {
        pause(id);
    }
}

void DownloadService::cancelAll()
{
    for (const QString& id : m_insertionOrder) {
        cancel(id);
    }
}

void DownloadService::clearFinished()
{
    std::vector<QString> kept;
    kept.reserve(m_insertionOrder.size());
    bool removedAny = false;
    for (const QString& id : m_insertionOrder) {
        const auto it = m_tasks.find(id);
        if (it == m_tasks.end()) {
            continue;
        }
        if (isTerminalStatus(it.value()->info.status)) {
            m_tasks.erase(it);
            removedAny = true;
        } else {
            kept.push_back(id);
        }
    }
    if (!removedAny) {
        return;
    }
    m_insertionOrder = std::move(kept);
    emit countsChanged();
    emit tasksChanged();
}

int DownloadService::finishedCount() const
{
    int count = 0;
    for (const auto& task : m_tasks) {
        if (isTerminalStatus(task->info.status)) {
            ++count;
        }
    }
    return count;
}

QVariantList DownloadService::tasksVariant() const
{
    QVariantList list;
    list.reserve(static_cast<qsizetype>(m_insertionOrder.size()));
    for (auto it = m_insertionOrder.rbegin(); it != m_insertionOrder.rend(); ++it) {
        const auto taskIt = m_tasks.find(*it);
        if (taskIt == m_tasks.end()) {
            continue;
        }
        const TaskInfo& info = taskIt.value()->info;
        QVariantMap map;
        map.insert(QStringLiteral("taskId"), info.id);
        map.insert(QStringLiteral("title"), info.displayTitle.isEmpty()
                                                ? QFileInfo(info.destinationPath).fileName()
                                                : info.displayTitle);
        map.insert(QStringLiteral("status"), static_cast<int>(info.status));
        map.insert(QStringLiteral("statusLabel"), statusLabelFor(info.status));
        map.insert(QStringLiteral("bytesReceived"), info.bytesReceived);
        map.insert(QStringLiteral("bytesTotal"), info.bytesTotal);
        map.insert(QStringLiteral("errorMessage"), info.errorMessage);
        map.insert(QStringLiteral("destinationPath"), info.destinationPath);

        double ratio = 0.0;
        if (info.status == Status::Completed || info.status == Status::CachedNotModified) {
            ratio = 1.0;
        } else if (info.bytesTotal > 0) {
            ratio = qBound(
                0.0, static_cast<double>(info.bytesReceived) / static_cast<double>(info.bytesTotal),
                1.0);
        }
        map.insert(QStringLiteral("progress"), ratio);

        const QString recvText = SystemUtils::formatFileSize(info.bytesReceived);
        const QString sizeText = (info.bytesTotal > 0)
                                     ? QStringLiteral("%1 / %2").arg(
                                           recvText, SystemUtils::formatFileSize(info.bytesTotal))
                                     : recvText;
        map.insert(QStringLiteral("sizeText"), sizeText);
        map.insert(QStringLiteral("canPause"),
                   info.status == Status::Downloading || info.status == Status::Queued);
        map.insert(QStringLiteral("canResume"), info.status == Status::Paused);
        map.insert(QStringLiteral("canCancel"), !isTerminalStatus(info.status));
        map.insert(QStringLiteral("canRetry"),
                   info.status == Status::Failed || info.status == Status::Cancelled);
        list.append(map);
    }
    return list;
}

std::optional<DownloadService::TaskInfo> DownloadService::taskInfo(const QString& taskId) const
{
    const auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) {
        return std::nullopt;
    }
    return it.value()->info;
}

std::vector<DownloadService::TaskInfo> DownloadService::allTasks() const
{
    std::vector<TaskInfo> result;
    result.reserve(m_insertionOrder.size());
    for (const QString& id : m_insertionOrder) {
        if (const auto it = m_tasks.find(id); it != m_tasks.end()) {
            result.push_back(it.value()->info);
        }
    }
    return result;
}

std::shared_ptr<DownloadService::InternalTask> DownloadService::pickNextQueuedTask() const
{
    std::shared_ptr<InternalTask> best;
    for (const QString& id : m_insertionOrder) {
        const auto it = m_tasks.find(id);
        if (it == m_tasks.end()) {
            continue;
        }
        const std::shared_ptr<InternalTask>& candidate = it.value();
        if (candidate->info.status != Status::Queued) {
            continue;
        }
        if (best == nullptr ||
            static_cast<int>(candidate->info.priority) < static_cast<int>(best->info.priority)) {
            best = candidate;
            if (best->info.priority == Priority::High) {
                break;
            }
        }
    }
    return best;
}

void DownloadService::dispatchNext()
{
    while (activeCount() < m_network->maxConcurrentRequests()) {
        const std::shared_ptr<InternalTask> next = pickNextQueuedTask();
        if (next == nullptr) {
            break;
        }
        startTask(next);
    }
}

void DownloadService::startTask(const std::shared_ptr<InternalTask>& task)
{
    QString etag;
    QString lastModified;

    const QFileInfo destInfo(task->info.destinationPath);
    if (m_database != nullptr && destInfo.exists() && destInfo.size() > 0) {
        if (const std::optional<DownloadCacheRecord> cached =
                m_database->findDownloadCache(task->info.url.toString());
            cached.has_value()) {
            const bool shaMatchesExpected =
                task->info.expectedSha256.isEmpty() ||
                cached->contentHash.compare(task->info.expectedSha256, Qt::CaseInsensitive) == 0;
            if (shaMatchesExpected) {
                etag = cached->etag;
                lastModified = cached->lastModified;
            }
        }
    }

    QNetworkRequest request = m_network->buildRequest(task->info.url, etag, lastModified);

    task->resumeOffset = 0;
    task->resumedFromPartial = false;
    task->responseInitialized = false;
    task->statusCode = 0;
    task->abortIntent = AbortIntent::None;
    task->hasher.reset();

    const QFileInfo partInfo(partFilePathFor(task->info.destinationPath));
    if (etag.isEmpty() && lastModified.isEmpty() && partInfo.exists() && partInfo.size() > 0) {
        task->resumeOffset = partInfo.size();
        request.setRawHeader("Range",
                             QByteArray("bytes=") + QByteArray::number(task->resumeOffset) + "-");
    }

    updateStatus(task, Status::Downloading);

    QNetworkReply* reply = m_network->accessManager()->get(request);
    task->reply = reply;

    connect(reply, &QNetworkReply::metaDataChanged, this, [task]() {
        if (task->reply.isNull() || task->responseInitialized) {
            return;
        }
        const HttpCacheHeaders headers = NetworkManager::extractCacheHeaders(task->reply.data());
        task->statusCode = headers.statusCode;
        task->info.etag = headers.etag;
        task->info.lastModified = headers.lastModified;

        if (headers.notModified) {
            task->responseInitialized = true;
            return;
        }

        if (task->statusCode >= 400) {
            task->responseInitialized = true;
            return;
        }

        const QString partPath = partFilePathFor(task->info.destinationPath);
        if (!QDir().mkpath(QFileInfo(partPath).absolutePath())) {
            task->info.errorMessage =
                QStringLiteral("Failed creating directory for %1").arg(partPath);
            task->reply->abort();
            return;
        }

        task->partFile = std::make_unique<QFile>(partPath);
        const bool isPartialAppend = (task->statusCode == 206 && task->resumeOffset > 0);
        const QIODevice::OpenMode mode = isPartialAppend
                                             ? (QIODevice::WriteOnly | QIODevice::Append)
                                             : (QIODevice::WriteOnly | QIODevice::Truncate);
        if (!task->partFile->open(mode)) {
            task->info.errorMessage =
                QStringLiteral("Failed opening temporary file %1").arg(partPath);
            task->partFile.reset();
            task->reply->abort();
            return;
        }

        task->resumedFromPartial = isPartialAppend;
        if (!isPartialAppend) {
            task->resumeOffset = 0;
            task->info.bytesReceived = 0;
        }
        task->responseInitialized = true;
    });

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, task](qint64 received, qint64 total) {
                if (task->info.status != Status::Downloading) {
                    return;
                }
                const qint64 base = task->resumedFromPartial ? task->resumeOffset : 0;
                task->info.bytesReceived = base + received;
                task->info.bytesTotal = (total > 0) ? (base + total) : 0;
                emit taskProgress(task->info.id, task->info.bytesReceived, task->info.bytesTotal);
                if (!task->progressNotifyClock.isValid() ||
                    task->progressNotifyClock.elapsed() >= 200) {
                    task->progressNotifyClock.restart();
                    emit tasksChanged();
                }
            });

    connect(reply, &QNetworkReply::readyRead, this, [task]() {
        if (task->reply.isNull() || task->partFile == nullptr || !task->partFile->isOpen()) {
            return;
        }
        const QByteArray chunk = task->reply->readAll();
        if (chunk.isEmpty()) {
            return;
        }
        if (task->partFile->write(chunk) != chunk.size()) {
            task->info.errorMessage = QStringLiteral("Disk write failed while streaming download");
            task->reply->abort();
            return;
        }
        if (!task->resumedFromPartial) {
            task->hasher.addData(chunk);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, task]() { finalizeReply(task); });
}

void DownloadService::finalizeReply(const std::shared_ptr<InternalTask>& task)
{
    QNetworkReply* reply = task->reply.data();
    task->reply.clear();

    const QNetworkReply::NetworkError netErr =
        (reply != nullptr) ? reply->error() : QNetworkReply::UnknownNetworkError;
    const QString replyErrorString = (reply != nullptr) ? reply->errorString() : QString{};
    const HttpCacheHeaders headers = NetworkManager::extractCacheHeaders(reply);

    if (headers.statusCode > 0) {
        task->statusCode = headers.statusCode;
    }
    if (!headers.etag.isEmpty()) {
        task->info.etag = headers.etag;
    }
    if (!headers.lastModified.isEmpty()) {
        task->info.lastModified = headers.lastModified;
    }

    // Drain any buffered final bytes cleanly before closing the temporary file.
    if (reply != nullptr && task->partFile != nullptr && task->partFile->isOpen()) {
        const QByteArray tail = reply->readAll();
        if (!tail.isEmpty()) {
            if (task->partFile->write(tail) == tail.size() && !task->resumedFromPartial) {
                task->hasher.addData(tail);
            }
        }
    }

    if (reply != nullptr) {
        reply->deleteLater();
    }

    if (task->abortIntent == AbortIntent::Pause) {
        task->abortIntent = AbortIntent::None;
        cleanupActiveHandles(task, false);
        updateStatus(task, Status::Paused);
        dispatchNext();
        return;
    }

    if (task->abortIntent == AbortIntent::Cancel) {
        task->abortIntent = AbortIntent::None;
        cleanupActiveHandles(task, true);
        updateStatus(task, Status::Cancelled);
        emit taskFinished(task->info.id, false);
        dispatchNext();
        return;
    }

    if (headers.notModified) {
        cleanupActiveHandles(task, true);
        if (m_database != nullptr) {
            if (const std::optional<DownloadCacheRecord> existing =
                    m_database->findDownloadCache(task->info.url.toString());
                existing.has_value()) {
                task->info.actualSha256 = existing->contentHash;
                DownloadCacheRecord updated = *existing;
                if (!headers.etag.isEmpty()) {
                    updated.etag = headers.etag;
                }
                if (!headers.lastModified.isEmpty()) {
                    updated.lastModified = headers.lastModified;
                }
                (void)m_database->upsertDownloadCache(updated);
            }
        }
        updateStatus(task, Status::CachedNotModified);
        emit taskFinished(task->info.id, true);
        dispatchNext();
        return;
    }

    const bool httpOk = (netErr == QNetworkReply::NoError && task->statusCode < 400);
    if (!httpOk) {
        const QString reason =
            task->info.errorMessage.isEmpty() ? replyErrorString : task->info.errorMessage;
        const bool transient = NetworkManager::isRetryableStatus(task->statusCode) ||
                               netErr == QNetworkReply::TimeoutError ||
                               netErr == QNetworkReply::TemporaryNetworkFailureError ||
                               netErr == QNetworkReply::RemoteHostClosedError;

        cleanupActiveHandles(task, !transient);
        if (transient && task->info.retryCount < m_maxRetries) {
            scheduleRetry(task, reason);
            dispatchNext();
            return;
        }

        task->info.errorMessage = reason;
        updateStatus(task, Status::Failed);
        emit taskFinished(task->info.id, false);
        dispatchNext();
        return;
    }

    if (task->partFile == nullptr) {
        task->info.errorMessage = QStringLiteral("No file stream was opened for download");
        updateStatus(task, Status::Failed);
        emit taskFinished(task->info.id, false);
        dispatchNext();
        return;
    }

    task->partFile->flush();
    task->partFile->close();
    const QString partPath = task->partFile->fileName();
    task->partFile.reset();

    const QString computedHash = task->resumedFromPartial
                                     ? SystemUtils::sha256File(partPath)
                                     : QString::fromLatin1(task->hasher.result().toHex());
    task->info.actualSha256 = computedHash;

    if (!task->info.expectedSha256.isEmpty() &&
        computedHash.compare(task->info.expectedSha256, Qt::CaseInsensitive) != 0) {
        QFile::remove(partPath);
        task->info.errorMessage = QStringLiteral("SHA-256 checksum mismatch (expected %1, got %2)")
                                      .arg(task->info.expectedSha256, computedHash);
        updateStatus(task, Status::Failed);
        emit taskFinished(task->info.id, false);
        dispatchNext();
        return;
    }

    QString moveError;
    if (!replaceTargetAtomically(partPath, task->info.destinationPath, &moveError)) {
        QFile::remove(partPath);
        task->info.errorMessage = moveError;
        updateStatus(task, Status::Failed);
        emit taskFinished(task->info.id, false);
        dispatchNext();
        return;
    }

    if (m_database != nullptr) {
        DownloadCacheRecord cacheRecord;
        cacheRecord.url = task->info.url.toString();
        cacheRecord.etag = task->info.etag;
        cacheRecord.lastModified = task->info.lastModified;
        cacheRecord.localPath = task->info.destinationPath;
        cacheRecord.contentHash = computedHash;
        (void)m_database->upsertDownloadCache(cacheRecord);
    }

    task->info.bytesReceived = QFileInfo(task->info.destinationPath).size();
    if (task->info.bytesTotal <= 0) {
        task->info.bytesTotal = task->info.bytesReceived;
    }
    emit taskProgress(task->info.id, task->info.bytesReceived, task->info.bytesTotal);

    updateStatus(task, Status::Completed);
    emit taskFinished(task->info.id, true);
    dispatchNext();
}

void DownloadService::cleanupActiveHandles(const std::shared_ptr<InternalTask>& task,
                                           bool removePartFile)
{
    if (task->partFile != nullptr) {
        if (task->partFile->isOpen()) {
            task->partFile->flush();
            task->partFile->close();
        }
        const QString path = task->partFile->fileName();
        task->partFile.reset();
        if (removePartFile && !path.isEmpty()) {
            QFile::remove(path);
        }
    } else if (removePartFile) {
        QFile::remove(partFilePathFor(task->info.destinationPath));
    }
}

void DownloadService::scheduleRetry(const std::shared_ptr<InternalTask>& task,
                                    const QString& reason)
{
    const int delayMs =
        NetworkManager::retryDelayMs(task->info.retryCount, m_retryBaseDelayMs, 15000);
    ++task->info.retryCount;
    task->info.errorMessage = reason;
    updateStatus(task, Status::Paused);

    const QPointer<DownloadService> guard(this);
    QTimer::singleShot(delayMs, this, [guard, task]() {
        if (guard.isNull() || task->info.status != Status::Paused) {
            return;
        }
        guard->updateStatus(task, Status::Queued);
        guard->dispatchNext();
    });
}

void DownloadService::updateStatus(const std::shared_ptr<InternalTask>& task, Status nextStatus)
{
    if (task->info.status == nextStatus) {
        return;
    }
    task->info.status = nextStatus;
    emit taskStateChanged(task->info.id, nextStatus);
    emit countsChanged();
    emit tasksChanged();
}

} // namespace kmedia
