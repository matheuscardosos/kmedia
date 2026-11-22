#pragma once

#include <QDate>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>

#include <functional>
#include <vector>

namespace kmedia {

class CloudSyncWatcher;
class DatabaseManager;
class PlaylistModel;

/**
 * Brings the media of a shared folder into the meeting days: files in `<folder>/YYYY-MM-DD/`
 * show up in that day as watched media, placed by the folder's `.section-order.json`, then by
 * exported file names, then in the day's default section. Moving them in KMedia writes their
 * placement back, folders for upcoming meeting days are created, and folders whose week is over
 * can be removed.
 */
class WatchedFolderMedia : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY settingsChanged FINAL)
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool deletePastFolders READ deletePastFolders WRITE setDeletePastFolders NOTIFY
                   settingsChanged FINAL)

public:
    using MeetingKindProvider = std::function<QString(const QDate&)>;
    using CoWeekProvider = std::function<bool(const QDate&)>;

    WatchedFolderMedia(CloudSyncWatcher* watcher, PlaylistModel* playlist,
                       QObject* parent = nullptr);

    void setDatabase(DatabaseManager* database);
    void setScheduleProviders(MeetingKindProvider meetingKind, CoWeekProvider coWeek);
    void setToday(const QDate& today);
    void setBlockedFolder(const QString& folder);

    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool enabled);
    [[nodiscard]] QString folder() const;
    void setFolder(const QString& folder);
    [[nodiscard]] bool deletePastFolders() const;
    void setDeletePastFolders(bool deletePast);
    [[nodiscard]] bool isActive() const;

    void reconcileAll();
    void syncDay(const QDate& date);
    void prepareDayFolders(const std::vector<QDate>& meetingDays);
    void writePlacements();

    [[nodiscard]] static QString defaultSection(const QString& meetingKind, bool coWeek);
    [[nodiscard]] static QString titleFromFileName(const QString& fileName);

signals:
    void settingsChanged();
    void dayChanged(const QString& dateIso);
    void folderConflict();

private:
    void apply();
    void scheduleDay(const QDate& date);
    void flushDays();
    [[nodiscard]] QDate today() const;
    [[nodiscard]] QString dayFolderFor(const QDate& date) const;

    QPointer<CloudSyncWatcher> m_watcher;
    QPointer<PlaylistModel> m_playlist;
    DatabaseManager* m_database{nullptr};
    MeetingKindProvider m_meetingKind;
    CoWeekProvider m_coWeek;
    QDate m_today;
    QString m_blockedFolder;
    bool m_enabled{false};
    QString m_folder;
    bool m_deletePastFolders{true};
    QSet<QDate> m_pendingDays;
    QTimer m_dayTimer;
    QTimer m_placementTimer;
};

} // namespace kmedia
