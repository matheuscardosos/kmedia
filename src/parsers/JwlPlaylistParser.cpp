#include "parsers/JwlPlaylistParser.h"

#include "parsers/SqlitePackageUtils.h"
#include "parsers/ZipArchiveReader.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QTemporaryDir>

namespace kmedia {

namespace {

constexpr qint64 kTicksPerMillisecond = 10000;
constexpr int kEndActionLoop = 3;

JwlPlaylistManifest parseManifestJson(const QByteArray& rawJson)
{
    JwlPlaylistManifest manifest;
    const QJsonDocument doc = QJsonDocument::fromJson(rawJson);
    if (!doc.isObject()) {
        return manifest;
    }

    const QJsonObject root = doc.object();
    manifest.name = root.value(QStringLiteral("name")).toString().trimmed();
    manifest.creationDate = root.value(QStringLiteral("creationDate")).toString().trimmed();
    manifest.lastModifiedDate = root.value(QStringLiteral("lastModifiedDate")).toString().trimmed();

    const QJsonObject backup = root.value(QStringLiteral("userDataBackup")).toObject();
    if (const QString dbName = backup.value(QStringLiteral("databaseName")).toString().trimmed();
        !dbName.isEmpty()) {
        manifest.databaseName = dbName;
    }
    manifest.hash = backup.value(QStringLiteral("hash")).toString().trimmed();
    if (manifest.hash.isEmpty()) {
        manifest.hash = root.value(QStringLiteral("hash")).toString().trimmed();
    }
    return manifest;
}

QHash<qint64, QStringList> readMarkerLabels(const ScopedReadOnlySqliteDb& db)
{
    QHash<qint64, QStringList> markers;
    const QSet<QString> cols = db.tableColumns(QStringLiteral("PlaylistItemMarker"));
    if (!cols.contains(QStringLiteral("PlaylistItemId")) ||
        !cols.contains(QStringLiteral("Label"))) {
        return markers;
    }

    QSqlQuery query(db.database());
    if (!query.exec(QStringLiteral(
            "SELECT PlaylistItemId, Label FROM PlaylistItemMarker ORDER BY PlaylistItemId ASC"))) {
        return markers;
    }
    while (query.next()) {
        const qint64 itemId = query.value(0).toLongLong();
        const QString label = query.value(1).toString().trimmed();
        if (!label.isEmpty()) {
            markers[itemId].append(label);
        }
    }
    return markers;
}

QString colOrFallback(const QString& alias, const QSet<QString>& cols, const QString& col,
                      const QString& fallback)
{
    return cols.contains(col) ? QStringLiteral("%1.%2").arg(alias, col) : fallback;
}

} // namespace

qint64 JwlPlaylistParser::ticksToMs(qint64 ticks)
{
    return ticks > 0 ? (ticks / kTicksPerMillisecond) : 0;
}

std::pair<qint64, qint64> JwlPlaylistParser::computeTrimRangeMs(qint64 baseDurationTicks,
                                                                qint64 itemDurationTicks,
                                                                qint64 startTrimOffsetTicks,
                                                                qint64 endTrimOffsetTicks)
{
    const qint64 durationTicks =
        baseDurationTicks > 0 ? baseDurationTicks : (itemDurationTicks > 0 ? itemDurationTicks : 0);
    const qint64 trimStartMs = startTrimOffsetTicks >= 0 ? ticksToMs(startTrimOffsetTicks) : 0;

    qint64 trimEndMs = 0;
    if (durationTicks > 0 && endTrimOffsetTicks > 0 && durationTicks >= endTrimOffsetTicks) {
        trimEndMs = ticksToMs(durationTicks - endTrimOffsetTicks);
    }
    return {trimStartMs, trimEndMs};
}

JwlPlaylistPackage JwlPlaylistParser::parse(const QString& jwlPlaylistPath)
{
    JwlPlaylistPackage pkg;
    pkg.archivePath = jwlPlaylistPath;

    ZipArchiveReader zip;
    QString error;
    if (!zip.openFile(jwlPlaylistPath, &error)) {
        pkg.errorMessage = error;
        return pkg;
    }

    const std::vector<ZipEntryInfo> entries = zip.listEntries({}, &error);
    if (entries.empty()) {
        pkg.errorMessage = error.isEmpty() ? QStringLiteral("Empty jwlplaylist archive") : error;
        return pkg;
    }

    bool hasManifest = false;
    for (const ZipEntryInfo& entry : entries) {
        if (entry.isDirectory) {
            continue;
        }
        pkg.archiveFiles.append(entry.name);
        if (entry.name == QStringLiteral("manifest.json")) {
            hasManifest = true;
        }
    }

    if (hasManifest) {
        const std::optional<QByteArray> rawManifest =
            zip.readEntry(QStringLiteral("manifest.json"), 1024 * 1024);
        if (rawManifest.has_value()) {
            pkg.manifest = parseManifestJson(*rawManifest);
        }
    }

    QString dbEntry;
    if (pkg.archiveFiles.contains(pkg.manifest.databaseName)) {
        dbEntry = pkg.manifest.databaseName;
    } else {
        for (const QString& fileName : pkg.archiveFiles) {
            if (fileName.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive)) {
                dbEntry = fileName;
                break;
            }
        }
    }

