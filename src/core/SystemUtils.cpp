#include "core/SystemUtils.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#elif defined(Q_OS_MACOS)
#include <mach/mach.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#endif

namespace kmedia {

namespace {

constexpr qint64 kHashChunkBytes = 64 * 1024;

} // namespace

QString SystemUtils::standardAppDataDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.kmedia");
    }
    return QDir::cleanPath(base);
}

QString SystemUtils::standardCacheDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath() + QStringLiteral("/kmedia-cache");
    }
    return QDir::cleanPath(base);
}

qint64 SystemUtils::availableDiskBytes(const QString& path)
{
    if (path.isEmpty()) {
        return -1;
    }

    QString targetPath = QDir::cleanPath(path);
    while (!QDir(targetPath).exists()) {
        const QString parent = QDir(targetPath).absoluteFilePath(QStringLiteral(".."));
        const QString cleanedParent = QDir::cleanPath(parent);
        if (cleanedParent == targetPath) {
            break;
        }
        targetPath = cleanedParent;
    }

    const QStorageInfo storage(targetPath);
    if (!storage.isValid() || !storage.isReady()) {
        return -1;
    }
    return storage.bytesAvailable();
}

qint64 SystemUtils::processPrivateMemoryBytes()
{
#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof(counters)) != FALSE) {
        return static_cast<qint64>(counters.PrivateUsage);
    }
    return 0;
#else
    return processMemoryRssBytes();
#endif
}

qint64 SystemUtils::processMemoryRssBytes()
{
#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof(counters)) != FALSE) {
        return static_cast<qint64>(counters.WorkingSetSize);
    }
    return 0;
#elif defined(Q_OS_MACOS)
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info),
                  &count) == KERN_SUCCESS) {
        return static_cast<qint64>(info.resident_size);
    }
    return 0;
#elif defined(Q_OS_LINUX)
    QFile statm(QStringLiteral("/proc/self/statm"));
    if (statm.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QList<QByteArray> parts = statm.readLine().trimmed().split(' ');
        if (parts.size() >= 2) {
            bool ok = false;
            const qint64 pages = parts.at(1).toLongLong(&ok);
            const long pageSize = sysconf(_SC_PAGESIZE);
            if (ok && pages >= 0 && pageSize > 0) {
                return pages * static_cast<qint64>(pageSize);
            }
        }
    }
    return 0;
#else
    return 0;
#endif
}

QString SystemUtils::sha256File(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hasher(QCryptographicHash::Sha256);
    QByteArray buffer;
    buffer.resize(static_cast<qsizetype>(kHashChunkBytes));
    while (!file.atEnd()) {
        const qint64 readBytes = file.read(buffer.data(), kHashChunkBytes);
        if (readBytes < 0) {
            return {};
        }
        if (readBytes > 0) {
            hasher.addData(QByteArrayView(buffer.constData(), static_cast<qsizetype>(readBytes)));
        }
    }
    return QString::fromLatin1(hasher.result().toHex());
}

QString SystemUtils::sha256Hex(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString SystemUtils::sanitizeFileName(const QString& rawName)
{
    static const QRegularExpression kInvalidChars(QStringLiteral(R"([<>:"/\\|?*\x00-\x1F])"));
    QString cleaned = rawName.trimmed();
    cleaned.replace(kInvalidChars, QStringLiteral("_"));
    while (cleaned.endsWith(QLatin1Char('.')) || cleaned.endsWith(QLatin1Char(' '))) {
        cleaned.chop(1);
    }
    if (cleaned.isEmpty() || cleaned == QStringLiteral(".") || cleaned == QStringLiteral("..")) {
        return QStringLiteral("item");
    }
    return cleaned;
}

QString SystemUtils::formatFileSize(qint64 bytes)
{
    if (bytes < 0) {
        return QStringLiteral("0 B");
    }
    static constexpr const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 4) {
        value /= 1024.0;
        ++unitIndex;
    }
    if (unitIndex == 0) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', 1),
                                       QLatin1StringView(kUnits[unitIndex]));
}

QString SystemUtils::formatDurationMs(qint64 durationMs)
{
    const qint64 totalSeconds = qMax<qint64>(0, durationMs / 1000);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

} // namespace kmedia
