#include "network/CatalogClient.h"

#include "core/DatabaseManager.h"
#include "core/NetworkManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>

#include <utility>

namespace kmedia {

namespace {

QString normalizedBaseDomain(const QString& baseDomain)
{
    QString clean = baseDomain.trimmed();
    clean.remove(QStringLiteral("https://"));
    clean.remove(QStringLiteral("http://"));
    while (clean.endsWith(QLatin1Char('/'))) {
        clean.chop(1);
    }
    return clean.isEmpty() ? QStringLiteral("jw.org") : clean;
}

void parseAssetObject(const QJsonObject& obj, const QString& pubSymbol, int issueTag,
                      const QString& formatKey, std::vector<CatalogMediaAsset>& out)
{
    const QJsonObject fileObj = obj.value(QStringLiteral("file")).toObject();
    const QString url = fileObj.value(QStringLiteral("url")).toString().trimmed();
    if (url.isEmpty()) {
        return;
    }

    CatalogMediaAsset asset;
    asset.url = url;
    asset.sha256 = fileObj.value(QStringLiteral("checksum")).toString().trimmed();
    asset.fileSize = obj.value(QStringLiteral("filesize")).toInteger(0);
    asset.mimeType = obj.value(QStringLiteral("mimetype")).toString().trimmed();
    asset.label = obj.value(QStringLiteral("label")).toString().trimmed();
    asset.title = obj.value(QStringLiteral("title")).toString().trimmed();
    asset.fileFormat = formatKey;
    asset.keySymbol = pubSymbol;
    asset.issueTagNumber = issueTag;
    asset.track = obj.value(QStringLiteral("track")).toInt(0);
    asset.durationSeconds = obj.value(QStringLiteral("duration")).toDouble(0.0);
    asset.modifiedDatetime = fileObj.value(QStringLiteral("modifiedDatetime")).toString().trimmed();
    out.push_back(std::move(asset));
}

} // namespace

CatalogClient::CatalogClient(NetworkManager* networkManager, DatabaseManager* databaseManager,
                             QObject* parent)
    : QObject(parent), m_network(networkManager), m_database(databaseManager)
{
    if (m_network == nullptr) {
        m_ownedNetwork = std::make_unique<NetworkManager>();
        m_network = m_ownedNetwork.get();
    }
}

void CatalogClient::setDatabaseManager(DatabaseManager* databaseManager)
{
    m_database = databaseManager;
}

QUrl CatalogClient::languagesEndpointUrl(const QString& baseDomain)
{
    return QUrl(
        QStringLiteral("https://www.%1/en/languages/json/").arg(normalizedBaseDomain(baseDomain)));
}

QUrl CatalogClient::pubMediaLinksUrl(const QString& pubSymbol, const QString& langCode,
                                     int issueTagNumber, const QString& fileFormat, int track,
                                     const QString& docId, const QString& baseUrl)
{
    QUrl url(baseUrl);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("alllangs"), QStringLiteral("0"));
    if (!docId.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("docid"), docId.trimmed());
    } else if (!pubSymbol.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("pub"), pubSymbol.trimmed());
    }
    if (issueTagNumber > 0) {
        query.addQueryItem(QStringLiteral("issue"), QString::number(issueTagNumber));
    }
    if (!langCode.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("langwritten"), langCode.trimmed());
    }
    if (!fileFormat.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("fileformat"), fileFormat.trimmed());
    }
    if (track > 0) {
        query.addQueryItem(QStringLiteral("track"), QString::number(track));
    }
    query.addQueryItem(QStringLiteral("txtCMSLang"), QStringLiteral("E"));
    query.addQueryItem(QStringLiteral("output"), QStringLiteral("json"));
    url.setQuery(query);
    return url;
}

std::vector<CatalogLanguage> CatalogClient::parseLanguagesJson(const QByteArray& json)
{
    std::vector<CatalogLanguage> languages;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return languages;
    }

    const QJsonArray arr = doc.object().value(QStringLiteral("languages")).toArray();
    languages.reserve(static_cast<size_t>(arr.size()));
    for (const QJsonValue& val : arr) {
        if (!val.isObject()) {
            continue;
        }
        const QJsonObject obj = val.toObject();
        const QString code = obj.value(QStringLiteral("langcode")).toString().trimmed();
        if (code.isEmpty()) {
            continue;
        }
        CatalogLanguage lang;
        lang.code = code;
        lang.symbol = obj.value(QStringLiteral("symbol")).toString().trimmed();
        lang.name = obj.value(QStringLiteral("name")).toString().trimmed();
        lang.vernacularName = obj.value(QStringLiteral("vernacularName")).toString().trimmed();
        lang.isSignLanguage = obj.value(QStringLiteral("isSignLanguage")).toBool(false);
        lang.isRtl = obj.value(QStringLiteral("isRTL")).toBool(false) ||
                     obj.value(QStringLiteral("direction"))
                             .toString()
                             .compare(QStringLiteral("rtl"), Qt::CaseInsensitive) == 0;
        languages.push_back(std::move(lang));
    }
    return languages;
}

