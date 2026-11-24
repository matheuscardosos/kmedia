#include "network/TimerRemoteServer.h"

#include "core/MeetingTimer.h"

#include <QDateTime>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <qrcodegen.hpp>

namespace kmedia {

namespace {
constexpr int kKeepAliveMs = 20000;
constexpr int kRequestTimeoutMs = 10000;
constexpr qsizetype kMaxRequestBytes = 8192;
constexpr int kMinPort = 1024;
constexpr int kMaxPort = 65535;
// Open connections of any kind, and what a page that stops reading may leave queued.
constexpr int kMaxOpenSockets = 48;
constexpr qint64 kMaxQueuedBytes = 256 * 1024;
const char* const kRequestProperty = "kmediaRequest";
const char* const kEventsProperty = "kmediaEvents";

// The page only runs its own inline script and talks back to this server.
constexpr auto kPagePolicy =
    "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
    "connect-src 'self'; media-src blob:; img-src data:; base-uri 'none'; form-action 'none'; "
    "frame-ancestors 'none'";

bool sameCode(const QString& given, const QString& expected)
{
    if (given.size() != expected.size() || expected.isEmpty()) {
        return false;
    }
    // Compared in full, so the time taken does not hint at how much of a guess was right.
    ushort difference = 0;
    for (qsizetype i = 0; i < expected.size(); ++i) {
        difference |= static_cast<ushort>(given.at(i).unicode() ^ expected.at(i).unicode());
    }
    return difference == 0;
}
} // namespace

TimerRemoteServer::TimerRemoteServer(MeetingTimer* timer, QObject* parent)
    : QObject(parent), m_timer(timer), m_accessCode(newAccessCode())
{
    m_keepAlive.setInterval(kKeepAliveMs);
    connect(&m_keepAlive, &QTimer::timeout, this, &TimerRemoteServer::sendKeepAlive);
    // Bursts of timer changes go out as one event.
    m_broadcastSoon.setSingleShot(true);
    m_broadcastSoon.setInterval(30);
    connect(&m_broadcastSoon, &QTimer::timeout, this, &TimerRemoteServer::broadcast);
    if (timer != nullptr) {
        for (const auto signal : {&MeetingTimer::partsChanged, &MeetingTimer::runningChanged,
                                  &MeetingTimer::optionsChanged}) {
            connect(timer, signal, &m_broadcastSoon, qOverload<>(&QTimer::start));
        }
    }
    connect(this, &TimerRemoteServer::pageChanged, &m_broadcastSoon, qOverload<>(&QTimer::start));
}

TimerRemoteServer::~TimerRemoteServer()
{
    stopServer();
}

bool TimerRemoteServer::isEnabled() const
{
    return m_enabled;
}

void TimerRemoteServer::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    restart();
}

int TimerRemoteServer::port() const
{
    return m_port;
}

void TimerRemoteServer::setPort(int port)
{
    const int valid = port == 0 ? 0 : qBound(kMinPort, port, kMaxPort);
    if (m_port == valid) {
        return;
    }
    m_port = valid;
    if (m_enabled) {
        restart();
    } else {
        emit stateChanged();
    }
}

bool TimerRemoteServer::isFocusLayout() const
{
    return m_focusLayout;
}

void TimerRemoteServer::setFocusLayout(bool focus)
{
    if (m_focusLayout == focus) {
        return;
    }
    m_focusLayout = focus;
    emit stateChanged();
    m_broadcastSoon.start();
}

QString TimerRemoteServer::accessCode() const
{
    return m_accessCode;
}

void TimerRemoteServer::setAccessCode(const QString& code)
{
    const QString valid = code.trimmed().isEmpty() ? newAccessCode() : code.trimmed();
    if (m_accessCode == valid) {
        return;
    }
    m_accessCode = valid;
    // Pages opened with the old code lose their stream.
    if (m_enabled) {
        restart();
    } else {
        emit stateChanged();
    }
}

void TimerRemoteServer::renewAccessCode()
{
    setAccessCode(newAccessCode());
}

bool TimerRemoteServer::isRunning() const
{
    return m_server != nullptr && m_server->isListening();
}

TimerRemoteServer::Problem TimerRemoteServer::problem() const
{
    return m_problem;
}

QStringList TimerRemoteServer::urls() const
{
    return m_urls;
}

int TimerRemoteServer::clientCount() const
{
    return static_cast<int>(m_clients.size());
}

quint16 TimerRemoteServer::listeningPort() const
{
    return isRunning() ? m_server->serverPort() : 0;
}

QString TimerRemoteServer::partTitle() const
{
    return m_partTitle;
}

void TimerRemoteServer::setPartTitle(const QString& title)
{
    if (m_partTitle == title) {
        return;
    }
    m_partTitle = title;
    emit pageChanged();
}

QVariantMap TimerRemoteServer::pageTexts() const
{
    return m_pageTexts;
}

