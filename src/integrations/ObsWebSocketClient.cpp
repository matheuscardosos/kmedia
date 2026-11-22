#include "integrations/ObsWebSocketClient.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

#include <algorithm>

namespace kmedia {

namespace {

constexpr int kOpHello = 0;
constexpr int kOpIdentify = 1;
constexpr int kOpIdentified = 2;
constexpr int kOpEvent = 5;
constexpr int kOpRequest = 6;
constexpr int kOpRequestResponse = 7;

constexpr int kRpcVersion = 1;
constexpr int kObsCodeOutputRunning = 500;
constexpr int kObsCodeOutputNotRunning = 501;

} // namespace

ObsWebSocketClient::ObsWebSocketClient(QObject* parent) : QObject(parent)
{
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &ObsWebSocketClient::connectToObs);

    connect(&m_socket, &QWebSocket::connected, this, &ObsWebSocketClient::onSocketConnected);
    connect(&m_socket, &QWebSocket::disconnected, this, &ObsWebSocketClient::onSocketDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived, this,
            &ObsWebSocketClient::onSocketTextMessage);
    connect(&m_socket, &QWebSocket::errorOccurred, this, &ObsWebSocketClient::onSocketError);
}

ObsWebSocketClient::~ObsWebSocketClient()
{
    m_manualDisconnect = true;
    m_reconnectTimer.stop();
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.abort();
    }
}

ObsWebSocketClient::ConnectionState ObsWebSocketClient::connectionState() const
{
    return m_connectionState;
}

bool ObsWebSocketClient::isConnected() const
{
    return m_connectionState == ConnectionState::Connected;
}

bool ObsWebSocketClient::isEnabled() const
{
    return m_enabled;
}

void ObsWebSocketClient::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    emit configChanged();
    if (!m_enabled) {
        disconnectFromObs();
    } else if (m_processRunning) {
        connectToObs();
    }
}

QString ObsWebSocketClient::host() const
{
    return m_host;
}

void ObsWebSocketClient::setHost(const QString& host)
{
    const QString cleaned = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    if (m_host == cleaned) {
        return;
    }
    m_host = cleaned;
    emit configChanged();
}

int ObsWebSocketClient::port() const
{
    return m_port;
}

void ObsWebSocketClient::setPort(int port)
{
    const int clamped = qBound(1, port, 65535);
    if (m_port == clamped) {
        return;
    }
    m_port = clamped;
    emit configChanged();
}

QString ObsWebSocketClient::password() const
{
    return m_password;
}

void ObsWebSocketClient::setPassword(const QString& password)
{
    if (m_password == password) {
        return;
    }
    m_password = password;
    emit configChanged();
}

QString ObsWebSocketClient::stageScene() const
{
    return m_stageScene;
}

void ObsWebSocketClient::setStageScene(const QString& sceneName)
{
    if (m_stageScene == sceneName.trimmed()) {
        return;
    }
    m_stageScene = sceneName.trimmed();
    emit sceneMapChanged();
}

QString ObsWebSocketClient::mediaScene() const
{
    return m_mediaScene;
}

void ObsWebSocketClient::setMediaScene(const QString& sceneName)
{
    if (m_mediaScene == sceneName.trimmed()) {
        return;
    }
    m_mediaScene = sceneName.trimmed();
    emit sceneMapChanged();
}

QString ObsWebSocketClient::imageScene() const
{
    return m_imageScene;
}

void ObsWebSocketClient::setImageScene(const QString& sceneName)
{
    if (m_imageScene == sceneName.trimmed()) {
        return;
    }
    m_imageScene = sceneName.trimmed();
    emit sceneMapChanged();
}

QString ObsWebSocketClient::sceneForPlayback(bool hasMedia, bool stillImage, bool standby) const
{
    if (standby || !hasMedia) {
        return m_stageScene;
    }
    if (stillImage && !m_imageScene.isEmpty()) {
        return m_imageScene;
    }
    return m_mediaScene;
}

QStringList ObsWebSocketClient::availableScenes() const
{
    return m_availableScenes;
}

QString ObsWebSocketClient::currentProgramScene() const
{
    return m_currentProgramScene;
}

bool ObsWebSocketClient::isRecordingActive() const
{
    return m_recordingActive;
}

QString ObsWebSocketClient::lastError() const
{
    return m_lastError;
}

void ObsWebSocketClient::setProcessRunning(bool running)
{
    m_processRunning = running;
    if (m_enabled && m_processRunning && m_connectionState == ConnectionState::Disconnected) {
        connectToObs();
    } else if (!m_processRunning && !isConnected()) {
        m_reconnectTimer.stop();
    }
}

void ObsWebSocketClient::connectToObs()
{
    if (m_connectionState == ConnectionState::Connecting ||
        m_connectionState == ConnectionState::Authenticating ||
        m_connectionState == ConnectionState::Connected) {
        return;
    }

    m_manualDisconnect = false;
    m_reconnectTimer.stop();
    m_pendingRequests.clear();
    setLastError({});
    setConnectionState(ConnectionState::Connecting);

    QUrl wsUrl;
    wsUrl.setScheme(QStringLiteral("ws"));
    wsUrl.setHost(m_host);
    wsUrl.setPort(m_port);
    m_socket.open(wsUrl);
}

