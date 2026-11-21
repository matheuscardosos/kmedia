#include "network/PublicationStore.h"

#include "network/CatalogClient.h"
#include "parsers/JwpubParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>
#include <memory>

namespace kmedia {

namespace {

constexpr int kWorkerThreads = 2;

const CatalogMediaAsset* bestAsset(const std::vector<CatalogMediaAsset>& assets,
                                   const QString& format, int maxResolution)
{
    const CatalogMediaAsset* best = nullptr;
    const CatalogMediaAsset* lowest = nullptr;
    for (const CatalogMediaAsset& asset : assets) {
        if (asset.url.isEmpty() || asset.fileFormat.compare(format, Qt::CaseInsensitive) != 0) {
            continue;
        }
        const int resolution = PublicationStore::resolutionOf(asset.label);
        if (lowest == nullptr || resolution < PublicationStore::resolutionOf(lowest->label)) {
            lowest = &asset;
        }
        if (resolution <= maxResolution &&
            (best == nullptr || resolution > PublicationStore::resolutionOf(best->label))) {
            best = &asset;
        }
    }
    // Nothing at or below the preferred resolution: take the smallest file available.
    return best != nullptr ? best : lowest;
}

} // namespace

PublicationStore::PublicationStore(CatalogClient* catalogClient, DownloadService* downloadService,
                                   QObject* parent)
    : QObject(parent), m_catalogClient(catalogClient), m_downloadService(downloadService)
{
    m_workers.setMaxThreadCount(kWorkerThreads);
    if (m_downloadService != nullptr) {
        connect(m_downloadService, &DownloadService::taskFinished, this,
                &PublicationStore::onDownloadFinished);
    }
}

PublicationStore::~PublicationStore()
{
    m_workers.waitForDone();
}

void PublicationStore::setCacheRoot(const QString& cacheRoot)
{
    m_cacheRoot = cacheRoot.trimmed();
}

QString PublicationStore::cacheRoot() const
{
    return m_cacheRoot;
}

void PublicationStore::setMaxResolution(int maxResolution)
{
    m_maxResolution = maxResolution > 0 ? maxResolution : 720;
}

QString PublicationStore::publicationKey(const QString& symbol, const QString& languageCode,
                                         int issueTagNumber)
{
    QStringList parts{symbol.trimmed(), languageCode.trimmed()};
    if (issueTagNumber > 0) {
        parts << QString::number(issueTagNumber);
    }
    parts.removeAll(QString());
    return parts.join(QLatin1Char('_'));
}

int PublicationStore::resolutionOf(const QString& label)
{
    static const QRegularExpression kResolution(QStringLiteral("([0-9]{3,4})p"));
    const QRegularExpressionMatch match = kResolution.match(label);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

QString PublicationStore::publicationFolder(const QString& symbol, const QString& languageCode,
                                            int issueTagNumber) const
{
    return QDir(m_cacheRoot)
        .filePath(QStringLiteral("Publications/") +
                  publicationKey(symbol, languageCode, issueTagNumber));
}

void PublicationStore::runInBackground(QObject* context, std::function<void()> work,
                                       std::function<void()> done)
{
    const QPointer<QObject> guard(context);
    m_workers.start([this, guard, work = std::move(work), done = std::move(done)]() {
        work();
        QMetaObject::invokeMethod(
            this,
            [guard, done]() {
                if (!guard.isNull()) {
                    done();
                }
            },
            Qt::QueuedConnection);
    });
}

void PublicationStore::ensurePublication(const QString& symbol, int issueTagNumber,
                                         const QString& languageCode,
                                         DownloadService::Priority priority, QObject* context,
                                         PublicationCallback callback)
{
    const QString key = publicationKey(symbol, languageCode, issueTagNumber);
    if (m_cacheRoot.isEmpty() || symbol.trimmed().isEmpty() || languageCode.trimmed().isEmpty()) {
        QMetaObject::invokeMethod(
            this, [callback]() { callback(std::nullopt); }, Qt::QueuedConnection);
        return;
    }

    PendingPublication& pending = m_pendingPublications[key];
    pending.waiters.emplace_back(QPointer<QObject>(context), std::move(callback));
    if (pending.waiters.size() > 1) {
        return;
    }
    pending.symbol = symbol.trimmed();
    pending.languageCode = languageCode.trimmed();
    pending.issueTagNumber = issueTagNumber;

    const QString jwpubPath = QDir(publicationFolder(symbol, languageCode, issueTagNumber))
                                  .filePath(key + QStringLiteral(".jwpub"));
    if (QFileInfo(jwpubPath).size() > 0) {
        extractPublication(key, jwpubPath);
        return;
    }
    if (m_catalogClient == nullptr || m_downloadService == nullptr) {
        finishPublication(key, std::nullopt);
        return;
    }

    m_catalogClient->requestPubMediaLinks(
        CatalogClient::pubMediaLinksUrl(pending.symbol, pending.languageCode, issueTagNumber,
                                        QStringLiteral("JWPUB")),
        pending.languageCode, this,
        [this, key, jwpubPath, priority](bool ok, const std::vector<CatalogMediaAsset>& assets) {
            const CatalogMediaAsset* archive =
                ok ? bestAsset(assets, QStringLiteral("JWPUB"), 0) : nullptr;
            if (archive == nullptr) {
                finishPublication(key, std::nullopt);
                return;
            }
            const QString taskId = m_downloadService->enqueue(
                QUrl(archive->url), jwpubPath, archive->sha256, priority, {}, archive->title);
            const auto info = m_downloadService->taskInfo(taskId);
            if (!info.has_value() || info->status == DownloadService::Status::Failed) {
                finishPublication(key, std::nullopt);
                return;
            }
            m_publicationTasks.insert(taskId, key);
        });
}

void PublicationStore::extractPublication(const QString& key, const QString& jwpubPath)
{
    const QString folder = QFileInfo(jwpubPath).absolutePath();
    auto databasePath = std::make_shared<QString>();
    runInBackground(
        this,
        [jwpubPath, folder, databasePath]() {
            *databasePath = JwpubParser::extractContents(jwpubPath, folder);
            if (databasePath->isEmpty()) {
                // A damaged archive is downloaded again on the next attempt.
                QFile::remove(jwpubPath);
            }
        },
        [this, key, folder, databasePath]() {
            if (databasePath->isEmpty()) {
                finishPublication(key, std::nullopt);
                return;
            }
            const PendingPublication pending = m_pendingPublications.value(key);
            finishPublication(key, LocalPublication{pending.symbol, pending.languageCode,
                                                    pending.issueTagNumber, folder, *databasePath});
        });
}

void PublicationStore::finishPublication(const QString& key,
                                         const std::optional<LocalPublication>& result)
{
    const PendingPublication pending = m_pendingPublications.take(key);
    for (const auto& [context, callback] : pending.waiters) {
        if (!context.isNull()) {
            callback(result);
        }
    }
}

void PublicationStore::resolveMediaFile(const MediaFileRequest& request,
                                        DownloadService::Priority priority, QObject* context,
                                        MediaCallback callback)
{
    tryResolveLanguage(request, 0, priority, QPointer<QObject>(context), callback);
}

void PublicationStore::tryResolveLanguage(const MediaFileRequest& request, int languageIndex,
                                          DownloadService::Priority priority,
                                          const QPointer<QObject>& context,
                                          const MediaCallback& callback)
{
    if (context.isNull()) {
        return;
    }
    if (languageIndex >= request.languages.size() || m_catalogClient == nullptr) {
        callback(ResolvedMediaFile{});
        return;
    }

    const QString language = request.languages.at(languageIndex);
    const QString format = request.isAudio ? QStringLiteral("MP3") : QStringLiteral("MP4");
    const bool bySymbol = !request.keySymbol.isEmpty();
    const QUrl url =
        bySymbol ? CatalogClient::pubMediaLinksUrl(request.keySymbol, language,
                                                   request.issueTagNumber, format, request.track)
                 : CatalogClient::pubMediaLinksUrl({}, language, 0, format, request.track,
                                                   QString::number(request.mepsDocumentId));

    m_catalogClient->requestPubMediaLinks(
        url, language, this,
        [this, request, languageIndex, priority, context, callback, language, format,
         bySymbol](bool ok, const std::vector<CatalogMediaAsset>& assets) {
            const CatalogMediaAsset* best =
                ok ? bestAsset(assets, format, m_maxResolution) : nullptr;
            if (best == nullptr) {
                tryResolveLanguage(request, languageIndex + 1, priority, context, callback);
                return;
            }
            if (context.isNull()) {
                return;
            }

            ResolvedMediaFile resolved;
            resolved.ok = true;
            resolved.title = best->title;
            resolved.durationSeconds = best->durationSeconds;
            if (best->durationSeconds > kStreamOnlySeconds) {
                resolved.streamUrl = best->url;
                callback(resolved);
                return;
            }

            const QString folderSymbol =
                bySymbol ? request.keySymbol : QString::number(request.mepsDocumentId);
            const QString fileName = QFileInfo(QUrl(best->url).path()).fileName();
            resolved.filePath = QDir(publicationFolder(folderSymbol, language,
                                                       bySymbol ? request.issueTagNumber : 0))
                                    .filePath(fileName);
            const bool alreadyQueued = std::find(m_mediaTasks.cbegin(), m_mediaTasks.cend(),
                                                 resolved.filePath) != m_mediaTasks.cend();
            if (!fileName.isEmpty() && QFileInfo(resolved.filePath).size() <= 0 && !alreadyQueued &&
                m_downloadService != nullptr) {
                const QString taskId = m_downloadService->enqueue(
                    QUrl(best->url), resolved.filePath, best->sha256, priority, {}, best->title);
                m_mediaTasks.insert(taskId, resolved.filePath);
            }
            callback(resolved);
        });
}

void PublicationStore::onDownloadFinished(const QString& taskId, bool success)
{
    if (const auto publication = m_publicationTasks.constFind(taskId);
        publication != m_publicationTasks.constEnd()) {
        const QString key = publication.value();
        m_publicationTasks.erase(publication);
        const PendingPublication pending = m_pendingPublications.value(key);
        const QString jwpubPath =
            QDir(publicationFolder(pending.symbol, pending.languageCode, pending.issueTagNumber))
                .filePath(key + QStringLiteral(".jwpub"));
        if (success && QFileInfo(jwpubPath).size() > 0) {
            extractPublication(key, jwpubPath);
        } else {
            finishPublication(key, std::nullopt);
        }
        return;
    }
    if (const auto media = m_mediaTasks.constFind(taskId); media != m_mediaTasks.constEnd()) {
        const QString filePath = media.value();
        m_mediaTasks.erase(media);
        if (success) {
            emit mediaFileReady(filePath);
        } else {
            emit mediaFileFailed(filePath);
        }
    }
}

} // namespace kmedia
