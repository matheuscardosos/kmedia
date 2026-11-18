#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>

#include <atomic>
#include <functional>
#include <list>

namespace kmedia {

class ThumbnailCache : public QObject
{
    Q_OBJECT

public:
    using CustomExtractor = std::function<QImage(const QString& sourcePath, int targetWidth)>;

    explicit ThumbnailCache(QObject* parent = nullptr);
    ~ThumbnailCache() override;

    Q_DISABLE_COPY_MOVE(ThumbnailCache)

    [[nodiscard]] bool initialize(const QString& cacheDir,
                                  qint64 maxCacheBytes = 128LL * 1024 * 1024);

    [[nodiscard]] QString cacheDir() const;
    [[nodiscard]] qint64 maxCacheBytes() const;
    void setMaxCacheBytes(qint64 maxBytes);
    [[nodiscard]] qint64 totalCachedBytes() const;
    [[nodiscard]] int cachedCount() const;
    [[nodiscard]] int targetWidth() const;
    void setTargetWidth(int width);
    [[nodiscard]] QByteArray imageFormat() const;

    void setCustomExtractor(CustomExtractor extractor);

    [[nodiscard]] QString cachedPathIfPresent(const QString& key);
    void requestThumbnail(const QString& key, const QString& sourcePath);
    void clear();
    void waitForAll();

    [[nodiscard]] static QString makeKeyForFile(const QString& filePath);

signals:
    void thumbnailReady(const QString& key, const QString& cachedPath);
    void thumbnailFailed(const QString& key, const QString& reason);

private:
    struct CacheEntry
    {
        QString key;
        QString filePath;
        qint64 bytes{0};
    };

    void handleTaskFinished(quint64 generation, const QString& cacheKey, const QString& originalKey,
                            const QString& outPath, qint64 bytes, bool ok, const QString& error);
    void recordAndEnforceLimit(const QString& key, const QString& filePath, qint64 bytes);
    void touchKey(const QString& key, bool updateDiskTimestamp);
    void evictIfNeeded();
    void scanExistingFiles();
    [[nodiscard]] QString outputFilePathForKey(const QString& key) const;

    QString m_cacheDir;
    qint64 m_maxCacheBytes{128LL * 1024 * 1024};
    qint64 m_totalBytes{0};
    int m_targetWidth{320};
    QByteArray m_imageFormat;
    CustomExtractor m_customExtractor;

    std::list<CacheEntry> m_lruList;
    QHash<QString, std::list<CacheEntry>::iterator> m_index;
    QSet<QString> m_inFlight;
    QThreadPool m_pool;
    std::atomic<quint64> m_generation{1};
    std::atomic<bool> m_shuttingDown{false};
};

} // namespace kmedia
