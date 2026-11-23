#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>
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

/** Where a verse is read in a Bible chapter recording. */
struct BibleVerseMarker
{
    int verse{0};
    qint64 startMs{0};
    qint64 durationMs{0};
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
    QString pubMediaId;
    bool streamOnly{false};
    QString imageUrl;
    // The subtitles JW.org publishes for a video (VTT), when it has some.
    QString subtitleUrl;
    QString subtitleChecksum;
    std::vector<BibleVerseMarker> verseMarkers;
};

/** A book of the Bible recording: its name in the language asked for, and its chapters. */
struct JwBibleBook
{
    int number{0};
    QString name;
    std::vector<CatalogMediaAsset> chapters; // every file of every chapter, `track` = chapter
};

/** A video of the JW.org video library, with every file it comes in. */
struct JwVideo
{
    QString key;
    QString title;
    QString firstPublished;
    double durationSeconds{0.0};
    QString imageUrl;
    std::vector<CatalogMediaAsset> files;
};

/** One category of the JW.org video library: its subcategories and the videos it lists. */
struct JwVideoCategory
{
    QString key;
    QString name;
    QStringList subcategories;
    std::vector<JwVideo> videos;
};

struct CatalogCongregationResult
{
    QString orgName;
    QString address;
    QString languageCode;
    int midweekDay{3};
    QString midweekTime{QStringLiteral("19:30")};
    int weekendDay{7};
    QString weekendTime{QStringLiteral("09:30")};
};

/**
 * Polite catalog API client for JW language lists, pub-media, Mediator, and hub congregation
 * endpoints. Builds standardized URLs, parses JSON payloads, and honors HTTP ETag caching.
 */
class CatalogClient : public QObject
{
    Q_OBJECT

public:
    static constexpr double kLongMediaDurationSeconds = 2100.0;

    explicit CatalogClient(NetworkManager* networkManager = nullptr,
                           DatabaseManager* databaseManager = nullptr, QObject* parent = nullptr);
    ~CatalogClient() override;

    Q_DISABLE_COPY_MOVE(CatalogClient)

    void setDatabaseManager(DatabaseManager* databaseManager);

    [[nodiscard]] static QUrl
    languagesEndpointUrl(const QString& baseDomain = QStringLiteral("jw.org"));

    [[nodiscard]] static QUrl hubLanguagesEndpointUrl();

    [[nodiscard]] static QUrl
    pubMediaLinksUrl(const QString& pubSymbol, const QString& langCode, int issueTagNumber = 0,
                     const QString& fileFormat = {}, int track = 0, const QString& docId = {},
                     const QString& baseUrl =
                         QStringLiteral("https://b.jw-cdn.org/apis/pub-media/GETPUBMEDIALINKS"));

    [[nodiscard]] static QUrl
    mediatorMediaUrl(const QString& pubMediaKey, const QString& langCode = QStringLiteral("E"),
                     const QString& baseUrl =
                         QStringLiteral("https://b.jw-cdn.org/apis/mediator/v1/media-items"));

    [[nodiscard]] static QUrl
    mediatorCategoryUrl(const QString& langCode, const QString& categoryKey, bool subcategoriesOnly,
                        const QString& baseUrl =
                            QStringLiteral("https://b.jw-cdn.org/apis/mediator/v1/categories"));

    // The yeartext of a year in a language, from the Watchtower Online Library.
    // Where a video's subtitles are kept: next to it, with its name, so the player loads them.
    [[nodiscard]] static QString subtitlePathFor(const QString& videoPath);

    [[nodiscard]] static QUrl yeartextUrl(const QString& langCode, int year);
    // Plain text, one line per paragraph of the yeartext.
    [[nodiscard]] static QString parseYeartextJson(const QByteArray& json);
    using TextCallback = std::function<void(bool ok, const QString& text)>;
    void requestYeartext(const QString& langCode, int year, QObject* context,
                         TextCallback callback);

    // The recording of one Bible book (`nwt`): MP3, or MP4 for sign languages.
    [[nodiscard]] static QUrl bibleBookUrl(const QString& langCode, int bookNumber,
                                           const QString& fileFormat);
    [[nodiscard]] static JwBibleBook parseBibleBookJson(const QByteArray& json,
                                                        const QString& langCode);

    [[nodiscard]] static QString buildPubMediaKey(const QString& pubSymbol, int issueTagNumber,
                                                  qint64 docId = 0, int track = 0,
                                                  bool isVideo = true);

    [[nodiscard]] static QString pinyinSongFilename(int songNumber, int height = 720);

    [[nodiscard]] static QUrl congregationSearchUrl(
        const QString& queryText,
        const QString& baseUrl = QStringLiteral("https://hub.jw.org/meetings/api/meeting-search"));

    [[nodiscard]] static std::vector<CatalogLanguage> parseLanguagesJson(const QByteArray& json);
    [[nodiscard]] static std::vector<CatalogMediaAsset>
    parsePubMediaLinksJson(const QByteArray& json, const QString& preferredLangCode = {});
    [[nodiscard]] static std::vector<CatalogMediaAsset>
    parseMediatorMediaJson(const QByteArray& json, const QString& pubMediaKey = {});
    [[nodiscard]] static JwVideoCategory parseMediatorCategoryJson(const QByteArray& json);
    [[nodiscard]] static std::vector<CatalogCongregationResult>
    parseCongregationSearchJson(const QByteArray& json);

    void fetchLanguages(const QUrl& endpoint = languagesEndpointUrl());
    void fetchPubMediaLinks(const QUrl& endpoint, const QString& preferredLangCode = {});
    void fetchMediatorMedia(const QUrl& endpoint, const QString& pubMediaKey = {});
    void searchCongregations(const QString& queryText);

    using AssetsCallback =
        std::function<void(bool ok, const std::vector<kmedia::CatalogMediaAsset>& assets)>;
    /**
     * Same request as fetchPubMediaLinks, answered only through `callback` (never through the
     * shared signals) and dropped when `context` is destroyed first.
     */
    void requestPubMediaLinks(const QUrl& endpoint, const QString& preferredLangCode,
                              QObject* context, AssetsCallback callback);

    using BibleBookCallback = std::function<void(bool ok, const kmedia::JwBibleBook& book)>;
    void requestBibleBook(const QUrl& endpoint, const QString& langCode, QObject* context,
                          BibleBookCallback callback);

    using CategoryCallback = std::function<void(bool ok, const kmedia::JwVideoCategory& category)>;
    void requestMediatorCategory(const QUrl& endpoint, QObject* context, CategoryCallback callback);

signals:
    void languagesLoaded(const std::vector<kmedia::CatalogLanguage>& languages, bool fromCache);
    void pubMediaAssetsLoaded(const std::vector<kmedia::CatalogMediaAsset>& assets, bool fromCache);
    void mediatorAssetsLoaded(const QString& pubMediaKey,
                              const std::vector<kmedia::CatalogMediaAsset>& assets, bool fromCache);
    void congregationsLoaded(const std::vector<kmedia::CatalogCongregationResult>& results,
                             bool fromCache);
    void requestFailed(const QString& url, const QString& errorMessage);

private:
    template <typename Callback>
    void fetchWithEtagCache(const QUrl& endpoint, Callback&& onResult,
                            std::function<void(const QString&)> onError = {});

    std::unique_ptr<NetworkManager> m_ownedNetwork;
    NetworkManager* m_network{nullptr};
    DatabaseManager* m_database{nullptr};
};

} // namespace kmedia
