#include "network/MeetingCatalogService.h"

#include "core/SystemUtils.h"
#include "models/PlaylistModel.h"
#include "network/DownloadService.h"
#include "parsers/JwpubParser.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

#include <algorithm>
#include <utility>

namespace kmedia {

namespace {

constexpr int kMaxSecondaryCatalogQueries = 20;

QString buildAssetKey(const CatalogMediaAsset& asset)
{
    return QStringLiteral("%1|%2|%3|%4")
        .arg(asset.keySymbol.toLower(), QString::number(asset.issueTagNumber),
             QString::number(asset.track), asset.fileFormat.toUpper());
}

QString formatSecondsDuration(double durationSeconds)
{
    if (durationSeconds <= 0.0) {
        return {};
    }
    return SystemUtils::formatDurationMs(static_cast<qint64>(durationSeconds * 1000.0));
}

QString normalizeFileExtension(const CatalogMediaAsset& asset)
{
    const QString fmt = asset.fileFormat.trimmed().toLower();
    if (fmt == QStringLiteral("jwpub")) {
        return QStringLiteral("jwpub");
    }
    if (fmt == QStringLiteral("mp4")) {
        return QStringLiteral("mp4");
    }
    if (fmt == QStringLiteral("mp3")) {
        return QStringLiteral("mp3");
    }
    if (fmt == QStringLiteral("pdf")) {
        return QStringLiteral("pdf");
    }
    const QString suffix = QFileInfo(QUrl(asset.url).path()).suffix().toLower();
    return suffix.isEmpty() ? QStringLiteral("bin") : suffix;
}

} // namespace

MeetingCatalogService::MeetingCatalogService(CatalogClient* catalogClient,
                                             DownloadService* downloadService,
                                             PlaylistModel* playlistModel, QObject* parent)
    : QObject(parent), m_catalogClient(catalogClient), m_downloadService(downloadService),
      m_playlistModel(playlistModel), m_targetWeekMonday(weekMondayForDate(QDate::currentDate()))
{
    seedDefaultLanguages();

    if (m_catalogClient != nullptr) {
        connect(m_catalogClient, &CatalogClient::languagesLoaded, this,
                &MeetingCatalogService::onLanguagesLoaded);
        connect(m_catalogClient, &CatalogClient::pubMediaAssetsLoaded, this,
                &MeetingCatalogService::onPubMediaAssetsLoaded);
        connect(m_catalogClient, &CatalogClient::requestFailed, this,
                &MeetingCatalogService::onRequestFailed);
    }

    if (m_downloadService != nullptr) {
        connect(m_downloadService, &DownloadService::taskFinished, this,
                &MeetingCatalogService::onDownloadFinished);
        connect(m_downloadService, &DownloadService::taskStateChanged, this,
                [this](const QString&, DownloadService::Status) { emit assetsChanged(); });
    }
}

void MeetingCatalogService::setStorageRoot(const QString& storageRoot)
{
    m_storageRoot = storageRoot.trimmed();
    emit assetsChanged();
}

void MeetingCatalogService::setPlaylistModel(PlaylistModel* playlistModel)
{
    m_playlistModel = playlistModel;
}

bool MeetingCatalogService::isBusy() const
{
    return m_busy;
}

QString MeetingCatalogService::statusMessage() const
{
    return m_statusMessage;
}

QString MeetingCatalogService::selectedLanguageCode() const
{
    return m_selectedLanguageCode;
}

void MeetingCatalogService::setSelectedLanguageCode(const QString& code)
{
    const QString clean = code.trimmed().isEmpty() ? QStringLiteral("E") : code.trimmed();
    if (m_selectedLanguageCode == clean) {
        return;
    }
    m_selectedLanguageCode = clean;
    emit selectedLanguageChanged();
}

QString MeetingCatalogService::selectedLanguageLabel() const
{
    for (const CatalogLanguage& lang : m_languages) {
        if (lang.code.compare(m_selectedLanguageCode, Qt::CaseInsensitive) == 0) {
            const QString primary = lang.vernacularName.isEmpty() ? lang.name : lang.vernacularName;
            return QStringLiteral("%1 (%2)").arg(primary, lang.code);
        }
    }
    return m_selectedLanguageCode;
}

QVariantList MeetingCatalogService::languagesVariant() const
{
    QVariantList list;
    list.reserve(static_cast<qsizetype>(m_languages.size()));
    for (const CatalogLanguage& lang : m_languages) {
        QVariantMap map;
        map.insert(QStringLiteral("code"), lang.code);
        map.insert(QStringLiteral("symbol"), lang.symbol);
        map.insert(QStringLiteral("name"), lang.name);
        map.insert(QStringLiteral("vernacularName"), lang.vernacularName);
        const QString display = lang.vernacularName.isEmpty() ? lang.name : lang.vernacularName;
        map.insert(QStringLiteral("label"), QStringLiteral("%1 (%2)").arg(display, lang.code));
        list.append(map);
    }
    return list;
}

QDate MeetingCatalogService::targetWeekMonday() const
{
    return m_targetWeekMonday;
}

QString MeetingCatalogService::weekLabel() const
{
    return formatWeekRange(m_targetWeekMonday);
}

int MeetingCatalogService::midweekIssueTag() const
{
    return midweekIssueTagForDate(m_targetWeekMonday);
}

int MeetingCatalogService::watchtowerIssueTag() const
{
    return watchtowerIssueTagForDate(m_targetWeekMonday);
}

QVariantList MeetingCatalogService::assetsVariant() const
{
    QVariantList list;
    list.reserve(static_cast<qsizetype>(m_assets.size()));
    for (int i = 0; i < static_cast<int>(m_assets.size()); ++i) {
        const CatalogMediaAsset& asset = m_assets.at(static_cast<size_t>(i));
        const QString path = localPathForAsset(asset);
        const QFileInfo fi(path);
        const bool existsOnDisk = fi.exists() && fi.size() > 0;
        const QString taskId = m_assetTaskIds.value(asset.url);

        QVariantMap map;
        map.insert(QStringLiteral("index"), i);
        map.insert(QStringLiteral("title"),
                   asset.title.isEmpty()
                       ? QStringLiteral("%1 (%2)").arg(asset.keySymbol.toUpper(), asset.fileFormat)
                       : asset.title);
        map.insert(QStringLiteral("label"), asset.label);
        map.insert(QStringLiteral("pubSymbol"), asset.keySymbol.toUpper());
        map.insert(QStringLiteral("issueTag"), asset.issueTagNumber);
        map.insert(QStringLiteral("fileFormat"), asset.fileFormat.toUpper());
        map.insert(QStringLiteral("sizeText"), asset.fileSize > 0
                                                   ? SystemUtils::formatFileSize(asset.fileSize)
                                                   : QStringLiteral("--"));
        map.insert(QStringLiteral("durationText"), formatSecondsDuration(asset.durationSeconds));
        map.insert(QStringLiteral("url"), asset.url);
        map.insert(QStringLiteral("sha256"), asset.sha256);
        map.insert(QStringLiteral("localPath"), path);
        map.insert(QStringLiteral("downloaded"), existsOnDisk);
        map.insert(QStringLiteral("taskId"), taskId);
        map.insert(QStringLiteral("sectionTitle"),
                   inferMeetingSection(asset.keySymbol, asset.title, asset.label));
        list.append(map);
    }
    return list;
}

QDate MeetingCatalogService::weekMondayForDate(const QDate& date)
{
    const QDate effective = date.isValid() ? date : QDate::currentDate();
    return effective.addDays(-(effective.dayOfWeek() - 1));
}

int MeetingCatalogService::midweekIssueTagForDate(const QDate& date)
{
    const QDate monday = weekMondayForDate(date);
    const int month = monday.month();
    const int oddMonth = (month % 2 == 1) ? month : (month - 1);
    return (monday.year() * 100) + oddMonth;
}

int MeetingCatalogService::watchtowerIssueTagForDate(const QDate& date)
{
    const QDate issueMonth = weekMondayForDate(date).addMonths(-2);
    return (issueMonth.year() * 100) + issueMonth.month();
}

QString MeetingCatalogService::formatWeekRange(const QDate& weekMonday)
{
    const QDate start = weekMondayForDate(weekMonday);
    const QDate end = start.addDays(6);
    return QStringLiteral("%1 - %2").arg(start.toString(Qt::ISODate), end.toString(Qt::ISODate));
}

QString MeetingCatalogService::inferMeetingSection(const QString& pubSymbol, const QString& title,
                                                   const QString& label)
{
    const QString sym = pubSymbol.trimmed().toLower();
    const QString text = (title + QLatin1Char(' ') + label).toLower();

    if (text.contains(QStringLiteral("treasure")) || text.contains(QStringLiteral("tesouro")) ||
        text.contains(QStringLiteral("gems")) || text.contains(QStringLiteral("joias"))) {
        return QStringLiteral("Treasures From God's Word");
    }
    if (text.contains(QStringLiteral("apply yourself")) ||
        text.contains(QStringLiteral("ministério")) || text.contains(QStringLiteral("ministry"))) {
        return QStringLiteral("Apply Yourself to the Field Ministry");
    }
    if (text.contains(QStringLiteral("living as christians")) ||
        text.contains(QStringLiteral("vida cristã"))) {
        return QStringLiteral("Living as Christians");
    }
    if (text.contains(QStringLiteral("watchtower")) || text.contains(QStringLiteral("sentinela")) ||
        sym == QStringLiteral("w")) {
        return QStringLiteral("Watchtower Study");
    }
    if (sym == QStringLiteral("s-34") || text.contains(QStringLiteral("public talk")) ||
        text.contains(QStringLiteral("discurso"))) {
        return QStringLiteral("Public Talk");
    }
    if (sym == QStringLiteral("mwb") || sym == QStringLiteral("mwbv")) {
        return QStringLiteral("Midweek Meeting");
    }
    return QStringLiteral("Meeting Media");
}

int MeetingCatalogService::videoResolutionScore(const CatalogMediaAsset& asset)
{
    const QString text = (asset.label + QLatin1Char(' ') + asset.url).toLower();
    if (text.contains(QStringLiteral("720p"))) {
        return 50;
    }
    if (text.contains(QStringLiteral("480p"))) {
        return 40;
    }
    if (text.contains(QStringLiteral("360p"))) {
        return 30;
    }
    if (text.contains(QStringLiteral("1080p"))) {
        return 25;
    }
    if (text.contains(QStringLiteral("240p"))) {
        return 10;
    }
    return 15;
}

std::vector<CatalogMediaAsset>
MeetingCatalogService::deduplicateAssets(const std::vector<CatalogMediaAsset>& rawAssets)
{
    std::vector<CatalogMediaAsset> result;
    QHash<QString, size_t> slotIndexByKey;

    for (const CatalogMediaAsset& item : rawAssets) {
        const QString fmt = item.fileFormat.trimmed().toUpper();
        if (fmt == QStringLiteral("MP4")) {
            const QString logicalGroup =
                (item.track > 0)
                    ? QStringLiteral("%1|%2|track:%3")
                          .arg(item.keySymbol.toLower(), QString::number(item.issueTagNumber),
                               QString::number(item.track))
                    : QStringLiteral("%1|%2|title:%3")
                          .arg(item.keySymbol.toLower(), QString::number(item.issueTagNumber),
                               item.title.trimmed().toLower());

            const auto existing = slotIndexByKey.constFind(logicalGroup);
            if (existing != slotIndexByKey.constEnd()) {
                const size_t idx = existing.value();
                if (videoResolutionScore(item) > videoResolutionScore(result.at(idx))) {
                    result[idx] = item;
                }
                continue;
            }
            slotIndexByKey.insert(logicalGroup, result.size());
            result.push_back(item);
            continue;
        }

        const QString exactKey = buildAssetKey(item) + QLatin1Char('|') + item.url;
        if (!slotIndexByKey.contains(exactKey)) {
            slotIndexByKey.insert(exactKey, result.size());
            result.push_back(item);
        }
    }

    return result;
}

void MeetingCatalogService::refreshLanguages()
{
    if (m_catalogClient == nullptr) {
        return;
    }
    setStatusMessage(QStringLiteral("Updating JW language catalog..."));
    m_catalogClient->fetchLanguages();
}

void MeetingCatalogService::fetchWeekCatalog()
{
    if (m_catalogClient == nullptr) {
        return;
    }

    m_assets.clear();
    m_assetTaskIds.clear();
    emit assetsChanged();

    m_pendingCatalogRequests = 2;
    setBusy(true);
    setStatusMessage(
        QStringLiteral("Fetching week %1 (%2)...").arg(weekLabel(), m_selectedLanguageCode));

    m_catalogClient->fetchPubMediaLinks(CatalogClient::pubMediaLinksUrl(QStringLiteral("mwb"),
                                                                        m_selectedLanguageCode,
                                                                        midweekIssueTag()),
                                        m_selectedLanguageCode);

    m_catalogClient->fetchPubMediaLinks(CatalogClient::pubMediaLinksUrl(QStringLiteral("w"),
                                                                        m_selectedLanguageCode,
                                                                        watchtowerIssueTag()),
                                        m_selectedLanguageCode);
}

void MeetingCatalogService::fetchPublication(const QString& pubSymbol, int issueTag,
                                             const QString& fileFormat)
{
    const QString sym = pubSymbol.trimmed().toLower();
    if (m_catalogClient == nullptr || sym.isEmpty()) {
        return;
    }

    m_autoSyncPending = false;
    m_assets.clear();
    m_assetTaskIds.clear();
    emit assetsChanged();

    m_pendingCatalogRequests = 1;
    setBusy(true);
    setStatusMessage(
        QStringLiteral("Searching %1 (%2)...").arg(sym.toUpper(), m_selectedLanguageCode));

    m_catalogClient->fetchPubMediaLinks(
        CatalogClient::pubMediaLinksUrl(sym, m_selectedLanguageCode, issueTag, fileFormat),
        m_selectedLanguageCode);
}

void MeetingCatalogService::syncCurrentWeekToPlaylist()
{
    m_autoSyncPending = true;
    m_autoImportedCount = 0;
    m_resolvedExternalKeys.clear();
    fetchWeekCatalog();
}

QString MeetingCatalogService::downloadAssetAt(int index)
{
    if (m_downloadService == nullptr || index < 0 || index >= static_cast<int>(m_assets.size())) {
        return {};
    }

    const CatalogMediaAsset& asset = m_assets.at(static_cast<size_t>(index));
    const QString localPath = localPathForAsset(asset);
    const QFileInfo fi(localPath);
    if (fi.exists() && fi.size() > 0) {
        if (m_playlistModel != nullptr) {
            (void)m_playlistModel->importLocalFile(
                localPath, inferMeetingSection(asset.keySymbol, asset.title, asset.label));
        }
        emit assetsChanged();
        return {};
    }

    const DownloadService::Priority prio =
        (asset.fileFormat.compare(QStringLiteral("JWPUB"), Qt::CaseInsensitive) == 0)
            ? DownloadService::Priority::High
            : DownloadService::Priority::Normal;

    const QString displayTitle =
        asset.title.isEmpty()
            ? QStringLiteral("%1 %2 (%3)")
                  .arg(asset.keySymbol.toUpper(), QString::number(asset.issueTagNumber),
                       asset.fileFormat.toUpper())
            : QStringLiteral("%1 (%2)").arg(asset.title, asset.fileFormat.toUpper());

    const QString taskId = m_downloadService->enqueue(QUrl(asset.url), localPath, asset.sha256,
                                                      prio, {}, displayTitle);
    m_assetTaskIds.insert(asset.url, taskId);
    m_pendingDownloads.insert(taskId,
                              PendingDownloadTarget{
                                  localPath,
                                  inferMeetingSection(asset.keySymbol, asset.title, asset.label),
                                  asset.fileFormat.toUpper(),
                              });
    emit assetsChanged();
    return taskId;
}

void MeetingCatalogService::previousWeek()
{
    m_targetWeekMonday = m_targetWeekMonday.addDays(-7);
    emit weekChanged();
}

void MeetingCatalogService::nextWeek()
{
    m_targetWeekMonday = m_targetWeekMonday.addDays(7);
    emit weekChanged();
}

void MeetingCatalogService::goToCurrentWeek()
{
    setTargetWeekDate(QDate::currentDate());
}

void MeetingCatalogService::setTargetWeekDate(const QDate& date)
{
    const QDate target = weekMondayForDate(date);
    if (m_targetWeekMonday == target) {
        return;
    }
    m_targetWeekMonday = target;
    emit weekChanged();
}

void MeetingCatalogService::onLanguagesLoaded(const std::vector<CatalogLanguage>& languages,
                                              bool fromCache)
{
    Q_UNUSED(fromCache)
    if (languages.empty()) {
        return;
    }

    std::vector<CatalogLanguage> sorted = languages;
    std::sort(sorted.begin(), sorted.end(), [](const CatalogLanguage& a, const CatalogLanguage& b) {
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });

    m_languages = std::move(sorted);
    emit languagesChanged();
    emit selectedLanguageChanged();
    setStatusMessage(
        QStringLiteral("Loaded %1 catalog languages").arg(static_cast<int>(m_languages.size())));
}

void MeetingCatalogService::onPubMediaAssetsLoaded(const std::vector<CatalogMediaAsset>& assets,
                                                   bool)
{
    if (m_pendingCatalogRequests > 0) {
        --m_pendingCatalogRequests;
    }

    std::vector<CatalogMediaAsset> merged = m_assets;
    merged.insert(merged.end(), assets.begin(), assets.end());
    m_assets = deduplicateAssets(merged);
    emit assetsChanged();

    if (m_pendingCatalogRequests == 0) {
        setBusy(false);
        setStatusMessage(QStringLiteral("Found %1 package/media items for %2")
                             .arg(static_cast<int>(m_assets.size()))
                             .arg(weekLabel()));
        if (m_autoSyncPending) {
            enqueueMissingAutoSyncAssets();
        }
    }
}

void MeetingCatalogService::onRequestFailed(const QString&, const QString& errorMessage)
{
    if (m_pendingCatalogRequests > 0) {
        --m_pendingCatalogRequests;
    }
    if (m_pendingCatalogRequests == 0) {
        setBusy(false);
        if (m_autoSyncPending && !m_assets.empty()) {
            enqueueMissingAutoSyncAssets();
        } else {
            m_autoSyncPending = false;
        }
    }
    setStatusMessage(errorMessage);
    emit catalogError(errorMessage);
}

void MeetingCatalogService::onDownloadFinished(const QString& taskId, bool success)
{
    const auto it = m_pendingDownloads.find(taskId);
    if (it == m_pendingDownloads.end()) {
        return;
    }

    const PendingDownloadTarget target = it.value();
    m_pendingDownloads.erase(it);

    if (success && QFileInfo::exists(target.localPath)) {
        if (m_playlistModel != nullptr) {
            if (m_playlistModel->importLocalFile(target.localPath, target.sectionTitle)) {
                ++m_autoImportedCount;
            }
        }
        if (target.fileFormat == QStringLiteral("JWPUB")) {
            resolveLinkedJwpubMedia(target.localPath);
        }
    }

    if (m_pendingDownloads.isEmpty() && m_pendingCatalogRequests == 0) {
        setBusy(false);
        if (m_autoImportedCount > 0) {
            emit weekSynced(m_autoImportedCount);
            setStatusMessage(
                QStringLiteral("Synced %1 week item(s) to playlist").arg(m_autoImportedCount));
        }
    }
    emit assetsChanged();
}

void MeetingCatalogService::seedDefaultLanguages()
{
    m_languages = {
        {QStringLiteral("E"), QStringLiteral("en"), QStringLiteral("English"),
         QStringLiteral("English"), false, false},
        {QStringLiteral("T"), QStringLiteral("pt"), QStringLiteral("Portuguese (Brazil)"),
         QStringLiteral("Português (Brasil)"), false, false},
        {QStringLiteral("TPO"), QStringLiteral("pt"), QStringLiteral("Portuguese (Portugal)"),
         QStringLiteral("Português (Portugal)"), false, false},
        {QStringLiteral("S"), QStringLiteral("es"), QStringLiteral("Spanish"),
         QStringLiteral("Español"), false, false},
        {QStringLiteral("F"), QStringLiteral("fr"), QStringLiteral("French"),
         QStringLiteral("Français"), false, false},
        {QStringLiteral("X"), QStringLiteral("de"), QStringLiteral("German"),
         QStringLiteral("Deutsch"), false, false},
        {QStringLiteral("I"), QStringLiteral("it"), QStringLiteral("Italian"),
         QStringLiteral("Italiano"), false, false},
        {QStringLiteral("P"), QStringLiteral("pl"), QStringLiteral("Polish"),
         QStringLiteral("Polski"), false, false},
        {QStringLiteral("J"), QStringLiteral("ja"), QStringLiteral("Japanese"),
         QStringLiteral("日本語"), false, false},
        {QStringLiteral("KO"), QStringLiteral("ko"), QStringLiteral("Korean"),
         QStringLiteral("한국어"), false, false},
        {QStringLiteral("LSB"), QStringLiteral("pt-x-lsb"),
         QStringLiteral("Brazilian Sign Language"), QStringLiteral("Língua Brasileira de Sinais"),
         true, false},
        {QStringLiteral("ASL"), QStringLiteral("ase"), QStringLiteral("American Sign Language"),
         QStringLiteral("American Sign Language"), true, false},
    };
}

void MeetingCatalogService::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void MeetingCatalogService::setStatusMessage(const QString& message)
{
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusChanged();
}

QString MeetingCatalogService::localPathForAsset(const CatalogMediaAsset& asset) const
{
    const QString root =
        m_storageRoot.isEmpty() ? SystemUtils::standardAppDataDir() : m_storageRoot;
    const QString pubDir = asset.keySymbol.trimmed().isEmpty()
                               ? QStringLiteral("catalog")
                               : asset.keySymbol.trimmed().toLower();
    const QString issuePart =
        asset.issueTagNumber > 0 ? QString::number(asset.issueTagNumber) : QStringLiteral("common");
    const QString dirPath = QStringLiteral("%1/catalog/%2/%3_%4")
                                .arg(root, SystemUtils::sanitizeFileName(m_selectedLanguageCode),
                                     SystemUtils::sanitizeFileName(pubDir), issuePart);

    const QUrl parsedUrl(asset.url);
    const QString urlFileName = QFileInfo(parsedUrl.path()).fileName();
    const QString fallbackName =
        SystemUtils::sanitizeFileName(asset.title.isEmpty() ? pubDir : asset.title) +
        QLatin1Char('.') + normalizeFileExtension(asset);
    const QString safeName =
        urlFileName.isEmpty() ? fallbackName : SystemUtils::sanitizeFileName(urlFileName);

    return QDir(dirPath).filePath(safeName);
}

void MeetingCatalogService::enqueueMissingAutoSyncAssets()
{
    m_autoSyncPending = false;
    bool startedDownloads = false;

    for (int i = 0; i < static_cast<int>(m_assets.size()); ++i) {
        const CatalogMediaAsset& asset = m_assets.at(static_cast<size_t>(i));
        const QString fmt = asset.fileFormat.trimmed().toUpper();
        if (fmt != QStringLiteral("JWPUB") && fmt != QStringLiteral("MP4")) {
            continue;
        }
        const QString localPath = localPathForAsset(asset);
        const QFileInfo fi(localPath);
        if (fi.exists() && fi.size() > 0) {
            if (m_playlistModel != nullptr) {
                if (m_playlistModel->importLocalFile(
                        localPath,
                        inferMeetingSection(asset.keySymbol, asset.title, asset.label))) {
                    ++m_autoImportedCount;
                }
            }
            if (fmt == QStringLiteral("JWPUB")) {
                resolveLinkedJwpubMedia(localPath);
            }
            continue;
        }

        if (!downloadAssetAt(i).isEmpty()) {
            startedDownloads = true;
        }
    }

    if (startedDownloads) {
        setBusy(true);
        setStatusMessage(QStringLiteral("Downloading meeting packages and videos..."));
    } else if (m_autoImportedCount > 0) {
        emit weekSynced(m_autoImportedCount);
        setStatusMessage(QStringLiteral("Loaded %1 cached meeting item(s) into playlist")
                             .arg(m_autoImportedCount));
    }
}

void MeetingCatalogService::resolveLinkedJwpubMedia(const QString& jwpubPath)
{
    if (m_catalogClient == nullptr) {
        return;
    }

    const JwpubPackage pkg = JwpubParser::parse(jwpubPath);
    if (!pkg.valid) {
        return;
    }

    for (const JwpubMediaItem& mm : pkg.mediaItems) {
        if (!mm.filePath.isEmpty() || mm.keySymbol.trimmed().isEmpty() ||
            (mm.issueTagNumber <= 0 && mm.track <= 0)) {
            continue;
        }
        if (m_resolvedExternalKeys.size() >= kMaxSecondaryCatalogQueries) {
            break;
        }
        const QString extKey = QStringLiteral("%1|%2|%3|%4")
                                   .arg(mm.keySymbol.toLower(), QString::number(mm.issueTagNumber),
                                        QString::number(mm.track), QString::number(mm.documentId));
        if (m_resolvedExternalKeys.contains(extKey)) {
            continue;
        }
        m_resolvedExternalKeys.insert(extKey);

        ++m_pendingCatalogRequests;
        m_autoSyncPending = true;
        const QString fmt = (mm.keySymbol.compare(QStringLiteral("sjjm"), Qt::CaseInsensitive) == 0)
                                ? QStringLiteral("MP3")
                                : QStringLiteral("MP4");
        m_catalogClient->fetchPubMediaLinks(
            CatalogClient::pubMediaLinksUrl(mm.keySymbol, m_selectedLanguageCode, mm.issueTagNumber,
                                            fmt, mm.track),
            m_selectedLanguageCode);
    }
}

} // namespace kmedia