    if (dbEntry.isEmpty()) {
        pkg.errorMessage = QStringLiteral("jwlplaylist archive does not contain a SQLite database");
        return pkg;
    }

    QTemporaryDir sandbox;
    sandbox.setAutoRemove(true);
    if (!sandbox.isValid()) {
        pkg.errorMessage = QStringLiteral("Failed to create temporary jwlplaylist sandbox");
        return pkg;
    }

    std::vector<QString> extractedDb;
    if (!zip.extractEntries(sandbox.path(), {dbEntry}, {}, &extractedDb, &error) ||
        extractedDb.empty()) {
        pkg.errorMessage =
            error.isEmpty() ? QStringLiteral("Failed to extract playlist SQLite DB") : error;
        return pkg;
    }

    ScopedReadOnlySqliteDb db;
    if (!db.open(extractedDb.front(), &error)) {
        pkg.errorMessage = error;
        return pkg;
    }

    pkg.playlistName = pkg.manifest.name;
    if (pkg.playlistName.isEmpty() && db.tableExists(QStringLiteral("Tag"))) {
        const QSet<QString> tagCols = db.tableColumns(QStringLiteral("Tag"));
        if (tagCols.contains(QStringLiteral("Name"))) {
            QSqlQuery tagQuery(db.database());
            const QString orderCol = tagCols.contains(QStringLiteral("TagId"))
                                         ? QStringLiteral("TagId ASC")
                                         : QStringLiteral("ROWID ASC");
            if (tagQuery.exec(
                    QStringLiteral("SELECT Name FROM Tag ORDER BY %1 LIMIT 1").arg(orderCol)) &&
                tagQuery.next()) {
                pkg.playlistName = tagQuery.value(0).toString().trimmed();
            }
        }
    }

    const QSet<QString> piCols = db.tableColumns(QStringLiteral("PlaylistItem"));
    if (!piCols.contains(QStringLiteral("PlaylistItemId"))) {
        pkg.errorMessage = QStringLiteral("PlaylistItem table is missing in jwlplaylist database");
        return pkg;
    }

    const QSet<QString> pimCols =
        db.tableColumns(QStringLiteral("PlaylistItemIndependentMediaMap"));
    const QSet<QString> imCols = db.tableColumns(QStringLiteral("IndependentMedia"));
    const QSet<QString> plmCols = db.tableColumns(QStringLiteral("PlaylistItemLocationMap"));
    const QSet<QString> locCols = db.tableColumns(QStringLiteral("Location"));
    const QSet<QString> ptmCols = db.tableColumns(QStringLiteral("TagMap"));

    const bool hasPim = pimCols.contains(QStringLiteral("PlaylistItemId")) &&
                        pimCols.contains(QStringLiteral("IndependentMediaId"));
    const bool hasIm = hasPim && imCols.contains(QStringLiteral("IndependentMediaId"));
    const bool hasPlm = plmCols.contains(QStringLiteral("PlaylistItemId")) &&
                        plmCols.contains(QStringLiteral("LocationId"));
    const bool hasLoc = hasPlm && locCols.contains(QStringLiteral("LocationId"));
    const bool hasTagMap = ptmCols.contains(QStringLiteral("PlaylistItemId")) &&
                           ptmCols.contains(QStringLiteral("Position"));

