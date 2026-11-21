#include "parsers/CloudSyncWatcher.h"

#include "core/DatedFolderLayout.h"
#include "media/PlaybackController.h"
#include "parsers/ZipArchiveReader.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <algorithm>

namespace kmedia {

namespace {

constexpr int kMaxWatchedFiles = 512;
constexpr int kMaxSettlementRetries = 4;

QString cleanNormalizedPath(const QString& rawPath)
{
    if (rawPath.trimmed().isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(rawPath).absoluteFilePath());
}

bool isChildOrEqualPath(const QString& candidate, const QString& root)
{
    return candidate == root || candidate.startsWith(root + QLatin1Char('/'));
}

} // namespace

CloudSyncWatcher::CloudSyncWatcher(QObject* parent) : QObject(parent)
{
    m_clock.start();
    m_debounceTimer.setSingleShot(true);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            &CloudSyncWatcher::onDirectoryChanged);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &CloudSyncWatcher::onFileChanged);
    connect(&m_debounceTimer, &QTimer::timeout, this, &CloudSyncWatcher::processDebounceQueue);
}

int CloudSyncWatcher::debounceMs() const
{
    return m_debounceMs;
}

void CloudSyncWatcher::setDebounceMs(int debounceMs)
{
    const int clamped = qBound(20, debounceMs, 10000);
    if (m_debounceMs == clamped) {
        return;
    }
    m_debounceMs = clamped;
    emit debounceMsChanged();
}

bool CloudSyncWatcher::dayFoldersOnly() const
{
    return m_dayFoldersOnly;
}

void CloudSyncWatcher::setDayFoldersOnly(bool dayFoldersOnly)
{
    if (m_dayFoldersOnly == dayFoldersOnly) {
        return;
    }
    m_dayFoldersOnly = dayFoldersOnly;
    const QStringList roots = m_roots;
    clearWatchedFolders();
    for (const QString& root : roots) {
        (void)addWatchedFolder(root);
    }
    emit dayFoldersOnlyChanged();
}

QStringList CloudSyncWatcher::watchedFolders() const
{
    return m_roots;
}

QStringList CloudSyncWatcher::watchedDirectories() const
{
    return m_registeredDirs.values();
}

CloudSyncWatcher::CloudProvider CloudSyncWatcher::classifyProvider(const QString& folderPath)
{
    const QString lower = folderPath.toLower();
    if (lower.contains(QStringLiteral("dropbox"))) {
        return CloudProvider::Dropbox;
    }
    if (lower.contains(QStringLiteral("onedrive"))) {
        return CloudProvider::OneDrive;
    }
    if (lower.contains(QStringLiteral("google drive")) ||
        lower.contains(QStringLiteral("googledrive")) ||
        lower.contains(QStringLiteral("my drive"))) {
        return CloudProvider::GoogleDrive;
    }
    if (lower.contains(QStringLiteral("nextcloud"))) {
        return CloudProvider::Nextcloud;
    }
    return CloudProvider::Custom;
}

QString CloudSyncWatcher::providerDisplayName(CloudProvider provider)
{
    switch (provider) {
    case CloudProvider::Dropbox:
        return QStringLiteral("Dropbox");
    case CloudProvider::OneDrive:
        return QStringLiteral("OneDrive");
    case CloudProvider::GoogleDrive:
        return QStringLiteral("Google Drive");
    case CloudProvider::Nextcloud:
        return QStringLiteral("Nextcloud");
    case CloudProvider::Custom:
        return QStringLiteral("Custom");
    }
    return QStringLiteral("Custom");
}

CloudSyncWatcher::MediaCategory CloudSyncWatcher::classifyMediaFile(const QString& filePath)
{
    const QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == QStringLiteral("jwpub")) {
        return MediaCategory::Jwpub;
    }
    if (ext == QStringLiteral("jwlplaylist")) {
        return MediaCategory::JwlPlaylist;
    }

    switch (PlaybackController::detectMediaKind(QUrl::fromLocalFile(filePath), false)) {
    case PlaybackController::MediaKind::Video:
        return MediaCategory::Video;
    case PlaybackController::MediaKind::Audio:
        return MediaCategory::Audio;
    case PlaybackController::MediaKind::Image:
        return MediaCategory::Image;
    case PlaybackController::MediaKind::Pdf:
        return MediaCategory::Pdf;
    case PlaybackController::MediaKind::None:
        return MediaCategory::Unknown;
    }
    return MediaCategory::Unknown;
}