void TimerRemoteServer::setPageTexts(const QVariantMap& texts)
{
    if (m_pageTexts == texts) {
        return;
    }
    m_pageTexts = texts;
    emit pageChanged();
}

QString TimerRemoteServer::newAccessCode()
{
    // No letters or digits that read alike.
    static const QString alphabet = QStringLiteral("abcdefghjkmnpqrstuvwxyz23456789");
    QString code;
    for (int i = 0; i < 8; ++i) {
        code.append(alphabet.at(
            static_cast<qsizetype>(QRandomGenerator::system()->bounded(alphabet.size()))));
    }
    return code;
}

QVariantMap TimerRemoteServer::qrCode(const QString& text)
{
    try {
        const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.toUtf8().constData(),
                                                                   qrcodegen::QrCode::Ecc::MEDIUM);
        const int size = qr.getSize();
        QString modules;
        modules.reserve(size * size);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                modules.append(qr.getModule(x, y) ? QLatin1Char('1') : QLatin1Char('0'));
            }
        }
        return {{QStringLiteral("size"), size}, {QStringLiteral("modules"), modules}};
    } catch (const qrcodegen::data_too_long&) {
        return {};
    }
}

QStringList TimerRemoteServer::localAddresses()
{
    QStringList addresses;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& interface : interfaces) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLinkLocal()) {
                addresses.append(address.toString());
            }
        }
    }
    return addresses;
}

void TimerRemoteServer::stopServer()
{
    m_keepAlive.stop();
    for (const QPointer<QTcpSocket>& client : std::as_const(m_clients)) {
        if (!client.isNull()) {
            client->disconnect(this);
            client->abort();
            client->deleteLater();
        }
    }
    const bool hadClients = !m_clients.isEmpty();
    m_clients.clear();
    if (m_server != nullptr) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    if (hadClients) {
        emit clientCountChanged();
    }
}

void TimerRemoteServer::restart()
{
    stopServer();
    m_urls.clear();
    m_problem = Problem::None;
    if (m_enabled) {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &TimerRemoteServer::handleConnection);
        if (!m_server->listen(QHostAddress::Any, static_cast<quint16>(m_port))) {
            m_problem = m_server->serverError() == QAbstractSocket::AddressInUseError
                            ? Problem::PortInUse
                            : Problem::Failed;
            m_server->deleteLater();
            m_server = nullptr;
        } else {
            const QStringList addresses = localAddresses();
            for (const QString& address : addresses) {
                m_urls.append(QStringLiteral("http://%1:%2/%3/")
                                  .arg(address)
                                  .arg(m_server->serverPort())
                                  .arg(m_accessCode));
            }
            // The page still works on this computer, but no tablet can reach it.
            if (addresses.isEmpty()) {
                m_problem = Problem::NoNetwork;
            }
            m_keepAlive.start();
        }
    }
    emit stateChanged();
}

void TimerRemoteServer::handleConnection()
{
    while (m_server != nullptr && m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }
        // Too many open connections, slow requests included, and new ones are turned away.
        if (m_openSockets >= kMaxOpenSockets) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        ++m_openSockets;
        connect(socket, &QObject::destroyed, this, [this]() { --m_openSockets; });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            if (socket->property(kEventsProperty).toBool()) {
                socket->readAll();
                return;
            }
            QByteArray request = socket->property(kRequestProperty).toByteArray();
            request.append(socket->readAll());
            if (request.size() > kMaxRequestBytes) {
                socket->abort();
                return;
            }
            socket->setProperty(kRequestProperty, request);
            if (request.contains("\r\n\r\n")) {
                handleRequest(socket);
            }
        });
        // A request that never completes is dropped.
        QTimer::singleShot(kRequestTimeoutMs, socket, [socket]() {
            if (!socket->property(kEventsProperty).toBool() &&
                socket->state() == QAbstractSocket::ConnectedState &&
                !socket->property(kRequestProperty).toByteArray().contains("\r\n\r\n")) {
                socket->abort();
            }
        });
    }
}