    QString joins;
    if (hasPim) {
        joins +=
            QStringLiteral(" LEFT JOIN PlaylistItemIndependentMediaMap pim ON pi.PlaylistItemId = "
                           "pim.PlaylistItemId");
    }
    if (hasIm) {
        joins += QStringLiteral(
            " LEFT JOIN IndependentMedia im ON pim.IndependentMediaId = im.IndependentMediaId");
    }
    if (hasPlm) {
        joins += QStringLiteral(
            " LEFT JOIN PlaylistItemLocationMap plm ON pi.PlaylistItemId = plm.PlaylistItemId");
    }
    if (hasLoc) {
        joins += QStringLiteral(" LEFT JOIN Location l ON plm.LocationId = l.LocationId");
    }
    if (hasTagMap) {
        joins += QStringLiteral(" LEFT JOIN TagMap tm ON pi.PlaylistItemId = tm.PlaylistItemId");
    }

    const QString orderBy =
        hasTagMap
            ? QStringLiteral("COALESCE(tm.Position, pi.PlaylistItemId) ASC, pi.PlaylistItemId ASC")
            : QStringLiteral("pi.PlaylistItemId ASC");

    const QString sql =
        QStringLiteral(
            "SELECT pi.PlaylistItemId, %1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, "
            "%14, %15, %16, %17, %18, %19, %20, %21, %22 "
            "FROM PlaylistItem pi %23 ORDER BY %24")
            .arg(colOrFallback(QStringLiteral("pi"), piCols, QStringLiteral("Label"),
                               QStringLiteral("''")),
                 colOrFallback(QStringLiteral("pi"), piCols, QStringLiteral("StartTrimOffsetTicks"),
                               QStringLiteral("0")),
                 colOrFallback(QStringLiteral("pi"), piCols, QStringLiteral("EndTrimOffsetTicks"),
                               QStringLiteral("0")),
                 colOrFallback(QStringLiteral("pi"), piCols, QStringLiteral("Accuracy"),
                               QStringLiteral("0")),
                 colOrFallback(QStringLiteral("pi"), piCols, QStringLiteral("EndAction"),
                               QStringLiteral("0")),
                 colOrFallback(QStringLiteral("pi"), piCols, QStringLiteral("ThumbnailFilePath"),
                               QStringLiteral("''")),
                 hasPlm ? colOrFallback(QStringLiteral("plm"), plmCols,
                                        QStringLiteral("BaseDurationTicks"), QStringLiteral("0"))
                        : QStringLiteral("0"),
                 hasPim ? colOrFallback(QStringLiteral("pim"), pimCols,
                                        QStringLiteral("DurationTicks"), QStringLiteral("0"))
                        : QStringLiteral("0"),
                 hasIm ? colOrFallback(QStringLiteral("im"), imCols,
                                       QStringLiteral("OriginalFilename"), QStringLiteral("''"))
                       : QStringLiteral("''"),
                 hasIm ? colOrFallback(QStringLiteral("im"), imCols, QStringLiteral("FilePath"),
                                       QStringLiteral("''"))
                       : QStringLiteral("''"),
                 hasIm ? colOrFallback(QStringLiteral("im"), imCols, QStringLiteral("MimeType"),
                                       QStringLiteral("''"))
                       : QStringLiteral("''"),
                 hasIm ? colOrFallback(QStringLiteral("im"), imCols, QStringLiteral("Hash"),
                                       QStringLiteral("''"))
                       : QStringLiteral("''"),
                 hasLoc ? colOrFallback(QStringLiteral("l"), locCols, QStringLiteral("BookNumber"),
                                        QStringLiteral("0"))
                        : QStringLiteral("0"),
                 hasLoc ? colOrFallback(QStringLiteral("l"), locCols,
                                        QStringLiteral("ChapterNumber"), QStringLiteral("0"))
                        : QStringLiteral("0"),
                 hasLoc ? colOrFallback(QStringLiteral("l"), locCols, QStringLiteral("DocumentId"),
                                        QStringLiteral("0"))
                        : QStringLiteral("0"),
                 hasLoc ? colOrFallback(QStringLiteral("l"), locCols, QStringLiteral("Track"),
                                        QStringLiteral("0"))
                        : QStringLiteral("0"),
                 hasLoc ? colOrFallback(QStringLiteral("l"), locCols,
                                        QStringLiteral("IssueTagNumber"), QStringLiteral("0"))
                        : QStringLiteral("0"),
                 hasLoc ? colOrFallback(QStringLiteral("l"), locCols, QStringLiteral("KeySymbol"),
                                        QStringLiteral("''"))
                        : QStringLiteral("''"),
                 hasLoc ? colOrFallback(QStringLiteral("l"), locCols,
                                        QStringLiteral("MepsLanguage"), QStringLiteral("0"))
                        : QStringLiteral("0"),
                 hasLoc ? colOrFallback(QStringLiteral("l"), locCols, QStringLiteral("Type"),
                                        QStringLiteral("0"))
                        : QStringLiteral("0"),
                 hasLoc ? colOrFallback(QStringLiteral("l"), locCols, QStringLiteral("Title"),
                                        QStringLiteral("''"))
                        : QStringLiteral("''"),
                 hasTagMap ? QStringLiteral("COALESCE(tm.Position, 0)") : QStringLiteral("-1"),
                 joins, orderBy);

