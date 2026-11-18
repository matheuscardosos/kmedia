#include "parsers/JwpubParser.h"

#include "parsers/SqlitePackageUtils.h"
#include "parsers/ZipArchiveReader.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

namespace kmedia {

namespace {

constexpr qint64 kMaxInMemoryContentsBytes = 64LL * 1024 * 1024;

QString sqlIdent(const QString& name)
{
    return QStringLiteral("\"%1\"").arg(name);
}

QString firstNonEmpty(const QStringList& values)
{
    for (const QString& value : values) {
        if (!value.trimmed().isEmpty()) {
            return value.trimmed();
        }
    }
    return {};
}

bool openInnerContentsReader(const ZipArchiveReader& outerZip, const QTemporaryDir& sandbox,
                             ZipArchiveReader& innerZip, QString* error)
{
    const std::vector<ZipEntryInfo> outerEntries = outerZip.listEntries({}, error);
    if (outerEntries.empty()) {
        if (error != nullptr && error->isEmpty()) {
            *error = QStringLiteral("JWPUB archive contains no readable entries");
        }
        return false;
    }

    const ZipEntryInfo* contentsEntry = nullptr;
    for (const ZipEntryInfo& entry : outerEntries) {
        if (entry.name == QStringLiteral("contents")) {
            contentsEntry = &entry;
            break;
        }
    }
    if (contentsEntry == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("JWPUB archive does not contain required 'contents' entry");
        }
        return false;
    }

    if (contentsEntry->uncompressedSize >= 0 &&
        contentsEntry->uncompressedSize <= kMaxInMemoryContentsBytes) {
        const std::optional<QByteArray> contentsBytes =
            outerZip.readEntry(QStringLiteral("contents"), kMaxInMemoryContentsBytes, error);
        if (!contentsBytes.has_value()) {
            return false;
        }
        return innerZip.openMemory(*contentsBytes, error);
    }

