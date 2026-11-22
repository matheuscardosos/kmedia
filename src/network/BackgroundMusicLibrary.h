#pragma once

#include "media/BackgroundMusicPlanner.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QUrl>

#include <functional>
#include <vector>

namespace kmedia {

class CatalogClient;
class DownloadService;
struct CatalogMediaAsset;

/**
 * The songbook that background music plays, in the media language: `sjjm` as MP3, or `sjj` as
 * MP4 for sign languages, inspired by Meeting Media Manager. The song list is fetched once per
 * language; a song plays from `<cache>/Music` once downloaded, and streams from JW.org while it
 * downloads in the background at low priority.
 */
class BackgroundMusicLibrary : public QObject
{
    Q_OBJECT

public:
    using SongsCallback = std::function<void(const std::vector<BackgroundSong>& songs)>;

    BackgroundMusicLibrary(CatalogClient* catalogClient, DownloadService* downloadService,
                           QObject* parent = nullptr);

    void setCacheRoot(const QString& cacheRoot);
    void setLanguage(const QString& languageCode, bool signLanguage);
    [[nodiscard]] QString languageCode() const;

    // Answers with the songbook of the current language (empty when it cannot be fetched).
    void requestSongs(QObject* context, SongsCallback callback);

    // The downloaded file when it is complete, otherwise the JW.org file to stream.
    [[nodiscard]] QUrl playbackUrl(const BackgroundSong& song) const;
    [[nodiscard]] QString localPath(const BackgroundSong& song) const;
    // Downloads a song at low priority unless it is already on disk or on its way.
    void cacheSong(const BackgroundSong& song);

    [[nodiscard]] static QString songbookSymbol(bool signLanguage);
    [[nodiscard]] static QString songbookFormat(bool signLanguage);
    // One file per song, the smallest when the songbook comes in several sizes, by track.
    [[nodiscard]] static std::vector<BackgroundSong>
    songsFromAssets(const std::vector<CatalogMediaAsset>& assets);

private:
    struct Waiter
    {
        QPointer<QObject> context;
        SongsCallback callback;
    };

    void finishRequest(const QString& key, const std::vector<BackgroundSong>& songs);
    [[nodiscard]] QString songbookKey() const;

    CatalogClient* m_catalogClient{nullptr};
    DownloadService* m_downloadService{nullptr};
    QString m_cacheRoot;
    QString m_languageCode{QStringLiteral("E")};
    bool m_signLanguage{false};
    QHash<QString, std::vector<BackgroundSong>> m_songbooks;
    QHash<QString, QList<Waiter>> m_waiters;
    QSet<QString> m_queuedFiles;
    QHash<QString, QString> m_downloadTasks; // task id -> file
};

} // namespace kmedia