    QSqlQuery query(db.database());
    if (!query.exec(sql)) {
        pkg.errorMessage = QStringLiteral("Failed querying jwlplaylist entries");
        return pkg;
    }

    const QHash<qint64, QStringList> markerMap = readMarkerLabels(db);

    int seqIndex = 0;
    while (query.next()) {
        JwlPlaylistEntry item;
        item.playlistItemId = query.value(0).toLongLong();
        item.label = query.value(1).toString().trimmed();
        const qint64 startTrimTicks = query.value(2).toLongLong();
        const qint64 endTrimTicks = query.value(3).toLongLong();
        item.accuracy = query.value(4).toInt();
        item.endAction = query.value(5).toInt();
        item.repeatLoop = (item.endAction == kEndActionLoop);
        item.thumbnailArchivePath = query.value(6).toString().trimmed();
        const qint64 baseDurationTicks = query.value(7).toLongLong();
        const qint64 itemDurationTicks = query.value(8).toLongLong();
        item.originalFilename = query.value(9).toString().trimmed();
        item.mediaArchivePath = query.value(10).toString().trimmed();
        item.mimeType = query.value(11).toString().trimmed();
        item.hash = query.value(12).toString().trimmed();
        item.bookNumber = query.value(13).toInt();
        item.chapterNumber = query.value(14).toInt();
        item.documentId = query.value(15).toLongLong();
        item.track = query.value(16).toInt();
        item.issueTagNumber = query.value(17).toInt();
        item.keySymbol = query.value(18).toString().trimmed();
        item.mepsLanguage = query.value(19).toInt();
        item.locationType = query.value(20).toInt();
        item.locationTitle = query.value(21).toString().trimmed();

        const int tagPosition = query.value(22).toInt();
        item.position = tagPosition >= 0 ? tagPosition : seqIndex;

        const qint64 rawDuration = baseDurationTicks > 0 ? baseDurationTicks : itemDurationTicks;
        item.durationMs = ticksToMs(rawDuration);
        const auto [startMs, endMs] =
            computeTrimRangeMs(baseDurationTicks, itemDurationTicks, startTrimTicks, endTrimTicks);
        item.trimStartMs = startMs;
        item.trimEndMs = endMs;
        item.markerLabels = markerMap.value(item.playlistItemId);

        pkg.entries.push_back(std::move(item));
        ++seqIndex;
    }

    pkg.valid = true;
    return pkg;
}

bool JwlPlaylistParser::extractAssets(const QString& jwlPlaylistPath, const QString& destDir,
                                      const std::vector<QString>& requestedArchivePaths,
                                      std::vector<QString>* extractedPaths, QString* error)
{
    ZipArchiveReader zip;
    if (!zip.openFile(jwlPlaylistPath, error)) {
        return false;
    }

    if (!requestedArchivePaths.empty()) {
        for (const QString& relPath : requestedArchivePaths) {
            if (!ZipArchiveReader::isSafeRelativeEntryName(relPath)) {
                if (error != nullptr) {
                    *error =
                        QStringLiteral("Unsafe jwlplaylist asset path rejected: %1").arg(relPath);
                }
                return false;
            }
        }
        return zip.extractEntries(destDir, requestedArchivePaths, {}, extractedPaths, error);
    }

    const std::vector<ZipEntryInfo> entries = zip.listEntries({}, error);
    if (entries.empty()) {
        return false;
    }

    std::vector<QString> mediaFilter;
    for (const ZipEntryInfo& entry : entries) {
        if (entry.isDirectory || entry.name == QStringLiteral("manifest.json") ||
            entry.name.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive)) {
            continue;
        }
        mediaFilter.push_back(entry.name);
    }

    if (mediaFilter.empty()) {
        if (extractedPaths != nullptr) {
            extractedPaths->clear();
        }
        return true;
    }
    return zip.extractEntries(destDir, mediaFilter, {}, extractedPaths, error);
}

} // namespace kmedia
