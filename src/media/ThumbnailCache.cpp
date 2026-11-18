#include "media/ThumbnailCache.h"

#include "core/SystemUtils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QMetaObject>
#include <QPdfDocument>
#include <QSaveFile>

#include <algorithm>
#include <vector>

namespace kmedia {

namespace {

QByteArray selectBestThumbnailFormat()
{
    const QList<QByteArray> supported = QImageWriter::supportedImageFormats();
    if (supported.contains("webp")) {
        return QByteArrayLiteral("webp");
    }
    if (supported.contains("jpeg") || supported.contains("jpg")) {
        return QByteArrayLiteral("jpg");
    }
    return QByteArrayLiteral("png");
}

QSize scaledSizeForWidth(const QSize& sourceSize, int targetWidth)
{
    if (!sourceSize.isValid() || sourceSize.width() <= 0 || sourceSize.height() <= 0) {
        return {targetWidth, qMax(1, (targetWidth * 9) / 16)};
    }
    if (sourceSize.width() <= targetWidth) {
        return sourceSize;
    }
    const int scaledHeight = qMax(1, (sourceSize.height() * targetWidth) / sourceSize.width());
    return {targetWidth, scaledHeight};
}

QImage renderPdfThumbnail(const QString& sourcePath, int targetWidth)
{
    QPdfDocument doc;
    if (doc.load(sourcePath) != QPdfDocument::Error::None || doc.pageCount() <= 0) {
        return {};
    }
    const QSizeF pagePoints = doc.pagePointSize(0);
    if (pagePoints.width() <= 0.0 || pagePoints.height() <= 0.0) {
        return {};
    }
    const int renderWidth = qMax(64, targetWidth);
    const int renderHeight =
        qMax(64, static_cast<int>((pagePoints.height() * renderWidth) / pagePoints.width()));
    return doc.render(0, QSize(renderWidth, renderHeight));
}

QImage decodeScaledThumbnail(const QString& sourcePath, int targetWidth,
                             const ThumbnailCache::CustomExtractor& customExtractor)
{
    const QFileInfo info(sourcePath);
    if (!info.exists() || !info.isFile()) {
        return {};
    }

    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("pdf")) {
        return renderPdfThumbnail(sourcePath, targetWidth);
    }

    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    if (reader.canRead()) {
        const QSize originalSize = reader.size();
        if (originalSize.isValid() && originalSize.width() > targetWidth) {
            reader.setScaledSize(scaledSizeForWidth(originalSize, targetWidth));
        }
        QImage image = reader.read();
        if (!image.isNull()) {
            if (image.width() > targetWidth) {
                return image.scaledToWidth(targetWidth, Qt::SmoothTransformation);
            }
            return image;
        }
    }

    if (customExtractor) {
        QImage extracted = customExtractor(sourcePath, targetWidth);
        if (!extracted.isNull() && extracted.width() > targetWidth) {
            return extracted.scaledToWidth(targetWidth, Qt::SmoothTransformation);
        }
        return extracted;
    }

    return {};
}

} // namespace

ThumbnailCache::ThumbnailCache(QObject* parent)
    : QObject(parent), m_imageFormat(selectBestThumbnailFormat())
{
    m_pool.setMaxThreadCount(2);
}

ThumbnailCache::~ThumbnailCache()
{
    m_shuttingDown.store(true, std::memory_order_release);
    m_pool.waitForDone();
}

bool ThumbnailCache::initialize(const QString& cacheDir, qint64 maxCacheBytes)
{
    if (cacheDir.trimmed().isEmpty()) {
        return false;
    }

    m_generation.fetch_add(1, std::memory_order_acq_rel);
    m_pool.waitForDone();
    m_inFlight.clear();
    m_cacheDir = QDir::cleanPath(cacheDir);
    if (!QDir().mkpath(m_cacheDir)) {
        return false;
    }

    m_maxCacheBytes = qMax<qint64>(1024, maxCacheBytes);
    scanExistingFiles();
    evictIfNeeded();
    return true;
}

QString ThumbnailCache::cacheDir() const
{
    return m_cacheDir;
}