void ObsWebSocketClient::disconnectFromObs()
{
    m_manualDisconnect = true;
    m_reconnectTimer.stop();
    m_pendingRequests.clear();
    m_reconnectAttempts = 0;

    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.close();
    } else {
        setConnectionState(ConnectionState::Disconnected);
    }

    if (!m_availableScenes.isEmpty()) {
        m_availableScenes.clear();
        emit availableScenesChanged();
    }
    if (m_recordingActive) {
        m_recordingActive = false;
        emit recordingStateChanged();
    }
}

void ObsWebSocketClient::requestSceneList()
{
    if (isConnected()) {
        (void)sendRequest(QStringLiteral("GetSceneList"));
    }
}

void ObsWebSocketClient::requestRecordStatus()
{
    if (isConnected()) {
        (void)sendRequest(QStringLiteral("GetRecordStatus"));
    }
}

bool ObsWebSocketClient::setCurrentProgramScene(const QString& sceneName)
{
    const QString target = sceneName.trimmed();
    if (!isConnected() || target.isEmpty()) {
        return false;
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("sceneName"), target);
    return !sendRequest(QStringLiteral("SetCurrentProgramScene"), payload).isEmpty();
}

bool ObsWebSocketClient::startRecording()
{
    if (!isConnected()) {
        return false;
    }
    return !sendRequest(QStringLiteral("StartRecord")).isEmpty();
}

bool ObsWebSocketClient::stopRecording()
{
    if (!isConnected()) {
        return false;
    }
    return !sendRequest(QStringLiteral("StopRecord")).isEmpty();
}

void ObsWebSocketClient::applyPlaybackState(bool hasMedia, bool stillImage, bool standby)
{
    if (!isConnected()) {
        return;
    }

    const QString targetScene = sceneForPlayback(hasMedia, stillImage, standby);
    if (targetScene.isEmpty()) {
        return;
    }
    if (!m_availableScenes.isEmpty() && !m_availableScenes.contains(targetScene)) {
        return;
    }
    (void)setCurrentProgramScene(targetScene);
}

QString ObsWebSocketClient::computeAuthResponse(const QString& password, const QString& salt,
                                                const QString& challenge)
{
    const QByteArray secretDigest =
        QCryptographicHash::hash((password + salt).toUtf8(), QCryptographicHash::Sha256);
    const QByteArray secretBase64 = secretDigest.toBase64();
    const QByteArray authDigest =
        QCryptographicHash::hash(secretBase64 + challenge.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(authDigest.toBase64());
}

void ObsWebSocketClient::onSocketConnected()
{
    setConnectionState(ConnectionState::Authenticating);
}

void ObsWebSocketClient::onSocketDisconnected()
{
    m_pendingRequests.clear();
    if (m_manualDisconnect || !m_enabled) {
        setConnectionState(ConnectionState::Disconnected);
        return;
    }

    setConnectionState(ConnectionState::Disconnected);
    scheduleReconnect();
}

void ObsWebSocketClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    if (m_manualDisconnect) {
        return;
    }
    setLastError(m_socket.errorString());
    setConnectionState(ConnectionState::Error);
    if (m_socket.state() == QAbstractSocket::UnconnectedState) {
        scheduleReconnect();
    }
}

void ObsWebSocketClient::onSocketTextMessage(const QString& message)
{
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        return;
    }

    const QJsonObject packet = doc.object();
    const int op = packet.value(QStringLiteral("op")).toInt(-1);
    const QJsonObject data = packet.value(QStringLiteral("d")).toObject();

    switch (op) {
    case kOpHello:
        handleHello(data);
        break;
    case kOpIdentified:
        handleIdentified();
        break;
    case kOpEvent:
        handleEvent(data);
        break;
    case kOpRequestResponse:
        handleRequestResponse(data);
        break;
    default:
        break;
    }
}

void ObsWebSocketClient::handleHello(const QJsonObject& data)
{
    QJsonObject identify;
    identify.insert(QStringLiteral("rpcVersion"), kRpcVersion);

    if (data.contains(QStringLiteral("authentication"))) {
        const QJsonObject authObj = data.value(QStringLiteral("authentication")).toObject();
        const QString salt = authObj.value(QStringLiteral("salt")).toString();
        const QString challenge = authObj.value(QStringLiteral("challenge")).toString();
        if (!salt.isEmpty() && !challenge.isEmpty()) {
            identify.insert(QStringLiteral("authentication"),
                            computeAuthResponse(m_password, salt, challenge));
        }
    }

    sendJsonPacket(kOpIdentify, identify);
}

void ObsWebSocketClient::handleIdentified()
{
    m_reconnectAttempts = 0;
    setLastError({});
    setConnectionState(ConnectionState::Connected);
    requestSceneList();
    requestRecordStatus();
}