    // Stream oversized 'contents' entry to scoped sandbox rather than spiking RAM.
    const std::vector<QString> filter = {QStringLiteral("contents")};
    if (!outerZip.extractEntries(sandbox.path(), filter, {}, nullptr, error)) {
        return false;
    }
    return innerZip.openFile(sandbox.filePath(QStringLiteral("contents")), error);
}

JwpubManifest parseManifestJson(const QByteArray& rawJson)
{
    JwpubManifest manifest;
    const QJsonDocument doc = QJsonDocument::fromJson(rawJson);
    if (!doc.isObject()) {
        return manifest;
    }

    const QJsonObject root = doc.object();
    const QJsonObject pub = root.value(QStringLiteral("publication")).toObject();

    manifest.name = root.value(QStringLiteral("name")).toString().trimmed();
    manifest.hash = firstNonEmpty({root.value(QStringLiteral("hash")).toString(),
                                   pub.value(QStringLiteral("hash")).toString()});
    manifest.timestamp = root.value(QStringLiteral("timestamp")).toString().trimmed();

    manifest.symbol = firstNonEmpty({pub.value(QStringLiteral("symbol")).toString(),
                                     pub.value(QStringLiteral("undatedSymbol")).toString(),
                                     root.value(QStringLiteral("symbol")).toString()});
    manifest.shortTitle =
        firstNonEmpty({pub.value(QStringLiteral("shortTitle")).toString(),
                       pub.value(QStringLiteral("title")).toString(), manifest.name});
    manifest.issueTagNumber = pub.contains(QStringLiteral("issueTagNumber"))
                                  ? pub.value(QStringLiteral("issueTagNumber")).toInt()
                                  : root.value(QStringLiteral("issueTagNumber")).toInt();
    manifest.mepsLanguageIndex = pub.contains(QStringLiteral("language"))
                                     ? pub.value(QStringLiteral("language")).toInt()
                                     : root.value(QStringLiteral("language")).toInt();
    return manifest;
}

JwpubPublicationInfo readPublicationInfo(const ScopedReadOnlySqliteDb& db,
                                         const JwpubManifest& manifest)
{
    JwpubPublicationInfo info;
    info.title = manifest.shortTitle;
    info.symbol = manifest.symbol;
    info.issueTagNumber = manifest.issueTagNumber;
    info.mepsLanguageIndex = manifest.mepsLanguageIndex;

    const QSet<QString> cols = db.tableColumns(QStringLiteral("Publication"));
    if (cols.isEmpty()) {
        return info;
    }

    QStringList wanted = {QStringLiteral("Title"),          QStringLiteral("ShortTitle"),
                          QStringLiteral("IssueTitle"),     QStringLiteral("Symbol"),
                          QStringLiteral("UndatedSymbol"),  QStringLiteral("UniqueEnglishSymbol"),
                          QStringLiteral("IssueTagNumber"), QStringLiteral("MepsLanguageIndex"),
                          QStringLiteral("Year"),           QStringLiteral("VersionNumber")};
    QStringList selectCols;
    for (const QString& col : wanted) {
        if (cols.contains(col)) {
            selectCols.append(sqlIdent(col));
        }
    }
    if (selectCols.isEmpty()) {
        return info;
    }

    QSqlQuery query(db.database());
    if (!query.exec(QStringLiteral("SELECT %1 FROM Publication LIMIT 1")
                        .arg(selectCols.join(QStringLiteral(", ")))) ||
        !query.next()) {
        return info;
    }

    auto val = [&](const QString& name) -> QVariant {
        const qsizetype idx = selectCols.indexOf(sqlIdent(name));
        return idx >= 0 ? query.value(static_cast<int>(idx)) : QVariant{};
    };

    const QString dbTitle = firstNonEmpty({val(QStringLiteral("IssueTitle")).toString(),
                                           val(QStringLiteral("Title")).toString(),
                                           val(QStringLiteral("ShortTitle")).toString()});
    if (!dbTitle.isEmpty()) {
        info.title = dbTitle;
    }
    const QString sym = firstNonEmpty({val(QStringLiteral("Symbol")).toString(),
                                       val(QStringLiteral("UndatedSymbol")).toString()});
    if (!sym.isEmpty()) {
        info.symbol = sym;
    }
    if (const QString undated = val(QStringLiteral("UndatedSymbol")).toString().trimmed();
        !undated.isEmpty()) {
        info.undatedSymbol = undated;
    }
    if (const QString eng = val(QStringLiteral("UniqueEnglishSymbol")).toString().trimmed();
        !eng.isEmpty()) {
        info.uniqueEnglishSymbol = eng;
    }
    if (cols.contains(QStringLiteral("IssueTagNumber"))) {
        info.issueTagNumber = val(QStringLiteral("IssueTagNumber")).toInt();
    }
    if (cols.contains(QStringLiteral("MepsLanguageIndex"))) {
        info.mepsLanguageIndex = val(QStringLiteral("MepsLanguageIndex")).toInt();
    }
    if (cols.contains(QStringLiteral("Year"))) {
        info.year = val(QStringLiteral("Year")).toInt();
    }
    if (cols.contains(QStringLiteral("VersionNumber"))) {
        info.versionNumber = val(QStringLiteral("VersionNumber")).toInt();
    }
    return info;
}

std::vector<JwpubDocument> readDocuments(const ScopedReadOnlySqliteDb& db)
{
    std::vector<JwpubDocument> docs;
    const QSet<QString> cols = db.tableColumns(QStringLiteral("Document"));
    if (!cols.contains(QStringLiteral("DocumentId"))) {
        return docs;
    }

    const QString mepsExpr = cols.contains(QStringLiteral("MepsDocumentId"))
                                 ? QStringLiteral("MepsDocumentId")
                                 : QStringLiteral("0");
    const QString titleExpr =
        cols.contains(QStringLiteral("Title")) ? QStringLiteral("Title") : QStringLiteral("''");
    const QString featureExpr = cols.contains(QStringLiteral("FeatureTitle"))
                                    ? QStringLiteral("FeatureTitle")
                                    : QStringLiteral("''");
    const QString classExpr =
        cols.contains(QStringLiteral("Class")) ? QStringLiteral("Class") : QStringLiteral("0");

    QSqlQuery query(db.database());
    const QString sql =
        QStringLiteral("SELECT DocumentId, %1, %2, %3, %4 FROM Document ORDER BY DocumentId ASC")
            .arg(mepsExpr, titleExpr, featureExpr, classExpr);
    if (!query.exec(sql)) {
        return docs;
    }

    while (query.next()) {
        JwpubDocument doc;
        doc.documentId = query.value(0).toLongLong();
        doc.mepsDocumentId = query.value(1).toLongLong();
        doc.title = query.value(2).toString().trimmed();
        doc.featureTitle = query.value(3).toString().trimmed();
        doc.classCode = query.value(4).toInt();
        docs.push_back(std::move(doc));
    }
    return docs;
}

QHash<qint64, std::vector<JwpubVideoMarker>> readVideoMarkers(const ScopedReadOnlySqliteDb& db)
{
    QHash<qint64, std::vector<JwpubVideoMarker>> markers;
    const QSet<QString> cols = db.tableColumns(QStringLiteral("VideoMarker"));
    if (!cols.contains(QStringLiteral("MultimediaId")) ||
        !cols.contains(QStringLiteral("StartTimeTicks"))) {
        return markers;
    }

    const QString idExpr = cols.contains(QStringLiteral("VideoMarkerId"))
                               ? QStringLiteral("VideoMarkerId")
                               : QStringLiteral("0");
    const QString labelExpr =
        cols.contains(QStringLiteral("Label")) ? QStringLiteral("Label") : QStringLiteral("''");
    const QString durExpr = cols.contains(QStringLiteral("DurationTicks"))
                                ? QStringLiteral("DurationTicks")
                                : QStringLiteral("0");
    const QString endTransExpr = cols.contains(QStringLiteral("EndTransitionDurationTicks"))
                                     ? QStringLiteral("EndTransitionDurationTicks")
                                     : QStringLiteral("0");

    QSqlQuery query(db.database());
    const QString sql = QStringLiteral("SELECT MultimediaId, %1, %2, StartTimeTicks, %3, %4 "
                                       "FROM VideoMarker ORDER BY StartTimeTicks ASC")
                            .arg(idExpr, labelExpr, durExpr, endTransExpr);
    if (!query.exec(sql)) {
        return markers;
    }

    while (query.next()) {
        JwpubVideoMarker marker;
        const qint64 mmId = query.value(0).toLongLong();
        marker.videoMarkerId = query.value(1).toLongLong();
        marker.label = query.value(2).toString();
        marker.startTimeTicks = query.value(3).toLongLong();
        marker.durationTicks = query.value(4).toLongLong();
        marker.endTransitionDurationTicks = query.value(5).toLongLong();
        markers[mmId].push_back(std::move(marker));
    }
    return markers;
}

std::vector<JwpubMediaItem>
readMultimediaItems(const ScopedReadOnlySqliteDb& db,
                    const QHash<qint64, std::vector<JwpubVideoMarker>>& markerMap)
{
    std::vector<JwpubMediaItem> items;
    const QSet<QString> mmCols = db.tableColumns(QStringLiteral("Multimedia"));
    if (!mmCols.contains(QStringLiteral("MultimediaId"))) {
        return items;
    }

    const QSet<QString> dmCols = db.tableColumns(QStringLiteral("DocumentMultimedia"));
    const QSet<QString> docCols = db.tableColumns(QStringLiteral("Document"));
    const bool hasDocMm = dmCols.contains(QStringLiteral("DocumentId")) &&
                          dmCols.contains(QStringLiteral("MultimediaId"));
    const bool hasDoc = docCols.contains(QStringLiteral("DocumentId"));

    auto colOrDefault = [&](const QString& prefix, const QSet<QString>& cols, const QString& col,
                            const QString& fallback) {
        return cols.contains(col) ? QStringLiteral("%1.%2").arg(prefix, col) : fallback;
    };

    const QString docIdExpr = hasDocMm
                                  ? QStringLiteral("dm.DocumentId")
                                  : colOrDefault(QStringLiteral("m"), mmCols,
                                                 QStringLiteral("DocumentId"), QStringLiteral("0"));
    const QString docTitleExpr = (hasDocMm && hasDoc && docCols.contains(QStringLiteral("Title")))
                                     ? QStringLiteral("d.Title")
                                     : QStringLiteral("''");
    const QString beginParaExpr =
        hasDocMm ? colOrDefault(QStringLiteral("dm"), dmCols,
                                QStringLiteral("BeginParagraphOrdinal"), QStringLiteral("0"))
                 : QStringLiteral("0");
    const QString endParaExpr =
        hasDocMm ? colOrDefault(QStringLiteral("dm"), dmCols, QStringLiteral("EndParagraphOrdinal"),
                                QStringLiteral("0"))
                 : QStringLiteral("0");

    const QStringList selectExprs = {
        QStringLiteral("m.MultimediaId"),
        docIdExpr,
        docTitleExpr,
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("FilePath"), QStringLiteral("''")),
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("Label"), QStringLiteral("''")),
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("Caption"), QStringLiteral("''")),
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("MimeType"), QStringLiteral("''")),
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("MajorType"), QStringLiteral("0")),
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("MinorType"), QStringLiteral("0")),
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("CategoryType"),
                     QStringLiteral("0")),
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("Track"), QStringLiteral("0")),
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("KeySymbol"),
                     QStringLiteral("''")),
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("IssueTagNumber"),
                     QStringLiteral("0")),
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("MepsLanguageIndex"),
                     QStringLiteral("0")),
        beginParaExpr,
        endParaExpr,
    };

    const QString docMmJoin =
        hasDocMm
            ? QStringLiteral("INNER JOIN DocumentMultimedia dm ON dm.MultimediaId = m.MultimediaId")
            : QString();
    const QString docJoin =
        (hasDocMm && hasDoc)
            ? QStringLiteral("INNER JOIN Document d ON d.DocumentId = dm.DocumentId")
            : QString();
    const QString orderBy =
        hasDocMm ? (dmCols.contains(QStringLiteral("BeginParagraphOrdinal"))
                        ? QStringLiteral(
                              "dm.DocumentId ASC, dm.BeginParagraphOrdinal ASC, m.MultimediaId ASC")
                        : QStringLiteral("dm.DocumentId ASC, m.MultimediaId ASC"))
                 : QStringLiteral("m.MultimediaId ASC");

    const QString sql =
        QStringLiteral("SELECT %1 FROM Multimedia m %2 %3 ORDER BY %4")
            .arg(selectExprs.join(QStringLiteral(", ")), docMmJoin, docJoin, orderBy);

    QSqlQuery query(db.database());
    if (!query.exec(sql)) {
        return items;
    }

    while (query.next()) {
        JwpubMediaItem item;
        item.multimediaId = query.value(0).toLongLong();
        item.documentId = query.value(1).toLongLong();
        item.documentTitle = query.value(2).toString().trimmed();
        item.filePath = query.value(3).toString().trimmed();
        item.label = query.value(4).toString().trimmed();
        item.caption = query.value(5).toString().trimmed();
        item.mimeType = query.value(6).toString().trimmed();
        item.majorType = query.value(7).toInt();
        item.minorType = query.value(8).toInt();
        item.categoryType = query.value(9).toInt();
        item.track = query.value(10).toInt();
        item.keySymbol = query.value(11).toString().trimmed();
        item.issueTagNumber = query.value(12).toInt();
        item.mepsLanguageIndex = query.value(13).toInt();
        item.beginParagraphOrdinal = query.value(14).toInt();
        item.endParagraphOrdinal = query.value(15).toInt();
        item.videoMarkers = markerMap.value(item.multimediaId);
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace

JwpubPackage JwpubParser::parse(const QString& jwpubPath)
{
    JwpubPackage pkg;
    pkg.archivePath = jwpubPath;

    ZipArchiveReader outerZip;
    QString error;
    if (!outerZip.openFile(jwpubPath, &error)) {
        pkg.errorMessage = error;
        return pkg;
    }

    const std::vector<ZipEntryInfo> outerEntries = outerZip.listEntries({}, &error);
    if (outerEntries.empty()) {
        pkg.errorMessage = error.isEmpty() ? QStringLiteral("Empty JWPUB archive") : error;
        return pkg;
    }

    for (const ZipEntryInfo& entry : outerEntries) {
        if (entry.name == QStringLiteral("manifest.json")) {
            const std::optional<QByteArray> manifestBytes =
                outerZip.readEntry(QStringLiteral("manifest.json"), 1024 * 1024);
            if (manifestBytes.has_value()) {
                pkg.manifest = parseManifestJson(*manifestBytes);
            }
            break;
        }
    }

    QTemporaryDir sandbox;
    sandbox.setAutoRemove(true);
    if (!sandbox.isValid()) {
        pkg.errorMessage = QStringLiteral("Failed to create scoped JWPUB inspection sandbox");
        return pkg;
    }

    ZipArchiveReader innerZip;
    if (!openInnerContentsReader(outerZip, sandbox, innerZip, &error)) {
        pkg.errorMessage = error;
        return pkg;
    }

    const std::vector<ZipEntryInfo> innerEntries = innerZip.listEntries({}, &error);
    if (innerEntries.empty()) {
        pkg.errorMessage =
            error.isEmpty() ? QStringLiteral("JWPUB 'contents' archive is empty") : error;
        return pkg;
    }

    QString dbEntryName;
    for (const ZipEntryInfo& entry : innerEntries) {
        if (!entry.isDirectory) {
            pkg.archiveFiles.append(entry.name);
            if (dbEntryName.isEmpty() &&
                entry.name.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive)) {
                dbEntryName = entry.name;
            }
        }
    }

    if (dbEntryName.isEmpty()) {
        pkg.errorMessage = QStringLiteral("JWPUB 'contents' archive contains no SQLite database");
        return pkg;
    }

    std::vector<QString> extractedDbPaths;
    if (!innerZip.extractEntries(sandbox.path(), {dbEntryName}, {}, &extractedDbPaths, &error) ||
        extractedDbPaths.empty()) {
        pkg.errorMessage =
            error.isEmpty() ? QStringLiteral("Failed to extract JWPUB SQLite database") : error;
        return pkg;
    }

    ScopedReadOnlySqliteDb db;
    if (!db.open(extractedDbPaths.front(), &error)) {
        pkg.errorMessage = error;
        return pkg;
    }

    pkg.publication = readPublicationInfo(db, pkg.manifest);
    pkg.documents = readDocuments(db);
    const QHash<qint64, std::vector<JwpubVideoMarker>> markers = readVideoMarkers(db);
    pkg.mediaItems = readMultimediaItems(db, markers);
    pkg.valid = true;
    return pkg;
}

bool JwpubParser::extractMediaAssets(const QString& jwpubPath, const QString& destDir,
                                     const std::vector<QString>& requestedArchivePaths,
                                     std::vector<QString>* extractedPaths, QString* error)
{
    if (requestedArchivePaths.empty()) {
        if (extractedPaths != nullptr) {
            extractedPaths->clear();
        }
        return true;
    }

    for (const QString& relPath : requestedArchivePaths) {
        if (!ZipArchiveReader::isSafeRelativeEntryName(relPath)) {
            if (error != nullptr) {
                *error = QStringLiteral("Unsafe archive asset path rejected: %1").arg(relPath);
            }
            return false;
        }
    }

    ZipArchiveReader outerZip;
    if (!outerZip.openFile(jwpubPath, error)) {
        return false;
    }

    QTemporaryDir sandbox;
    sandbox.setAutoRemove(true);
    if (!sandbox.isValid()) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to create temporary extraction sandbox");
        }
        return false;
    }

    ZipArchiveReader innerZip;
    if (!openInnerContentsReader(outerZip, sandbox, innerZip, error)) {
        return false;
    }

    return innerZip.extractEntries(destDir, requestedArchivePaths, {}, extractedPaths, error);
}

} // namespace kmedia
