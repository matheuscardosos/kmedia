#pragma once

#include "network/CatalogClient.h"

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
    Q_PROPERTY(QVariantList languages READ languagesVariant NOTIFY languagesChanged FINAL)
    Q_PROPERTY(QDate targetWeekMonday READ targetWeekMonday NOTIFY weekChanged FINAL)
    Q_PROPERTY(QString weekLabel READ weekLabel NOTIFY weekChanged FINAL)
    Q_PROPERTY(QString weekEventType READ weekEventType NOTIFY weekChanged FINAL)
    Q_PROPERTY(QString weekEventLabel READ weekEventLabel NOTIFY weekChanged FINAL)
    Q_PROPERTY(int midweekIssueTag READ midweekIssueTag NOTIFY weekChanged FINAL)
    Q_PROPERTY(int watchtowerIssueTag READ watchtowerIssueTag NOTIFY weekChanged FINAL)
    Q_PROPERTY(QVariantList assets READ assetsVariant NOTIFY assetsChanged FINAL)

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

    [[nodiscard]] QVariantList languagesVariant() const;
    [[nodiscard]] QDate targetWeekMonday() const;
    [[nodiscard]] QString weekLabel() const;
    [[nodiscard]] QString weekEventType() const;
    [[nodiscard]] QString weekEventLabel() const;
    [[nodiscard]] int midweekIssueTag() const;
    [[nodiscard]] int watchtowerIssueTag() const;
    [[nodiscard]] QVariantList assetsVariant() const;

    [[nodiscard]] static QDate weekMondayForDate(const QDate& date);
    [[nodiscard]] static int midweekIssueTagForDate(const QDate& date);
    [[nodiscard]] static int watchtowerIssueTagForDate(const QDate& date);
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
    QString downloadAssetAt(int index);
    void previousWeek();
    void nextWeek();
    void goToCurrentWeek();
    void setTargetWeekDate(const QDate& date);

signals:
    void busyChanged();
    void statusChanged();
    void selectedLanguageChanged();
    void languagesChanged();
    void weekChanged();
    void assetsChanged();
    void weekSynced(int importedCount);
    void catalogError(const QString& message);

private slots:
    void onLanguagesLoaded(const std::vector<kmedia::CatalogLanguage>& languages, bool fromCache);
    void onPubMediaAssetsLoaded(const std::vector<kmedia::CatalogMediaAsset>& assets,
                                bool fromCache);
    void onRequestFailed(const QString& url, const QString& errorMessage);
    void onDownloadFinished(const QString& taskId, bool success);

private:
    struct PendingDownloadTarget
    {
        QString localPath;
        QString sectionTitle;
        QString fileFormat;
    };

    void seedDefaultLanguages();
    void setBusy(bool busy);
    void setStatusMessage(const QString& message);
    [[nodiscard]] QString localPathForAsset(const CatalogMediaAsset& asset) const;
    void enqueueMissingAutoSyncAssets();
    void resolveLinkedJwpubMedia(const QString& jwpubPath);
    void applyWeekEventSection();

    CatalogClient* m_catalogClient{nullptr};
    DownloadService* m_downloadService{nullptr};
    PlaylistModel* m_playlistModel{nullptr};

    QString m_storageRoot;
    QString m_selectedLanguageCode{QStringLiteral("E")};
    QDate m_targetWeekMonday;
    bool m_busy{false};
    bool m_autoSyncPending{false};
    int m_pendingCatalogRequests{0};
    int m_autoImportedCount{0};
    QString m_statusMessage;

    std::vector<CatalogLanguage> m_languages;
    std::vector<CatalogMediaAsset> m_assets;
    QHash<QString, QString> m_assetTaskIds;
    QHash<QString, PendingDownloadTarget> m_pendingDownloads;
    QSet<QString> m_resolvedExternalKeys;
    QHash<QString, QPair<QString, QString>> m_specialEventsByMonday;
};

} // namespace kmedia
