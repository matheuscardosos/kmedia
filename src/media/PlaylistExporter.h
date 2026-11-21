#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

#include <vector>

namespace kmedia {

struct ExportMediaItem
{
    QString filePath;
    QString title;
    QString songNumber;
    bool hidden{false};
};

struct ExportSection
{
    QString label;
    std::vector<ExportMediaItem> items;
};

struct DayExportOptions
{
    QDate date;
    QString exportRoot;
    QString songLabel{QStringLiteral("Song")};
    bool convertFilesToMp4{false};
    QString ffmpegExecutable;
    QString conversionCacheDir;
};

struct DayExportResult
{
    bool success{false};
    QString dayFolder;
    int copiedCount{0};
    int skippedCount{0};
    int convertedCount{0};
    int removedCount{0};
    QStringList exportedFiles;
    QStringList conversionFailures;
    QString errorMessage;
};

/**
 * Exports one meeting day into `<root>/YYYY-MM-DD/` with `"<NN> <Section> - <NN> <Title>.<ext>"`
 * names, skips files that are already current, removes files that no longer belong to the day,
 * and can turn images and audio into MP4 videos through FFmpeg. The work blocks on disk and
 * process I/O, so callers run it on a worker thread.
 */
class PlaylistExporter
{
public:
    PlaylistExporter() = delete;

    static constexpr int kImageVideoSeconds = 5;

    [[nodiscard]] static QString findFfmpegExecutable(const QString& overridePath = {});
    [[nodiscard]] static bool shouldConvertToMp4(const QString& filePath);
    [[nodiscard]] static QStringList conversionArguments(const QString& sourcePath,
                                                         const QString& targetPath);
    [[nodiscard]] static QString convertToMp4(const QString& ffmpegExecutable,
                                              const QString& sourcePath, const QString& cacheDir,
                                              QString* error = nullptr);
    [[nodiscard]] static DayExportResult exportDay(const std::vector<ExportSection>& sections,
                                                   const DayExportOptions& options);
};

} // namespace kmedia