std::vector<CatalogMediaAsset>
CatalogClient::parsePubMediaLinksJson(const QByteArray& json, const QString& preferredLangCode)
{
    std::vector<CatalogMediaAsset> assets;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return assets;
    }

    const QJsonObject root = doc.object();
    const QString pubSymbol = root.value(QStringLiteral("pub")).toString().trimmed();
    const int issueTag = root.value(QStringLiteral("issue")).toString().toInt();
    const QJsonObject filesByLang = root.value(QStringLiteral("files")).toObject();

    const QStringList langKeys =
        (!preferredLangCode.isEmpty() && filesByLang.contains(preferredLangCode))
            ? QStringList{preferredLangCode}
            : filesByLang.keys();

    for (const QString& langKey : langKeys) {
        const QJsonObject formatsObj = filesByLang.value(langKey).toObject();
        for (auto it = formatsObj.constBegin(); it != formatsObj.constEnd(); ++it) {
            const QString formatKey = it.key();
            if (!it.value().isArray()) {
                continue;
            }
            const QJsonArray mediaArray = it.value().toArray();
            for (const QJsonValue& itemVal : mediaArray) {
                if (itemVal.isObject()) {
                    parseAssetObject(itemVal.toObject(), pubSymbol, issueTag, formatKey, assets);
                }
            }
        }
    }
    return assets;
}

template <typename Callback>
void CatalogClient::fetchWithEtagCache(const QUrl& endpoint, Callback&& onResult)
{
    if (!endpoint.isValid()) {
        emit requestFailed(endpoint.toString(), QStringLiteral("Invalid catalog endpoint URL"));
        return;
    }

    QString etag;
    QString lastModified;
    if (m_database != nullptr) {
        if (const std::optional<DownloadCacheRecord> cached =
                m_database->findDownloadCache(endpoint.toString());
            cached.has_value()) {
            etag = cached->etag;
            lastModified = cached->lastModified;
        }
    }

    QNetworkReply* reply =
        m_network->accessManager()->get(m_network->buildRequest(endpoint, etag, lastModified));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, endpoint, handler = std::forward<Callback>(onResult)]() {
                const HttpCacheHeaders headers = NetworkManager::extractCacheHeaders(reply);
                const QByteArray body = reply->readAll();
                const QNetworkReply::NetworkError netErr = reply->error();
                const QString errText = reply->errorString();
                reply->deleteLater();

                if (headers.notModified) {
                    handler(QByteArray{}, true);
                    return;
                }
                if (netErr != QNetworkReply::NoError || headers.statusCode >= 400) {
                    emit requestFailed(endpoint.toString(), errText);
                    return;
                }
                if (m_database != nullptr) {
                    DownloadCacheRecord rec;
                    rec.url = endpoint.toString();
                    rec.etag = headers.etag;
                    rec.lastModified = headers.lastModified;
                    (void)m_database->upsertDownloadCache(rec);
                }
                handler(body, false);
            });
}

void CatalogClient::fetchLanguages(const QUrl& endpoint)
{
    fetchWithEtagCache(endpoint, [this](const QByteArray& body, bool fromCache) {
        if (fromCache) {
            emit languagesLoaded({}, true);
            return;
        }
        emit languagesLoaded(parseLanguagesJson(body), false);
    });
}

void CatalogClient::fetchPubMediaLinks(const QUrl& endpoint, const QString& preferredLangCode)
{
    fetchWithEtagCache(endpoint, [this, preferredLangCode](const QByteArray& body, bool fromCache) {
        if (fromCache) {
            emit pubMediaAssetsLoaded({}, true);
            return;
        }
        emit pubMediaAssetsLoaded(parsePubMediaLinksJson(body, preferredLangCode), false);
    });
}

} // namespace kmedia