void ObsWebSocketClient::handleEvent(const QJsonObject& data)
{
    const QString eventType = data.value(QStringLiteral("eventType")).toString();
    const QJsonObject eventData = data.value(QStringLiteral("eventData")).toObject();

    if (eventType == QStringLiteral("CurrentProgramSceneChanged")) {
        const QString nextScene = eventData.value(QStringLiteral("sceneName")).toString();
        if (m_currentProgramScene != nextScene) {
            m_currentProgramScene = nextScene;
            emit currentProgramSceneChanged();
        }
    } else if (eventType == QStringLiteral("RecordStateChanged")) {
        const bool active = eventData.value(QStringLiteral("outputActive")).toBool(false);
        if (m_recordingActive != active) {
            m_recordingActive = active;
            emit recordingStateChanged();
        }
    }
}

void ObsWebSocketClient::handleRequestResponse(const QJsonObject& data)
{
    const QString requestType = data.value(QStringLiteral("requestType")).toString();
    const QString requestId = data.value(QStringLiteral("requestId")).toString();
    m_pendingRequests.remove(requestId);

    const QJsonObject status = data.value(QStringLiteral("requestStatus")).toObject();
    const bool ok = status.value(QStringLiteral("result")).toBool(false);
    const int code = status.value(QStringLiteral("code")).toInt(0);
    const QJsonObject responseData = data.value(QStringLiteral("responseData")).toObject();

    if (ok) {
        if (requestType == QStringLiteral("GetSceneList")) {
            QStringList scenes;
            const QJsonArray scenesArr = responseData.value(QStringLiteral("scenes")).toArray();
            for (const QJsonValue& val : scenesArr) {
                const QString name = val.toObject().value(QStringLiteral("sceneName")).toString();
                if (!name.isEmpty()) {
                    scenes.append(name);
                }
            }
            if (m_availableScenes != scenes) {
                m_availableScenes = scenes;
                emit availableScenesChanged();
            }
            const QString current =
                responseData.value(QStringLiteral("currentProgramSceneName")).toString();
            if (!current.isEmpty() && m_currentProgramScene != current) {
                m_currentProgramScene = current;
                emit currentProgramSceneChanged();
            }
        } else if (requestType == QStringLiteral("GetRecordStatus")) {
            const bool active = responseData.value(QStringLiteral("outputActive")).toBool(false);
            if (m_recordingActive != active) {
                m_recordingActive = active;
                emit recordingStateChanged();
            }
        } else if (requestType == QStringLiteral("StartRecord")) {
            m_recordingActive = true;
            emit recordingStateChanged();
        } else if (requestType == QStringLiteral("StopRecord")) {
            m_recordingActive = false;
            emit recordingStateChanged();
        }
        emit requestCompleted(requestType, true, code);
        return;
    }

    // OBS already-running (500) / already-stopped (501) responses synchronize state cleanly.
    if (requestType == QStringLiteral("StartRecord") && code == kObsCodeOutputRunning) {
        m_recordingActive = true;
        emit recordingStateChanged();
        emit requestCompleted(requestType, true, code);
        return;
    }
    if (requestType == QStringLiteral("StopRecord") && code == kObsCodeOutputNotRunning) {
        m_recordingActive = false;
        emit recordingStateChanged();
        emit requestCompleted(requestType, true, code);
        return;
    }

    const QString comment = status.value(QStringLiteral("comment")).toString();
    setLastError(comment.isEmpty()
                     ? QStringLiteral("OBS request '%1' failed (%2)").arg(requestType).arg(code)
                     : comment);
    emit requestCompleted(requestType, false, code);
}

QString ObsWebSocketClient::sendRequest(const QString& requestType, const QJsonObject& requestData)
{
    if (!isConnected()) {
        return {};
    }

    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pendingRequests.insert(requestId, requestType);

    QJsonObject data;
    data.insert(QStringLiteral("requestType"), requestType);
    data.insert(QStringLiteral("requestId"), requestId);
    if (!requestData.isEmpty()) {
        data.insert(QStringLiteral("requestData"), requestData);
    }
    sendJsonPacket(kOpRequest, data);
    return requestId;
}

void ObsWebSocketClient::sendJsonPacket(int opCode, const QJsonObject& data)
{
    QJsonObject packet;
    packet.insert(QStringLiteral("op"), opCode);
    packet.insert(QStringLiteral("d"), data);
    m_socket.sendTextMessage(
        QString::fromUtf8(QJsonDocument(packet).toJson(QJsonDocument::Compact)));
}

void ObsWebSocketClient::scheduleReconnect()
{
    if (m_manualDisconnect || !m_enabled || !m_processRunning) {
        return;
    }

    const int shift = std::min(m_reconnectAttempts, 4);
    const int delayMs = std::min(30000, 1500 * (1 << shift));
    ++m_reconnectAttempts;
    m_reconnectTimer.start(delayMs);
}

void ObsWebSocketClient::setConnectionState(ConnectionState nextState)
{
    if (m_connectionState == nextState) {
        return;
    }
    m_connectionState = nextState;
    emit connectionStateChanged();
}

void ObsWebSocketClient::setLastError(const QString& error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

} // namespace kmedia
