#include "parsers/JwpubParser.h"

#include "parsers/SqlitePackageUtils.h"
#include "parsers/ZipArchiveReader.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <algorithm>
#include <utility>

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

QHash<qint64, std::pair<int, int>> readDatedTextRanges(const ScopedReadOnlySqliteDb& db)
{
    QHash<qint64, std::pair<int, int>> ranges;
    const QSet<QString> cols = db.tableColumns(QStringLiteral("DatedText"));
    if (!cols.contains(QStringLiteral("DocumentId")) ||
        !cols.contains(QStringLiteral("FirstDateOffset"))) {
        return ranges;
    }

    const QString lastExpr = cols.contains(QStringLiteral("LastDateOffset"))
                                 ? QStringLiteral("LastDateOffset")
                                 : QStringLiteral("FirstDateOffset");
    QSqlQuery query(db.database());
    if (!query.exec(QStringLiteral("SELECT DocumentId, FirstDateOffset, %1 FROM DatedText "
                                   "ORDER BY FirstDateOffset ASC")
                        .arg(lastExpr))) {
        return ranges;
    }
    while (query.next()) {
        const qint64 docId = query.value(0).toLongLong();
        const int firstOffset = query.value(1).toInt();
        const int lastOffset = query.value(2).toInt();
        if (!ranges.contains(docId)) {
            ranges.insert(docId, {firstOffset, lastOffset > 0 ? lastOffset : firstOffset});
        }
    }
    return ranges;
}

QHash<qint64, int> readMaxParagraphs(const ScopedReadOnlySqliteDb& db)
{
    QHash<qint64, int> maxParas;
    const QSet<QString> dpCols = db.tableColumns(QStringLiteral("DocumentParagraph"));
    if (dpCols.contains(QStringLiteral("DocumentId")) &&
        dpCols.contains(QStringLiteral("ParagraphIndex"))) {
        QSqlQuery query(db.database());
        if (query.exec(QStringLiteral("SELECT DocumentId, MAX(ParagraphIndex) "
                                      "FROM DocumentParagraph GROUP BY DocumentId"))) {
            while (query.next()) {
                maxParas.insert(query.value(0).toLongLong(), query.value(1).toInt());
            }
        }
    }

    const QSet<QString> dmCols = db.tableColumns(QStringLiteral("DocumentMultimedia"));
    if (dmCols.contains(QStringLiteral("DocumentId")) &&
        dmCols.contains(QStringLiteral("EndParagraphOrdinal"))) {
        QSqlQuery query(db.database());
        if (query.exec(QStringLiteral("SELECT DocumentId, MAX(EndParagraphOrdinal) "
                                      "FROM DocumentMultimedia GROUP BY DocumentId"))) {
            while (query.next()) {
                const qint64 docId = query.value(0).toLongLong();
                const int candidate = query.value(1).toInt();
                if (candidate > maxParas.value(docId, 0)) {
                    maxParas.insert(docId, candidate);
                }
            }
        }
    }
    return maxParas;
}

QHash<qint64, QHash<int, QString>> readQuestionLabels(const ScopedReadOnlySqliteDb& db)
{
    QHash<qint64, QHash<int, QString>> labels;
    const QSet<QString> cols = db.tableColumns(QStringLiteral("Question"));
    if (!cols.contains(QStringLiteral("DocumentId")) ||
        !cols.contains(QStringLiteral("TargetParagraphNumberLabel"))) {
        return labels;
    }

    const QString ordCol = cols.contains(QStringLiteral("BeginParagraphOrdinal"))
                               ? QStringLiteral("BeginParagraphOrdinal")
                               : (cols.contains(QStringLiteral("ParagraphOrdinal"))
                                      ? QStringLiteral("ParagraphOrdinal")
                                      : (cols.contains(QStringLiteral("ParagraphIndex"))
                                             ? QStringLiteral("ParagraphIndex")
                                             : QString()));
    if (ordCol.isEmpty()) {
        return labels;
    }

    QSqlQuery query(db.database());
    if (!query.exec(
            QStringLiteral("SELECT DocumentId, %1, TargetParagraphNumberLabel FROM Question")
                .arg(ordCol))) {
        return labels;
    }
    while (query.next()) {
        const qint64 docId = query.value(0).toLongLong();
        const int ordinal = query.value(1).toInt();
        const QString label = query.value(2).toString().trimmed();
        if (docId > 0 && ordinal > 0 && !label.isEmpty()) {
            labels[docId].insert(ordinal, label);
        }
    }
    return labels;
}

