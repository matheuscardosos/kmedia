#include "network/MeetingCatalogService.h"

#include "core/MeetingSections.h"
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
      m_playlistModel(playlistModel), m_targetWeekMonday(weekMondayForDate(QDate::currentDate())),
      m_targetDate(QDate::currentDate())
{
    seedDefaultLanguages();

    if (m_catalogClient != nullptr) {
        connect(m_catalogClient, &CatalogClient::languagesLoaded, this,
                &MeetingCatalogService::onLanguagesLoaded);
        connect(m_catalogClient, &CatalogClient::pubMediaAssetsLoaded, this,
                &MeetingCatalogService::onPubMediaAssetsLoaded);
        connect(m_catalogClient, &CatalogClient::mediatorAssetsLoaded, this,
                &MeetingCatalogService::onMediatorAssetsLoaded);
        connect(m_catalogClient, &CatalogClient::congregationsLoaded, this,
                &MeetingCatalogService::onCongregationsLoaded);
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

void MeetingCatalogService::setSpecialWeekEvents(
    const QHash<QString, QPair<QString, QString>>& eventsByMonday)
{
    if (m_specialEventsByMonday == eventsByMonday) {
        return;
    }
    m_specialEventsByMonday = eventsByMonday;
    emit weekChanged();
}

QString MeetingCatalogService::weekEventType() const
{
    return m_specialEventsByMonday.value(m_targetWeekMonday.toString(Qt::ISODate)).first;
}

QString MeetingCatalogService::weekEventLabel() const
{
    return m_specialEventsByMonday.value(m_targetWeekMonday.toString(Qt::ISODate)).second;
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

bool MeetingCatalogService::isSignLanguage(const QString& code) const
{
    return std::any_of(
        m_languages.begin(), m_languages.end(), [&code](const CatalogLanguage& language) {
            return language.isSignLanguage && language.code.compare(code, Qt::CaseInsensitive) == 0;
        });
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

QDate MeetingCatalogService::targetDate() const
{
    return m_targetDate.isValid() ? m_targetDate : m_targetWeekMonday;
}

QString MeetingCatalogService::targetMeetingKind() const
{
    return m_targetMeetingKind;
}

QString MeetingCatalogService::fallbackLanguageCode() const
{
    return m_fallbackLanguageCode;
}

void MeetingCatalogService::setFallbackLanguageCode(const QString& code)
{
    const QString clean = code.trimmed().isEmpty() ? QStringLiteral("E") : code.trimmed();
    if (m_fallbackLanguageCode == clean) {
        return;
    }
    m_fallbackLanguageCode = clean;
    emit fallbackLanguageChanged();
}

bool MeetingCatalogService::includePrinted() const
{
    return m_includePrinted;
}

void MeetingCatalogService::setIncludePrinted(bool includePrinted)
{
    if (m_includePrinted == includePrinted) {
        return;
    }
    m_includePrinted = includePrinted;
    emit filtersChanged();
}

bool MeetingCatalogService::excludeFootnotes() const
{
    return m_excludeFootnotes;
}

void MeetingCatalogService::setExcludeFootnotes(bool excludeFootnotes)
{
    if (m_excludeFootnotes == excludeFootnotes) {
        return;
    }
    m_excludeFootnotes = excludeFootnotes;
    emit filtersChanged();
}

bool MeetingCatalogService::excludeWtParagraphVideos() const
{
    return m_excludeWtParagraphVideos;
}

void MeetingCatalogService::setExcludeWtParagraphVideos(bool exclude)
{
    if (m_excludeWtParagraphVideos == exclude) {
        return;
    }
    m_excludeWtParagraphVideos = exclude;
    emit filtersChanged();
}

bool MeetingCatalogService::excludeTh() const
{
    return m_excludeTh;
}

void MeetingCatalogService::setExcludeTh(bool excludeTh)
{
    if (m_excludeTh == excludeTh) {
        return;
    }
    m_excludeTh = excludeTh;
    emit filtersChanged();
}

bool MeetingCatalogService::excludeCbsPubs() const
{
    return m_excludeCbsPubs;
}

void MeetingCatalogService::setExcludeCbsPubs(bool excludeCbsPubs)
{
    if (m_excludeCbsPubs == excludeCbsPubs) {
        return;
    }
    m_excludeCbsPubs = excludeCbsPubs;
    emit filtersChanged();
}

bool MeetingCatalogService::autoLoadWeekMedia() const
{
    return m_autoLoadWeekMedia;
}

void MeetingCatalogService::setAutoLoadWeekMedia(bool autoLoad)
{
    if (m_autoLoadWeekMedia == autoLoad) {
        return;
    }
    m_autoLoadWeekMedia = autoLoad;
    emit filtersChanged();
}

QVariantList MeetingCatalogService::congregationResults() const
{
    QVariantList out;
    out.reserve(static_cast<qsizetype>(m_congregationResults.size()));
    for (int i = 0; i < static_cast<int>(m_congregationResults.size()); ++i) {
        const CatalogCongregationResult& c = m_congregationResults.at(static_cast<size_t>(i));
        QVariantMap map;
        map.insert(QStringLiteral("index"), i);
        map.insert(QStringLiteral("orgName"), c.orgName);
        map.insert(QStringLiteral("address"), c.address);
        map.insert(QStringLiteral("languageCode"), c.languageCode);
        map.insert(QStringLiteral("midweekDay"), c.midweekDay);
        map.insert(QStringLiteral("midweekTime"), c.midweekTime);
        map.insert(QStringLiteral("weekendDay"), c.weekendDay);
        map.insert(QStringLiteral("weekendTime"), c.weekendTime);
        out.append(map);
    }
    return out;
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
                   MeetingSections::translatedLabel(
                       inferMeetingSection(asset.keySymbol, asset.title, asset.label)));
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

std::vector<int> MeetingCatalogService::watchtowerCandidateIssueTags(const QDate& date)
{
    const QDate base = date.isValid() ? date : QDate::currentDate();
    std::vector<int> tags;
    static constexpr int kWeeksBack[] = {8, 6, 10, 12};
    for (const int weeks : kWeeksBack) {
        const QDate candidate = base.addDays(-7 * weeks);
        const int tag = (candidate.year() * 100) + candidate.month();
        if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
            tags.push_back(tag);
        }
    }
    return tags;
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
        return MeetingSections::kTreasures;
    }
    if (text.contains(QStringLiteral("apply yourself")) ||
        text.contains(QStringLiteral("ministério")) || text.contains(QStringLiteral("ministry"))) {
        return MeetingSections::kApplyYourself;
    }
    if (text.contains(QStringLiteral("living as christians")) ||
        text.contains(QStringLiteral("vida cristã"))) {
        return MeetingSections::kLivingAsChristians;
    }
    if (text.contains(QStringLiteral("watchtower")) || text.contains(QStringLiteral("sentinela")) ||
        sym == QStringLiteral("w")) {
        return MeetingSections::kWatchtowerStudy;
    }
    if (sym == QStringLiteral("s-34") || text.contains(QStringLiteral("public talk")) ||
        text.contains(QStringLiteral("discurso"))) {
        return MeetingSections::kPublicTalk;
    }
    if (sym == QStringLiteral("mwb") || sym == QStringLiteral("mwbv")) {
        return MeetingSections::kImportedMedia;
    }
    return MeetingSections::kImportedMedia;
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
    m_mediatorFallbackTried.clear();
    m_pendingStreamMeta.clear();
    emit assetsChanged();

    const QDate effectiveDate = targetDate();
    if (m_targetMeetingKind == QStringLiteral("memorial")) {
        const QString miSymbol =
            QStringLiteral("mi%1").arg(QString::number(effectiveDate.year()).right(2));
        m_pendingCatalogRequests = 3;
        setBusy(true);
        setStatusMessage(QStringLiteral("Fetching Memorial package (%1, Songs 18 & 25)...")
                             .arg(miSymbol.toUpper()));
        m_catalogClient->fetchPubMediaLinks(
            CatalogClient::pubMediaLinksUrl(miSymbol, m_selectedLanguageCode),
            m_selectedLanguageCode);
        m_catalogClient->fetchPubMediaLinks(
            CatalogClient::pubMediaLinksUrl(QStringLiteral("sjjm"), m_selectedLanguageCode, 0,
                                            QStringLiteral("MP3"), 18),
            m_selectedLanguageCode);
        m_catalogClient->fetchPubMediaLinks(
            CatalogClient::pubMediaLinksUrl(QStringLiteral("sjjm"), m_selectedLanguageCode, 0,
                                            QStringLiteral("MP3"), 25),
            m_selectedLanguageCode);
        return;
    }

    const std::vector<int> wtTags = watchtowerCandidateIssueTags(effectiveDate);
    m_pendingCatalogRequests = 1 + static_cast<int>(wtTags.size());
    setBusy(true);
    setStatusMessage(
        QStringLiteral("Fetching week %1 (%2)...").arg(weekLabel(), m_selectedLanguageCode));

    m_catalogClient->fetchPubMediaLinks(CatalogClient::pubMediaLinksUrl(QStringLiteral("mwb"),
                                                                        m_selectedLanguageCode,
                                                                        midweekIssueTag()),
                                        m_selectedLanguageCode);

    for (const int wtTag : wtTags) {
        m_catalogClient->fetchPubMediaLinks(
            CatalogClient::pubMediaLinksUrl(QStringLiteral("w"), m_selectedLanguageCode, wtTag),
            m_selectedLanguageCode);
    }
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
    applyWeekEventSection();
    const QString evType = weekEventType();
    if (evType == QStringLiteral("circuit_assembly") ||
        evType == QStringLiteral("regional_convention") ||
        (evType == QStringLiteral("memorial") &&
         (m_targetMeetingKind != QStringLiteral("memorial") || m_catalogClient == nullptr))) {
        m_autoSyncPending = false;
        setBusy(false);
        setStatusMessage(
            QStringLiteral("%1 week: skipped regular weekly auto-sync").arg(weekEventLabel()));
        emit weekSynced(0);
        return;
    }

    m_autoSyncPending = true;
    m_autoImportedCount = 0;
    m_resolvedExternalKeys.clear();
    fetchWeekCatalog();
}

void MeetingCatalogService::syncDateToPlaylist(const QDate& date, const QString& meetingKind)
{
    setTargetMeetingContext(date, meetingKind);
    syncCurrentWeekToPlaylist();
}

void MeetingCatalogService::searchCongregations(const QString& queryText)
{
    if (m_catalogClient == nullptr || queryText.trimmed().isEmpty()) {
        return;
    }
    setBusy(true);
    setStatusMessage(
        QStringLiteral("Searching congregations matching \"%1\"...").arg(queryText.trimmed()));
    m_catalogClient->searchCongregations(queryText);
}

bool MeetingCatalogService::applyCongregationResult(int index)
{
    if (index < 0 || index >= static_cast<int>(m_congregationResults.size())) {
        return false;
    }
    const CatalogCongregationResult& chosen = m_congregationResults.at(static_cast<size_t>(index));
    if (!chosen.languageCode.isEmpty()) {
        setSelectedLanguageCode(chosen.languageCode);
    }
    emit congregationScheduleApplied(chosen.orgName, chosen.languageCode, chosen.midweekDay,
                                     chosen.midweekTime, chosen.weekendDay, chosen.weekendTime);
    setStatusMessage(QStringLiteral("Applied schedule for %1").arg(chosen.orgName));
    return true;
}

void MeetingCatalogService::applyWeekEventSection()
{
    if (m_playlistModel == nullptr) {
        return;
    }
    const QString evType = weekEventType();
    const QString evLabel = weekEventLabel();
    if (evType.isEmpty()) {
        return;
    }
    m_playlistModel->setPlaylistSubtitle(QStringLiteral("%1 | %2").arg(weekLabel(), evLabel));
    if (evType == QStringLiteral("co_visit")) {
        (void)m_playlistModel->ensureSection(MeetingSections::kCircuitOverseer);
    }
}

QString MeetingCatalogService::downloadAssetAt(int index)
{
    if (m_downloadService == nullptr || index < 0 || index >= static_cast<int>(m_assets.size())) {
        return {};
    }

    const CatalogMediaAsset& asset = m_assets.at(static_cast<size_t>(index));
    if (asset.streamOnly || asset.durationSeconds > CatalogClient::kLongMediaDurationSeconds) {
        if (m_playlistModel != nullptr) {
            PlaylistEntryView streamEntry;
            streamEntry.title = asset.title.isEmpty() ? asset.keySymbol.toUpper() : asset.title;
            streamEntry.subtitle = QStringLiteral("Stream (>35m)");
            streamEntry.sectionId = inferMeetingSection(asset.keySymbol, asset.title, asset.label);
            streamEntry.streamUrl = asset.url;
            streamEntry.pubMediaId = asset.pubMediaId;
            streamEntry.durationMs = static_cast<qint64>(asset.durationSeconds * 1000.0);
            streamEntry.sourceKind = QStringLiteral("stream");
            (void)m_playlistModel->appendEntry(streamEntry);
        }
        return {};
    }

    const QString localPath = localPathForAsset(asset);
    const QFileInfo fi(localPath);
    if (fi.exists() && fi.size() > 0) {
        if (m_playlistModel != nullptr) {
            const QString pubHint = asset.keySymbol.trimmed().toLower();
            if (asset.fileFormat.compare(QStringLiteral("JWPUB"), Qt::CaseInsensitive) == 0) {
                QString wtSub;
                (void)m_playlistModel->importJwpubForWeek(
                    localPath, buildFilterOptions(pubHint),
                    inferMeetingSection(asset.keySymbol, asset.title, asset.label),
                    weekEventType() == QStringLiteral("co_visit"), &wtSub);
                if (!wtSub.isEmpty() && m_targetMeetingKind == QStringLiteral("weekend")) {
                    m_playlistModel->setPlaylistSubtitle(wtSub);
                }
            } else {
                (void)m_playlistModel->importLocalFile(
                    localPath, inferMeetingSection(asset.keySymbol, asset.title, asset.label));
            }
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
                                  {},
                                  {},
                                  {},
                                  false,
                              });
    emit assetsChanged();
    return taskId;
}

void MeetingCatalogService::previousWeek()
{
    m_targetWeekMonday = m_targetWeekMonday.addDays(-7);
    if (m_targetDate.isValid()) {
        m_targetDate = m_targetDate.addDays(-7);
    } else {
        m_targetDate = m_targetWeekMonday;
    }
    emit weekChanged();
}

void MeetingCatalogService::nextWeek()
{
    m_targetWeekMonday = m_targetWeekMonday.addDays(7);
    if (m_targetDate.isValid()) {
        m_targetDate = m_targetDate.addDays(7);
    } else {
        m_targetDate = m_targetWeekMonday;
    }
    emit weekChanged();
}

void MeetingCatalogService::goToCurrentWeek()
{
    setTargetWeekDate(QDate::currentDate());
}

void MeetingCatalogService::setTargetWeekDate(const QDate& date)
{
    const QDate effective = date.isValid() ? date : QDate::currentDate();
    const QDate target = weekMondayForDate(effective);
    if (m_targetWeekMonday == target && m_targetDate == effective) {
        return;
    }
    m_targetWeekMonday = target;
    m_targetDate = effective;
    emit weekChanged();
}

void MeetingCatalogService::setTargetMeetingContext(const QDate& date, const QString& meetingKind)
{
    const QDate effective = date.isValid() ? date : QDate::currentDate();
    const QDate target = weekMondayForDate(effective);
    const QString cleanKind = meetingKind.trimmed().isEmpty() ? QStringLiteral("midweek")
                                                              : meetingKind.trimmed().toLower();
    if (m_targetWeekMonday == target && m_targetDate == effective &&
        m_targetMeetingKind == cleanKind) {
        return;
    }
    m_targetWeekMonday = target;
    m_targetDate = effective;
    m_targetMeetingKind = cleanKind;
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

void MeetingCatalogService::onMediatorAssetsLoaded(const QString& pubMediaKey,
                                                   const std::vector<CatalogMediaAsset>& assets,
                                                   bool)
{
    if (m_pendingCatalogRequests > 0) {
        --m_pendingCatalogRequests;
    }

    const PendingDownloadTarget meta = m_pendingStreamMeta.take(pubMediaKey);
    if (assets.empty()) {
        // Fallback chain: retry once with secondary fallback language before GETPUBMEDIALINKS.
        if (m_catalogClient != nullptr && !m_fallbackLanguageCode.isEmpty() &&
            m_fallbackLanguageCode.compare(m_selectedLanguageCode, Qt::CaseInsensitive) != 0 &&
            !m_mediatorFallbackTried.contains(pubMediaKey)) {
            m_mediatorFallbackTried.insert(pubMediaKey, m_fallbackLanguageCode);
            m_pendingStreamMeta.insert(pubMediaKey, meta);
            ++m_pendingCatalogRequests;
            m_catalogClient->fetchMediatorMedia(
                CatalogClient::mediatorMediaUrl(pubMediaKey, m_fallbackLanguageCode), pubMediaKey);
            return;
        }
    } else {
        const CatalogMediaAsset* best = nullptr;
        for (const CatalogMediaAsset& item : assets) {
            if (best == nullptr || videoResolutionScore(item) > videoResolutionScore(*best)) {
                best = &item;
            }
        }
        if (best != nullptr) {
            if (best->streamOnly ||
                best->durationSeconds > CatalogClient::kLongMediaDurationSeconds) {
                if (m_playlistModel != nullptr) {
                    PlaylistEntryView entry;
                    entry.title = best->title.isEmpty() ? pubMediaKey : best->title;
                    entry.subtitle = QStringLiteral("Stream (>35m)");
                    entry.sectionId =
                        meta.sectionId.isEmpty()
                            ? inferMeetingSection(best->keySymbol, best->title, best->label)
                            : meta.sectionId;
                    entry.streamUrl = best->url;
                    entry.pubMediaId = pubMediaKey;
                    entry.durationMs = static_cast<qint64>(best->durationSeconds * 1000.0);
                    entry.sourceKind = QStringLiteral("stream");
                    entry.groupKey = meta.groupKey;
                    entry.tagType = meta.tagType;
                    entry.tagValue = meta.tagValue;
                    entry.isCbs = meta.isCbs;
                    if (m_playlistModel->appendEntry(entry)) {
                        ++m_autoImportedCount;
                    }
                }
            } else if (m_downloadService != nullptr) {
                const QString localPath = localPathForAsset(*best);
                if (QFileInfo::exists(localPath) && QFileInfo(localPath).size() > 0) {
                    if (m_playlistModel != nullptr) {
                        PlaylistEntryView entry;
                        entry.title = best->title;
                        entry.sectionId = meta.sectionId;
                        entry.filePath = localPath;
                        entry.pubMediaId = pubMediaKey;
                        entry.groupKey = meta.groupKey;
                        entry.tagType = meta.tagType;
                        entry.tagValue = meta.tagValue;
                        entry.isCbs = meta.isCbs;
                        entry.sourceKind = QStringLiteral("jwpub");
                        if (m_playlistModel->appendEntry(entry)) {
                            ++m_autoImportedCount;
                        }
                    }
                } else {
                    const QString taskId = m_downloadService->enqueue(
                        QUrl(best->url), localPath, best->sha256, DownloadService::Priority::Normal,
                        {}, best->title.isEmpty() ? pubMediaKey : best->title);
                    PendingDownloadTarget dl = meta;
                    dl.localPath = localPath;
                    dl.fileFormat = best->fileFormat;
                    m_pendingDownloads.insert(taskId, dl);
                }
            }
        }
    }

    if (m_pendingCatalogRequests == 0 && m_pendingDownloads.isEmpty()) {
        setBusy(false);
        if (m_autoImportedCount > 0) {
            emit weekSynced(m_autoImportedCount);
            setStatusMessage(
                QStringLiteral("Synced %1 week item(s) to playlist").arg(m_autoImportedCount));
        }
    }
}

void MeetingCatalogService::onCongregationsLoaded(
    const std::vector<CatalogCongregationResult>& results, bool)
{
    m_congregationResults = results;
    setBusy(false);
    emit congregationsChanged();
    setStatusMessage(
        QStringLiteral("Found %1 congregation(s)").arg(static_cast<int>(results.size())));
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
        if (target.fileFormat == QStringLiteral("JWPUB")) {
            if (m_playlistModel != nullptr) {
                QString wtSub;
                const QString pubHint = QFileInfo(target.localPath)
                                            .completeBaseName()
                                            .section(QLatin1Char('_'), 0, 0)
                                            .toLower();
                m_autoImportedCount += m_playlistModel->importJwpubForWeek(
                    target.localPath, buildFilterOptions(pubHint), target.sectionId,
                    weekEventType() == QStringLiteral("co_visit"), &wtSub);
                if (!wtSub.isEmpty() && m_targetMeetingKind == QStringLiteral("weekend")) {
                    m_playlistModel->setPlaylistSubtitle(wtSub);
                }
            }
            resolveLinkedJwpubMedia(target.localPath);
        } else if (m_playlistModel != nullptr) {
            PlaylistEntryView entry;
            entry.title = QFileInfo(target.localPath).completeBaseName();
            entry.sectionId = target.sectionId;
            entry.filePath = target.localPath;
            entry.groupKey = target.groupKey;
            entry.tagType = target.tagType;
            entry.tagValue = target.tagValue;
            entry.isCbs = target.isCbs;
            entry.sourceKind = QStringLiteral("catalog");
            if (m_playlistModel->appendEntry(entry)) {
                ++m_autoImportedCount;
            }
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

JwpubWeekFilterOptions MeetingCatalogService::buildFilterOptions(const QString& pubSymbolHint) const
{
    JwpubWeekFilterOptions opts;
    opts.targetDate = targetDate();
    opts.pubSymbolHint = pubSymbolHint.trimmed().toLower();
    opts.includePrinted = m_includePrinted;
    opts.excludeFootnotes = m_excludeFootnotes;
    opts.excludeWtParagraphVideos = m_excludeWtParagraphVideos;
    opts.excludeTh = m_excludeTh;
    opts.excludeCbsPubs = m_excludeCbsPubs;
    return opts;
}

void MeetingCatalogService::enqueueMissingAutoSyncAssets()
{
    m_autoSyncPending = false;
    bool startedDownloads = false;
    const bool isCoVisit = (weekEventType() == QStringLiteral("co_visit"));
    const QString kindLower = m_targetMeetingKind.trimmed().toLower();

    for (int i = 0; i < static_cast<int>(m_assets.size()); ++i) {
        const CatalogMediaAsset& asset = m_assets.at(static_cast<size_t>(i));
        const QString fmt = asset.fileFormat.trimmed().toUpper();
        if (fmt != QStringLiteral("JWPUB") && fmt != QStringLiteral("MP4") &&
            fmt != QStringLiteral("MP3")) {
            continue;
        }
        const QString symLower = asset.keySymbol.trimmed().toLower();
        // When explicitly syncing midweek or weekend day, import only the relevant JWPUB package.
        if (fmt == QStringLiteral("JWPUB")) {
            if (kindLower == QStringLiteral("midweek") && symLower == QStringLiteral("w")) {
                continue;
            }
            if (kindLower == QStringLiteral("weekend") && symLower == QStringLiteral("mwb")) {
                continue;
            }
        }

        const QString localPath = localPathForAsset(asset);
        const QFileInfo fi(localPath);
        if (fi.exists() && fi.size() > 0) {
            if (fmt == QStringLiteral("JWPUB")) {
                if (m_playlistModel != nullptr) {
                    QString wtSub;
                    m_autoImportedCount += m_playlistModel->importJwpubForWeek(
                        localPath, buildFilterOptions(symLower),
                        inferMeetingSection(asset.keySymbol, asset.title, asset.label), isCoVisit,
                        &wtSub);
                    if (!wtSub.isEmpty() && kindLower == QStringLiteral("weekend")) {
                        m_playlistModel->setPlaylistSubtitle(wtSub);
                    }
                }
                resolveLinkedJwpubMedia(localPath);
            } else if (m_playlistModel != nullptr) {
                if (m_playlistModel->importLocalFile(
                        localPath,
                        inferMeetingSection(asset.keySymbol, asset.title, asset.label))) {
                    ++m_autoImportedCount;
                }
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

void MeetingCatalogService::resolveSingleLinkedMedia(
    const QString& pubSymbol, int issueTag, qint64 docId, int track, const QString& /*title*/,
    const QString& sectionId, const QString& groupKey, const QString& tagType,
    const QString& tagValue, bool isCbs)
{
    if (m_catalogClient == nullptr) {
        return;
    }
    const QString extKey = QStringLiteral("%1|%2|%3|%4")
                               .arg(pubSymbol.toLower(), QString::number(issueTag),
                                    QString::number(track), QString::number(docId));
    if (m_resolvedExternalKeys.contains(extKey) ||
        m_resolvedExternalKeys.size() >= kMaxSecondaryCatalogQueries) {
        return;
    }
    m_resolvedExternalKeys.insert(extKey);

    const bool isAudio = (pubSymbol.compare(QStringLiteral("sjjm"), Qt::CaseInsensitive) == 0 ||
                          pubSymbol.compare(QStringLiteral("sjj"), Qt::CaseInsensitive) == 0);
    const QString effectiveSym =
        (pubSymbol.compare(QStringLiteral("sjj"), Qt::CaseInsensitive) == 0)
            ? QStringLiteral("sjjm")
            : pubSymbol;
    const QString mediaKey =
        CatalogClient::buildPubMediaKey(effectiveSym, issueTag, docId, track, !isAudio);

    PendingDownloadTarget target;
    target.sectionId = sectionId.isEmpty() ? QString(MeetingSections::kImportedMedia) : sectionId;
    target.groupKey = groupKey;
    target.tagType = tagType;
    target.tagValue = tagValue;
    target.isCbs = isCbs;
    m_pendingStreamMeta.insert(mediaKey, target);

    ++m_pendingCatalogRequests;
    m_autoSyncPending = true;
    m_catalogClient->fetchMediatorMedia(
        CatalogClient::mediatorMediaUrl(mediaKey, m_selectedLanguageCode), mediaKey);
    const QString fmt = isAudio ? QStringLiteral("MP3") : QStringLiteral("MP4");
    ++m_pendingCatalogRequests;
    m_catalogClient->fetchPubMediaLinks(
        CatalogClient::pubMediaLinksUrl(effectiveSym, m_selectedLanguageCode, issueTag, fmt, track,
                                        docId > 0 ? QString::number(docId) : QString()),
        m_selectedLanguageCode);
}

void MeetingCatalogService::resolveLinkedJwpubMedia(const QString& jwpubPath)
{
    if (m_catalogClient == nullptr) {
        return;
    }

    const QString symHint =
        QFileInfo(jwpubPath).completeBaseName().section(QLatin1Char('_'), 0, 0).toLower();
    const JwpubPackage pkg = JwpubParser::parseForWeek(jwpubPath, buildFilterOptions(symHint));
    if (!pkg.valid) {
        return;
    }

    for (const JwpubMediaItem& mm : pkg.mediaItems) {
        if (!mm.filePath.isEmpty() || mm.keySymbol.trimmed().isEmpty() ||
            (mm.issueTagNumber <= 0 && mm.track <= 0)) {
            continue;
        }
        resolveSingleLinkedMedia(mm.keySymbol, mm.issueTagNumber, mm.documentId, mm.track, mm.label,
                                 inferMeetingSection(mm.keySymbol, mm.documentTitle, mm.label),
                                 mm.groupCaption, mm.tagType, mm.tagValue, mm.isCbsCandidate);
    }

    for (const JwpubExtractRef& ref : pkg.extracts) {
        if (ref.songNumber > 0) {
            resolveSingleLinkedMedia(
                QStringLiteral("sjjm"), 0, 0, ref.songNumber,
                QStringLiteral("Song %1").arg(ref.songNumber),
                inferMeetingSection(pkg.publication.undatedSymbol, ref.caption, QString()),
                ref.caption, QStringLiteral("song"), QStringLiteral("Song %1").arg(ref.songNumber),
                false);
        } else if (ref.isCbsSource && ref.refMepsDocumentId > 0) {
            resolveSingleLinkedMedia(ref.refPubSymbol, ref.refIssueTagNumber, ref.refMepsDocumentId,
                                     0, ref.caption, MeetingSections::kLivingAsChristians,
                                     ref.caption, QString(), QString(), true);
        }
    }
}

} // namespace kmedia
