#include "network/CatalogClient.h"

#include "core/DatabaseManager.h"
#include "core/NetworkManager.h"
#include "core/SystemUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QSaveFile>
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

// The preview picture of a media item: a wide picture first, at the smallest size from "sm" up.
QString bestImageUrl(const QJsonObject& images)
{
    static constexpr QLatin1StringView kTypes[] = {
        QLatin1StringView("wss"), QLatin1StringView("lsr"), QLatin1StringView("sqr"),
        QLatin1StringView("pnr")};
    static constexpr QLatin1StringView kSizes[] = {QLatin1StringView("sm"), QLatin1StringView("md"),
                                                   QLatin1StringView("lg"),
                                                   QLatin1StringView("xl")};
    for (const QLatin1StringView type : kTypes) {
        const QJsonObject sizes = images.value(type).toObject();
        for (const QLatin1StringView size : kSizes) {
            const QString url = sizes.value(size).toString().trimmed();
            if (!url.isEmpty()) {
                return url;
            }
        }
        for (auto it = sizes.constBegin(); it != sizes.constEnd(); ++it) {
            const QString url = it.value().toString().trimmed();
            if (!url.isEmpty()) {
                return url;
            }
        }
    }
    return {};
}

CatalogMediaAsset assetFromMediatorFile(const QJsonObject& file, const QString& title,
                                        double mediaDuration, const QString& key,
                                        const QString& imageUrl)
{
    CatalogMediaAsset asset;
    asset.url = file.value(QStringLiteral("progressiveDownloadURL"))
                    .toString(file.value(QStringLiteral("url")).toString())
                    .trimmed();
    asset.sha256 = file.value(QStringLiteral("checksum")).toString().trimmed();
    asset.fileSize = file.value(QStringLiteral("filesize")).toInteger(0);
    asset.mimeType = file.value(QStringLiteral("mimetype")).toString().trimmed();
    asset.label = file.value(QStringLiteral("label")).toString().trimmed();
    asset.title = title;
    asset.durationSeconds = file.value(QStringLiteral("duration")).toDouble(mediaDuration);
    asset.pubMediaId = key;
    asset.streamOnly = asset.durationSeconds > CatalogClient::kLongMediaDurationSeconds;
    asset.imageUrl = imageUrl;
    if (asset.mimeType.contains(QStringLiteral("audio"), Qt::CaseInsensitive) ||
        asset.url.endsWith(QStringLiteral(".mp3"), Qt::CaseInsensitive)) {
        asset.fileFormat = QStringLiteral("MP3");
    } else if (asset.url.endsWith(QStringLiteral(".mp4"), Qt::CaseInsensitive)) {
        asset.fileFormat = QStringLiteral("MP4");
    } else {
        // Low resolution 3GP copies are not meant for a projector.
        asset.fileFormat = QFileInfo(QUrl(asset.url).path()).suffix().toUpper();
    }
    return asset;
}

// "00:01:07.343" as milliseconds.
qint64 clockToMs(const QString& clock)
{
    const QStringList parts = clock.trimmed().split(QLatin1Char(':'));
    double seconds = 0.0;
    for (const QString& part : parts) {
        seconds = (seconds * 60.0) + part.toDouble();
    }
    return static_cast<qint64>(seconds * 1000.0 + 0.5);
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
    asset.streamOnly = asset.durationSeconds > CatalogClient::kLongMediaDurationSeconds;
    asset.imageUrl = obj.value(QStringLiteral("trackImage"))
                         .toObject()
                         .value(QStringLiteral("url"))
                         .toString()
                         .trimmed();
    const QJsonArray markers =
        obj.value(QStringLiteral("markers")).toObject().value(QStringLiteral("markers")).toArray();
    for (const QJsonValue& markerValue : markers) {
        const QJsonObject marker = markerValue.toObject();
        const int verse = marker.value(QStringLiteral("verseNumber")).toInt(0);
        if (verse > 0) {
            asset.verseMarkers.push_back(BibleVerseMarker{
                verse, clockToMs(marker.value(QStringLiteral("startTime")).toString()),
                clockToMs(marker.value(QStringLiteral("duration")).toString())});
        }
    }
    out.push_back(std::move(asset));
}

