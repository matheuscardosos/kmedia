#include "media/PlaylistExporter.h"

#include "core/DatedFolderLayout.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>

namespace kmedia {

namespace {

constexpr int kConversionTimeoutMs = 5 * 60 * 1000;

bool hasSuffix(const QString& filePath, std::initializer_list<const char*> suffixes)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    for (const char* candidate : suffixes) {
        if (suffix == QLatin1StringView(candidate)) {
            return true;
        }
    }
    return false;
}

bool isConvertibleImage(const QString& filePath)
{
    return hasSuffix(filePath, {"apng", "avif", "gif", "png", "webp", "bmp", "ico", "cur", "jpg",
                                "jpeg", "jfif", "pjpeg", "pjp", "heic"});
}

bool isAudio(const QString& filePath)
{
    return hasSuffix(filePath, {"mp3", "wav", "ogg", "flac", "m4a", "aac"});
}

QString titleWithoutExtension(const QString& title, const QFileInfo& source)
{
    QString cleaned = title.trimmed();
    const QString extension = QLatin1Char('.') + source.suffix();
    if (!source.suffix().isEmpty() && cleaned.endsWith(extension, Qt::CaseInsensitive)) {
        cleaned.chop(extension.size());
    }
    return cleaned.isEmpty() ? source.completeBaseName() : cleaned;
}

bool isCurrentExport(const QFileInfo& source, const QString& destinationPath, bool converted)
{
    const QFileInfo destination(destinationPath);
    if (!destination.exists()) {
        return false;
    }
    if (converted) {
        return destination.size() > 0 && destination.lastModified() > source.lastModified();
    }
    return destination.size() == source.size();
}

QString defaultConversionCacheDir()
{
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return QDir(cacheRoot.isEmpty() ? QDir::tempPath() : cacheRoot)
        .filePath(QStringLiteral("converted"));
}

} // namespace

QString PlaylistExporter::findFfmpegExecutable(const QString& overridePath)
{
    const QString cleaned = overridePath.trimmed();
    if (!cleaned.isEmpty() && QFileInfo(cleaned).isExecutable()) {
        return QDir::cleanPath(cleaned);
    }
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

bool PlaylistExporter::shouldConvertToMp4(const QString& filePath)
{
    return isConvertibleImage(filePath) || isAudio(filePath);
}

QStringList PlaylistExporter::conversionArguments(const QString& sourcePath,
                                                  const QString& targetPath)
{
    if (isAudio(sourcePath)) {
        return {QStringLiteral("-y"),
                QStringLiteral("-i"),
                sourcePath,
                QStringLiteral("-vn"),
                QStringLiteral("-c:a"),
                QStringLiteral("aac"),
                QStringLiteral("-b:a"),
                QStringLiteral("192k"),
                QStringLiteral("-f"),
                QStringLiteral("mp4"),
                targetPath};
    }
    // Still images become short Full HD clips; both sides stay even for yuv420p.
    return {QStringLiteral("-y"),
            QStringLiteral("-loop"),
            QStringLiteral("1"),
            QStringLiteral("-f"),
            QStringLiteral("image2"),
            QStringLiteral("-i"),
            sourcePath,
            QStringLiteral("-t"),
            QString::number(kImageVideoSeconds),
            QStringLiteral("-vf"),
            QStringLiteral("scale='min(1920,iw)':'min(1080,ih)':force_original_aspect_ratio="
                           "decrease,scale=trunc(iw/2)*2:trunc(ih/2)*2"),
            QStringLiteral("-c:v"),
            QStringLiteral("libx264"),
            QStringLiteral("-pix_fmt"),
            QStringLiteral("yuv420p"),
            QStringLiteral("-r"),
            QStringLiteral("30"),
            QStringLiteral("-f"),
            QStringLiteral("mp4"),
            targetPath};
}

QString PlaylistExporter::convertToMp4(const QString& ffmpegExecutable, const QString& sourcePath,
                                       const QString& cacheDir, QString* error)
{
    const QFileInfo source(sourcePath);
    if (ffmpegExecutable.trimmed().isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("FFmpeg executable not found");
        }
        return {};
    }
    if (!source.exists()) {
        if (error != nullptr) {
            *error = QStringLiteral("Source file does not exist");
        }
        return {};
    }

    const QString targetDir = cacheDir.trimmed().isEmpty() ? defaultConversionCacheDir() : cacheDir;
    if (!QDir().mkpath(targetDir)) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to create conversion folder");
        }
        return {};
    }

    const QString pathHash = QString::fromLatin1(
        QCryptographicHash::hash(source.absoluteFilePath().toUtf8(), QCryptographicHash::Sha1)
            .toHex()
            .left(8));
    const QString targetPath = QDir(targetDir).filePath(
        source.completeBaseName() + QLatin1Char('-') + pathHash + QStringLiteral(".mp4"));
    const QFileInfo cached(targetPath);
    if (cached.exists() && cached.size() > 0 && cached.lastModified() > source.lastModified()) {
        return targetPath;
    }

    const QString partialPath = targetPath + QStringLiteral(".part");
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(ffmpegExecutable, conversionArguments(source.absoluteFilePath(), partialPath));
    const bool finished = process.waitForStarted() && process.waitForFinished(kConversionTimeoutMs);
    if (!finished) {
        process.kill();
        process.waitForFinished();
    }
    const bool succeeded = finished && process.exitStatus() == QProcess::NormalExit &&
                           process.exitCode() == 0 && QFileInfo(partialPath).size() > 0;
    if (!succeeded) {
        QFile::remove(partialPath);
        if (error != nullptr) {
            *error = QStringLiteral("FFmpeg could not convert %1").arg(source.fileName());
        }
        return {};
    }

    QFile::remove(targetPath);
    if (!QFile::rename(partialPath, targetPath)) {
        QFile::remove(partialPath);
        if (error != nullptr) {
            *error = QStringLiteral("Failed to store converted file");
        }
        return {};
    }
    return targetPath;
}

