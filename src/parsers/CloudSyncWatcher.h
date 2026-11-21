#pragma once

#include <QDate>
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <vector>

namespace kmedia {

/**
 * Event-driven watcher for locally synced Dropbox, OneDrive, Google Drive, and Nextcloud folders.
 * Uses QFileSystemWatcher recursively with a 500 ms per-file stability debounce and zero
 * steady-state disk-polling timers.
 */
class CloudSyncWatcher : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int debounceMs READ debounceMs WRITE setDebounceMs NOTIFY debounceMsChanged FINAL)
    Q_PROPERTY(QStringList watchedFolders READ watchedFolders NOTIFY watchedFoldersChanged FINAL)
    Q_PROPERTY(bool dayFoldersOnly READ dayFoldersOnly WRITE setDayFoldersOnly NOTIFY
                   dayFoldersOnlyChanged FINAL)

public:
    enum class CloudProvider {
        Custom = 0,
        Dropbox,
        OneDrive,
        GoogleDrive,
        Nextcloud,
    };
    Q_ENUM(CloudProvider)

    enum class MediaCategory {
        Unknown = 0,
        Video,
        Audio,
        Image,
        Pdf,
        Jwpub,
        JwlPlaylist,
    };
    Q_ENUM(MediaCategory)

    struct DetectedCloudFolder
    {
        CloudProvider provider{CloudProvider::Custom};
        QString providerName;
        QString path;
    };

    explicit CloudSyncWatcher(QObject* parent = nullptr);
    ~CloudSyncWatcher() override = default;

    Q_DISABLE_COPY_MOVE(CloudSyncWatcher)

    [[nodiscard]] int debounceMs() const;
    void setDebounceMs(int debounceMs);

    [[nodiscard]] bool dayFoldersOnly() const;
    void setDayFoldersOnly(bool dayFoldersOnly);

    [[nodiscard]] QStringList watchedFolders() const;
    [[nodiscard]] QStringList watchedDirectories() const;

    Q_INVOKABLE bool addWatchedFolder(const QString& rootDir);
    Q_INVOKABLE void removeWatchedFolder(const QString& rootDir);
    Q_INVOKABLE void clearWatchedFolders();
    Q_INVOKABLE int scanAndEmitExisting(const QString& rootDir);

    [[nodiscard]] static CloudProvider classifyProvider(const QString& folderPath);
    [[nodiscard]] static QString providerDisplayName(CloudProvider provider);
    [[nodiscard]] static MediaCategory classifyMediaFile(const QString& filePath);
    [[nodiscard]] static std::vector<DetectedCloudFolder>
    detectLocalCloudFolders(const QString& homeDirOverride = {});

signals:
    void debounceMsChanged();
    void watchedFoldersChanged();
    void dayFoldersOnlyChanged();
    void mediaDiscovered(const QString& filePath, kmedia::CloudSyncWatcher::MediaCategory category,
                         const QString& rootFolder,
                         kmedia::CloudSyncWatcher::CloudProvider provider);
    void datedMediaDiscovered(const QString& filePath, const QDate& meetingDate,
                              kmedia::CloudSyncWatcher::MediaCategory category,
                              const QString& rootFolder);
    void mediaUpdated(const QString& filePath, kmedia::CloudSyncWatcher::MediaCategory category,
                      const QString& rootFolder, kmedia::CloudSyncWatcher::CloudProvider provider);
    void mediaRemoved(const QString& filePath, const QString& rootFolder);
    void syncIssueDetected(const QString& filePath, const QString& reason);

private:
    struct TrackedFileState
    {
        qint64 size{-1};
        qint64 lastModifiedMs{0};
        QString rootFolder;
        MediaCategory category{MediaCategory::Unknown};
    };

    struct PendingChange
    {
        qint64 readyAtMs{0};
        qint64 observedSize{-1};
        qint64 observedModifiedMs{0};
        int retryCount{0};
        QString rootFolder;
        bool knownExisting{false};
    };

    void onDirectoryChanged(const QString& dirPath);
    void onFileChanged(const QString& filePath);
    void processDebounceQueue();

    void registerDirectoryTree(const QString& rootFolder, const QString& currentDir,
                               bool recordBaselineOnly);
    void unregisterSubtree(const QString& dirPrefix);
    void reconcileDirectory(const QString& dirPath, const QString& rootFolder);
    void schedulePendingCheck(const QString& filePath, const QString& rootFolder, qint64 size,
                              qint64 modifiedMs, bool knownExisting);
    void armDebounceTimer();
    [[nodiscard]] QString rootForPath(const QString& path) const;
    [[nodiscard]] bool isPathUnderWatchedRoot(const QString& path) const;
    [[nodiscard]] bool shouldWatchDirectory(const QString& rootFolder,
                                            const QString& dirPath) const;
    [[nodiscard]] bool shouldTrackFilesIn(const QString& rootFolder, const QString& dirPath) const;
    [[nodiscard]] bool checkArchiveOrMediaReadiness(const QString& filePath, MediaCategory category,
                                                    QString* reason) const;

    QFileSystemWatcher m_watcher;
    QTimer m_debounceTimer;
    QElapsedTimer m_clock;
    int m_debounceMs{500};
    bool m_dayFoldersOnly{false};

    QStringList m_roots;
    QSet<QString> m_registeredDirs;
    QSet<QString> m_registeredFiles;
    QHash<QString, TrackedFileState> m_trackedFiles;
    QHash<QString, PendingChange> m_pending;
};

} // namespace kmedia
