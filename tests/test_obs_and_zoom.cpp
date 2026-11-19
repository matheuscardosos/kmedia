#include "integrations/ObsWebSocketClient.h"
#include "integrations/ZoomBridge.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSignalSpy>
#include <QWebSocket>
#include <QWebSocketServer>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ObsWebSocketClient computes v5 SHA-256 auth hash and automates scenes over WebSocket",
          "[integrations][obs]")
{
    // Verify OBS WebSocket v5 specification test vector computation:
    // base64(sha256(base64(sha256(password + salt)) + challenge))
    const QString auth1 = kmedia::ObsWebSocketClient::computeAuthResponse(
        QStringLiteral("supersecret"), QStringLiteral("randomsalt"),
        QStringLiteral("randomchallenge"));
    const QString auth2 = kmedia::ObsWebSocketClient::computeAuthResponse(
        QStringLiteral("supersecret"), QStringLiteral("randomsalt"),
        QStringLiteral("randomchallenge"));
    REQUIRE_FALSE(auth1.isEmpty());
    REQUIRE(auth1 == auth2);
    REQUIRE(auth1 != kmedia::ObsWebSocketClient::computeAuthResponse(
                         QStringLiteral("wrongpass"), QStringLiteral("randomsalt"),
                         QStringLiteral("randomchallenge")));

    QWebSocketServer server(QStringLiteral("mock-obs-v5"), QWebSocketServer::NonSecureMode);
    REQUIRE(server.listen(QHostAddress::LocalHost, 0));

    QString lastSceneSet;
    bool sawValidAuth = false;
    QPointer<QWebSocket> clientSock;

    QObject::connect(&server, &QWebSocketServer::newConnection, &server, [&]() {
        clientSock = server.nextPendingConnection();
        QObject::connect(
            clientSock.data(), &QWebSocket::textMessageReceived, clientSock.data(),
            [&](const QString& text) {
                const QJsonObject pkt = QJsonDocument::fromJson(text.toUtf8()).object();
                const int op = pkt.value(QStringLiteral("op")).toInt();
                const QJsonObject d = pkt.value(QStringLiteral("d")).toObject();
                if (op == 1) {
                    sawValidAuth = (d.value(QStringLiteral("authentication")).toString() == auth1);
                    QJsonObject identified;
                    identified.insert(QStringLiteral("op"), 2);
                    identified.insert(QStringLiteral("d"),
                                      QJsonObject{{QStringLiteral("negotiatedRpcVersion"), 1}});
                    clientSock->sendTextMessage(QString::fromUtf8(
                        QJsonDocument(identified).toJson(QJsonDocument::Compact)));
                } else if (op == 6) {
                    const QString reqType = d.value(QStringLiteral("requestType")).toString();
                    const QString reqId = d.value(QStringLiteral("requestId")).toString();
                    const QJsonObject reqData = d.value(QStringLiteral("requestData")).toObject();

                    QJsonObject respData;
                    QJsonObject status{{QStringLiteral("result"), true},
                                       {QStringLiteral("code"), 100}};

                    if (reqType == QStringLiteral("GetSceneList")) {
                        QJsonArray scenes;
                        scenes.append(QJsonObject{
                            {QStringLiteral("sceneName"), QStringLiteral("Platform Camera")}});
                        scenes.append(QJsonObject{
                            {QStringLiteral("sceneName"), QStringLiteral("Media Playback")}});
                        respData.insert(QStringLiteral("scenes"), scenes);
                        respData.insert(QStringLiteral("currentProgramSceneName"),
                                        QStringLiteral("Platform Camera"));
                    } else if (reqType == QStringLiteral("GetRecordStatus")) {
                        respData.insert(QStringLiteral("outputActive"), false);
                    } else if (reqType == QStringLiteral("SetCurrentProgramScene")) {
                        lastSceneSet = reqData.value(QStringLiteral("sceneName")).toString();
                    } else if (reqType == QStringLiteral("StartRecord")) {
                        status = QJsonObject{{QStringLiteral("result"), false},
                                             {QStringLiteral("code"), 500}};
                    }

                    const QJsonObject reply{
                        {QStringLiteral("op"), 7},
                        {QStringLiteral("d"),
                         QJsonObject{{QStringLiteral("requestType"), reqType},
                                     {QStringLiteral("requestId"), reqId},
                                     {QStringLiteral("requestStatus"), status},
                                     {QStringLiteral("responseData"), respData}}}};
                    clientSock->sendTextMessage(
                        QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
                }
            });

        const QJsonObject hello{
            {QStringLiteral("op"), 0},
            {QStringLiteral("d"),
             QJsonObject{
                 {QStringLiteral("obsWebSocketVersion"), QStringLiteral("5.4.0")},
                 {QStringLiteral("rpcVersion"), 1},
                 {QStringLiteral("authentication"),
                  QJsonObject{{QStringLiteral("challenge"), QStringLiteral("randomchallenge")},
                              {QStringLiteral("salt"), QStringLiteral("randomsalt")}}}}}};
        clientSock->sendTextMessage(
            QString::fromUtf8(QJsonDocument(hello).toJson(QJsonDocument::Compact)));
    });

    kmedia::ObsWebSocketClient client;
    client.setHost(QStringLiteral("127.0.0.1"));
    client.setPort(static_cast<int>(server.serverPort()));
    client.setPassword(QStringLiteral("supersecret"));
    client.setPlayScene(QStringLiteral("Media Playback"));
    client.setPauseScene(QStringLiteral("Paused Camera"));
    client.setStandbyScene(QStringLiteral("Platform Camera"));

    QSignalSpy scenesSpy(&client, &kmedia::ObsWebSocketClient::availableScenesChanged);
    QSignalSpy reqSpy(&client, &kmedia::ObsWebSocketClient::requestCompleted);

    client.connectToObs();
    REQUIRE(scenesSpy.wait(3000));
    REQUIRE(sawValidAuth);
    REQUIRE(client.isConnected());
    REQUIRE(client.availableScenes() ==
            QStringList{QStringLiteral("Platform Camera"), QStringLiteral("Media Playback")});
    REQUIRE(client.currentProgramScene() == QStringLiteral("Platform Camera"));

    client.applyPlaybackState(true, false, false);
    REQUIRE(reqSpy.wait(2000));
    REQUIRE(lastSceneSet == QStringLiteral("Media Playback"));

    client.applyPlaybackState(true, false, true);
    REQUIRE(reqSpy.wait(2000));
    REQUIRE(lastSceneSet == QStringLiteral("Platform Camera"));

    REQUIRE(client.startRecording());
    REQUIRE(reqSpy.wait(2000));
    REQUIRE(client.isRecordingActive());

    client.disconnectFromObs();
}

TEST_CASE("ZoomBridge formats sanitized zoommtg deep links and coordinates audio/share settings",
          "[integrations][zoom]")
{
    REQUIRE(kmedia::ZoomBridge::presentationWindowTitle() ==
            QStringLiteral("KMedia - Presentation"));

    const QUrl url = kmedia::ZoomBridge::buildJoinUrl(
        QStringLiteral("845 1234 9012"), QStringLiteral("123456"), QStringLiteral("Sound Booth"));
    REQUIRE(url.isValid());
    REQUIRE(url.scheme() == QStringLiteral("zoommtg"));
    REQUIRE(url.toString().contains(QStringLiteral("confno=84512349012")));
    REQUIRE(url.toString().contains(QStringLiteral("pwd=123456")));

    kmedia::ZoomBridge zoom;
    QUrl launchedUrl;
    zoom.setUrlLauncher([&launchedUrl](const QUrl& u) {
        launchedUrl = u;
        return true;
    });
    zoom.setMeetingId(QStringLiteral("999-888-777"));
    zoom.setMeetingPassword(QStringLiteral("pass"));
    zoom.setZoomRunning(true);
    zoom.setAutoShareOnPlayback(true);

    QSignalSpy shareSpy(&zoom, &kmedia::ZoomBridge::shareTriggerRequested);
    REQUIRE(zoom.joinMeeting());
    REQUIRE(launchedUrl.toString().contains(QStringLiteral("confno=999888777")));

    zoom.notifyPresentationPlaybackStarted();
    REQUIRE(shareSpy.count() == 1);
    REQUIRE(shareSpy.at(0).at(1).toString() == QStringLiteral("KMedia - Presentation"));
}
