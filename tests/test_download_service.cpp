#include "core/DatabaseManager.h"
#include "core/NetworkManager.h"
#include "core/SystemUtils.h"
#include "network/CatalogClient.h"
#include "network/DownloadService.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

namespace {

class LoopbackHttpServer : public QObject
{
public:
    struct RouteResponse
    {
        int statusCode{200};
        QByteArray statusText{"OK"};
        QByteArray body;
        QByteArray etag;
        int delayMs{0};
    };

    explicit LoopbackHttpServer(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, &LoopbackHttpServer::onNewConnection);
    }

    bool start()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }

    [[nodiscard]] QUrl urlFor(const QString& path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path));
    }

    void setRoute(const QString& path, RouteResponse resp)
    {
        m_routes.insert(path, std::move(resp));
    }

    void setSequence(const QString& path, std::vector<RouteResponse> seq)
    {
        m_sequences.insert(path, std::move(seq));
    }

    [[nodiscard]] int maxObservedConcurrent() const
    {
        return m_maxConcurrent;
    }
    [[nodiscard]] int requestCount(const QString& path) const
    {
        return m_requestCounts.value(path, 0);
    }
    [[nodiscard]] QByteArray lastIfNoneMatch(const QString& path) const
    {
        return m_lastIfNoneMatch.value(path);
    }

private:
    void onNewConnection()
    {
        while (QTcpSocket* socket = m_server.nextPendingConnection()) {
            ++m_currentConcurrent;
            m_maxConcurrent = std::max(m_maxConcurrent, m_currentConcurrent);

            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer]() {
                buffer->append(socket->readAll());
                const qsizetype headerEnd = buffer->indexOf("\r\n\r\n");
                if (headerEnd < 0) {
                    return;
                }

                const QList<QByteArray> lines = buffer->left(headerEnd).split('\n');
                const QList<QByteArray> requestParts = lines.value(0).trimmed().split(' ');
                const QString path = QString::fromUtf8(requestParts.value(1));
                m_requestCounts[path] = m_requestCounts.value(path, 0) + 1;

                QByteArray ifNoneMatch;
                QByteArray rangeHeader;
                for (qsizetype i = 1; i < lines.size(); ++i) {
                    const QByteArray line = lines.at(i).trimmed();
                    if (line.toLower().startsWith("if-none-match:")) {
                        ifNoneMatch = line.mid(14).trimmed();
                    } else if (line.toLower().startsWith("range:")) {
                        rangeHeader = line.mid(6).trimmed();
                    }
                }
                m_lastIfNoneMatch.insert(path, ifNoneMatch);

                RouteResponse resp;
                if (m_sequences.contains(path) && !m_sequences[path].empty()) {
                    resp = m_sequences[path].front();
                    m_sequences[path].erase(m_sequences[path].begin());
                } else {
                    resp = m_routes.value(path,
                                          {404, "Not Found", QByteArrayLiteral("missing"), {}, 0});
                }

                auto sendResponse = [socket, resp, ifNoneMatch, rangeHeader]() {
                    QByteArray raw;
                    if (!resp.etag.isEmpty() && ifNoneMatch == resp.etag) {
                        raw.append("HTTP/1.1 304 Not Modified\r\n");
                        raw.append("ETag: " + resp.etag + "\r\n");
                        raw.append("Connection: close\r\n\r\n");
                    } else if (rangeHeader.startsWith("bytes=") && resp.statusCode == 200) {
                        const qsizetype dash = rangeHeader.indexOf('-');
                        const qint64 offset = rangeHeader.mid(6, dash - 6).toLongLong();
                        const QByteArray slice = (offset >= 0 && offset < resp.body.size())
                                                     ? resp.body.mid(offset)
                                                     : QByteArray{};
                        raw.append("HTTP/1.1 206 Partial Content\r\n");
                        raw.append("Content-Length: " + QByteArray::number(slice.size()) + "\r\n");
                        raw.append("Connection: close\r\n\r\n");
                        raw.append(slice);
                    } else {
                        raw.append("HTTP/1.1 " + QByteArray::number(resp.statusCode) + " " +
                                   resp.statusText + "\r\n");
                        if (!resp.etag.isEmpty()) {
                            raw.append("ETag: " + resp.etag + "\r\n");
                        }
                        raw.append("Content-Length: " + QByteArray::number(resp.body.size()) +
                                   "\r\n");
                        raw.append("Connection: close\r\n\r\n");
                        raw.append(resp.body);
                    }
                    socket->write(raw);
                    socket->flush();
                    socket->disconnectFromHost();
                };

                if (resp.delayMs > 0) {
                    QTimer::singleShot(resp.delayMs, socket, sendResponse);
                } else {
                    sendResponse();
                }
            });

            connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
                if (m_currentConcurrent > 0) {
                    --m_currentConcurrent;
                }
                socket->deleteLater();
            });
        }
    }

    QTcpServer m_server;
    QHash<QString, RouteResponse> m_routes;
    QHash<QString, std::vector<RouteResponse>> m_sequences;
    QHash<QString, int> m_requestCounts;
    QHash<QString, QByteArray> m_lastIfNoneMatch;
    int m_currentConcurrent{0};
    int m_maxConcurrent{0};
};

} // namespace

