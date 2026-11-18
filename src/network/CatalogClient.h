#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>
#include <vector>

namespace kmedia {

class DatabaseManager;
class NetworkManager;

struct CatalogLanguage
{
    QString code;
    QString symbol;
    QString name;
    QString vernacularName;
    bool isSignLanguage{false};
    bool isRtl{false};
};

struct CatalogMediaAsset
{
    QString url;
    QString sha256;
    qint64 fileSize{0};
    QString mimeType;
    QString label;
    QString title;
    QString fileFormat;
    QString keySymbol;
    int issueTagNumber{0};
    int track{0};
    double durationSeconds{0.0};
    QString modifiedDatetime;
};

/**
 * Polite catalog API client for JW language lists and pub-media endpoints.
 * Builds standardized URLs, parses JSON payloads, and honors HTTP ETag caching.
 */
class CatalogClient : public QObject
{
    Q_OBJECT

public:
    explicit CatalogClient(NetworkManager* networkManager = nullptr,
                           DatabaseManager* databaseManager = nullptr, QObject* parent = nullptr);
    ~CatalogClient() override = default;

    Q_DISABLE_COPY_MOVE(CatalogClient)

    void setDatabaseManager(DatabaseManager* databaseManager);

    [[nodiscard]] static QUrl
    languagesEndpointUrl(const QString& baseDomain = QStringLiteral("jw.org"));

    [[nodiscard]] static QUrl
    pubMediaLinksUrl(const QString& pubSymbol, const QString& langCode, int issueTagNumber = 0,
                     const QString& fileFormat = {}, int track = 0, const QString& docId = {},
                     const QString& baseUrl =
                         QStringLiteral("https://b.jw-cdn.org/apis/pub-media/GETPUBMEDIALINKS"));

    [[nodiscard]] static std::vector<CatalogLanguage> parseLanguagesJson(const QByteArray& json);
    [[nodiscard]] static std::vector<CatalogMediaAsset>
    parsePubMediaLinksJson(const QByteArray& json, const QString& preferredLangCode = {});

    void fetchLanguages(const QUrl& endpoint = languagesEndpointUrl());
    void fetchPubMediaLinks(const QUrl& endpoint, const QString& preferredLangCode = {});

signals:
    void languagesLoaded(const std::vector<kmedia::CatalogLanguage>& languages, bool fromCache);
    void pubMediaAssetsLoaded(const std::vector<kmedia::CatalogMediaAsset>& assets, bool fromCache);
    void requestFailed(const QString& url, const QString& errorMessage);

private:
    template <typename Callback>
    void fetchWithEtagCache(const QUrl& endpoint, Callback&& onResult);

    std::unique_ptr<NetworkManager> m_ownedNetwork;
    NetworkManager* m_network{nullptr};
    DatabaseManager* m_database{nullptr};
};

} // namespace kmedia
