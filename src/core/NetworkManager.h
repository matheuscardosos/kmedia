#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QUrl>

namespace kmedia {

struct HttpCacheHeaders
{
    QString etag;
    QString lastModified;
    int statusCode{0};
    bool notModified{false};
};

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject* parent = nullptr);

    Q_DISABLE_COPY_MOVE(NetworkManager)

    [[nodiscard]] QNetworkAccessManager* accessManager();
    [[nodiscard]] QString userAgent() const;
    void setUserAgent(const QString& userAgent);

    [[nodiscard]] int transferTimeoutMs() const;
    void setTransferTimeoutMs(int timeoutMs);

    [[nodiscard]] int maxConcurrentRequests() const;
    void setMaxConcurrentRequests(int maxConcurrent);

    [[nodiscard]] QNetworkRequest buildRequest(const QUrl& url, const QString& etag = {},
                                               const QString& lastModified = {}) const;

    [[nodiscard]] static HttpCacheHeaders extractCacheHeaders(const QNetworkReply* reply);
    [[nodiscard]] static bool isRetryableStatus(int statusCode);
    [[nodiscard]] static int retryDelayMs(int attemptIndex, int baseDelayMs = 1000,
                                          int maxDelayMs = 30000);

private:
    QNetworkAccessManager m_nam;
    QString m_userAgent;
    int m_transferTimeoutMs{30000};
    int m_maxConcurrentRequests{3};
};

} // namespace kmedia