int extractSongNumberFromText(const QString& caption, qint64 refMepsDocId)
{
    static const QRegularExpression digitsRe(QStringLiteral(R"((\d{1,3}))"));
    const QRegularExpressionMatch match = digitsRe.match(caption);
    if (match.hasMatch()) {
        const int num = match.captured(1).toInt();
        if (num >= 1 && num <= 200) {
            return num;
        }
    }
    if (refMepsDocId >= 1 && refMepsDocId <= 200) {
        return static_cast<int>(refMepsDocId);
    }
    return 0;
}

bool isIgnoredExtractSymbol(const QString& symLower)
{
    return symLower.startsWith(QStringLiteral("mwbr")) || symLower == QStringLiteral("it") ||
           symLower == QStringLiteral("snnw");
}

bool isCbsExtractSymbol(const QString& symLower)
{
    return symLower == QStringLiteral("wcg") || symLower == QStringLiteral("bt") ||
           symLower == QStringLiteral("lff") || symLower == QStringLiteral("lmd");
}

std::vector<JwpubExtractRef> readExtracts(const ScopedReadOnlySqliteDb& db)
{
    std::vector<JwpubExtractRef> extracts;
    const QSet<QString> deCols = db.tableColumns(QStringLiteral("DocumentExtract"));
    const QSet<QString> exCols = db.tableColumns(QStringLiteral("Extract"));
    if (!deCols.contains(QStringLiteral("DocumentId")) ||
        !deCols.contains(QStringLiteral("ExtractId")) ||
        !exCols.contains(QStringLiteral("ExtractId"))) {
        return extracts;
    }

    const QSet<QString> rpCols = db.tableColumns(QStringLiteral("RefPublication"));
    const bool hasRefPub = exCols.contains(QStringLiteral("RefPublicationId")) &&
                           rpCols.contains(QStringLiteral("RefPublicationId"));

    auto colOr = [](const QString& alias, const QSet<QString>& cols, const QString& col,
                    const QString& fallback) {
        return cols.contains(col) ? QStringLiteral("%1.%2").arg(alias, col) : fallback;
    };

    const QString sql =
        QStringLiteral("SELECT de.DocumentId, de.ExtractId, %1, %2, %3, %4, %5, %6, %7, %8, %9 "
                       "FROM DocumentExtract de "
                       "INNER JOIN Extract ex ON ex.ExtractId = de.ExtractId %10 "
                       "ORDER BY de.DocumentId ASC, %1 ASC, de.ExtractId ASC")
            .arg(colOr(QStringLiteral("de"), deCols, QStringLiteral("BeginParagraphOrdinal"),
                       QStringLiteral("0")),
                 colOr(QStringLiteral("de"), deCols, QStringLiteral("EndParagraphOrdinal"),
                       QStringLiteral("0")),
                 colOr(QStringLiteral("ex"), exCols, QStringLiteral("Caption"),
                       QStringLiteral("''")),
                 hasRefPub ? colOr(QStringLiteral("rp"), rpCols, QStringLiteral("UndatedSymbol"),
                                   QStringLiteral("''"))
                           : QStringLiteral("''"),
                 hasRefPub ? colOr(QStringLiteral("rp"), rpCols, QStringLiteral("IssueTagNumber"),
                                   QStringLiteral("0"))
                           : QStringLiteral("0"),
                 hasRefPub ? colOr(QStringLiteral("rp"), rpCols,
                                   QStringLiteral("MepsLanguageIndex"), QStringLiteral("0"))
                           : QStringLiteral("0"),
                 colOr(QStringLiteral("ex"), exCols, QStringLiteral("RefMepsDocumentId"),
                       QStringLiteral("0")),
                 colOr(QStringLiteral("ex"), exCols, QStringLiteral("RefBeginParagraphOrdinal"),
                       QStringLiteral("0")),
                 colOr(QStringLiteral("ex"), exCols, QStringLiteral("RefEndParagraphOrdinal"),
                       QStringLiteral("0")),
                 hasRefPub ? QStringLiteral("LEFT JOIN RefPublication rp ON rp.RefPublicationId = "
                                            "ex.RefPublicationId")
                           : QString());

    QSqlQuery query(db.database());
    if (!query.exec(sql)) {
        return extracts;
    }

    while (query.next()) {
        JwpubExtractRef ref;
        ref.documentId = query.value(0).toLongLong();
        ref.extractId = query.value(1).toLongLong();
        ref.beginParagraphOrdinal = query.value(2).toInt();
        ref.endParagraphOrdinal = query.value(3).toInt();
        ref.caption = query.value(4).toString().trimmed();
        ref.refPubSymbol = query.value(5).toString().trimmed();
        ref.refIssueTagNumber = query.value(6).toInt();
        ref.refMepsLanguageIndex = query.value(7).toInt();
        ref.refMepsDocumentId = query.value(8).toLongLong();
        ref.refBeginParagraphOrdinal = query.value(9).toInt();
        ref.refEndParagraphOrdinal = query.value(10).toInt();

        const QString symLower = ref.refPubSymbol.toLower();
        if (isIgnoredExtractSymbol(symLower)) {
            continue;
        }
        if (symLower == QStringLiteral("sjj")) {
            ref.songNumber = extractSongNumberFromText(ref.caption, ref.refMepsDocumentId);
        }
        ref.isTh = (symLower == QStringLiteral("th"));
        ref.isCbsSource = isCbsExtractSymbol(symLower);
        extracts.push_back(std::move(ref));
    }
    return extracts;
}

