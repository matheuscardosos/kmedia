#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QWebSocket>

namespace kmedia {

/**
 * Native OBS Studio WebSocket v5 client built on Qt6::WebSockets.
 * Implements Hello -> SHA-256 Challenge/Identify -> Identified handshake,
 * scene enumeration, and playback-driven program scene automation.
 */
class ObsWebSocketClient : public QObject
{
    Q_OBJECT

    Q_PROPERTY(
        ConnectionState connectionState READ connectionState NOTIFY connectionStateChanged FINAL)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionStateChanged FINAL)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY configChanged FINAL)
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY configChanged FINAL)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY configChanged FINAL)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY configChanged FINAL)
    Q_PROPERTY(QString playScene READ playScene WRITE setPlayScene NOTIFY sceneMapChanged FINAL)
    Q_PROPERTY(QString pauseScene READ pauseScene WRITE setPauseScene NOTIFY sceneMapChanged FINAL)
    Q_PROPERTY(QString stopScene READ stopScene WRITE setStopScene NOTIFY sceneMapChanged FINAL)
    Q_PROPERTY(
        QString standbyScene READ standbyScene WRITE setStandbyScene NOTIFY sceneMapChanged FINAL)
    Q_PROPERTY(QStringList availableScenes READ availableScenes NOTIFY availableScenesChanged FINAL)
    Q_PROPERTY(QString currentProgramScene READ currentProgramScene NOTIFY
                   currentProgramSceneChanged FINAL)
    Q_PROPERTY(bool recordingActive READ isRecordingActive NOTIFY recordingStateChanged FINAL)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
    enum class ConnectionState {
        Disconnected = 0,
        Connecting,
        Authenticating,
        Connected,
        Error,
    };
    Q_ENUM(ConnectionState)

    explicit ObsWebSocketClient(QObject* parent = nullptr);
    ~ObsWebSocketClient() override;

    Q_DISABLE_COPY_MOVE(ObsWebSocketClient)

    [[nodiscard]] ConnectionState connectionState() const;
    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool enabled);

    [[nodiscard]] QString host() const;
    void setHost(const QString& host);

    [[nodiscard]] int port() const;
    void setPort(int port);

    [[nodiscard]] QString password() const;
    void setPassword(const QString& password);

    [[nodiscard]] QString playScene() const;
    void setPlayScene(const QString& sceneName);
    [[nodiscard]] QString pauseScene() const;
    void setPauseScene(const QString& sceneName);
    [[nodiscard]] QString stopScene() const;
    void setStopScene(const QString& sceneName);
    [[nodiscard]] QString standbyScene() const;
    void setStandbyScene(const QString& sceneName);

    [[nodiscard]] QStringList availableScenes() const;
    [[nodiscard]] QString currentProgramScene() const;
    [[nodiscard]] bool isRecordingActive() const;
    [[nodiscard]] QString lastError() const;

    void setProcessRunning(bool running);

    Q_INVOKABLE void connectToObs();
    Q_INVOKABLE void disconnectFromObs();
    Q_INVOKABLE void requestSceneList();
    Q_INVOKABLE void requestRecordStatus();
    Q_INVOKABLE bool setCurrentProgramScene(const QString& sceneName);
    Q_INVOKABLE bool startRecording();
    Q_INVOKABLE bool stopRecording();
    Q_INVOKABLE void applyPlaybackState(bool playing, bool paused, bool standby);

    [[nodiscard]] static QString computeAuthResponse(const QString& password, const QString& salt,
                                                     const QString& challenge);

signals:
    void connectionStateChanged();
    void configChanged();
    void sceneMapChanged();
    void availableScenesChanged();
    void currentProgramSceneChanged();
    void recordingStateChanged();
    void lastErrorChanged();
    void requestCompleted(const QString& requestType, bool success, int statusCode);

private:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketTextMessage(const QString& message);
    void onSocketError(QAbstractSocket::SocketError error);
    void handleHello(const QJsonObject& data);
    void handleIdentified();
    void handleEvent(const QJsonObject& data);
    void handleRequestResponse(const QJsonObject& data);
    QString sendRequest(const QString& requestType, const QJsonObject& requestData = {});
    void sendJsonPacket(int opCode, const QJsonObject& data);
    void scheduleReconnect();
    void setConnectionState(ConnectionState nextState);
    void setLastError(const QString& error);

    QWebSocket m_socket;
    QTimer m_reconnectTimer;
    ConnectionState m_connectionState{ConnectionState::Disconnected};
    bool m_enabled{false};
    bool m_processRunning{false};
    bool m_manualDisconnect{false};
    int m_reconnectAttempts{0};

    QString m_host{QStringLiteral("127.0.0.1")};
    int m_port{4455};
    QString m_password;

    QString m_playScene;
    QString m_pauseScene;
    QString m_stopScene;
    QString m_standbyScene;

    QStringList m_availableScenes;
    QString m_currentProgramScene;
    bool m_recordingActive{false};
    QString m_lastError;

    QHash<QString, QString> m_pendingRequests;
};

} // namespace kmedia
