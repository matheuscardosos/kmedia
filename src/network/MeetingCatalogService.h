#pragma once

#include "network/CatalogClient.h"
#include "parsers/JwpubParser.h"

#include <QDate>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVariantList>

#include <vector>

namespace kmedia {

class DownloadService;
class PlaylistModel;

/**
 * Coordinates language catalog sync, weekly meeting date resolution (MWB and Watchtower),
 * deduplicated pub-media asset selection, download queueing, and automatic playlist population.
 */
class MeetingCatalogService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged FINAL)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged FINAL)
    Q_PROPERTY(QString selectedLanguageCode READ selectedLanguageCode WRITE setSelectedLanguageCode
                   NOTIFY selectedLanguageChanged FINAL)
    Q_PROPERTY(QString selectedLanguageLabel READ selectedLanguageLabel NOTIFY
                   selectedLanguageChanged FINAL)
    Q_PROPERTY(QString fallbackLanguageCode READ fallbackLanguageCode WRITE setFallbackLanguageCode
                   NOTIFY fallbackLanguageChanged FINAL)
    Q_PROPERTY(QVariantList languages READ languagesVariant NOTIFY languagesChanged FINAL)
    Q_PROPERTY(QDate targetWeekMonday READ targetWeekMonday NOTIFY weekChanged FINAL)
    Q_PROPERTY(QDate targetDate READ targetDate NOTIFY weekChanged FINAL)
    Q_PROPERTY(QString targetMeetingKind READ targetMeetingKind NOTIFY weekChanged FINAL)
    Q_PROPERTY(QString weekLabel READ weekLabel NOTIFY weekChanged FINAL)
    Q_PROPERTY(QString weekEventType READ weekEventType NOTIFY weekChanged FINAL)
    Q_PROPERTY(QString weekEventLabel READ weekEventLabel NOTIFY weekChanged FINAL)
    Q_PROPERTY(int midweekIssueTag READ midweekIssueTag NOTIFY weekChanged FINAL)
    Q_PROPERTY(int watchtowerIssueTag READ watchtowerIssueTag NOTIFY weekChanged FINAL)
    Q_PROPERTY(QVariantList assets READ assetsVariant NOTIFY assetsChanged FINAL)
    Q_PROPERTY(
        QVariantList congregationResults READ congregationResults NOTIFY congregationsChanged FINAL)
    Q_PROPERTY(
        bool includePrinted READ includePrinted WRITE setIncludePrinted NOTIFY filtersChanged FINAL)
    Q_PROPERTY(bool excludeFootnotes READ excludeFootnotes WRITE setExcludeFootnotes NOTIFY
                   filtersChanged FINAL)
    Q_PROPERTY(bool excludeWtParagraphVideos READ excludeWtParagraphVideos WRITE
                   setExcludeWtParagraphVideos NOTIFY filtersChanged FINAL)
    Q_PROPERTY(bool excludeTh READ excludeTh WRITE setExcludeTh NOTIFY filtersChanged FINAL)
    Q_PROPERTY(
        bool excludeCbsPubs READ excludeCbsPubs WRITE setExcludeCbsPubs NOTIFY filtersChanged FINAL)
    Q_PROPERTY(bool autoLoadWeekMedia READ autoLoadWeekMedia WRITE setAutoLoadWeekMedia NOTIFY
                   filtersChanged FINAL)

public:
    explicit MeetingCatalogService(CatalogClient* catalogClient, DownloadService* downloadService,
                                   PlaylistModel* playlistModel = nullptr,
                                   QObject* parent = nullptr);
    ~MeetingCatalogService() override = default;

    Q_DISABLE_COPY_MOVE(MeetingCatalogService)

    void setStorageRoot(const QString& storageRoot);
    void setPlaylistModel(PlaylistModel* playlistModel);
    void setSpecialWeekEvents(const QHash<QString, QPair<QString, QString>>& eventsByMonday);

    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] QString statusMessage() const;

    [[nodiscard]] QString selectedLanguageCode() const;
    void setSelectedLanguageCode(const QString& code);
    [[nodiscard]] QString selectedLanguageLabel() const;
    [[nodiscard]] bool isSignLanguage(const QString& code) const;
    [[nodiscard]] QString fallbackLanguageCode() const;
    void setFallbackLanguageCode(const QString& code);

    [[nodiscard]] QVariantList languagesVariant() const;
    [[nodiscard]] QDate targetWeekMonday() const;
    [[nodiscard]] QDate targetDate() const;
    [[nodiscard]] QString targetMeetingKind() const;
    [[nodiscard]] QString weekLabel() const;
    [[nodiscard]] QString weekEventType() const;
    [[nodiscard]] QString weekEventLabel() const;
    [[nodiscard]] int midweekIssueTag() const;
    [[nodiscard]] int watchtowerIssueTag() const;
    [[nodiscard]] QVariantList assetsVariant() const;
    [[nodiscard]] QVariantList congregationResults() const;

    [[nodiscard]] bool includePrinted() const;
    void setIncludePrinted(bool includePrinted);
    [[nodiscard]] bool excludeFootnotes() const;
    void setExcludeFootnotes(bool excludeFootnotes);
    [[nodiscard]] bool excludeWtParagraphVideos() const;
    void setExcludeWtParagraphVideos(bool exclude);
    [[nodiscard]] bool excludeTh() const;
    void setExcludeTh(bool excludeTh);
    [[nodiscard]] bool excludeCbsPubs() const;
    void setExcludeCbsPubs(bool excludeCbsPubs);
    [[nodiscard]] bool autoLoadWeekMedia() const;
    void setAutoLoadWeekMedia(bool autoLoad);

    [[nodiscard]] static QDate weekMondayForDate(const QDate& date);
    [[nodiscard]] static int midweekIssueTagForDate(const QDate& date);
    [[nodiscard]] static int watchtowerIssueTagForDate(const QDate& date);
    [[nodiscard]] static std::vector<int> watchtowerCandidateIssueTags(const QDate& date);
    [[nodiscard]] static QString formatWeekRange(const QDate& weekMonday);
    [[nodiscard]] static QString inferMeetingSection(const QString& pubSymbol, const QString& title,
                                                     const QString& label);
    [[nodiscard]] static int videoResolutionScore(const CatalogMediaAsset& asset);
    [[nodiscard]] static std::vector<CatalogMediaAsset>
    deduplicateAssets(const std::vector<CatalogMediaAsset>& rawAssets);

