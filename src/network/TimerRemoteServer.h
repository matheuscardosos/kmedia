#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

class QTcpServer;
class QTcpSocket;

namespace kmedia {

class MeetingTimer;

/**
 * The timer remote, inspired by Meeting Media Manager: a read only web page with the meeting
 * timer, served on the local network for a tablet or phone at the lectern.
 *
 * The page lives under a random access code (`http://<address>:<port>/<code>/`), so only devices
 * given the link or the QR code can open it, and any other path answers 404. The server takes
 * only GET requests and no actions: the page holds a server sent events stream open and redraws
 * on each timer change, computing the running clock itself, so an idle timer sends nothing but a
 * keep alive comment every 20 seconds.
 */
class TimerRemoteServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY stateChanged FINAL)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool focusLayout READ isFocusLayout WRITE setFocusLayout NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString accessCode READ accessCode NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool running READ isRunning NOTIFY stateChanged FINAL)
    Q_PROPERTY(Problem problem READ problem NOTIFY stateChanged FINAL)
    Q_PROPERTY(QStringList urls READ urls NOTIFY stateChanged FINAL)
    Q_PROPERTY(int clientCount READ clientCount NOTIFY clientCountChanged FINAL)
    // Set by the interface, in its language: the title of the part being timed and the page's
    // words.
    Q_PROPERTY(QString partTitle READ partTitle WRITE setPartTitle NOTIFY pageChanged FINAL)
    Q_PROPERTY(QVariantMap pageTexts READ pageTexts WRITE setPageTexts NOTIFY pageChanged FINAL)

public:
    // Why the page is not served; the interface words it.
    enum class Problem { None, PortInUse, NoNetwork, Failed };
    Q_ENUM(Problem)

    static constexpr int kDefaultPort = 8787;
    static constexpr int kMaxClients = 16;

    explicit TimerRemoteServer(MeetingTimer* timer, QObject* parent = nullptr);
    ~TimerRemoteServer() override;

    Q_DISABLE_COPY_MOVE(TimerRemoteServer)

    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool enabled);
    [[nodiscard]] int port() const;
    // 1024 to 65535; 0 lets the system pick one (for tests).
    void setPort(int port);
    [[nodiscard]] bool isFocusLayout() const;
    void setFocusLayout(bool focus);
    [[nodiscard]] QString accessCode() const;
    // An empty code makes a new one.
    void setAccessCode(const QString& code);
    Q_INVOKABLE void renewAccessCode();
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] Problem problem() const;
    [[nodiscard]] QStringList urls() const;
    [[nodiscard]] int clientCount() const;
    [[nodiscard]] quint16 listeningPort() const;
    [[nodiscard]] QString partTitle() const;
    void setPartTitle(const QString& title);
    [[nodiscard]] QVariantMap pageTexts() const;
    void setPageTexts(const QVariantMap& texts);

    // The timer as the page reads it, as JSON.
    [[nodiscard]] QByteArray stateJson() const;
    [[nodiscard]] static QString newAccessCode();
    // The modules of a QR code for `text`: {size, modules} with one '1' or '0' per module, row by
    // row; empty when the text does not fit.
    [[nodiscard]] Q_INVOKABLE static QVariantMap qrCode(const QString& text);

signals:
    void stateChanged();
    void clientCountChanged();
    void pageChanged();

private:
    void restart();
    void stopServer();
    void handleConnection();
    void handleRequest(QTcpSocket* socket);
    void respond(QTcpSocket* socket, int status, const QByteArray& reason,
                 const QByteArray& contentType, const QByteArray& body,
                 const QList<QPair<QByteArray, QByteArray>>& extraHeaders = {});
    void openEvents(QTcpSocket* socket);
    void broadcast();
    void sendKeepAlive();
    static void writeEvent(QTcpSocket* client, const QByteArray& event);
    [[nodiscard]] static QStringList localAddresses();

    QPointer<MeetingTimer> m_timer;
    QTcpServer* m_server{nullptr};
    QList<QPointer<QTcpSocket>> m_clients;
    int m_openSockets{0};
    QTimer m_keepAlive;
    QTimer m_broadcastSoon;
    bool m_enabled{false};
    int m_port{kDefaultPort};
    bool m_focusLayout{false};
    QString m_accessCode;
    Problem m_problem{Problem::None};
    QStringList m_urls;
    QString m_partTitle;
    QVariantMap m_pageTexts;
};

} // namespace kmedia