std::vector<JwpubDocument> readDocuments(const ScopedReadOnlySqliteDb& db)
{
    std::vector<JwpubDocument> docs;
    const QSet<QString> cols = db.tableColumns(QStringLiteral("Document"));
    if (!cols.contains(QStringLiteral("DocumentId"))) {
        return docs;
    }

    const QHash<qint64, std::pair<int, int>> datedRanges = readDatedTextRanges(db);
    const QHash<qint64, int> maxParas = readMaxParagraphs(db);

    const QString mepsExpr = cols.contains(QStringLiteral("MepsDocumentId"))
                                 ? QStringLiteral("MepsDocumentId")
                                 : QStringLiteral("0");
    const QString titleExpr =
        cols.contains(QStringLiteral("Title")) ? QStringLiteral("Title") : QStringLiteral("''");
    const QString subtitleExpr = cols.contains(QStringLiteral("Subtitle"))
                                     ? QStringLiteral("Subtitle")
                                     : QStringLiteral("''");
    const QString featureExpr = cols.contains(QStringLiteral("FeatureTitle"))
                                    ? QStringLiteral("FeatureTitle")
                                    : QStringLiteral("''");
    const QString classExpr =
        cols.contains(QStringLiteral("Class")) ? QStringLiteral("Class") : QStringLiteral("0");

    QSqlQuery query(db.database());
    const QString sql =
        QStringLiteral(
            "SELECT DocumentId, %1, %2, %3, %4, %5 FROM Document ORDER BY DocumentId ASC")
            .arg(mepsExpr, titleExpr, subtitleExpr, featureExpr, classExpr);
    if (!query.exec(sql)) {
        return docs;
    }

    while (query.next()) {
        JwpubDocument doc;
        doc.documentId = query.value(0).toLongLong();
        doc.mepsDocumentId = query.value(1).toLongLong();
        doc.title = query.value(2).toString().trimmed();
        doc.subtitle = query.value(3).toString().trimmed();
        doc.featureTitle = query.value(4).toString().trimmed();
        doc.classCode = query.value(5).toInt();
        const auto dateIt = datedRanges.constFind(doc.documentId);
        if (dateIt != datedRanges.constEnd()) {
            doc.firstDateOffset = dateIt->first;
            doc.lastDateOffset = dateIt->second;
        }
        doc.maxParagraphOrdinal = maxParas.value(doc.documentId, 0);
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

bool isProjectionCategoryType(int categoryType)
{
    return categoryType == 4 || categoryType == 6 || categoryType == 9 || categoryType == 10 ||
           categoryType == 25 || categoryType == 26;
}

void enrichMediaItemFromContext(JwpubMediaItem& item, const QHash<qint64, JwpubDocument>& docMap,
                                const QHash<qint64, QHash<int, QString>>& questionLabels,
                                const std::vector<JwpubExtractRef>& extracts)
{
    if (const auto docIt = docMap.constFind(item.documentId); docIt != docMap.constEnd()) {
        item.documentClass = docIt->classCode;
        item.mepsDocumentId = static_cast<int>(docIt->mepsDocumentId);
        item.maxParagraphOrdinal = docIt->maxParagraphOrdinal;
        if (item.documentTitle.isEmpty()) {
            item.documentTitle = docIt->title;
        }
        if (item.documentSubtitle.isEmpty()) {
            item.documentSubtitle = docIt->subtitle;
        }
    }

    item.isFootnote = (item.beginParagraphOrdinal == 1500);
    item.isPrinted = (item.majorType == 1 && !isProjectionCategoryType(item.categoryType));

    const auto qDocIt = questionLabels.constFind(item.documentId);
    if (qDocIt != questionLabels.constEnd() && item.beginParagraphOrdinal > 0) {
        item.paragraphLabel = qDocIt->value(item.beginParagraphOrdinal);
    }
    if (item.paragraphLabel == QStringLiteral("*")) {
        item.isFootnote = true;
    }

    if (item.beginParagraphOrdinal > 0 && item.maxParagraphOrdinal > 0 &&
        item.beginParagraphOrdinal >= item.maxParagraphOrdinal - 2 &&
        item.beginParagraphOrdinal < item.maxParagraphOrdinal) {
        item.isCbsCandidate = true;
    }

    for (const JwpubExtractRef& ref : extracts) {
        if (ref.documentId != item.documentId) {
            continue;
        }
        const bool inRange = (item.beginParagraphOrdinal > 0 && ref.beginParagraphOrdinal > 0 &&
                              item.beginParagraphOrdinal >= ref.beginParagraphOrdinal &&
                              item.beginParagraphOrdinal <= (ref.endParagraphOrdinal > 0
                                                                 ? ref.endParagraphOrdinal
                                                                 : ref.beginParagraphOrdinal));
        if (!inRange) {
            continue;
        }
        if (!ref.caption.isEmpty() && item.groupCaption.isEmpty()) {
            item.groupCaption = ref.caption;
        }
        if (!ref.refPubSymbol.isEmpty() && item.refPubSymbol.isEmpty()) {
            item.refPubSymbol = ref.refPubSymbol;
            item.refIssueTagNumber = ref.refIssueTagNumber;
            item.refMepsDocumentId = ref.refMepsDocumentId;
        }
        if (ref.songNumber > 0 && item.songNumber == 0) {
            item.songNumber = ref.songNumber;
        }
        if (ref.isCbsSource) {
            item.isCbsCandidate = true;
        }
    }

    if (item.isFootnote) {
        item.tagType = QStringLiteral("footnote");
        item.tagValue = QStringLiteral("*");
    } else if (item.songNumber > 0) {
        item.tagType = QStringLiteral("song");
        item.tagValue = QStringLiteral("Song %1").arg(item.songNumber);
    } else if (!item.paragraphLabel.isEmpty()) {
        item.tagType = QStringLiteral("para");
        item.tagValue = QStringLiteral("Para. %1").arg(item.paragraphLabel);
    } else if (item.beginParagraphOrdinal > 0) {
        item.tagType = QStringLiteral("para");
        item.tagValue = QStringLiteral("Para. %1").arg(item.beginParagraphOrdinal);
    }
}

std::vector<JwpubMediaItem> readMultimediaItems(
    const ScopedReadOnlySqliteDb& db, const QHash<qint64, std::vector<JwpubVideoMarker>>& markerMap,
    const std::vector<JwpubDocument>& documents, const std::vector<JwpubExtractRef>& extracts,
    const QHash<qint64, QHash<int, QString>>& questionLabels)
{
    std::vector<JwpubMediaItem> items;
    const QSet<QString> mmCols = db.tableColumns(QStringLiteral("Multimedia"));
    if (!mmCols.contains(QStringLiteral("MultimediaId"))) {
        return items;
    }

    QHash<qint64, JwpubDocument> docMap;
    for (const JwpubDocument& d : documents) {
        docMap.insert(d.documentId, d);
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
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("LinkMultimediaId"),
                     QStringLiteral("0")),
        colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("SuppressZoom"),
                     QStringLiteral("0")),
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

    QSet<qint64> seenMmIds;
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
        item.linkMultimediaId = query.value(16).toLongLong();
        item.suppressZoom = query.value(17).toInt() != 0;
        item.videoMarkers = markerMap.value(item.multimediaId);
        enrichMediaItemFromContext(item, docMap, questionLabels, extracts);
        seenMmIds.insert(item.multimediaId);
        items.push_back(std::move(item));
    }

    // Include unlinked Memorial screen items (CategoryType == 26) or linked targets when
    // DocumentMultimedia omits them.
    if (hasDocMm && mmCols.contains(QStringLiteral("CategoryType"))) {
        QSqlQuery unlinkedQuery(db.database());
        const QString unlinkedSql =
            QStringLiteral(
                "SELECT %1 FROM Multimedia m WHERE m.CategoryType = 26 ORDER BY m.MultimediaId ASC")
                .arg(QStringList{
                    QStringLiteral("m.MultimediaId"),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("DocumentId"),
                                 QStringLiteral("0")),
                    QStringLiteral("''"),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("FilePath"),
                                 QStringLiteral("''")),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("Label"),
                                 QStringLiteral("''")),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("Caption"),
                                 QStringLiteral("''")),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("MimeType"),
                                 QStringLiteral("''")),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("MajorType"),
                                 QStringLiteral("0")),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("MinorType"),
                                 QStringLiteral("0")),
                    QStringLiteral("m.CategoryType"),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("Track"),
                                 QStringLiteral("0")),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("KeySymbol"),
                                 QStringLiteral("''")),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("IssueTagNumber"),
                                 QStringLiteral("0")),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("MepsLanguageIndex"),
                                 QStringLiteral("0")),
                    QStringLiteral("-1"),
                    QStringLiteral("-1"),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("LinkMultimediaId"),
                                 QStringLiteral("0")),
                    colOrDefault(QStringLiteral("m"), mmCols, QStringLiteral("SuppressZoom"),
                                 QStringLiteral("0")),
                }
                         .join(QStringLiteral(", ")));
        if (unlinkedQuery.exec(unlinkedSql)) {
            while (unlinkedQuery.next()) {
                const qint64 mmId = unlinkedQuery.value(0).toLongLong();
                if (seenMmIds.contains(mmId)) {
                    continue;
                }
                JwpubMediaItem item;
                item.multimediaId = mmId;
                item.documentId = unlinkedQuery.value(1).toLongLong();
                item.filePath = unlinkedQuery.value(3).toString().trimmed();
                item.label = unlinkedQuery.value(4).toString().trimmed();
                item.caption = unlinkedQuery.value(5).toString().trimmed();
                item.mimeType = unlinkedQuery.value(6).toString().trimmed();
                item.majorType = unlinkedQuery.value(7).toInt();
                item.minorType = unlinkedQuery.value(8).toInt();
                item.categoryType = unlinkedQuery.value(9).toInt();
                item.track = unlinkedQuery.value(10).toInt();
                item.keySymbol = unlinkedQuery.value(11).toString().trimmed();
                item.issueTagNumber = unlinkedQuery.value(12).toInt();
                item.mepsLanguageIndex = unlinkedQuery.value(13).toInt();
                item.beginParagraphOrdinal = -1;
                item.endParagraphOrdinal = -1;
                item.linkMultimediaId = unlinkedQuery.value(16).toLongLong();
                item.suppressZoom = unlinkedQuery.value(17).toInt() != 0;
                item.videoMarkers = markerMap.value(item.multimediaId);
                enrichMediaItemFromContext(item, docMap, questionLabels, extracts);
                items.push_back(std::move(item));
            }
        }
    }

    return items;
}