public slots:
    void refreshLanguages();
    void fetchWeekCatalog();
    void fetchPublication(const QString& pubSymbol, int issueTag = 0,
                          const QString& fileFormat = {});
    void syncCurrentWeekToPlaylist();
    void syncDateToPlaylist(const QDate& date, const QString& meetingKind);
    void searchCongregations(const QString& queryText);
    bool applyCongregationResult(int index);
    QString downloadAssetAt(int index);
    void previousWeek();
    void nextWeek();
    void goToCurrentWeek();
    void setTargetWeekDate(const QDate& date);
    void setTargetMeetingContext(const QDate& date, const QString& meetingKind);

signals:
    void busyChanged();
    void statusChanged();
    void selectedLanguageChanged();
    void fallbackLanguageChanged();
    void languagesChanged();
    void weekChanged();
    void assetsChanged();
    void congregationsChanged();
    void filtersChanged();
    void congregationScheduleApplied(const QString& name, const QString& langCode, int midDay,
                                     const QString& midTime, int endDay, const QString& endTime);
    void weekSynced(int importedCount);
    void catalogError(const QString& message);

private slots:
    void onLanguagesLoaded(const std::vector<kmedia::CatalogLanguage>& languages, bool fromCache);
    void onPubMediaAssetsLoaded(const std::vector<kmedia::CatalogMediaAsset>& assets,
                                bool fromCache);
    void onMediatorAssetsLoaded(const QString& pubMediaKey,
                                const std::vector<kmedia::CatalogMediaAsset>& assets,
                                bool fromCache);
    void onCongregationsLoaded(const std::vector<kmedia::CatalogCongregationResult>& results,
                               bool fromCache);
    void onRequestFailed(const QString& url, const QString& errorMessage);
    void onDownloadFinished(const QString& taskId, bool success);

private:
    struct PendingDownloadTarget
    {
        QString localPath;
        QString sectionId;
        QString fileFormat;
        QString tagType;
        QString tagValue;
        QString groupKey;
        bool isCbs{false};
    };

    void seedDefaultLanguages();
    void setBusy(bool busy);
    void setStatusMessage(const QString& message);
    [[nodiscard]] QString localPathForAsset(const CatalogMediaAsset& asset) const;
    [[nodiscard]] JwpubWeekFilterOptions buildFilterOptions(const QString& pubSymbolHint) const;
    void enqueueMissingAutoSyncAssets();
    void resolveLinkedJwpubMedia(const QString& jwpubPath);
    void resolveSingleLinkedMedia(const QString& pubSymbol, int issueTag, qint64 docId, int track,
                                  const QString& title, const QString& sectionId,
                                  const QString& groupKey, const QString& tagType,
                                  const QString& tagValue, bool isCbs);
    void applyWeekEventSection();

    CatalogClient* m_catalogClient{nullptr};
    DownloadService* m_downloadService{nullptr};
    PlaylistModel* m_playlistModel{nullptr};

    QString m_storageRoot;
    QString m_selectedLanguageCode{QStringLiteral("E")};
    QString m_fallbackLanguageCode{QStringLiteral("E")};
    QDate m_targetWeekMonday;
    QDate m_targetDate;
    QString m_targetMeetingKind{QStringLiteral("midweek")};
    bool m_busy{false};
    bool m_autoSyncPending{false};
    int m_pendingCatalogRequests{0};
    int m_autoImportedCount{0};
    QString m_statusMessage;
    bool m_includePrinted{false};
    bool m_excludeFootnotes{true};
    bool m_excludeWtParagraphVideos{false};
    bool m_excludeTh{false};
    bool m_excludeCbsPubs{false};
    bool m_autoLoadWeekMedia{true};

    std::vector<CatalogLanguage> m_languages;
    std::vector<CatalogMediaAsset> m_assets;
    std::vector<CatalogCongregationResult> m_congregationResults;
    QHash<QString, QString> m_assetTaskIds;
    QHash<QString, PendingDownloadTarget> m_pendingDownloads;
    QSet<QString> m_resolvedExternalKeys;
    QHash<QString, QString> m_mediatorFallbackTried;
    QHash<QString, PendingDownloadTarget> m_pendingStreamMeta;
    QHash<QString, QPair<QString, QString>> m_specialEventsByMonday;
};

} // namespace kmedia
