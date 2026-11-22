#include "network/MeetingCatalogService.h"

#include <algorithm>
#include <utility>

namespace kmedia {

MeetingCatalogService::MeetingCatalogService(CatalogClient* catalogClient, QObject* parent)
    : QObject(parent), m_catalogClient(catalogClient)
{
    seedDefaultLanguages();

    if (m_catalogClient != nullptr) {
        connect(m_catalogClient, &CatalogClient::languagesLoaded, this,
                &MeetingCatalogService::onLanguagesLoaded);
        connect(m_catalogClient, &CatalogClient::congregationsLoaded, this,
                &MeetingCatalogService::onCongregationsLoaded);
        connect(m_catalogClient, &CatalogClient::requestFailed, this,
                &MeetingCatalogService::onRequestFailed);
    }
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

void MeetingCatalogService::refreshLanguages()
{
    if (m_catalogClient == nullptr) {
        return;
    }
    setStatusMessage(QStringLiteral("Updating JW language catalog..."));
    m_catalogClient->fetchLanguages();
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
    setBusy(false);
    setStatusMessage(errorMessage);
    emit catalogError(errorMessage);
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

} // namespace kmedia
