#pragma once

#include "network/CatalogClient.h"

#include <QDate>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <vector>

namespace kmedia {

/**
 * The congregation's media settings and lookups: the JW language catalog (media language and
 * fallback), the media filters the meeting sync applies, and the hub.jw.org congregation search.
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

public:
    explicit MeetingCatalogService(CatalogClient* catalogClient, QObject* parent = nullptr);
    ~MeetingCatalogService() override = default;

    Q_DISABLE_COPY_MOVE(MeetingCatalogService)

    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] QString statusMessage() const;

    [[nodiscard]] QString selectedLanguageCode() const;
    void setSelectedLanguageCode(const QString& code);
    [[nodiscard]] QString selectedLanguageLabel() const;
    [[nodiscard]] bool isSignLanguage(const QString& code) const;
    [[nodiscard]] QString fallbackLanguageCode() const;
    void setFallbackLanguageCode(const QString& code);

    [[nodiscard]] QVariantList languagesVariant() const;
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

    [[nodiscard]] static QDate weekMondayForDate(const QDate& date);
    [[nodiscard]] static int midweekIssueTagForDate(const QDate& date);
    [[nodiscard]] static int watchtowerIssueTagForDate(const QDate& date);
    [[nodiscard]] static QString formatWeekRange(const QDate& weekMonday);

public slots:
    void refreshLanguages();
    void searchCongregations(const QString& queryText);
    bool applyCongregationResult(int index);

signals:
    void busyChanged();
    void statusChanged();
    void selectedLanguageChanged();
    void fallbackLanguageChanged();
    void languagesChanged();
    void congregationsChanged();
    void filtersChanged();
    void congregationScheduleApplied(const QString& name, const QString& langCode, int midDay,
                                     const QString& midTime, int endDay, const QString& endTime);
    void catalogError(const QString& message);

private slots:
    void onLanguagesLoaded(const std::vector<kmedia::CatalogLanguage>& languages, bool fromCache);
    void onCongregationsLoaded(const std::vector<kmedia::CatalogCongregationResult>& results,
                               bool fromCache);
    void onRequestFailed(const QString& url, const QString& errorMessage);

private:
    void seedDefaultLanguages();
    void setBusy(bool busy);
    void setStatusMessage(const QString& message);

    CatalogClient* m_catalogClient{nullptr};

    QString m_selectedLanguageCode{QStringLiteral("E")};
    QString m_fallbackLanguageCode{QStringLiteral("E")};
    bool m_busy{false};
    QString m_statusMessage;
    bool m_includePrinted{false};
    bool m_excludeFootnotes{true};
    bool m_excludeWtParagraphVideos{false};
    bool m_excludeTh{false};
    bool m_excludeCbsPubs{false};

    std::vector<CatalogLanguage> m_languages;
    std::vector<CatalogCongregationResult> m_congregationResults;
};

} // namespace kmedia