int parseDayOfWeekValue(const QJsonValue& val, int fallbackDay)
{
    if (val.isDouble()) {
        const int d = val.toInt();
        return (d >= 1 && d <= 7) ? d : fallbackDay;
    }
    const QString text = val.toString().trimmed().toLower();
    if (text.startsWith(QStringLiteral("mon"))) {
        return 1;
    }
    if (text.startsWith(QStringLiteral("tue"))) {
        return 2;
    }
    if (text.startsWith(QStringLiteral("wed"))) {
        return 3;
    }
    if (text.startsWith(QStringLiteral("thu"))) {
        return 4;
    }
    if (text.startsWith(QStringLiteral("fri"))) {
        return 5;
    }
    if (text.startsWith(QStringLiteral("sat"))) {
        return 6;
    }
    if (text.startsWith(QStringLiteral("sun"))) {
        return 7;
    }
    return fallbackDay;
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

CatalogClient::~CatalogClient() = default;

void CatalogClient::setDatabaseManager(DatabaseManager* databaseManager)
{
    m_database = databaseManager;
}

QUrl CatalogClient::languagesEndpointUrl(const QString& baseDomain)
{
    return QUrl(
        QStringLiteral("https://www.%1/en/languages/json/").arg(normalizedBaseDomain(baseDomain)));
}

QUrl CatalogClient::hubLanguagesEndpointUrl()
{
    return QUrl(QStringLiteral("https://hub.jw.org/meetings/api/languages"));
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

QUrl CatalogClient::bibleBookUrl(const QString& langCode, int bookNumber, const QString& fileFormat)
{
    QUrl url = pubMediaLinksUrl(QStringLiteral("nwt"), langCode, 0, fileFormat);
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("booknum"), QString::number(bookNumber));
    url.setQuery(query);
    return url;
}

JwBibleBook CatalogClient::parseBibleBookJson(const QByteArray& json, const QString& langCode)
{
    JwBibleBook book;
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    book.number = root.value(QStringLiteral("booknum")).toInt(0);
    book.name = root.value(QStringLiteral("pubName"))
                    .toString()
                    .replace(QStringLiteral("&nbsp;"), QStringLiteral(" "))
                    .trimmed();
    for (CatalogMediaAsset& asset : parsePubMediaLinksJson(json, langCode)) {
        if (asset.track > 0) {
            book.chapters.push_back(std::move(asset));
        }
    }
    return book;
}

void CatalogClient::requestBibleBook(const QUrl& endpoint, const QString& langCode,
                                     QObject* context, BibleBookCallback callback)
{
    const QPointer<QObject> guard(context);
    fetchWithEtagCache(
        endpoint,
        [guard, callback, langCode](const QByteArray& body, bool) {
            if (guard.isNull()) {
                return;
            }
            const JwBibleBook book =
                body.isEmpty() ? JwBibleBook{} : parseBibleBookJson(body, langCode);
            callback(!book.chapters.empty(), book);
        },
        [guard, callback](const QString&) {
            if (!guard.isNull()) {
                callback(false, {});
            }
        });
}

QUrl CatalogClient::mediatorMediaUrl(const QString& pubMediaKey, const QString& langCode,
                                     const QString& baseUrl)
{
    const QString cleanLang =
        langCode.trimmed().isEmpty() ? QStringLiteral("E") : langCode.trimmed();
    QString cleanBase = baseUrl.trimmed();
    while (cleanBase.endsWith(QLatin1Char('/'))) {
        cleanBase.chop(1);
    }
    QUrl url(QStringLiteral("%1/%2/%3").arg(cleanBase, cleanLang, pubMediaKey.trimmed()));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("clientType"), QStringLiteral("www"));
    url.setQuery(query);
    return url;
}

QUrl CatalogClient::mediatorCategoryUrl(const QString& langCode, const QString& categoryKey,
                                        bool subcategoriesOnly, const QString& baseUrl)
{
    QString cleanBase = baseUrl.trimmed();
    while (cleanBase.endsWith(QLatin1Char('/'))) {
        cleanBase.chop(1);
    }
    QUrl url(QStringLiteral("%1/%2/%3").arg(cleanBase, langCode.trimmed(), categoryKey.trimmed()));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("clientType"), QStringLiteral("www"));
    // The category tree is walked without media; the videos come from each leaf category.
    query.addQueryItem(QStringLiteral("detailed"),
                       subcategoriesOnly ? QStringLiteral("1") : QStringLiteral("0"));
    if (subcategoriesOnly) {
        query.addQueryItem(QStringLiteral("mediaLimit"), QStringLiteral("0"));
    }
    url.setQuery(query);
    return url;
}

