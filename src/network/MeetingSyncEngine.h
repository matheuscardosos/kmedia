#pragma once

#include "network/DownloadService.h"
#include "network/PublicationStore.h"
#include "parsers/MeetingMediaBuilder.h"
#include "parsers/PublicationDatabase.h"

#include <QDate>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantMap>

#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace kmedia {

class DatabaseManager;

/**
 * Keeps every meeting of a rolling window ready without user action: from the Monday of the
 * current week up to about five weeks ahead, each meeting day that is new, failed, or missing
 * files is synced in the background (two days at a time) into its own date workspace, and its
 * status (pending, syncing, complete, error) is tracked per day.
 */
class MeetingSyncEngine : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged FINAL)
    Q_PROPERTY(QVariantMap dayStatuses READ dayStatuses NOTIFY dayStatusesChanged FINAL)
    Q_PROPERTY(QString windowStart READ windowStart NOTIFY dayStatusesChanged FINAL)
    Q_PROPERTY(QString windowEnd READ windowEnd NOTIFY dayStatusesChanged FINAL)

public:
    struct Settings
    {
        QString languageCode{QStringLiteral("E")};
        QString fallbackLanguageCode;
        QString songbookSymbol{QStringLiteral("sjjm")};
        bool includePrinted{false};
        bool excludeFootnotes{true};
        bool excludeWtParagraphVideos{false};
        bool excludeTh{false};
        bool excludeCbsVideos{false};
        bool meteredConnection{false};
        bool signLanguage{false};
    };

    using MeetingKindProvider = std::function<QString(const QDate&)>;
    using CoWeekProvider = std::function<bool(const QDate&)>;

    static constexpr int kDaysInFuture = 35;
    static constexpr int kMaxConcurrentDays = 2;

    explicit MeetingSyncEngine(PublicationStore* store, QObject* parent = nullptr);
    ~MeetingSyncEngine() override;

    Q_DISABLE_COPY_MOVE(MeetingSyncEngine)

    void setDatabase(DatabaseManager* database);
    void setSettings(const Settings& settings);
    [[nodiscard]] const Settings& settings() const;
    void setScheduleProviders(MeetingKindProvider meetingKind, CoWeekProvider coWeek);
    void setToday(const QDate& today);

    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] QVariantMap dayStatuses() const;
    [[nodiscard]] QString windowStart() const;
    [[nodiscard]] QString windowEnd() const;
    Q_INVOKABLE [[nodiscard]] QString statusForDate(const QString& dateIso) const;

    [[nodiscard]] static std::vector<QDate> lookupWindow(const QDate& today, bool metered);
    [[nodiscard]] static int midweekIssueTag(const QDate& weekMonday);
    [[nodiscard]] static std::vector<int> watchtowerIssueTags(const QDate& weekMonday);
    [[nodiscard]] static QString extractPublicationSymbol(const QString& uniqueEnglishSymbol,
                                                          int issueTagNumber);
    [[nodiscard]] static QString playlistNameFor(const QString& dateIso, const QString& kind);

public slots:
    void refresh();
    void resyncDay(const QString& dateIso);
    void resetAll();
    void refreshAddedMediaDays();

signals:
    void busyChanged();
    void dayStatusesChanged();
    void dayStatusChanged(const QString& dateIso, const QString& status);
    void dayContentChanged(const QString& dateIso);
    void syncIdle();

private:
    struct DayJob
    {
        QDate date;
        QString dateIso;
        QString kind;
        bool coWeek{false};
        qint64 playlistId{0};
        DownloadService::Priority priority{DownloadService::Priority::Normal};
        std::vector<PublicationMediaRow> rows;
        std::vector<PublicationExtractRow> extracts;
        QString documentTitle;
        int pendingResolutions{0};
        int failedMedia{0};
        bool finished{false};
        quint64 generation{0};
    };
    using JobPtr = std::shared_ptr<DayJob>;

    void enqueue(const QDate& date, const QString& kind, qint64 playlistId);
    void pump();
    void startJob(const JobPtr& job);
    void finishJob(const JobPtr& job);
    void failJob(const JobPtr& job);
    void releaseJob(const JobPtr& job);
    void setStatus(const QString& dateIso, const QString& status);

    void loadMidweek(const JobPtr& job, int languageIndex);
    void processExtract(const JobPtr& job, size_t extractIndex, int languageIndex);
    void loadWeekend(const JobPtr& job, int languageIndex, size_t issueIndex);
    void loadMemorial(const JobPtr& job, int languageIndex);
    void resolveMissingMedia(const JobPtr& job);
    void writeDay(const JobPtr& job, const std::vector<DayMediaItem>& items);
    void onMediaFileReady(const QString& filePath);

    [[nodiscard]] QStringList languages(const QString& preferred = {}) const;
    [[nodiscard]] QDate today() const;
    [[nodiscard]] DownloadService::Priority priorityFor(const QDate& date) const;
    [[nodiscard]] bool hasMissingSyncedFiles(qint64 playlistId) const;

    PublicationStore* m_store{nullptr};
    DatabaseManager* m_database{nullptr};
    Settings m_settings;
    MeetingKindProvider m_meetingKind;
    CoWeekProvider m_coWeek;
    QDate m_today;
    std::deque<JobPtr> m_queue;
    QHash<QString, JobPtr> m_active;
    QHash<QString, QString> m_statuses;
    QSet<QString> m_addedMediaDays;
    QHash<QString, QSet<QString>> m_daysByFile;
    quint64 m_generation{0};
};

} // namespace kmedia
