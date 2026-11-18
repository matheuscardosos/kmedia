#include "core/DatabaseManager.h"
#include "core/NetworkManager.h"
#include "core/SettingsStore.h"
#include "core/SystemUtils.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("SettingsStore reads defaults, persists updates, and emits signals", "[settings][core]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    kmedia::DatabaseManager db;
    REQUIRE(db.open(tempDir.filePath(QStringLiteral("settings.db"))));

    kmedia::SettingsStore store;
    store.bindDatabase(&db);

    REQUIRE(store.mediaLanguage() == QStringLiteral("E"));
    REQUIRE(store.backgroundMusicFadeMs() == 5000);
    REQUIRE(store.maxConcurrentDownloads() == 3);
    REQUIRE(store.hardwareDecoder() == QStringLiteral("auto-safe"));
    REQUIRE(store.thumbnailCacheMaxMb() == 128);

    QSignalSpy changedSpy(&store, &kmedia::SettingsStore::settingChanged);
    REQUIRE(store.setValue(QStringLiteral("audio/background_fade_ms"), 7500));
    REQUIRE(store.setValue(QStringLiteral("zoom/auto_share"), true));
    REQUIRE(changedSpy.count() == 2);
    REQUIRE(store.backgroundMusicFadeMs() == 7500);
    REQUIRE(store.boolValue(QStringLiteral("zoom/auto_share"), false));

    kmedia::SettingsStore reloaded;
    reloaded.bindDatabase(&db);
    REQUIRE(reloaded.backgroundMusicFadeMs() == 7500);
    REQUIRE(reloaded.boolValue(QStringLiteral("zoom/auto_share"), false));
}

TEST_CASE("NetworkManager configures polite User-Agent, conditional headers, and backoff",
          "[network][core]")
{
    kmedia::NetworkManager net;
    REQUIRE(net.userAgent().startsWith(QStringLiteral("KMedia/")));
    REQUIRE(net.maxConcurrentRequests() == 3);

    const QNetworkRequest req = net.buildRequest(
        QUrl(QStringLiteral("https://example.org/catalog.json")), QStringLiteral("\"etag-42\""),
        QStringLiteral("Wed, 18 Nov 2026 12:00:00 GMT"));

    REQUIRE(req.rawHeader("If-None-Match") == QByteArray("\"etag-42\""));
    REQUIRE(req.rawHeader("If-Modified-Since") == QByteArray("Wed, 18 Nov 2026 12:00:00 GMT"));
    REQUIRE(req.attribute(QNetworkRequest::Http2AllowedAttribute).toBool());

    REQUIRE(kmedia::NetworkManager::isRetryableStatus(429));
    REQUIRE(kmedia::NetworkManager::isRetryableStatus(503));
    REQUIRE_FALSE(kmedia::NetworkManager::isRetryableStatus(404));
    REQUIRE(kmedia::NetworkManager::retryDelayMs(0, 1000, 16000) == 1000);
    REQUIRE(kmedia::NetworkManager::retryDelayMs(2, 1000, 16000) == 4000);
    REQUIRE(kmedia::NetworkManager::retryDelayMs(10, 1000, 16000) == 16000);
}

TEST_CASE("SystemUtils hashes files, sanitizes filenames, and formats units", "[system][core]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString samplePath = tempDir.filePath(QStringLiteral("sample.bin"));
    QFile f(samplePath);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write("kmedia-test-payload");
    f.close();

    const QString fileHash = kmedia::SystemUtils::sha256File(samplePath);
    REQUIRE(fileHash == kmedia::SystemUtils::sha256Hex(QByteArray("kmedia-test-payload")));
    REQUIRE(fileHash.size() == 64);

    REQUIRE(kmedia::SystemUtils::sanitizeFileName(QStringLiteral("Talk: #1 / Part?")) ==
            QStringLiteral("Talk_ #1 _ Part_"));
    REQUIRE(kmedia::SystemUtils::sanitizeFileName(QStringLiteral("..")) == QStringLiteral("item"));
    REQUIRE(kmedia::SystemUtils::formatFileSize(512) == QStringLiteral("512 B"));
    REQUIRE(kmedia::SystemUtils::formatDurationMs(65000) == QStringLiteral("01:05"));
    REQUIRE(kmedia::SystemUtils::formatDurationMs(3661000) == QStringLiteral("1:01:01"));
    REQUIRE(kmedia::SystemUtils::processMemoryRssBytes() > 0);
}
