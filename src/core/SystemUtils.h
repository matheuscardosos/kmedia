#pragma once

#include <QString>
#include <QtGlobal>

namespace kmedia {

class SystemUtils
{
public:
    SystemUtils() = delete;

    [[nodiscard]] static QString standardAppDataDir();
    [[nodiscard]] static QString standardCacheDir();
    [[nodiscard]] static qint64 availableDiskBytes(const QString& path);
    [[nodiscard]] static qint64 processMemoryRssBytes();
    [[nodiscard]] static QString sha256File(const QString& filePath);
    [[nodiscard]] static QString sha256Hex(const QByteArray& data);
    [[nodiscard]] static QString sanitizeFileName(const QString& rawName);
    [[nodiscard]] static QString formatFileSize(qint64 bytes);
    [[nodiscard]] static QString formatDurationMs(qint64 durationMs);
};

} // namespace kmedia
