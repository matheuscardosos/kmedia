#include "core/MeetingTimer.h"
#include "network/TimerRemoteServer.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTest>

#include <catch2/catch_test_macros.hpp>

namespace {

// Sends a raw request and reads until the server closes, or until `until` shows up.
QByteArray exchange(quint16 port, const QByteArray& request, QTcpSocket* keep = nullptr,
                    const QByteArray& until = {})
{
    QTcpSocket local;
    QTcpSocket* socket = keep != nullptr ? keep : &local;
    socket->connectToHost(QStringLiteral("127.0.0.1"), port);
    if (!socket->waitForConnected(3000)) {
        return {};
    }
    socket->write(request);
    QByteArray response;
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < 5000) {
        QTest::qWait(20);
        response += socket->readAll();
        if (!until.isEmpty() && response.contains(until)) {
            break;
        }
        if (until.isEmpty() && socket->state() == QAbstractSocket::UnconnectedState) {
            response += socket->readAll();
            break;
        }
    }
    return response;
}

QByteArray get(quint16 port, const QByteArray& path)
{
    return exchange(port, "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
}

} // namespace

TEST_CASE("The timer remote serves its page only under the access code", "[network][timer]")
{
    kmedia::MeetingTimer timer;
    kmedia::TimerRemoteServer server(&timer);
    server.setPort(0);
    server.setAccessCode(QStringLiteral("abc23456"));
    server.setEnabled(true);
    REQUIRE(server.isRunning());
    const quint16 port = server.listeningPort();
    REQUIRE(port > 0);

    REQUIRE(get(port, "/").startsWith("HTTP/1.1 404"));
    REQUIRE(get(port, "/abc23457/").startsWith("HTTP/1.1 404"));
    REQUIRE(get(port, "/abc23456/other").startsWith("HTTP/1.1 404"));

    const QByteArray redirect = get(port, "/abc23456");
    REQUIRE(redirect.startsWith("HTTP/1.1 301"));
    REQUIRE(redirect.contains("Location: /abc23456/"));

    const QByteArray page = get(port, "/abc23456/?from=qr");
    REQUIRE(page.startsWith("HTTP/1.1 200"));
    REQUIRE(page.contains("Content-Type: text/html"));
    REQUIRE(page.contains("Content-Security-Policy: default-src 'none'"));
    REQUIRE(page.contains("new EventSource('events')"));

    // Nothing but reading is accepted.
    REQUIRE(exchange(port, "POST /abc23456/ HTTP/1.1\r\nContent-Length: 0\r\n\r\n")
                .startsWith("HTTP/1.1 405"));
    REQUIRE(exchange(port, "garbage\r\n\r\n").startsWith("HTTP/1.1 400"));

    // A new code shuts out the old link.
    server.renewAccessCode();
    REQUIRE(server.accessCode() != QStringLiteral("abc23456"));
    REQUIRE(server.accessCode().size() == 8);
    const quint16 renewedPort = server.listeningPort();
    REQUIRE(get(renewedPort, "/abc23456/").startsWith("HTTP/1.1 404"));
    REQUIRE(get(renewedPort, "/" + server.accessCode().toUtf8() + "/").startsWith("HTTP/1.1 200"));

    server.setEnabled(false);
    REQUIRE_FALSE(server.isRunning());
    REQUIRE(server.urls().isEmpty());
}

TEST_CASE("The timer remote streams the timer to the page", "[network][timer]")
{
    kmedia::MeetingTimer timer;
    timer.setMeeting(QStringLiteral("2026-11-29"), QStringLiteral("weekend"), false,
                     QDateTime(QDate(2026, 11, 29), QTime(9, 30)));
    kmedia::TimerRemoteServer server(&timer);
    server.setPort(0);
    server.setAccessCode(QStringLiteral("stream22"));
    server.setPageTexts({{QStringLiteral("remaining"), QStringLiteral("Remaining")}});
    server.setEnabled(true);
    const quint16 port = server.listeningPort();

    QTcpSocket events;
    const QByteArray opened =
        exchange(port, "GET /stream22/events HTTP/1.1\r\nHost: x\r\n\r\n", &events, "\n\n{");
    QByteArray stream = opened;
    QElapsedTimer clock;
    clock.start();
    while (!stream.contains("}\n\n") && clock.elapsed() < 3000) {
        QTest::qWait(20);
        stream += events.readAll();
    }
    REQUIRE(stream.startsWith("HTTP/1.1 200"));
    REQUIRE(stream.contains("Content-Type: text/event-stream"));
    REQUIRE(server.clientCount() == 1);
    const qsizetype dataAt = stream.indexOf("data: ");
    REQUIRE(dataAt > 0);
    const QJsonObject first =
        QJsonDocument::fromJson(stream.mid(dataAt + 6, stream.indexOf("\n\n", dataAt) - dataAt - 6))
            .object();
    REQUIRE_FALSE(first.value(QStringLiteral("running")).toBool());
    REQUIRE(first.value(QStringLiteral("texts")).toObject().value(QStringLiteral("remaining")) ==
            QStringLiteral("Remaining"));
    REQUIRE(first.value(QStringLiteral("meetingStart")).toDouble() > 0);

    // Starting a part goes out at once, with its title from the interface.
    server.setPartTitle(QStringLiteral("Public talk"));
    timer.startPart(1);
    QByteArray next;
    clock.restart();
    while (!next.contains("Public talk") && clock.elapsed() < 3000) {
        QTest::qWait(20);
        next += events.readAll();
    }
    REQUIRE(next.contains("\"running\":true"));
    REQUIRE(next.contains("\"targetMs\":1800000"));

    events.abort();
    clock.restart();
    while (server.clientCount() > 0 && clock.elapsed() < 3000) {
        QTest::qWait(20);
    }
    REQUIRE(server.clientCount() == 0);
}

TEST_CASE("The timer remote reports a port in use and makes QR codes", "[network][timer]")
{
    kmedia::TimerRemoteServer first(nullptr);
    first.setPort(0);
    first.setEnabled(true);
    REQUIRE(first.isRunning());

    kmedia::TimerRemoteServer second(nullptr);
    second.setPort(first.listeningPort());
    second.setEnabled(true);
    REQUIRE_FALSE(second.isRunning());
    REQUIRE(second.problem() == kmedia::TimerRemoteServer::Problem::PortInUse);

    const QVariantMap qr =
        kmedia::TimerRemoteServer::qrCode(QStringLiteral("http://192.168.0.10:8787/abc23456/"));
    const int size = qr.value(QStringLiteral("size")).toInt();
    REQUIRE(size >= 21);
    REQUIRE(qr.value(QStringLiteral("modules")).toString().size() == static_cast<qsizetype>(size) * size);
    REQUIRE(kmedia::TimerRemoteServer::newAccessCode() !=
            kmedia::TimerRemoteServer::newAccessCode());
}