std::vector<CloudSyncWatcher::DetectedCloudFolder>
CloudSyncWatcher::detectLocalCloudFolders(const QString& homeDirOverride)
{
    const QString baseHome = homeDirOverride.trimmed().isEmpty()
                                 ? QDir::homePath()
                                 : cleanNormalizedPath(homeDirOverride);
    const QDir home(baseHome);

    const std::vector<std::pair<QString, CloudProvider>> candidates = {
        {QStringLiteral("Dropbox"), CloudProvider::Dropbox},
        {QStringLiteral("OneDrive"), CloudProvider::OneDrive},
        {QStringLiteral("Google Drive"), CloudProvider::GoogleDrive},
        {QStringLiteral("My Drive"), CloudProvider::GoogleDrive},
        {QStringLiteral("Nextcloud"), CloudProvider::Nextcloud},
    };

    std::vector<DetectedCloudFolder> detected;
    QSet<QString> seenPaths;
    for (const auto& [dirName, provider] : candidates) {
        const QString fullPath = cleanNormalizedPath(home.filePath(dirName));
        if (!fullPath.isEmpty() && !seenPaths.contains(fullPath) && QDir(fullPath).exists()) {
            seenPaths.insert(fullPath);
            DetectedCloudFolder entry;
            entry.provider = provider;
            entry.providerName = providerDisplayName(provider);
            entry.path = fullPath;
            detected.push_back(std::move(entry));
        }
    }
    return detected;
}

bool CloudSyncWatcher::addWatchedFolder(const QString& rootDir)
{
    const QString cleanRoot = cleanNormalizedPath(rootDir);
    if (cleanRoot.isEmpty() || !QFileInfo(cleanRoot).isDir()) {
        return false;
    }
    if (m_roots.contains(cleanRoot)) {
        return true;
    }

    m_roots.append(cleanRoot);
    registerDirectoryTree(cleanRoot, cleanRoot, true);
    emit watchedFoldersChanged();
    return true;
}

void CloudSyncWatcher::removeWatchedFolder(const QString& rootDir)
{
    const QString cleanRoot = cleanNormalizedPath(rootDir);
    if (!m_roots.removeOne(cleanRoot)) {
        return;
    }
    unregisterSubtree(cleanRoot);
    emit watchedFoldersChanged();
}

void CloudSyncWatcher::clearWatchedFolders()
{
    m_debounceTimer.stop();
    m_pending.clear();
    m_trackedFiles.clear();

    if (!m_registeredDirs.isEmpty()) {
        m_watcher.removePaths(m_registeredDirs.values());
        m_registeredDirs.clear();
    }
    if (!m_registeredFiles.isEmpty()) {
        m_watcher.removePaths(m_registeredFiles.values());
        m_registeredFiles.clear();
    }
    if (!m_roots.isEmpty()) {
        m_roots.clear();
        emit watchedFoldersChanged();
    }
}

int CloudSyncWatcher::scanAndEmitExisting(const QString& rootDir)
{
    const QString cleanRoot = cleanNormalizedPath(rootDir);
    if (!m_roots.contains(cleanRoot)) {
        return 0;
    }

    int count = 0;
    const CloudProvider provider = classifyProvider(cleanRoot);
    for (auto it = m_trackedFiles.constBegin(); it != m_trackedFiles.constEnd(); ++it) {
        if (it.value().rootFolder == cleanRoot) {
            emit mediaDiscovered(it.key(), it.value().category, cleanRoot, provider);
            const QDate meetingDate = DatedFolderLayout::dateForFile(it.key());
            if (meetingDate.isValid()) {
                emit datedMediaDiscovered(it.key(), meetingDate, it.value().category, cleanRoot);
            }
            ++count;
        }
    }
    return count;
}

bool CloudSyncWatcher::shouldWatchDirectory(const QString& rootFolder, const QString& dirPath) const
{
    if (!m_dayFoldersOnly || dirPath == rootFolder) {
        return true;
    }
    // Only `YYYY-MM-DD` folders directly under the root hold meeting day media.
    const QFileInfo dirInfo(dirPath);
    const QString name = dirInfo.fileName();
    return cleanNormalizedPath(dirInfo.absolutePath()) == rootFolder &&
           DatedFolderLayout::isDayFolderName(name) &&
           !DatedFolderLayout::isLongPastDayFolder(name, QDate::currentDate());
}