TEST_CASE("CatalogClient builds standardized endpoints and parses JW catalog JSON",
          "[network][catalog]")
{
    const QUrl langUrl = kmedia::CatalogClient::languagesEndpointUrl();
    REQUIRE(langUrl.toString() == QStringLiteral("https://www.jw.org/en/languages/json/"));

    const QUrl mediaUrl = kmedia::CatalogClient::pubMediaLinksUrl(
        QStringLiteral("mwb"), QStringLiteral("E"), 20261100, QStringLiteral("MP4"), 1);
    REQUIRE(mediaUrl.toString().contains(QStringLiteral("pub=mwb")));
    REQUIRE(mediaUrl.toString().contains(QStringLiteral("issue=20261100")));
    REQUIRE(mediaUrl.toString().contains(QStringLiteral("langwritten=E")));

    const QByteArray langsJson = QByteArrayLiteral(
        R"({"languages":[{"langcode":"E","symbol":"en","name":"English","vernacularName":"English","isSignLanguage":false,"isRTL":false},{"langcode":"T","symbol":"pt","name":"Portuguese","vernacularName":"Português","isSignLanguage":false,"direction":"ltr"}]})");
    const auto langs = kmedia::CatalogClient::parseLanguagesJson(langsJson);
    REQUIRE(langs.size() == 2);
    REQUIRE(langs.at(0).code == QStringLiteral("E"));
    REQUIRE(langs.at(1).name == QStringLiteral("Portuguese"));

    const QByteArray pubMediaJson = QByteArrayLiteral(
        R"({"pub":"mwb","issue":"20261100","files":{"E":{"MP4":[{"title":"Treasures Video","label":"720p","track":1,"filesize":12345,"mimetype":"video/mp4","duration":185.5,"file":{"url":"https://cdn.example/v.mp4","checksum":"deadbeef","modifiedDatetime":"2026-11-01"},"trackImage":{"url":"https://cdn.example/v_lsr_sm.jpg","checksum":""}}]}}})");
    const auto assets =
        kmedia::CatalogClient::parsePubMediaLinksJson(pubMediaJson, QStringLiteral("E"));
    REQUIRE(assets.size() == 1);
    REQUIRE(assets.at(0).url == QStringLiteral("https://cdn.example/v.mp4"));
    REQUIRE(assets.at(0).sha256 == QStringLiteral("deadbeef"));
    REQUIRE(assets.at(0).track == 1);
    REQUIRE(assets.at(0).imageUrl == QStringLiteral("https://cdn.example/v_lsr_sm.jpg"));
}

TEST_CASE("DownloadService streams files, validates ETags, checks checksums, retries, and bounds "
          "concurrency",
          "[network][downloads]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    kmedia::DatabaseManager db;
    REQUIRE(db.open(tempDir.filePath(QStringLiteral("profile.db"))));

    LoopbackHttpServer server;
    REQUIRE(server.start());

    const QByteArray payload("KMedia media stream payload 0123456789");
    const QString payloadSha = kmedia::SystemUtils::sha256Hex(payload);
    server.setRoute(QStringLiteral("/file.mp4"),
                    {200, "OK", payload, QByteArrayLiteral("\"etag-v1\""), 0});
    server.setSequence(QStringLiteral("/flaky.mp4"),
                       {{503, "Service Unavailable", QByteArrayLiteral("retry"), {}, 0},
                        {200, "OK", payload, QByteArrayLiteral("\"flaky-1\""), 0}});
    server.setRoute(QStringLiteral("/slow1.mp4"), {200, "OK", QByteArrayLiteral("S1"), {}, 60});
    server.setRoute(QStringLiteral("/slow2.mp4"), {200, "OK", QByteArrayLiteral("S2"), {}, 60});
    server.setRoute(QStringLiteral("/slow3.mp4"), {200, "OK", QByteArrayLiteral("S3"), {}, 60});

    kmedia::NetworkManager network;
    kmedia::DownloadService service(&network, &db);
    service.setMaxConcurrent(2);
    service.setRetryBaseDelayMs(25);

    QSignalSpy finishedSpy(&service, &kmedia::DownloadService::taskFinished);

    // 1. Fresh download + ETag save + SHA-256 verification.
    const QString destPath = tempDir.filePath(QStringLiteral("media/file.mp4"));
    const QString t1 =
        service.enqueue(server.urlFor(QStringLiteral("/file.mp4")), destPath, payloadSha);
    REQUIRE(finishedSpy.wait(3000));
    const auto info1 = service.taskInfo(t1);
    REQUIRE(info1.has_value());
    REQUIRE(info1->status == kmedia::DownloadService::Status::Completed);
    REQUIRE(info1->actualChecksum == payloadSha);
    REQUIRE(QFileInfo(destPath).size() == payload.size());

    // 2. Conditional ETag request hits 304 Not Modified cleanly.
    const QString t2 =
        service.enqueue(server.urlFor(QStringLiteral("/file.mp4")), destPath, payloadSha);
    REQUIRE(finishedSpy.wait(3000));
    const auto info2 = service.taskInfo(t2);
    REQUIRE(info2.has_value());
    REQUIRE(info2->status == kmedia::DownloadService::Status::CachedNotModified);
    REQUIRE(server.lastIfNoneMatch(QStringLiteral("/file.mp4")) ==
            QByteArrayLiteral("\"etag-v1\""));

    // 3. SHA-256 mismatch rejects file and cleans temporary artifact.
    const QString badDest = tempDir.filePath(QStringLiteral("media/corrupt.mp4"));
    const QString tBad = service.enqueue(
        server.urlFor(QStringLiteral("/file.mp4")), badDest,
        QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000"));
    REQUIRE(finishedSpy.wait(3000));
    const auto infoBad = service.taskInfo(tBad);
    REQUIRE(infoBad.has_value());
    REQUIRE(infoBad->status == kmedia::DownloadService::Status::Failed);
    REQUIRE_FALSE(QFileInfo::exists(badDest));
    REQUIRE_FALSE(QFileInfo::exists(badDest + QStringLiteral(".kmedia.part")));

    // 3b. JW.org publishes MD5 checksums: a matching one verifies, a wrong one fails.
    const QString payloadMd5 =
        QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Md5).toHex());
    const QString md5Dest = tempDir.filePath(QStringLiteral("media/md5.mp4"));
    const QString tMd5 =
        service.enqueue(server.urlFor(QStringLiteral("/file.mp4")), md5Dest, payloadMd5);
    REQUIRE(finishedSpy.wait(3000));
    REQUIRE(service.taskInfo(tMd5)->status == kmedia::DownloadService::Status::Completed);
    REQUIRE(service.taskInfo(tMd5)->actualChecksum == payloadMd5);
    REQUIRE(QFileInfo(md5Dest).size() == payload.size());

    const QString badMd5Dest = tempDir.filePath(QStringLiteral("media/bad-md5.mp4"));
    const QString tBadMd5 = service.enqueue(server.urlFor(QStringLiteral("/file.mp4")), badMd5Dest,
                                            QStringLiteral("00000000000000000000000000000000"));
    REQUIRE(finishedSpy.wait(3000));
    REQUIRE(service.taskInfo(tBadMd5)->status == kmedia::DownloadService::Status::Failed);
    REQUIRE_FALSE(QFileInfo::exists(badMd5Dest));

    // 4. Retry backoff recovers cleanly from transient 503.
    const QString flakyDest = tempDir.filePath(QStringLiteral("media/flaky.mp4"));
    const QString tFlaky =
        service.enqueue(server.urlFor(QStringLiteral("/flaky.mp4")), flakyDest, payloadSha);
    REQUIRE(finishedSpy.wait(4000));
    REQUIRE(service.taskInfo(tFlaky)->status == kmedia::DownloadService::Status::Completed);
    REQUIRE(server.requestCount(QStringLiteral("/flaky.mp4")) == 2);

    // 5. Bounded concurrency never exceeds maxConcurrent=2 while draining 3 slow transfers.
    const qsizetype beforeSlow = finishedSpy.count();
    service.enqueue(server.urlFor(QStringLiteral("/slow1.mp4")),
                    tempDir.filePath(QStringLiteral("s1.mp4")));
    service.enqueue(server.urlFor(QStringLiteral("/slow2.mp4")),
                    tempDir.filePath(QStringLiteral("s2.mp4")));
    service.enqueue(server.urlFor(QStringLiteral("/slow3.mp4")),
                    tempDir.filePath(QStringLiteral("s3.mp4")));
    QElapsedTimer waitClock;
    waitClock.start();
    while (finishedSpy.count() < beforeSlow + 3 && waitClock.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    REQUIRE(finishedSpy.count() == beforeSlow + 3);
    REQUIRE(server.maxObservedConcurrent() <= 2);

    // 6. Queued task pause, resume, and cancellation controls.
    service.setMaxConcurrent(1);
    const QString qA = service.enqueue(server.urlFor(QStringLiteral("/slow1.mp4")),
                                       tempDir.filePath(QStringLiteral("qa.mp4")));
    const QString qB = service.enqueue(server.urlFor(QStringLiteral("/slow2.mp4")),
                                       tempDir.filePath(QStringLiteral("qb.mp4")));
    Q_UNUSED(qA)
    REQUIRE(service.pause(qB));
    REQUIRE(service.taskInfo(qB)->status == kmedia::DownloadService::Status::Paused);
    REQUIRE(service.cancel(qB));
    REQUIRE(service.taskInfo(qB)->status == kmedia::DownloadService::Status::Cancelled);

    QElapsedTimer drainClock;
    drainClock.start();
    while (service.activeCount() > 0 && drainClock.elapsed() < 3000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}