qint64 ThumbnailCache::maxCacheBytes() const
{
    return m_maxCacheBytes;
}

void ThumbnailCache::setMaxCacheBytes(qint64 maxBytes)
{
    m_maxCacheBytes = qMax<qint64>(1024, maxBytes);
    evictIfNeeded();
}

qint64 ThumbnailCache::totalCachedBytes() const
{
    return m_totalBytes;
}

int ThumbnailCache::cachedCount() const
{
    return static_cast<int>(m_lruList.size());
}

int ThumbnailCache::targetWidth() const
{
    return m_targetWidth;
}

void ThumbnailCache::setTargetWidth(int width)
{
    m_targetWidth = qBound(64, width, 1280);
}

QByteArray ThumbnailCache::imageFormat() const
{
    return m_imageFormat;
}

void ThumbnailCache::setCustomExtractor(CustomExtractor extractor)
{
    m_customExtractor = std::move(extractor);
}

QString ThumbnailCache::cachedPathIfPresent(const QString& key)
{
    const QString safeKey = SystemUtils::sanitizeFileName(key);
    const auto it = m_index.constFind(safeKey);
    if (it != m_index.constEnd()) {
        auto listIt = it.value();
        if (QFileInfo::exists(listIt->filePath)) {
            touchKey(safeKey, true);
            return listIt->filePath;
        }
        m_totalBytes -= listIt->bytes;
        m_lruList.erase(listIt);
        m_index.remove(safeKey);
        return {};
    }

    const QString candidate = outputFilePathForKey(safeKey);
    const QFileInfo info(candidate);
    if (info.exists() && info.isFile() && info.size() > 0) {
        recordAndEnforceLimit(safeKey, candidate, info.size());
        return candidate;
    }
    return {};
}

void ThumbnailCache::requestThumbnail(const QString& key, const QString& sourcePath)
{
    if (m_cacheDir.isEmpty() || key.trimmed().isEmpty()) {
        emit thumbnailFailed(key, QStringLiteral("ThumbnailCache is not initialized"));
        return;
    }

    const QString safeKey = SystemUtils::sanitizeFileName(key);
    const QString cached = cachedPathIfPresent(safeKey);
    if (!cached.isEmpty()) {
        emit thumbnailReady(key, cached);
        return;
    }

    if (m_inFlight.contains(safeKey)) {
        return;
    }
    m_inFlight.insert(safeKey);

    const QString outPath = outputFilePathForKey(safeKey);
    const QByteArray format = m_imageFormat;
    const int width = m_targetWidth;
    const CustomExtractor extractor = m_customExtractor;
    const quint64 generation = m_generation.load(std::memory_order_acquire);

    m_pool.start([this, generation, safeKey, key, sourcePath, outPath, format, width, extractor]() {
        if (m_shuttingDown.load(std::memory_order_acquire) ||
            generation != m_generation.load(std::memory_order_acquire)) {
            return;
        }

        const QImage image = decodeScaledThumbnail(sourcePath, width, extractor);
        if (image.isNull()) {
            QMetaObject::invokeMethod(
                this,
                [this, generation, safeKey, key, outPath]() {
                    handleTaskFinished(generation, safeKey, key, outPath, 0, false,
                                       QStringLiteral("Unable to decode thumbnail from source"));
                },
                Qt::QueuedConnection);
            return;
        }

        QSaveFile saveFile(outPath);
        const bool saved = saveFile.open(QIODevice::WriteOnly) &&
                           image.save(&saveFile, format.constData(), 82) && saveFile.commit();
        const qint64 writtenBytes = saved ? QFileInfo(outPath).size() : 0;

        if (m_shuttingDown.load(std::memory_order_acquire) ||
            generation != m_generation.load(std::memory_order_acquire)) {
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, generation, safeKey, key, outPath, writtenBytes, saved]() {
                handleTaskFinished(generation, safeKey, key, outPath, writtenBytes, saved,
                                   saved ? QString()
                                         : QStringLiteral("Failed to persist thumbnail file"));
            },
            Qt::QueuedConnection);
    });
}