QString CatalogClient::buildPubMediaKey(const QString& pubSymbol, int issueTagNumber, qint64 docId,
                                        int track, bool isVideo)
{
    const QString kindTag = isVideo ? QStringLiteral("VIDEO") : QStringLiteral("AUDIO");
    if (docId > 0) {
        return QStringLiteral("doc-%1_%2_%3").arg(docId).arg(track).arg(kindTag);
    }
    const QString sym = pubSymbol.trimmed().toLower();
    if (issueTagNumber > 0) {
        return QStringLiteral("pub-%1_%2_%3_%4")
            .arg(sym)
            .arg(issueTagNumber)
            .arg(track)
            .arg(kindTag);
    }
    return QStringLiteral("pub-%1_%2_%3").arg(sym).arg(track).arg(kindTag);
}

QString CatalogClient::pinyinSongFilename(int songNumber, int height)
{
    const int safeSong = qBound(1, songNumber, 999);
    const int safeHeight = (height >= 720) ? 720 : ((height >= 480) ? 480 : 360);
    return QStringLiteral("sjjm_s-Pi_CHS_%1_r%2P.mp4")
        .arg(safeSong, 3, 10, QLatin1Char('0'))
        .arg(safeHeight);
}

QUrl CatalogClient::congregationSearchUrl(const QString& queryText, const QString& baseUrl)
{
    QUrl url(baseUrl);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), queryText.trimmed());
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

std::vector<CatalogMediaAsset> CatalogClient::parseMediatorMediaJson(const QByteArray& json,
                                                                     const QString& pubMediaKey)
{
    std::vector<CatalogMediaAsset> assets;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return assets;
    }

    const QJsonArray mediaArray = doc.object().value(QStringLiteral("media")).toArray();
    for (const QJsonValue& mediaVal : mediaArray) {
        const QJsonObject mediaObj = mediaVal.toObject();
        const QString title = mediaObj.value(QStringLiteral("title")).toString().trimmed();
        const double durationSec = mediaObj.value(QStringLiteral("duration")).toDouble(0.0);
        const QString mediaKey = mediaObj.value(QStringLiteral("naturalKey")).toString().trimmed();
        const QString imageUrl = bestImageUrl(mediaObj.value(QStringLiteral("images")).toObject());
        for (const QJsonValue& fileVal : mediaObj.value(QStringLiteral("files")).toArray()) {
            CatalogMediaAsset asset =
                assetFromMediatorFile(fileVal.toObject(), title, durationSec,
                                      pubMediaKey.isEmpty() ? mediaKey : pubMediaKey, imageUrl);
            if (!asset.url.isEmpty()) {
                assets.push_back(std::move(asset));
            }
        }
    }
    return assets;
}

JwVideoCategory CatalogClient::parseMediatorCategoryJson(const QByteArray& json)
{
    JwVideoCategory category;
    const QJsonObject root =
        QJsonDocument::fromJson(json).object().value(QStringLiteral("category")).toObject();
    category.key = root.value(QStringLiteral("key")).toString();
    category.name = root.value(QStringLiteral("name")).toString();
    for (const QJsonValue& sub : root.value(QStringLiteral("subcategories")).toArray()) {
        const QString key = sub.toObject().value(QStringLiteral("key")).toString();
        if (!key.isEmpty()) {
            category.subcategories << key;
        }
    }
    for (const QJsonValue& mediaVal : root.value(QStringLiteral("media")).toArray()) {
        const QJsonObject mediaObj = mediaVal.toObject();
        JwVideo video;
        video.key = mediaObj.value(QStringLiteral("naturalKey")).toString().trimmed();
        video.title = mediaObj.value(QStringLiteral("title")).toString().trimmed();
        video.firstPublished = mediaObj.value(QStringLiteral("firstPublished")).toString();
        video.durationSeconds = mediaObj.value(QStringLiteral("duration")).toDouble(0.0);
        video.imageUrl = bestImageUrl(mediaObj.value(QStringLiteral("images")).toObject());
        for (const QJsonValue& fileVal : mediaObj.value(QStringLiteral("files")).toArray()) {
            CatalogMediaAsset asset = assetFromMediatorFile(
                fileVal.toObject(), video.title, video.durationSeconds, video.key, video.imageUrl);
            if (!asset.url.isEmpty()) {
                video.files.push_back(std::move(asset));
            }
        }
        if (!video.key.isEmpty() && !video.files.empty()) {
            category.videos.push_back(std::move(video));
        }
    }
    return category;
}