QDate mondayForDate(const QDate& date)
{
    if (!date.isValid()) {
        return {};
    }
    return date.addDays(-(date.dayOfWeek() - 1));
}

int toDateOffsetInt(const QDate& date)
{
    return date.isValid() ? (date.year() * 10000 + date.month() * 100 + date.day()) : 0;
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
    pkg.extracts = readExtracts(db);
    const QHash<qint64, std::vector<JwpubVideoMarker>> markers = readVideoMarkers(db);
    const QHash<qint64, QHash<int, QString>> questionLabels = readQuestionLabels(db);
    pkg.mediaItems = readMultimediaItems(db, markers, pkg.documents, pkg.extracts, questionLabels);
    pkg.valid = true;
    return pkg;
}

JwpubPackage JwpubParser::parseForWeek(const QString& jwpubPath,
                                       const JwpubWeekFilterOptions& options)
{
    const JwpubPackage raw = parse(jwpubPath);
    if (!raw.valid) {
        return raw;
    }
    return filterPackageForWeek(raw, options);
}

JwpubPackage JwpubParser::filterPackageForWeek(const JwpubPackage& package,
                                               const JwpubWeekFilterOptions& options)
{
    if (!package.valid) {
        return package;
    }

    JwpubPackage out = package;
    const QString sym = options.pubSymbolHint.trimmed().isEmpty()
                            ? package.publication.undatedSymbol.trimmed().toLower()
                            : options.pubSymbolHint.trimmed().toLower();
    const bool isWatchtower = (sym == QStringLiteral("w"));
    const bool isMemorial = sym.startsWith(QStringLiteral("mi"));

    QSet<qint64> matchedDocIds;
    if (options.targetDate.isValid()) {
        const int targetInt = toDateOffsetInt(options.targetDate);
        const int mondayInt = toDateOffsetInt(mondayForDate(options.targetDate));
        for (const JwpubDocument& doc : package.documents) {
            if (doc.firstDateOffset > 0 &&
                ((doc.firstDateOffset <= targetInt &&
                  (doc.lastDateOffset == 0 || targetInt <= doc.lastDateOffset)) ||
                 doc.firstDateOffset == mondayInt)) {
                matchedDocIds.insert(doc.documentId);
            }
        }

        if (matchedDocIds.isEmpty() && isWatchtower) {
            std::vector<const JwpubDocument*> studyDocs;
            for (const JwpubDocument& doc : package.documents) {
                if (doc.classCode == 40) {
                    studyDocs.push_back(&doc);
                }
            }
            if (studyDocs.empty()) {
                for (const JwpubDocument& doc : package.documents) {
                    studyDocs.push_back(&doc);
                }
            }
            if (!studyDocs.empty()) {
                int weekIdx = 0;
                const QDate targetMon = mondayForDate(options.targetDate);
                if (package.publication.issueTagNumber >= 19000100) {
                    const int y = package.publication.issueTagNumber / 10000;
                    const int m = (package.publication.issueTagNumber / 100) % 100;
                    const QDate issueMonthStart(y, m, 1);
                    if (issueMonthStart.isValid()) {
                        const QDate studyMonth = issueMonthStart.addMonths(2);
                        QDate firstStudyMon = studyMonth;
                        while (firstStudyMon.dayOfWeek() != Qt::Monday) {
                            firstStudyMon = firstStudyMon.addDays(1);
                        }
                        weekIdx = static_cast<int>(firstStudyMon.daysTo(targetMon) / 7);
                    }
                } else {
                    weekIdx = (targetMon.day() - 1) / 7;
                }
                weekIdx = std::clamp(weekIdx, 0, static_cast<int>(studyDocs.size()) - 1);
                matchedDocIds.insert(studyDocs[static_cast<size_t>(weekIdx)]->documentId);
            }
        }
    }

    // Track primary video targets pointed to via LinkMultimediaId to drop duplicate crop stills.
    QHash<qint64, const JwpubMediaItem*> idToItem;
    for (const JwpubMediaItem& item : package.mediaItems) {
        idToItem.insert(item.multimediaId, &item);
    }

    if (!matchedDocIds.isEmpty()) {
        std::vector<JwpubDocument> filteredDocs;
        for (const JwpubDocument& doc : package.documents) {
            if (matchedDocIds.contains(doc.documentId)) {
                filteredDocs.push_back(doc);
            }
        }
        out.documents = std::move(filteredDocs);
    }

    std::vector<JwpubMediaItem> filteredMedia;
    for (JwpubMediaItem item : package.mediaItems) {
        if (item.suppressZoom) {
            continue;
        }
        const bool keepUnlinkedMemorial =
            isMemorial && item.categoryType == 26 && item.beginParagraphOrdinal == -1;
        if (!matchedDocIds.isEmpty() && !keepUnlinkedMemorial &&
            !matchedDocIds.contains(item.documentId)) {
            continue;
        }
        if (options.excludeFootnotes && item.isFootnote) {
            continue;
        }
        if (!options.includePrinted && item.isPrinted && !keepUnlinkedMemorial) {
            continue;
        }
        if (options.excludeWtParagraphVideos && isWatchtower && item.majorType == 2 &&
            item.beginParagraphOrdinal > 0) {
            continue;
        }
        if (options.excludeTh &&
            item.refPubSymbol.compare(QStringLiteral("th"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        if (options.excludeCbsPubs &&
            (item.refPubSymbol.compare(QStringLiteral("wcg"), Qt::CaseInsensitive) == 0 ||
             item.isCbsCandidate)) {
            continue;
        }
        if (item.linkMultimediaId > 0 && item.majorType == 1) {
            if (const auto linkedIt = idToItem.constFind(item.linkMultimediaId);
                linkedIt != idToItem.constEnd() && linkedIt.value()->majorType == 2) {
                continue;
            }
        }
        filteredMedia.push_back(std::move(item));
    }

    // Propagate labels from linked thumbnail items onto their target videos when needed.
    for (JwpubMediaItem& kept : filteredMedia) {
        if (kept.majorType == 2 && kept.label.isEmpty()) {
            for (const JwpubMediaItem& raw : package.mediaItems) {
                if (raw.linkMultimediaId == kept.multimediaId && !raw.label.isEmpty()) {
                    kept.label = raw.label;
                    break;
                }
            }
        }
    }
    out.mediaItems = std::move(filteredMedia);

    std::vector<JwpubExtractRef> filteredExtracts;
    for (const JwpubExtractRef& ref : package.extracts) {
        if (!matchedDocIds.isEmpty() && !matchedDocIds.contains(ref.documentId)) {
            continue;
        }
        if (options.excludeTh && ref.isTh) {
            continue;
        }
        if (options.excludeCbsPubs && ref.isCbsSource) {
            continue;
        }
        filteredExtracts.push_back(ref);
    }
    out.extracts = std::move(filteredExtracts);
    return out;
}

QString JwpubParser::extractContents(const QString& jwpubPath, const QString& destDir,
                                     QString* error)
{
    const QFileInfo archive(jwpubPath);
    if (!archive.exists()) {
        if (error != nullptr) {
            *error = QStringLiteral("Publication archive does not exist");
        }
        return {};
    }

    const QDir target(destDir);
    const QString markerPath = target.filePath(QStringLiteral(".kmedia-extracted"));
    const QByteArray signature = QStringLiteral("%1|%2")
                                     .arg(archive.size())
                                     .arg(archive.lastModified().toMSecsSinceEpoch())
                                     .toUtf8();
    const auto findDatabase = [&target]() -> QString {
        const QStringList databases =
            target.entryList({QStringLiteral("*.db")}, QDir::Files, QDir::Name);
        return databases.isEmpty() ? QString() : target.filePath(databases.first());
    };

    QFile marker(markerPath);
    if (marker.open(QIODevice::ReadOnly) && marker.readAll() == signature) {
        const QString existing = findDatabase();
        if (!existing.isEmpty()) {
            return existing;
        }
    }
    marker.close();

    ZipArchiveReader outerZip;
    if (!outerZip.openFile(jwpubPath, error)) {
        return {};
    }
    QTemporaryDir sandbox;
    sandbox.setAutoRemove(true);
    if (!sandbox.isValid()) {
        if (error != nullptr) {
            *error = QStringLiteral("Failed to create temporary extraction sandbox");
        }
        return {};
    }
    ZipArchiveReader innerZip;
    if (!openInnerContentsReader(outerZip, sandbox, innerZip, error)) {
        return {};
    }
    if (!QDir().mkpath(destDir) || !innerZip.extractEntries(destDir, {}, {}, nullptr, error)) {
        return {};
    }

    const QString database = findDatabase();
    if (database.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("Publication contains no SQLite database");
        }
        return {};
    }
    if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        marker.write(signature);
    }
    return database;
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