bool CloudSyncWatcher::shouldTrackFilesIn(const QString& rootFolder, const QString& dirPath) const
{
    if (!m_dayFoldersOnly) {
        return true;
    }
    return dirPath != rootFolder && shouldWatchDirectory(rootFolder, dirPath);
}

void CloudSyncWatcher::registerDirectoryTree(const QString& rootFolder, const QString& currentDir,
                                             bool recordBaselineOnly)
{
    auto watchDir = [this](const QString& dirPath) {
        if (!m_registeredDirs.contains(dirPath) && m_watcher.addPath(dirPath)) {
            m_registeredDirs.insert(dirPath);
        }
    };

    auto trackFile = [&](const QString& entryPath, const QFileInfo& info) {
        const MediaCategory category = classifyMediaFile(entryPath);
        if (category == MediaCategory::Unknown) {
            return;
        }

        const qint64 modifiedMs = info.lastModified().toMSecsSinceEpoch();
        if (recordBaselineOnly) {
            TrackedFileState state;
            state.size = info.size();
            state.lastModifiedMs = modifiedMs;
            state.rootFolder = rootFolder;
            state.category = category;
            m_trackedFiles.insert(entryPath, state);
            if (m_registeredFiles.size() < kMaxWatchedFiles &&
                !m_registeredFiles.contains(entryPath) && m_watcher.addPath(entryPath)) {
                m_registeredFiles.insert(entryPath);
            }
        } else {
            schedulePendingCheck(entryPath, rootFolder, info.size(), modifiedMs,
                                 m_trackedFiles.contains(entryPath));
        }
    };

    if (!shouldWatchDirectory(rootFolder, currentDir)) {
        return;
    }
    watchDir(currentDir);

    if (m_dayFoldersOnly) {
        const QDir dir(currentDir);
        if (currentDir == rootFolder) {
            const QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo& subInfo : subDirs) {
                const QString subPath = cleanNormalizedPath(subInfo.absoluteFilePath());
                if (shouldWatchDirectory(rootFolder, subPath)) {
                    registerDirectoryTree(rootFolder, subPath, recordBaselineOnly);
                }
            }
            return;
        }
        const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fileInfo : files) {
            trackFile(cleanNormalizedPath(fileInfo.absoluteFilePath()), fileInfo);
        }
        return;
    }

    QDirIterator it(currentDir, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString entryPath = cleanNormalizedPath(it.next());
        const QFileInfo info = it.fileInfo();
        if (info.isDir()) {
            watchDir(entryPath);
            continue;
        }
        trackFile(entryPath, info);
    }
}