std::vector<CatalogCongregationResult>
CatalogClient::parseCongregationSearchJson(const QByteArray& json)
{
    std::vector<CatalogCongregationResult> results;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    QJsonArray items;
    if (doc.isArray()) {
        items = doc.array();
    } else if (doc.isObject()) {
        const QJsonObject root = doc.object();
        if (root.contains(QStringLiteral("geoLocationList"))) {
            items = root.value(QStringLiteral("geoLocationList")).toArray();
        } else if (root.contains(QStringLiteral("features"))) {
            items = root.value(QStringLiteral("features")).toArray();
        } else if (root.contains(QStringLiteral("congregations"))) {
            items = root.value(QStringLiteral("congregations")).toArray();
        }
    }

    for (const QJsonValue& val : items) {
        if (!val.isObject()) {
            continue;
        }
        QJsonObject obj = val.toObject();
        if (obj.contains(QStringLiteral("properties")) &&
            obj.value(QStringLiteral("properties")).isObject()) {
            obj = obj.value(QStringLiteral("properties")).toObject();
        }
        const QString name = obj.value(QStringLiteral("orgName"))
                                 .toString(obj.value(QStringLiteral("name")).toString())
                                 .trimmed();
        if (name.isEmpty()) {
            continue;
        }
        CatalogCongregationResult item;
        item.orgName = name;
        item.address = obj.value(QStringLiteral("address")).toString().trimmed();
        item.languageCode =
            obj.value(QStringLiteral("lang"))
                .toString(obj.value(QStringLiteral("languageCode")).toString(QStringLiteral("E")))
                .trimmed();
        const QJsonObject sched = obj.contains(QStringLiteral("schedule"))
                                      ? obj.value(QStringLiteral("schedule")).toObject()
                                      : obj.value(QStringLiteral("currentSchedule")).toObject();
        const QJsonObject mid = sched.value(QStringLiteral("midweek")).toObject();
        const QJsonObject end = sched.value(QStringLiteral("weekend")).toObject();
        item.midweekDay = parseDayOfWeekValue(mid.value(QStringLiteral("weekday")), 3);
        item.midweekTime =
            mid.value(QStringLiteral("time")).toString(QStringLiteral("19:30")).trimmed();
        item.weekendDay = parseDayOfWeekValue(end.value(QStringLiteral("weekday")), 7);
        item.weekendTime =
            end.value(QStringLiteral("time")).toString(QStringLiteral("09:30")).trimmed();
        results.push_back(std::move(item));
    }
    return results;
}

template <typename Callback>
void CatalogClient::fetchWithEtagCache(const QUrl& endpoint, Callback&& onResult,
                                       std::function<void(const QString&)> onError)
{
    if (!endpoint.isValid()) {
        if (onError) {
            onError(QStringLiteral("Invalid catalog endpoint URL"));
            return;
        }
        emit requestFailed(endpoint.toString(), QStringLiteral("Invalid catalog endpoint URL"));
        return;
    }

    QString etag;
    QString lastModified;
    QString cachedBodyPath;
    if (m_database != nullptr) {
        if (const std::optional<DownloadCacheRecord> cached =
                m_database->findDownloadCache(endpoint.toString());
            cached.has_value() && !cached->localPath.isEmpty() &&
            QFileInfo::exists(cached->localPath)) {
            etag = cached->etag;
            lastModified = cached->lastModified;
            cachedBodyPath = cached->localPath;
        }
    }

    QNetworkReply* reply =
        m_network->accessManager()->get(m_network->buildRequest(endpoint, etag, lastModified));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, endpoint, cachedBodyPath, handler = std::forward<Callback>(onResult),
             onError]() {
                const HttpCacheHeaders headers = NetworkManager::extractCacheHeaders(reply);
                const QByteArray body = reply->readAll();
                const QNetworkReply::NetworkError netErr = reply->error();
                const QString errText = reply->errorString();
                reply->deleteLater();

                if (headers.notModified) {
                    QByteArray cachedJson;
                    if (!cachedBodyPath.isEmpty()) {
                        QFile cachedFile(cachedBodyPath);
                        if (cachedFile.open(QIODevice::ReadOnly)) {
                            cachedJson = cachedFile.readAll();
                        }
                    }
                    handler(cachedJson, true);
                    return;
                }
                if (netErr != QNetworkReply::NoError || headers.statusCode >= 400) {
                    if (onError) {
                        onError(errText);
                        return;
                    }
                    emit requestFailed(endpoint.toString(), errText);
                    return;
                }
                if (m_database != nullptr) {
                    const QString cacheDir =
                        SystemUtils::standardCacheDir() + QStringLiteral("/catalog");
                    QDir().mkpath(cacheDir);
                    const QString cachePath = QDir(cacheDir).filePath(
                        SystemUtils::sha256Hex(endpoint.toString().toUtf8()) +
                        QStringLiteral(".json"));
                    QSaveFile saveFile(cachePath);
                    bool saved = false;
                    if (saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        saveFile.write(body);
                        saved = saveFile.commit();
                    }

                    DownloadCacheRecord rec;
                    rec.url = endpoint.toString();
                    rec.etag = headers.etag;
                    rec.lastModified = headers.lastModified;
                    rec.localPath = saved ? cachePath : QString{};
                    (void)m_database->upsertDownloadCache(rec);
                }
                handler(body, false);
            });
}