void TimerRemoteServer::handleRequest(QTcpSocket* socket)
{
    const QByteArray request = socket->property(kRequestProperty).toByteArray();
    const QList<QByteArray> requestLine = request.left(request.indexOf("\r\n")).split(' ');
    if (requestLine.size() != 3 || !requestLine.at(2).startsWith("HTTP/1.")) {
        respond(socket, 400, "Bad Request", "text/plain; charset=utf-8", "Bad request\n");
        return;
    }
    const QByteArray method = requestLine.at(0);
    if (method != "GET" && method != "HEAD") {
        respond(socket, 405, "Method Not Allowed", "text/plain; charset=utf-8",
                "Method not allowed\n", {{"Allow", "GET, HEAD"}});
        return;
    }
    QByteArray target = requestLine.at(1);
    const qsizetype query = target.indexOf('?');
    if (query >= 0) {
        target.truncate(query);
    }
    const QStringList segments =
        QString::fromUtf8(target).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.isEmpty() || !sameCode(segments.first(), m_accessCode)) {
        respond(socket, 404, "Not Found", "text/plain; charset=utf-8", "Not found\n");
        return;
    }
    if (segments.size() == 1 && !target.endsWith('/')) {
        respond(socket, 301, "Moved Permanently", "text/plain; charset=utf-8", "",
                {{"Location", "/" + m_accessCode.toUtf8() + "/"}});
        return;
    }
    if (segments.size() == 1) {
        QFile page(QStringLiteral(":/kmedia/web/timer-remote.html"));
        if (!page.open(QIODevice::ReadOnly)) {
            respond(socket, 500, "Internal Server Error", "text/plain; charset=utf-8", "Error\n");
            return;
        }
        respond(socket, 200, "OK", "text/html; charset=utf-8",
                method == "HEAD" ? QByteArray{} : page.readAll(),
                {{"Content-Security-Policy", kPagePolicy}});
        return;
    }
    if (segments.size() == 2 && segments.at(1) == QLatin1String("events") && method == "GET") {
        openEvents(socket);
        return;
    }
    respond(socket, 404, "Not Found", "text/plain; charset=utf-8", "Not found\n");
}

void TimerRemoteServer::respond(QTcpSocket* socket, int status, const QByteArray& reason,
                                const QByteArray& contentType, const QByteArray& body,
                                const QList<QPair<QByteArray, QByteArray>>& extraHeaders)
{
    QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Cache-Control: no-store\r\n";
    response += "X-Content-Type-Options: nosniff\r\n";
    response += "Referrer-Policy: no-referrer\r\n";
    response += "Connection: close\r\n";
    for (const auto& header : extraHeaders) {
        response += header.first + ": " + header.second + "\r\n";
    }
    response += "\r\n";
    response += body;
    socket->write(response);
    socket->disconnectFromHost();
}

void TimerRemoteServer::openEvents(QTcpSocket* socket)
{
    if (m_clients.size() >= kMaxClients) {
        respond(socket, 503, "Service Unavailable", "text/plain; charset=utf-8", "Busy\n",
                {{"Retry-After", "10"}});
        return;
    }
    socket->setProperty(kEventsProperty, true);
    socket->setProperty(kRequestProperty, QByteArray{});
    socket->write("HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/event-stream; charset=utf-8\r\n"
                  "Cache-Control: no-store\r\n"
                  "X-Content-Type-Options: nosniff\r\n"
                  "Connection: keep-alive\r\n"
                  "\r\n"
                  "retry: 3000\n\n");
    socket->write("data: " + stateJson() + "\n\n");
    m_clients.append(socket);
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        m_clients.removeAll(socket);
        emit clientCountChanged();
    });
    emit clientCountChanged();
}

QByteArray TimerRemoteServer::stateJson() const
{
    QJsonObject state;
    state.insert(QStringLiteral("serverTime"), QDateTime::currentMSecsSinceEpoch());
    state.insert(QStringLiteral("focus"), m_focusLayout);
    state.insert(QStringLiteral("partTitle"), m_partTitle);
    state.insert(QStringLiteral("texts"), QJsonObject::fromVariantMap(m_pageTexts));
    if (!m_timer.isNull()) {
        state.insert(QStringLiteral("running"), m_timer->isRunning());
        state.insert(QStringLiteral("paused"), m_timer->isPaused());
        state.insert(QStringLiteral("elapsedMs"), m_timer->elapsedMs());
        state.insert(QStringLiteral("targetMs"), m_timer->targetMs());
        state.insert(QStringLiteral("countDown"), m_timer->isCountDown());
        state.insert(QStringLiteral("overtimeAmountOnly"), m_timer->isOvertimeAmountOnly());
        const QDateTime start = m_timer->meetingStart();
        state.insert(QStringLiteral("meetingStart"),
                     start.isValid() ? start.toMSecsSinceEpoch() : qint64{0});
    }
    return QJsonDocument(state).toJson(QJsonDocument::Compact);
}

void TimerRemoteServer::broadcast()
{
    if (m_clients.isEmpty()) {
        return;
    }
    const QByteArray event = "data: " + stateJson() + "\n\n";
    for (const QPointer<QTcpSocket>& client : std::as_const(m_clients)) {
        writeEvent(client, event);
    }
}

void TimerRemoteServer::sendKeepAlive()
{
    for (const QPointer<QTcpSocket>& client : std::as_const(m_clients)) {
        writeEvent(client, ": keep-alive\n\n");
    }
}

void TimerRemoteServer::writeEvent(QTcpSocket* client, const QByteArray& event)
{
    if (client == nullptr) {
        return;
    }
    // A page that stopped reading is dropped instead of queueing without end.
    if (client->bytesToWrite() > kMaxQueuedBytes) {
        client->abort();
        return;
    }
    client->write(event);
}

} // namespace kmedia