void CloudSyncWatcher::unregisterSubtree(const QString& dirPrefix)
{
    QStringList dirsToRemove;
    for (const QString& d : m_registeredDirs) {
        if (isChildOrEqualPath(d, dirPrefix)) {
            dirsToRemove.append(d);
        }
    }
    if (!dirsToRemove.isEmpty()) {
        m_watcher.removePaths(dirsToRemove);
        for (const QString& d : dirsToRemove) {
            m_registeredDirs.remove(d);
        }
    }

    QStringList filesToUnwatch;
    for (const QString& f : m_registeredFiles) {
        if (isChildOrEqualPath(f, dirPrefix)) {
            filesToUnwatch.append(f);
        }
    }
    if (!filesToUnwatch.isEmpty()) {
        m_watcher.removePaths(filesToUnwatch);
        for (const QString& f : filesToUnwatch) {
            m_registeredFiles.remove(f);
        }
    }

    for (auto it = m_trackedFiles.begin(); it != m_trackedFiles.end();) {
        if (isChildOrEqualPath(it.key(), dirPrefix)) {
            it = m_trackedFiles.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (isChildOrEqualPath(it.key(), dirPrefix)) {
            it = m_pending.erase(it);
        } else {
            ++it;
        }
    }
}

void CloudSyncWatcher::onDirectoryChanged(const QString& dirPath)
{
    const QString cleanDir = cleanNormalizedPath(dirPath);
    const QString rootFolder = rootForPath(cleanDir);
    if (rootFolder.isEmpty()) {
        return;
    }

    if (!QFileInfo(cleanDir).isDir()) {
        // Handle deleted watched directory subtree and emit removal events cleanly.
        QStringList removedFiles;
        for (auto it = m_trackedFiles.constBegin(); it != m_trackedFiles.constEnd(); ++it) {
            if (isChildOrEqualPath(it.key(), cleanDir)) {
                removedFiles.append(it.key());
            }
        }
        unregisterSubtree(cleanDir);
        for (const QString& removed : removedFiles) {
            emit mediaRemoved(removed, rootFolder);
        }
        return;
    }

    reconcileDirectory(cleanDir, rootFolder);
}

void CloudSyncWatcher::onFileChanged(const QString& filePath)
{
    const QString cleanFile = cleanNormalizedPath(filePath);
    const QString rootFolder = rootForPath(cleanFile);
    if (rootFolder.isEmpty()) {
        return;
    }

    const QFileInfo info(cleanFile);
    if (!info.exists()) {
        if (m_registeredFiles.remove(cleanFile)) {
            m_watcher.removePath(cleanFile);
        }
        m_pending.remove(cleanFile);
        if (m_trackedFiles.remove(cleanFile)) {
            emit mediaRemoved(cleanFile, rootFolder);
        }
        return;
    }

    if (classifyMediaFile(cleanFile) == MediaCategory::Unknown) {
        return;
    }
    schedulePendingCheck(cleanFile, rootFolder, info.size(),
                         info.lastModified().toMSecsSinceEpoch(),
                         m_trackedFiles.contains(cleanFile));
}

void CloudSyncWatcher::reconcileDirectory(const QString& dirPath, const QString& rootFolder)
{
    const QDir dir(dirPath);
    const QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& subInfo : subDirs) {
        const QString subPath = cleanNormalizedPath(subInfo.absoluteFilePath());
        if (!m_registeredDirs.contains(subPath) && shouldWatchDirectory(rootFolder, subPath)) {
            registerDirectoryTree(rootFolder, subPath, false);
        }
    }
    if (!shouldTrackFilesIn(rootFolder, dirPath)) {
        return;
    }

    QSet<QString> currentMediaFiles;
    const QFileInfoList fileEntries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fileInfo : fileEntries) {
        const QString filePath = cleanNormalizedPath(fileInfo.absoluteFilePath());
        if (classifyMediaFile(filePath) == MediaCategory::Unknown) {
            continue;
        }
        currentMediaFiles.insert(filePath);

        const qint64 modifiedMs = fileInfo.lastModified().toMSecsSinceEpoch();
        const qint64 size = fileInfo.size();
        const auto existingIt = m_trackedFiles.constFind(filePath);
        if (existingIt == m_trackedFiles.constEnd()) {
            schedulePendingCheck(filePath, rootFolder, size, modifiedMs, false);
        } else if (existingIt->size != size || existingIt->lastModifiedMs != modifiedMs) {
            schedulePendingCheck(filePath, rootFolder, size, modifiedMs, true);
        }
    }

    QStringList removedHere;
    for (auto it = m_trackedFiles.constBegin(); it != m_trackedFiles.constEnd(); ++it) {
        const QString trackedParent = cleanNormalizedPath(QFileInfo(it.key()).absolutePath());
        if (trackedParent == dirPath && !currentMediaFiles.contains(it.key())) {
            removedHere.append(it.key());
        }
    }
    for (const QString& removed : removedHere) {
        m_pending.remove(removed);
        if (m_registeredFiles.remove(removed)) {
            m_watcher.removePath(removed);
        }
        m_trackedFiles.remove(removed);
        emit mediaRemoved(removed, rootFolder);
    }
}

void CloudSyncWatcher::schedulePendingCheck(const QString& filePath, const QString& rootFolder,
                                            qint64 size, qint64 modifiedMs, bool knownExisting)
{
    PendingChange change = m_pending.value(filePath);
    change.readyAtMs = m_clock.elapsed() + m_debounceMs;
    change.observedSize = size;
    change.observedModifiedMs = modifiedMs;
    change.rootFolder = rootFolder;
    change.knownExisting = knownExisting || m_trackedFiles.contains(filePath);
    m_pending.insert(filePath, change);
    armDebounceTimer();
}