void CatalogClient::fetchLanguages(const QUrl& endpoint)
{
    fetchWithEtagCache(endpoint, [this](const QByteArray& body, bool fromCache) {
        if (body.isEmpty() && fromCache) {
            emit languagesLoaded({}, true);
            return;
        }
        emit languagesLoaded(parseLanguagesJson(body), fromCache);
    });
}

void CatalogClient::fetchPubMediaLinks(const QUrl& endpoint, const QString& preferredLangCode)
{
    fetchWithEtagCache(endpoint, [this, preferredLangCode](const QByteArray& body, bool fromCache) {
        if (body.isEmpty() && fromCache) {
            emit pubMediaAssetsLoaded({}, true);
            return;
        }
        emit pubMediaAssetsLoaded(parsePubMediaLinksJson(body, preferredLangCode), fromCache);
    });
}

void CatalogClient::fetchMediatorMedia(const QUrl& endpoint, const QString& pubMediaKey)
{
    fetchWithEtagCache(endpoint, [this, pubMediaKey](const QByteArray& body, bool fromCache) {
        if (body.isEmpty() && fromCache) {
            emit mediatorAssetsLoaded(pubMediaKey, {}, true);
            return;
        }
        emit mediatorAssetsLoaded(pubMediaKey, parseMediatorMediaJson(body, pubMediaKey),
                                  fromCache);
    });
}

void CatalogClient::requestPubMediaLinks(const QUrl& endpoint, const QString& preferredLangCode,
                                         QObject* context, AssetsCallback callback)
{
    const QPointer<QObject> guard(context);
    fetchWithEtagCache(
        endpoint,
        [guard, preferredLangCode, callback](const QByteArray& body, bool) {
            if (guard.isNull()) {
                return;
            }
            callback(!body.isEmpty(), body.isEmpty()
                                          ? std::vector<CatalogMediaAsset>{}
                                          : parsePubMediaLinksJson(body, preferredLangCode));
        },
        [guard, callback](const QString&) {
            if (!guard.isNull()) {
                callback(false, {});
            }
        });
}

void CatalogClient::requestMediatorCategory(const QUrl& endpoint, QObject* context,
                                            CategoryCallback callback)
{
    const QPointer<QObject> guard(context);
    fetchWithEtagCache(
        endpoint,
        [guard, callback](const QByteArray& body, bool) {
            if (guard.isNull()) {
                return;
            }
            callback(!body.isEmpty(),
                     body.isEmpty() ? JwVideoCategory{} : parseMediatorCategoryJson(body));
        },
        [guard, callback](const QString&) {
            if (!guard.isNull()) {
                callback(false, {});
            }
        });
}

void CatalogClient::searchCongregations(const QString& queryText)
{
    fetchWithEtagCache(congregationSearchUrl(queryText),
                       [this](const QByteArray& body, bool fromCache) {
                           if (body.isEmpty() && fromCache) {
                               emit congregationsLoaded({}, true);
                               return;
                           }
                           emit congregationsLoaded(parseCongregationSearchJson(body), fromCache);
                       });
}

} // namespace kmedia
