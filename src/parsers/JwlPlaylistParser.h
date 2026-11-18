#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <utility>
#include <vector>

namespace kmedia {

struct JwlPlaylistManifest
{
    QString name;
    QString creationDate;
    QString lastModifiedDate;
    QString databaseName{QStringLiteral("userData.db")};
    QString hash;
};

struct JwlPlaylistEntry
{
    qint64 playlistItemId{0};
    int position{0};
    QString label;
    QString originalFilename;
    QString mediaArchivePath;
    QString thumbnailArchivePath;
    QString mimeType;
    QString hash;
    qint64 durationMs{0};
    qint64 trimStartMs{0};
    qint64 trimEndMs{0};
    int endAction{0};
    bool repeatLoop{false};
    int accuracy{0};
    QString keySymbol;
    int issueTagNumber{0};
    int mepsLanguage{0};
    qint64 documentId{0};
    int track{0};
    int bookNumber{0};
    int chapterNumber{0};
    int locationType{0};
    QString locationTitle;
    QStringList markerLabels;
};

struct JwlPlaylistPackage
{
    bool valid{false};
    QString archivePath;
    QString playlistName;
    JwlPlaylistManifest manifest;
    std::vector<JwlPlaylistEntry> entries;
    QStringList archiveFiles;
    QString errorMessage;
};

/**
 * Parser and safe selective extractor for JW Library `.jwlplaylist` SQLite archives.
 */
class JwlPlaylistParser
{
public:
    [[nodiscard]] static JwlPlaylistPackage parse(const QString& jwlPlaylistPath);

    [[nodiscard]] static bool extractAssets(const QString& jwlPlaylistPath, const QString& destDir,
                                            const std::vector<QString>& requestedArchivePaths = {},
                                            std::vector<QString>* extractedPaths = nullptr,
                                            QString* error = nullptr);

    [[nodiscard]] static qint64 ticksToMs(qint64 ticks);
    [[nodiscard]] static std::pair<qint64, qint64> computeTrimRangeMs(qint64 baseDurationTicks,
                                                                      qint64 itemDurationTicks,
                                                                      qint64 startTrimOffsetTicks,
                                                                      qint64 endTrimOffsetTicks);
};

} // namespace kmedia
