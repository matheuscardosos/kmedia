#include "core/NetworkManager.h"

#include <QVariant>

namespace kmedia {

namespace {

QString defaultUserAgent()
{
    return QStringLiteral("KMedia/%1 (+https://matheuscardosos.github.io/kmedia)")
        .arg(QStringLiteral(KMEDIA_APP_VERSION));
}

} // namespace

NetworkManager::NetworkManager(QObject* parent) : QObject(parent), m_userAgent(defaultUserAgent())
{
    m_nam.setTransferTimeout(m_transferTimeoutMs);
}

QNetworkAccessManager* NetworkManager::accessManager()
{
    return &m_nam;
}

QString NetworkManager::userAgent() const
{
    return m_userAgent;
}

void NetworkManager::setUserAgent(const QString& userAgent)
{
    if (!userAgent.trimmed().isEmpty()) {
        m_userAgent = userAgent.trimmed();
    }
}

int NetworkManager::transferTimeoutMs() const
{
    return m_transferTimeoutMs;
}

void NetworkManager::setTransferTimeoutMs(int timeoutMs)
{
    m_transferTimeoutMs = qBound(1000, timeoutMs, 300000);
    m_nam.setTransferTimeout(m_transferTimeoutMs);
}

int NetworkManager::maxConcurrentRequests() const
{
    return m_maxConcurrentRequests;
}

void NetworkManager::setMaxConcurrentRequests(int maxConcurrent)
{
    m_maxConcurrentRequests = qBound(1, maxConcurrent, 6);
}

QNetworkRequest NetworkManager::buildRequest(const QUrl& url, const QString& etag,
                                             const QString& lastModified) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    if (!etag.trimmed().isEmpty()) {
        request.setRawHeader("If-None-Match", etag.trimmed().toUtf8());
    }
    if (!lastModified.trimmed().isEmpty()) {
        request.setRawHeader("If-Modified-Since", lastModified.trimmed().toUtf8());
    }
    return request;
}

HttpCacheHeaders NetworkManager::extractCacheHeaders(const QNetworkReply* reply)
{
    HttpCacheHeaders headers;
    if (reply == nullptr) {
        return headers;
    }

    const QVariant statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (statusAttr.isValid()) {
        headers.statusCode = statusAttr.toInt();
        headers.notModified = (headers.statusCode == 304);
    }
    headers.etag = QString::fromUtf8(reply->rawHeader("ETag")).trimmed();
    headers.lastModified = QString::fromUtf8(reply->rawHeader("Last-Modified")).trimmed();
    return headers;
}

bool NetworkManager::isRetryableStatus(int statusCode)
{
    return statusCode == 408 || statusCode == 429 || (statusCode >= 500 && statusCode < 600);
}

int NetworkManager::retryDelayMs(int attemptIndex, int baseDelayMs, int maxDelayMs)
{
    const int clampedAttempt = qBound(0, attemptIndex, 10);
    const qint64 delay = static_cast<qint64>(qMax(100, baseDelayMs)) << clampedAttempt;
    return static_cast<int>(qMin<qint64>(delay, qMax(baseDelayMs, maxDelayMs)));
}

} // namespace kmedia