void ThumbnailCache::clear()
{
    m_generation.fetch_add(1, std::memory_order_acq_rel);
    m_pool.waitForDone();
    for (const auto& entry : m_lruList) {
        QFile::remove(entry.filePath);
    }
    m_lruList.clear();
    m_index.clear();
    m_inFlight.clear();
    m_totalBytes = 0;
}

void ThumbnailCache::waitForAll()
{
    m_pool.waitForDone();
}

QString ThumbnailCache::makeKeyForFile(const QString& filePath)
{
    const QFileInfo info(filePath);
    const QString canonical = info.canonicalFilePath().isEmpty()
                                  ? QDir::cleanPath(info.absoluteFilePath())
                                  : info.canonicalFilePath();
    const QString descriptor = QStringLiteral("%1|%2|%3")
                                   .arg(canonical)
                                   .arg(info.size())
                                   .arg(info.lastModified().toMSecsSinceEpoch());
    return SystemUtils::sha256Hex(descriptor.toUtf8());
}

void ThumbnailCache::handleTaskFinished(quint64 generation, const QString& cacheKey,
                                        const QString& originalKey, const QString& outPath,
                                        qint64 bytes, bool ok, const QString& error)
{
    if (generation != m_generation.load(std::memory_order_acquire)) {
        return;
    }

    m_inFlight.remove(cacheKey);
    if (!ok || bytes <= 0) {
        emit thumbnailFailed(originalKey, error);
        return;
    }

    recordAndEnforceLimit(cacheKey, outPath, bytes);
    if (QFileInfo::exists(outPath)) {
        emit thumbnailReady(originalKey, outPath);
    } else {
        emit thumbnailFailed(originalKey,
                             QStringLiteral("Thumbnail evicted immediately by cache limit"));
    }
}

void ThumbnailCache::recordAndEnforceLimit(const QString& key, const QString& filePath,
                                           qint64 bytes)
{
    const auto it = m_index.constFind(key);
    if (it != m_index.constEnd()) {
        auto listIt = it.value();
        m_totalBytes -= listIt->bytes;
        listIt->filePath = filePath;
        listIt->bytes = bytes;
        m_totalBytes += bytes;
        m_lruList.splice(m_lruList.begin(), m_lruList, listIt);
    } else {
        m_lruList.push_front(CacheEntry{key, filePath, bytes});
        m_index.insert(key, m_lruList.begin());
        m_totalBytes += bytes;
    }

    evictIfNeeded();
}

void ThumbnailCache::touchKey(const QString& key, bool updateDiskTimestamp)
{
    const auto it = m_index.constFind(key);
    if (it == m_index.constEnd()) {
        return;
    }
    auto listIt = it.value();
    m_lruList.splice(m_lruList.begin(), m_lruList, listIt);

    if (updateDiskTimestamp) {
        QFile file(listIt->filePath);
        if (file.open(QIODevice::ReadWrite)) {
            file.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
        }
    }
}

void ThumbnailCache::evictIfNeeded()
{
    while (m_totalBytes > m_maxCacheBytes && !m_lruList.empty()) {
        const CacheEntry oldest = m_lruList.back();
        m_lruList.pop_back();
        m_index.remove(oldest.key);
        m_totalBytes = qMax<qint64>(0, m_totalBytes - oldest.bytes);
        QFile::remove(oldest.filePath);
    }
}

void ThumbnailCache::scanExistingFiles()
{
    m_lruList.clear();
    m_index.clear();
    m_totalBytes = 0;

    QDir dir(m_cacheDir);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time);

    for (const QFileInfo& info : entries) {
        const QString key = info.completeBaseName();
        const qint64 size = info.size();
        m_lruList.push_back(CacheEntry{key, info.absoluteFilePath(), size});
        m_index.insert(key, std::prev(m_lruList.end()));
        m_totalBytes += size;
    }
}

QString ThumbnailCache::outputFilePathForKey(const QString& key) const
{
    return QDir::cleanPath(m_cacheDir + QLatin1Char('/') + key + QLatin1Char('.') +
                           QString::fromLatin1(m_imageFormat));
}

} // namespace kmedia