DayExportResult PlaylistExporter::exportDay(const std::vector<ExportSection>& sections,
                                            const DayExportOptions& options)
{
    DayExportResult result;
    if (options.exportRoot.trimmed().isEmpty() || !options.date.isValid()) {
        result.errorMessage = QStringLiteral("Export folder and date are required");
        return result;
    }

    const QString dayFolder = QDir(QDir::cleanPath(options.exportRoot.trimmed()))
                                  .filePath(DatedFolderLayout::dayFolderName(options.date));
    if (!QDir().mkpath(dayFolder)) {
        result.errorMessage = QStringLiteral("Failed to create export folder %1").arg(dayFolder);
        return result;
    }
    result.dayFolder = dayFolder;

    const QString ffmpeg =
        options.convertFilesToMp4 ? findFfmpegExecutable(options.ffmpegExecutable) : QString();
    QSet<QString> expectedFiles;
    int sectionNumber = 1;

    for (const ExportSection& section : sections) {
        std::vector<const ExportMediaItem*> visibleItems;
        for (const ExportMediaItem& item : section.items) {
            if (!item.hidden) {
                visibleItems.push_back(&item);
            }
        }
        if (visibleItems.empty()) {
            continue;
        }

        const int currentSection = sectionNumber++;
        const int itemCount = static_cast<int>(visibleItems.size());
        for (int index = 0; index < itemCount; ++index) {
            const ExportMediaItem& item = *visibleItems.at(static_cast<size_t>(index));
            const QFileInfo source(item.filePath);
            if (item.filePath.trimmed().isEmpty() || !source.exists()) {
                continue;
            }

            const bool convert = options.convertFilesToMp4 && shouldConvertToMp4(item.filePath);
            const QString extension =
                convert
                    ? QStringLiteral(".mp4")
                    : (source.suffix().isEmpty() ? QString() : QLatin1Char('.') + source.suffix());
            const QString songPrefix =
                item.songNumber.trimmed().isEmpty()
                    ? QString()
                    : QStringLiteral("%1 %2 - ").arg(options.songLabel, item.songNumber.trimmed());
            const QString fileName = DatedFolderLayout::exportedFileName(
                currentSection, section.label, index + 1, itemCount, songPrefix,
                titleWithoutExtension(item.title, source), extension);
            const QString destinationPath =
                DatedFolderLayout::fitPathToByteLimit(QDir(dayFolder).filePath(fileName));
            const QString destinationName = QFileInfo(destinationPath).fileName();

            if (isCurrentExport(source, destinationPath, convert)) {
                expectedFiles.insert(destinationName);
                ++result.skippedCount;
                continue;
            }

            QString sourceToCopy = source.absoluteFilePath();
            if (convert) {
                QString conversionError;
                sourceToCopy = convertToMp4(ffmpeg, item.filePath, options.conversionCacheDir,
                                            &conversionError);
                if (sourceToCopy.isEmpty()) {
                    result.conversionFailures.append(item.filePath);
                    continue;
                }
            }

            expectedFiles.insert(destinationName);
            QFile::remove(destinationPath);
            if (!QFile::copy(sourceToCopy, destinationPath)) {
                continue;
            }
            ++result.copiedCount;
            if (convert) {
                ++result.convertedCount;
            }
            result.exportedFiles.append(destinationPath);
        }
    }

    const QFileInfoList existing =
        QDir(dayFolder).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& file : existing) {
        if (file.fileName().startsWith(QLatin1Char('.')) ||
            expectedFiles.contains(file.fileName())) {
            continue;
        }
        if (QFile::remove(file.absoluteFilePath())) {
            ++result.removedCount;
        }
    }

    result.success = true;
    return result;
}

} // namespace kmedia
