#include "network/BackgroundMusicLibrary.h"

#include "network/CatalogClient.h"
#include "network/DownloadService.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <map>

namespace kmedia {

BackgroundMusicLibrary::BackgroundMusicLibrary(CatalogClient* catalogClient,
                                               DownloadService* downloadService, QObject* parent)
    : QObject(parent), m_catalogClient(catalogClient), m_downloadService(downloadService)
{
    if (m_downloadService != nullptr) {
        // A finished download, failed or not, can be asked for again.
        connect(m_downloadService, &DownloadService::taskFinished, this,
                [this](const QString& taskId, bool /*success*/) {
                    m_queuedFiles.remove(m_downloadTasks.take(taskId));
                });
    }
}

void BackgroundMusicLibrary::setCacheRoot(const QString& cacheRoot)
{
    m_cacheRoot = cacheRoot;
}

void BackgroundMusicLibrary::setLanguage(const QString& languageCode, bool signLanguage)
{
    const QString cleaned =
        languageCode.trimmed().isEmpty() ? QStringLiteral("E") : languageCode.trimmed().toUpper();
    m_languageCode = cleaned;
    m_signLanguage = signLanguage;
}

QString BackgroundMusicLibrary::languageCode() const
{
    return m_languageCode;
}

QString BackgroundMusicLibrary::songbookSymbol(bool signLanguage)
{
    return signLanguage ? QStringLiteral("sjj") : QStringLiteral("sjjm");
}

QString BackgroundMusicLibrary::songbookFormat(bool signLanguage)
{
    return signLanguage ? QStringLiteral("MP4") : QStringLiteral("MP3");
}

QString BackgroundMusicLibrary::songbookKey() const
{
    return songbookSymbol(m_signLanguage) + QLatin1Char('_') + m_languageCode;
}

std::vector<BackgroundSong>
BackgroundMusicLibrary::songsFromAssets(const std::vector<CatalogMediaAsset>& assets)
{
    std::map<int, const CatalogMediaAsset*> byTrack;
    for (const CatalogMediaAsset& asset : assets) {
        if (asset.track <= 0 || asset.url.isEmpty()) {
            continue;
        }
        const auto found = byTrack.find(asset.track);
        if (found == byTrack.end()) {
            byTrack.emplace(asset.track, &asset);
        } else if (asset.fileSize > 0 &&
                   (found->second->fileSize <= 0 || asset.fileSize < found->second->fileSize)) {
            found->second = &asset;
        }
    }

    std::vector<BackgroundSong> songs;
    songs.reserve(byTrack.size());
    for (const auto& [track, asset] : byTrack) {
        BackgroundSong song;
        song.track = track;
        song.title = asset->title.isEmpty() ? QString::number(track) : asset->title;
        song.durationSeconds = asset->durationSeconds;
        song.remoteUrl = asset->url;
        song.checksum = asset->sha256;
        song.fileSize = asset->fileSize;
        songs.push_back(song);
    }
    return songs;
}

void BackgroundMusicLibrary::requestSongs(QObject* context, SongsCallback callback)
{
    const QString key = songbookKey();
    if (const auto known = m_songbooks.constFind(key); known != m_songbooks.constEnd()) {
        callback(known.value());
        return;
    }

    const bool alreadyAsked = m_waiters.contains(key);
    m_waiters[key].append(Waiter{context, std::move(callback)});
    if (alreadyAsked) {
        return;
    }
    if (m_catalogClient == nullptr) {
        finishRequest(key, {});
        return;
    }

    const QUrl endpoint = CatalogClient::pubMediaLinksUrl(
        songbookSymbol(m_signLanguage), m_languageCode, 0, songbookFormat(m_signLanguage));
    m_catalogClient->requestPubMediaLinks(
        endpoint, m_languageCode, this,
        [this, key](bool ok, const std::vector<CatalogMediaAsset>& assets) {
            finishRequest(key, ok ? songsFromAssets(assets) : std::vector<BackgroundSong>{});
        });
}

void BackgroundMusicLibrary::finishRequest(const QString& key,
                                           const std::vector<BackgroundSong>& songs)
{
    // A failed fetch is not remembered, so the next request tries again.
    if (!songs.empty()) {
        m_songbooks.insert(key, songs);
    }
    const QList<Waiter> waiters = m_waiters.take(key);
    for (const Waiter& waiter : waiters) {
        if (!waiter.context.isNull()) {
            waiter.callback(songs);
        }
    }
}

QString BackgroundMusicLibrary::localPath(const BackgroundSong& song) const
{
    if (m_cacheRoot.isEmpty() || song.remoteUrl.isEmpty()) {
        return {};
    }
    const QString fileName = QUrl(song.remoteUrl).fileName();
    if (fileName.isEmpty()) {
        return {};
    }
    return QDir(m_cacheRoot).filePath(QStringLiteral("Music/%1/%2").arg(songbookKey(), fileName));
}

QUrl BackgroundMusicLibrary::playbackUrl(const BackgroundSong& song) const
{
    const QString path = localPath(song);
    const QFileInfo file(path);
    const bool complete = !path.isEmpty() && file.isFile() &&
                          (song.fileSize > 0 ? file.size() == song.fileSize : file.size() > 0);
    return complete ? QUrl::fromLocalFile(path) : QUrl(song.remoteUrl);
}

void BackgroundMusicLibrary::cacheSong(const BackgroundSong& song)
{
    const QString path = localPath(song);
    if (path.isEmpty() || m_downloadService == nullptr || m_queuedFiles.contains(path) ||
        playbackUrl(song).isLocalFile()) {
        return;
    }
    (void)QDir().mkpath(QFileInfo(path).absolutePath());
    const QString taskId = m_downloadService->enqueue(
        QUrl(song.remoteUrl), path, song.checksum, DownloadService::Priority::Low, {}, song.title);
    if (!taskId.isEmpty()) {
        m_queuedFiles.insert(path);
        m_downloadTasks.insert(taskId, path);
    }
}

} // namespace kmedia