void CloudSyncWatcher::armDebounceTimer()
{
    if (m_pending.isEmpty()) {
        m_debounceTimer.stop();
        return;
    }

    const qint64 nowMs = m_clock.elapsed();
    qint64 nextDelay = m_debounceMs;
    for (const PendingChange& change : m_pending) {
        nextDelay = std::min(nextDelay, std::max<qint64>(1, change.readyAtMs - nowMs));
    }
    m_debounceTimer.start(static_cast<int>(nextDelay));
}

void CloudSyncWatcher::processDebounceQueue()
{
    const qint64 nowMs = m_clock.elapsed();
    const QStringList keys = m_pending.keys();

    for (const QString& filePath : keys) {
        PendingChange change = m_pending.value(filePath);
        if (change.readyAtMs > nowMs) {
            continue;
        }

        const QFileInfo info(filePath);
        if (!info.exists()) {
            m_pending.remove(filePath);
            if (m_trackedFiles.remove(filePath)) {
                emit mediaRemoved(filePath, change.rootFolder);
            }
            continue;
        }

        const qint64 curSize = info.size();
        const qint64 curModified = info.lastModified().toMSecsSinceEpoch();
        if (curSize != change.observedSize || curModified != change.observedModifiedMs) {
            change.observedSize = curSize;
            change.observedModifiedMs = curModified;
            change.readyAtMs = nowMs + m_debounceMs;
            change.retryCount = 0;
            m_pending.insert(filePath, change);
            continue;
        }

        const MediaCategory category = classifyMediaFile(filePath);
        if (category == MediaCategory::Unknown) {
            m_pending.remove(filePath);
            continue;
        }

        QString issueReason;
        if (!checkArchiveOrMediaReadiness(filePath, category, &issueReason)) {
            ++change.retryCount;
            if (change.retryCount <= kMaxSettlementRetries) {
                change.readyAtMs = nowMs + m_debounceMs;
                m_pending.insert(filePath, change);
            } else {
                m_pending.remove(filePath);
                emit syncIssueDetected(filePath, issueReason);
            }
            continue;
        }

        m_pending.remove(filePath);

        TrackedFileState state;
        state.size = curSize;
        state.lastModifiedMs = curModified;
        state.rootFolder = change.rootFolder;
        state.category = category;
        m_trackedFiles.insert(filePath, state);

        if (m_registeredFiles.size() < kMaxWatchedFiles && !m_registeredFiles.contains(filePath) &&
            m_watcher.addPath(filePath)) {
            m_registeredFiles.insert(filePath);
        }

        const CloudProvider provider = classifyProvider(change.rootFolder);
        if (change.knownExisting) {
            emit mediaUpdated(filePath, category, change.rootFolder, provider);
        } else {
            emit mediaDiscovered(filePath, category, change.rootFolder, provider);
            const QDate meetingDate = DatedFolderLayout::dateForFile(filePath);
            if (meetingDate.isValid()) {
                emit datedMediaDiscovered(filePath, meetingDate, category, change.rootFolder);
            }
        }
    }

    armDebounceTimer();
}

bool CloudSyncWatcher::checkArchiveOrMediaReadiness(const QString& filePath, MediaCategory category,
                                                    QString* reason) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (reason != nullptr) {
            *reason = QStringLiteral("File is currently locked or unavailable for reading");
        }
        return false;
    }

    if (file.size() <= 0) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Cloud placeholder file is still empty (0 bytes)");
        }
        return false;
    }
    file.close();

    if (category == MediaCategory::Jwpub || category == MediaCategory::JwlPlaylist) {
        ZipArchiveReader zip;
        QString zipError;
        if (!zip.openFile(filePath, &zipError) || zip.listEntries({}, &zipError).empty()) {
            if (reason != nullptr) {
                *reason =
                    zipError.isEmpty() ? QStringLiteral("Cloud archive still syncing") : zipError;
            }
            return false;
        }
    }
    return true;
}

QString CloudSyncWatcher::rootForPath(const QString& path) const
{
    for (const QString& root : m_roots) {
        if (isChildOrEqualPath(path, root)) {
            return root;
        }
    }
    return {};
}

bool CloudSyncWatcher::isPathUnderWatchedRoot(const QString& path) const
{
    return !rootForPath(path).isEmpty();
}

} // namespace kmedia
