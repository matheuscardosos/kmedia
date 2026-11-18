#include "media/ThumbnailCache.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPdfWriter>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace {

void writeNoisePng(const QString& path, int width, int height, quint32 seedBase)
{
    QImage img(width, height, QImage::Format_RGBA8888);
    for (int y = 0; y < height; ++y) {
        auto* row = reinterpret_cast<quint32*>(img.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const quint32 v = seedBase ^ ((static_cast<quint32>(x) * 2654435761u) ^
                                          (static_cast<quint32>(y) * 2246822519u));
            row[x] = 0xFF000000u | (v & 0x00FFFFFFu);
        }
    }
    REQUIRE(img.save(path, "PNG"));
}

} // namespace

TEST_CASE("ThumbnailCache downscales oversized images asynchronously and caches hits",
          "[thumbnails][media]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString srcImage = tempDir.filePath(QStringLiteral("large.png"));
    writeNoisePng(srcImage, 1280, 720, 0x112233u);

    kmedia::ThumbnailCache cache;
    REQUIRE(cache.initialize(tempDir.filePath(QStringLiteral("thumbs")), 8LL * 1024 * 1024));
    cache.setTargetWidth(320);

    const QString key = kmedia::ThumbnailCache::makeKeyForFile(srcImage);
    REQUIRE(cache.cachedPathIfPresent(key).isEmpty());

    QSignalSpy readySpy(&cache, &kmedia::ThumbnailCache::thumbnailReady);
    cache.requestThumbnail(key, srcImage);
    cache.requestThumbnail(key, srcImage); // deduplicated in-flight request

    REQUIRE(readySpy.wait(5000));
    REQUIRE(readySpy.count() == 1);

    const QString cachedPath = cache.cachedPathIfPresent(key);
    REQUIRE_FALSE(cachedPath.isEmpty());
    REQUIRE(QFileInfo::exists(cachedPath));

    const QImage thumb(cachedPath);
    REQUIRE_FALSE(thumb.isNull());
    REQUIRE(thumb.width() == 320);
    REQUIRE(thumb.height() == 180);
    REQUIRE(cache.totalCachedBytes() == QFileInfo(cachedPath).size());
}

TEST_CASE("ThumbnailCache renders PDF first-page thumbnails via QtPdf", "[thumbnails][pdf][media]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString pdfPath = tempDir.filePath(QStringLiteral("song.pdf"));
    {
        QPdfWriter writer(pdfPath);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QPainter painter(&writer);
        painter.fillRect(QRect(0, 0, writer.width(), writer.height()), QColor(0x14, 0x17, 0x1C));
        painter.setPen(QColor(0xFF, 0x6B, 0x00));
        painter.drawRect(100, 100, writer.width() - 200, writer.height() - 200);
        painter.end();
    }

    kmedia::ThumbnailCache cache;
    REQUIRE(cache.initialize(tempDir.filePath(QStringLiteral("thumbs")), 8LL * 1024 * 1024));

    const QString key = kmedia::ThumbnailCache::makeKeyForFile(pdfPath);
    QSignalSpy readySpy(&cache, &kmedia::ThumbnailCache::thumbnailReady);
    cache.requestThumbnail(key, pdfPath);
    REQUIRE(readySpy.wait(5000));

    const QString cachedPath = cache.cachedPathIfPresent(key);
    REQUIRE_FALSE(cachedPath.isEmpty());
    const QImage rendered(cachedPath);
    REQUIRE_FALSE(rendered.isNull());
    REQUIRE(rendered.width() == 320);
}

TEST_CASE("ThumbnailCache enforces disk LRU eviction when byte budget is exceeded",
          "[thumbnails][lru][media]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString img1 = tempDir.filePath(QStringLiteral("one.png"));
    const QString img2 = tempDir.filePath(QStringLiteral("two.png"));
    const QString img3 = tempDir.filePath(QStringLiteral("three.png"));
    writeNoisePng(img1, 640, 360, 0x10101u);
    writeNoisePng(img2, 640, 360, 0x20202u);
    writeNoisePng(img3, 640, 360, 0x30303u);

    // Measure size of a single encoded thumbnail so our LRU cap holds exactly two items
    kmedia::ThumbnailCache probe;
    REQUIRE(probe.initialize(tempDir.filePath(QStringLiteral("probe_cache")), 1024LL * 1024));
    QSignalSpy probeSpy(&probe, &kmedia::ThumbnailCache::thumbnailReady);
    probe.requestThumbnail(QStringLiteral("k1"), img1);
    REQUIRE(probeSpy.wait(5000));
    const qint64 singleSize = QFileInfo(probe.cachedPathIfPresent(QStringLiteral("k1"))).size();
    REQUIRE(singleSize > 0);

    const qint64 maxBudget = (singleSize * 2) + 512;
    kmedia::ThumbnailCache lru;
    REQUIRE(lru.initialize(tempDir.filePath(QStringLiteral("lru_cache")), maxBudget));

    QSignalSpy spy(&lru, &kmedia::ThumbnailCache::thumbnailReady);
    lru.requestThumbnail(QStringLiteral("item1"), img1);
    REQUIRE(spy.wait(5000));
    lru.requestThumbnail(QStringLiteral("item2"), img2);
    REQUIRE(spy.wait(5000));

    // Touch item1 so item2 becomes the least-recently-used entry
    REQUIRE_FALSE(lru.cachedPathIfPresent(QStringLiteral("item1")).isEmpty());

    lru.requestThumbnail(QStringLiteral("item3"), img3);
    REQUIRE(spy.wait(5000));

    REQUIRE(lru.totalCachedBytes() <= maxBudget);
    REQUIRE(lru.cachedCount() <= 2);
    REQUIRE_FALSE(lru.cachedPathIfPresent(QStringLiteral("item1")).isEmpty());
    REQUIRE_FALSE(lru.cachedPathIfPresent(QStringLiteral("item3")).isEmpty());
    REQUIRE(lru.cachedPathIfPresent(QStringLiteral("item2")).isEmpty());
}
