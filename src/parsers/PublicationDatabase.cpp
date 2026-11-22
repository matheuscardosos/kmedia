#include "parsers/PublicationDatabase.h"

#include <QHash>
#include <QSet>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringList>
#include <QVariant>

#include <algorithm>

namespace kmedia {

namespace {

struct DatabaseShape
{
    QString mediaTable;
    bool paragraphColumns{false};
    bool paragraphTable{false};
    bool questionLabels{false};
    bool linkColumn{false};
    bool suppressZoomColumn{false};
};

QString optionalColumn(const QSet<QString>& columns, const QString& table, const QString& column,
                       const QString& fallback)
{
    return columns.contains(column) ? QStringLiteral("%1.%2").arg(table, column)
                                    : QStringLiteral("%1 AS %2").arg(fallback, column);
}

int intValue(const QSqlRecord& record, const QSqlQuery& query, const char* name, int fallback = 0)
{
    const int index = record.indexOf(QLatin1StringView(name));
    if (index < 0 || query.value(index).isNull()) {
        return fallback;
    }
    return query.value(index).toInt();
}

qint64 longValue(const QSqlRecord& record, const QSqlQuery& query, const char* name)
{
    const int index = record.indexOf(QLatin1StringView(name));
    return index < 0 ? 0 : query.value(index).toLongLong();
}

QString textValue(const QSqlRecord& record, const QSqlQuery& query, const char* name)
{
    const int index = record.indexOf(QLatin1StringView(name));
    return index < 0 ? QString() : query.value(index).toString();
}

PublicationMediaRow readMediaRow(const QSqlQuery& query)
{
    const QSqlRecord record = query.record();
    PublicationMediaRow row;
    row.multimediaId = longValue(record, query, "MultimediaId");
    row.documentId = longValue(record, query, "DocumentId");
    row.mepsDocumentId = longValue(record, query, "MepsDocumentId");
    row.keySymbol = textValue(record, query, "KeySymbol");
    row.track = intValue(record, query, "Track");
    row.issueTagNumber = intValue(record, query, "IssueTagNumber");
    row.mepsLanguageIndex = intValue(record, query, "MepsLanguageIndex", -1);
    row.mimeType = textValue(record, query, "MimeType");
    row.categoryType = intValue(record, query, "CategoryType");
    row.label = textValue(record, query, "Label");
    row.caption = textValue(record, query, "Caption");
    row.filePath = textValue(record, query, "FilePath");
    row.linkMultimediaId = longValue(record, query, "LinkMultimediaId");
    row.linkedPreviewFilePath = textValue(record, query, "LinkedPreviewFilePath");
    row.beginParagraphOrdinal = intValue(record, query, "BeginParagraphOrdinal");
    row.endParagraphOrdinal = intValue(record, query, "EndParagraphOrdinal");
    const int beginPositionIndex = record.indexOf(QStringLiteral("BeginPosition"));
    row.hasBeginPosition = beginPositionIndex >= 0 && !query.value(beginPositionIndex).isNull();
    row.beginPosition = row.hasBeginPosition ? query.value(beginPositionIndex).toInt() : 0;
    row.targetParagraphNumberLabel = textValue(record, query, "TargetParagraphNumberLabel");
    row.isInNumberedParagraphs = intValue(record, query, "IsInNumberedParagraphs", -1);
    return row;
}

// Paragraph label of the first and last labeled paragraph covered by a media item, so a picture
// that spans paragraphs reads "4-5" instead of the label of its (possibly unlabeled) first one.
QString paragraphRangeLabelSql(const QString& mediaTable)
{
    const QString begin =
        QStringLiteral("(SELECT MIN(agg.BeginParagraphOrdinal) FROM %1 agg WHERE agg.DocumentId = "
                       "%1.DocumentId AND agg.MultimediaId = %1.MultimediaId)")
            .arg(mediaTable);
    const QString end =
        QStringLiteral("(SELECT MAX(agg.EndParagraphOrdinal) FROM %1 agg WHERE agg.DocumentId = "
                       "%1.DocumentId AND agg.MultimediaId = %1.MultimediaId)")
            .arg(mediaTable);
    const auto labeled = [&](const char* order) {
        return QStringLiteral("(SELECT dpr.ParagraphNumberLabel FROM DocumentParagraph dpr "
                              "WHERE dpr.DocumentId = %1.DocumentId AND dpr.ParagraphIndex >= %2 "
                              "AND dpr.ParagraphIndex <= %3 AND dpr.ParagraphNumberLabel IS NOT "
                              "NULL AND dpr.ParagraphNumberLabel <> '' ORDER BY dpr.ParagraphIndex "
                              "%4 LIMIT 1)")
            .arg(mediaTable, begin, end, QLatin1StringView(order));
    };
    const QString first = labeled("ASC");
    const QString last = labeled("DESC");
    return QStringLiteral("CASE WHEN %1 IS NULL THEN NULL WHEN %1 = %2 THEN %1 ELSE %1 || '-' || "
                          "%2 END")
        .arg(first, last);
}

DatabaseShape inspectShape(const ScopedReadOnlySqliteDb& db)
{
    DatabaseShape shape;
    shape.mediaTable = db.tableExists(QStringLiteral("DocumentMultimedia"))
                           ? QStringLiteral("DocumentMultimedia")
                           : QStringLiteral("Multimedia");
    shape.paragraphColumns =
        db.tableColumns(shape.mediaTable).contains(QStringLiteral("BeginParagraphOrdinal"));
    shape.paragraphTable = db.tableExists(QStringLiteral("DocumentParagraph"));
    if (db.tableColumns(QStringLiteral("Question"))
            .contains(QStringLiteral("TargetParagraphNumberLabel"))) {
        QSqlQuery count(db.database());
        shape.questionLabels = count.exec(QStringLiteral("SELECT COUNT(*) FROM Question")) &&
                               count.next() && count.value(0).toInt() > 0;
    }
    const QSet<QString> mediaColumns = db.tableColumns(QStringLiteral("Multimedia"));
    shape.linkColumn = mediaColumns.contains(QStringLiteral("LinkMultimediaId"));
    shape.suppressZoomColumn = mediaColumns.contains(QStringLiteral("SuppressZoom"));
    return shape;
}

QString mediaColumnList(const ScopedReadOnlySqliteDb& db)
{
    const QSet<QString> columns = db.tableColumns(QStringLiteral("Multimedia"));
    const QString table = QStringLiteral("Multimedia");
    return QStringList{
        QStringLiteral("Multimedia.MultimediaId"),
        optionalColumn(columns, table, QStringLiteral("KeySymbol"), QStringLiteral("NULL")),
        optionalColumn(columns, table, QStringLiteral("Track"), QStringLiteral("0")),
        optionalColumn(columns, table, QStringLiteral("IssueTagNumber"), QStringLiteral("0")),
        optionalColumn(columns, table, QStringLiteral("MepsLanguageIndex"), QStringLiteral("-1")),
        optionalColumn(columns, table, QStringLiteral("MimeType"), QStringLiteral("''")),
        optionalColumn(columns, table, QStringLiteral("CategoryType"), QStringLiteral("0")),
        optionalColumn(columns, table, QStringLiteral("Label"), QStringLiteral("''")),
        optionalColumn(columns, table, QStringLiteral("Caption"), QStringLiteral("''")),
        optionalColumn(columns, table, QStringLiteral("FilePath"), QStringLiteral("''")),
        optionalColumn(columns, table, QStringLiteral("LinkMultimediaId"), QStringLiteral("0")),
        // The media row's own document id wins over its parent document's, as it identifies
        // the file when it has to be downloaded.
        columns.contains(QStringLiteral("MepsDocumentId"))
            ? QStringLiteral("COALESCE(Multimedia.MepsDocumentId, 0) AS MepsDocumentId")
            : QStringLiteral("0 AS MepsDocumentId"),
    }
        .join(QStringLiteral(", "));
}

} // namespace

bool PublicationDatabase::open(const QString& dbFilePath, QString* error)
{
    return m_db.open(dbFilePath, error);
}

void PublicationDatabase::close()
{
    m_db.close();
}

bool PublicationDatabase::isOpen() const
{
    return m_db.isOpen();
}

// The documents that carry media, in publication order (for example the talks of S-34mp).
std::vector<PublicationDocumentEntry> PublicationDatabase::documentsWithMedia() const
{
    std::vector<PublicationDocumentEntry> documents;
    if (!m_db.isOpen() || !m_db.tableExists(QStringLiteral("Document")) ||
        !m_db.tableExists(QStringLiteral("DocumentMultimedia"))) {
        return documents;
    }
    QSqlQuery query(m_db.database());
    if (!query.exec(QStringLiteral(
            "SELECT DISTINCT Document.DocumentId, Document.Title FROM Document "
            "INNER JOIN DocumentMultimedia ON Document.DocumentId = DocumentMultimedia.DocumentId "
            "ORDER BY Document.DocumentId"))) {
        return documents;
    }
    while (query.next()) {
        documents.push_back(
            PublicationDocumentEntry{query.value(0).toLongLong(), query.value(1).toString()});
    }
    return documents;
}

PublicationSummary PublicationDatabase::summary() const
{
    PublicationSummary result;
    if (!m_db.isOpen() || !m_db.tableExists(QStringLiteral("Publication"))) {
        return result;
    }
    QSqlQuery query(m_db.database());
    if (!query.exec(QStringLiteral("SELECT * FROM Publication LIMIT 1")) || !query.next()) {
        return result;
    }
    const QSqlRecord record = query.record();
    const auto text = [&](const char* column) {
        const int index = record.indexOf(QLatin1StringView(column));
        return index >= 0 ? query.value(index).toString() : QString();
    };
    result.symbol = text("UndatedSymbol");
    result.year = text("Year").toInt();
    result.versionNumber = text("VersionNumber").toInt();
    return result;
}

qint64 PublicationDatabase::midweekDocumentId(const QDate& weekMonday) const
{
    if (!m_db.isOpen() || !m_db.tableExists(QStringLiteral("DatedText"))) {
        return -1;
    }
    QSqlQuery query(m_db.database());
    query.prepare(
        QStringLiteral("SELECT DocumentId FROM DatedText WHERE FirstDateOffset = :monday LIMIT 1"));
    query.bindValue(QStringLiteral(":monday"),
                    weekMonday.toString(QStringLiteral("yyyyMMdd")).toInt());
    if (!query.exec() || !query.next()) {
        return -1;
    }
    return query.value(0).toLongLong();
}

std::optional<WatchtowerStudyDocument>
PublicationDatabase::watchtowerStudyForWeek(const QDate& weekMonday) const
{
    if (!m_db.isOpen() || !m_db.tableExists(QStringLiteral("DatedText"))) {
        return std::nullopt;
    }
    // The position of the week among the issue's dated texts is the position of its study
    // article among the issue's study documents.
    QSqlQuery dated(m_db.database());
    if (!dated.exec(QStringLiteral("SELECT FirstDateOffset FROM DatedText"))) {
        return std::nullopt;
    }
    const int monday = weekMonday.toString(QStringLiteral("yyyyMMdd")).toInt();
    int weekIndex = -1;
    for (int index = 0; dated.next(); ++index) {
        if (dated.value(0).toInt() == monday) {
            weekIndex = index;
            break;
        }
    }
    if (weekIndex < 0) {
        return std::nullopt;
    }

    QSqlQuery study(m_db.database());
    study.prepare(QStringLiteral("SELECT DocumentId, Title FROM Document WHERE Class = 40 "
                                 "LIMIT 1 OFFSET :offset"));
    study.bindValue(QStringLiteral(":offset"), weekIndex);
    if (!study.exec() || !study.next()) {
        return std::nullopt;
    }
    return WatchtowerStudyDocument{study.value(0).toLongLong(), study.value(1).toString()};
}

std::vector<PublicationMediaRow>
PublicationDatabase::documentMultimedia(const MediaSelector& selector, bool includePrinted) const
{
    std::vector<PublicationMediaRow> rows;
    if (!m_db.isOpen() || !m_db.tableExists(QStringLiteral("Multimedia")) ||
        !m_db.tableExists(QStringLiteral("Document"))) {
        return rows;
    }

    const DatabaseShape shape = inspectShape(m_db);
    const QString& mm = shape.mediaTable;
    const bool paragraphJoin = shape.paragraphColumns &&
                               mm == QStringLiteral("DocumentMultimedia") && shape.paragraphTable;

    QString select = QStringLiteral("SELECT ") + mediaColumnList(m_db) +
                     QStringLiteral(", Document.DocumentId AS DocumentId");
    if (!m_db.tableColumns(QStringLiteral("Multimedia"))
             .contains(QStringLiteral("MepsDocumentId"))) {
        select.replace(QStringLiteral("0 AS MepsDocumentId"),
                       QStringLiteral("Document.MepsDocumentId AS MepsDocumentId"));
    }
    if (shape.paragraphColumns) {
        select += QStringLiteral(", %1.BeginParagraphOrdinal AS BeginParagraphOrdinal, "
                                 "%1.EndParagraphOrdinal AS EndParagraphOrdinal")
                      .arg(mm);
    }
    if (paragraphJoin) {
        select += QStringLiteral(", DocumentParagraph.BeginPosition AS BeginPosition");
    }
    if (shape.linkColumn) {
        select += QStringLiteral(", LinkedMultimedia.FilePath AS LinkedPreviewFilePath");
    }
    if (shape.paragraphColumns && shape.paragraphTable) {
        const QString rangeLabel = paragraphRangeLabelSql(mm);
        select += shape.questionLabels
                      ? QStringLiteral(", COALESCE(Question.TargetParagraphNumberLabel, %1) AS "
                                       "TargetParagraphNumberLabel")
                            .arg(rangeLabel)
                      : QStringLiteral(", %1 AS TargetParagraphNumberLabel").arg(rangeLabel);
    }

    QString from = QStringLiteral(" FROM Multimedia");
    if (mm == QStringLiteral("DocumentMultimedia")) {
        from +=
            QStringLiteral(" INNER JOIN DocumentMultimedia ON DocumentMultimedia.MultimediaId = "
                           "Multimedia.MultimediaId");
        if (paragraphJoin) {
            from += QStringLiteral(" LEFT JOIN DocumentParagraph ON "
                                   "DocumentMultimedia.BeginParagraphOrdinal = "
                                   "DocumentParagraph.ParagraphIndex AND "
                                   "DocumentParagraph.DocumentId = DocumentMultimedia.DocumentId");
        }
    }
    from += QStringLiteral(" INNER JOIN Document ON %1.DocumentId = Document.DocumentId").arg(mm);
    if (shape.linkColumn) {
        from += QStringLiteral(" LEFT JOIN Multimedia AS LinkedMultimedia ON "
                               "Multimedia.LinkMultimediaId = LinkedMultimedia.MultimediaId");
    }
    if (shape.questionLabels && shape.paragraphColumns) {
        from += QStringLiteral(" LEFT JOIN Question ON Question.DocumentId = %1.DocumentId AND "
                               "Question.TargetParagraphOrdinal = %1.BeginParagraphOrdinal")
                    .arg(mm);
    }

    QString where = selector.documentId >= 0
                        ? QStringLiteral(" WHERE Document.DocumentId = :doc")
                        : QStringLiteral(" WHERE Document.MepsDocumentId = :doc");
    // Videos and audio always; pictures except printed-only art (unless asked for) and the
    // categories that never show on screen.
    where += QStringLiteral(" AND ((Multimedia.MimeType LIKE '%video%' OR Multimedia.MimeType "
                            "LIKE '%audio%') OR (Multimedia.MimeType LIKE '%image%'");
    if (!includePrinted) {
        where +=
            QStringLiteral(" AND Multimedia.CategoryType <> 4 AND Multimedia.CategoryType <> 6");
    }
    where += QStringLiteral(" AND Multimedia.CategoryType <> 9 AND Multimedia.CategoryType <> 10 "
                            "AND Multimedia.CategoryType <> 25))");
    if (selector.hasParagraphRange && shape.paragraphColumns) {
        where += QStringLiteral(" AND %1.BeginParagraphOrdinal >= :begin AND "
                                "%1.EndParagraphOrdinal <= :end")
                     .arg(mm);
    }
    if (shape.suppressZoomColumn) {
        where += QStringLiteral(" AND COALESCE(Multimedia.SuppressZoom, 0) <> 1");
    }
    const QString groupAndSort =
        shape.paragraphColumns
            ? QStringLiteral(" GROUP BY Multimedia.MultimediaId ORDER BY %1.BeginParagraphOrdinal")
                  .arg(mm)
            : QString();

    QSqlQuery query(m_db.database());
    if (!query.prepare(select + from + where + groupAndSort)) {
        return rows;
    }
    query.bindValue(QStringLiteral(":doc"),
                    selector.documentId >= 0 ? selector.documentId : selector.mepsDocumentId);
    if (selector.hasParagraphRange && shape.paragraphColumns) {
        query.bindValue(QStringLiteral(":begin"), selector.beginParagraphOrdinal);
        query.bindValue(QStringLiteral(":end"), selector.endParagraphOrdinal);
    }
    if (!query.exec()) {
        return rows;
    }
    while (query.next()) {
        rows.push_back(readMediaRow(query));
    }

    for (PublicationMediaRow& row : rows) {
        row.videoMarkers = videoMarkers(row.multimediaId);
    }
    if (selector.documentId >= 0) {
        alignSongOrdinals(rows, selector.documentId, shape.paragraphColumns);
    }
    return dedupeLinkedMultimedia(std::move(rows));
}

std::vector<PublicationExtractRow> PublicationDatabase::documentExtracts(qint64 documentId,
                                                                         bool excludeTh) const
{
    std::vector<PublicationExtractRow> extracts;
    if (!m_db.isOpen() || !m_db.tableExists(QStringLiteral("DocumentExtract")) ||
        !m_db.tableExists(QStringLiteral("Extract")) ||
        !m_db.tableExists(QStringLiteral("RefPublication"))) {
        return extracts;
    }
    QString sql = QStringLiteral(
        "SELECT DocumentExtract.BeginParagraphOrdinal, DocumentExtract.EndParagraphOrdinal, "
        "DocumentExtract.DocumentId, Extract.RefMepsDocumentId, "
        "RefPublication.UniqueEnglishSymbol, "
        "RefPublication.IssueTagNumber, Extract.RefBeginParagraphOrdinal, "
        "Extract.RefEndParagraphOrdinal, Extract.Link, Extract.Caption "
        "FROM DocumentExtract "
        "INNER JOIN Extract ON DocumentExtract.ExtractId = Extract.ExtractId "
        "INNER JOIN RefPublication ON Extract.RefPublicationId = RefPublication.RefPublicationId "
        "INNER JOIN Document ON DocumentExtract.DocumentId = Document.DocumentId "
        "WHERE DocumentExtract.DocumentId = :doc "
        "AND NOT RefPublication.UniqueEnglishSymbol LIKE 'mwbr%' "
        "AND NOT RefPublication.UniqueEnglishSymbol = 'sjj' ");
    if (excludeTh) {
        sql += QStringLiteral("AND NOT RefPublication.UniqueEnglishSymbol = 'th' ");
    }
    sql += QStringLiteral("ORDER BY DocumentExtract.BeginParagraphOrdinal");

    QSqlQuery query(m_db.database());
    if (!query.prepare(sql)) {
        return extracts;
    }
    query.bindValue(QStringLiteral(":doc"), documentId);
    if (!query.exec()) {
        return extracts;
    }
    while (query.next()) {
        PublicationExtractRow row;
        row.beginParagraphOrdinal = query.value(0).toInt();
        row.endParagraphOrdinal = query.value(1).toInt();
        row.documentId = query.value(2).toLongLong();
        row.refMepsDocumentId = query.value(3).toLongLong();
        row.uniqueEnglishSymbol = query.value(4).toString();
        row.issueTagNumber = query.value(5).toInt();
        row.refBeginParagraphOrdinal = query.value(6).toInt();
        row.refEndParagraphOrdinal = query.value(7).toInt();
        row.link = query.value(8).toString();
        row.caption = query.value(9).toString();
        extracts.push_back(std::move(row));
    }
    return extracts;
}

std::vector<PublicationMediaRow> PublicationDatabase::weekendVideos(qint64 documentId) const
{
    std::vector<PublicationMediaRow> rows;
    if (!m_db.isOpen() || !m_db.tableExists(QStringLiteral("DocumentMultimedia")) ||
        !m_db.tableExists(QStringLiteral("DocumentParagraph"))) {
        return rows;
    }
    const DatabaseShape shape = inspectShape(m_db);
    // A video counts as being inside the numbered paragraphs when it sits between the first
    // and the last labeled paragraph of the article; anything outside reads as a footnote.
    QString sql =
        QStringLiteral("SELECT ") + mediaColumnList(m_db) +
        QStringLiteral(
            ", dm.DocumentId AS DocumentId, dm.BeginParagraphOrdinal AS BeginParagraphOrdinal, "
            "dm.EndParagraphOrdinal AS EndParagraphOrdinal, dp.BeginPosition AS BeginPosition, "
            "CASE WHEN dp.BeginPosition IS NULL THEN 0 "
            "WHEN dp.BeginPosition < (SELECT MIN(BeginPosition) FROM DocumentParagraph WHERE "
            "DocumentId = :doc1 AND ParagraphNumberLabel IS NOT NULL AND ParagraphNumberLabel != "
            "'') THEN 0 "
            "WHEN dp.BeginPosition > (SELECT MAX(EndPosition) FROM DocumentParagraph WHERE "
            "DocumentId = :doc2 AND ParagraphNumberLabel IS NOT NULL AND ParagraphNumberLabel != "
            "'') THEN 0 ELSE 1 END AS IsInNumberedParagraphs");
    if (shape.questionLabels) {
        sql += QStringLiteral(", q.TargetParagraphNumberLabel AS TargetParagraphNumberLabel");
    }
    sql = sql.replace(QStringLiteral("Multimedia."), QStringLiteral("m."));
    sql += QStringLiteral(" FROM Multimedia m "
                          "INNER JOIN DocumentMultimedia dm ON dm.MultimediaId = m.MultimediaId "
                          "INNER JOIN DocumentParagraph dp ON dm.BeginParagraphOrdinal = "
                          "dp.ParagraphIndex AND dm.DocumentId = dp.DocumentId ");
    if (shape.questionLabels) {
        sql += QStringLiteral("LEFT JOIN Question q ON q.DocumentId = dm.DocumentId AND "
                              "q.TargetParagraphOrdinal = dm.BeginParagraphOrdinal ");
    }
    sql += QStringLiteral("WHERE dm.DocumentId = :doc3 AND m.CategoryType = -1 "
                          "ORDER BY dp.BeginPosition");

    QSqlQuery query(m_db.database());
    if (!query.prepare(sql)) {
        return rows;
    }
    query.bindValue(QStringLiteral(":doc1"), documentId);
    query.bindValue(QStringLiteral(":doc2"), documentId);
    query.bindValue(QStringLiteral(":doc3"), documentId);
    if (!query.exec()) {
        return rows;
    }
    while (query.next()) {
        rows.push_back(readMediaRow(query));
    }
    for (PublicationMediaRow& row : rows) {
        row.videoMarkers = videoMarkers(row.multimediaId);
    }
    return rows;
}

std::vector<PublicationMediaRow>
PublicationDatabase::weekendMediaWithoutVideos(qint64 documentId,
                                               const QString& songbookSymbol) const
{
    std::vector<PublicationMediaRow> rows;
    if (!m_db.isOpen() || !m_db.tableExists(QStringLiteral("DocumentMultimedia")) ||
        !m_db.tableExists(QStringLiteral("DocumentParagraph"))) {
        return rows;
    }
    const DatabaseShape shape = inspectShape(m_db);
    const QString rangeLabel = paragraphRangeLabelSql(QStringLiteral("DocumentMultimedia"));
    QString sql =
        QStringLiteral("SELECT ") + mediaColumnList(m_db) +
        QStringLiteral(", DocumentMultimedia.DocumentId AS DocumentId, "
                       "DocumentMultimedia.BeginParagraphOrdinal AS BeginParagraphOrdinal, "
                       "DocumentMultimedia.EndParagraphOrdinal AS EndParagraphOrdinal, "
                       "DocumentParagraph.BeginPosition AS BeginPosition");
    if (shape.linkColumn) {
        sql += QStringLiteral(", LinkedMultimedia.FilePath AS LinkedPreviewFilePath");
    }
    sql += shape.questionLabels
               ? QStringLiteral(", COALESCE(Question.TargetParagraphNumberLabel, %1) AS "
                                "TargetParagraphNumberLabel")
                     .arg(rangeLabel)
               : QStringLiteral(", %1 AS TargetParagraphNumberLabel").arg(rangeLabel);
    sql += QStringLiteral(
        " FROM DocumentMultimedia "
        "INNER JOIN Multimedia ON DocumentMultimedia.MultimediaId = "
        "Multimedia.MultimediaId "
        "INNER JOIN DocumentParagraph ON DocumentMultimedia.DocumentId = "
        "DocumentParagraph.DocumentId AND "
        "DocumentMultimedia.BeginParagraphOrdinal = DocumentParagraph.ParagraphIndex");
    if (shape.linkColumn) {
        sql += QStringLiteral(" LEFT JOIN Multimedia AS LinkedMultimedia ON "
                              "Multimedia.LinkMultimediaId = LinkedMultimedia.MultimediaId");
    }
    if (shape.questionLabels) {
        sql += QStringLiteral(" LEFT JOIN Question ON Question.DocumentId = "
                              "DocumentMultimedia.DocumentId AND Question.TargetParagraphOrdinal = "
                              "DocumentMultimedia.BeginParagraphOrdinal");
    }
    sql += QStringLiteral(" WHERE DocumentMultimedia.DocumentId = :doc AND "
                          "Multimedia.CategoryType <> 9 AND Multimedia.CategoryType <> -1 AND "
                          "(Multimedia.KeySymbol != :songbook OR Multimedia.KeySymbol IS NULL) "
                          "GROUP BY DocumentMultimedia.MultimediaId "
                          "ORDER BY DocumentParagraph.BeginPosition");

    QSqlQuery query(m_db.database());
    if (!query.prepare(sql)) {
        return rows;
    }
    query.bindValue(QStringLiteral(":doc"), documentId);
    query.bindValue(QStringLiteral(":songbook"), songbookSymbol);
    if (!query.exec()) {
        return rows;
    }
    while (query.next()) {
        rows.push_back(readMediaRow(query));
    }
    return rows;
}

std::vector<PublicationMediaRow> PublicationDatabase::legacyWeekendSongs(qint64 documentId) const
{
    std::vector<PublicationMediaRow> rows;
    if (!m_db.isOpen() || !m_db.tableExists(QStringLiteral("DocumentMultimedia")) ||
        !m_db.tableColumns(QStringLiteral("Multimedia")).contains(QStringLiteral("DataType"))) {
        return rows;
    }
    QSqlQuery query(m_db.database());
    query.prepare(
        QStringLiteral("SELECT ") + mediaColumnList(m_db) +
        QStringLiteral(", DocumentMultimedia.DocumentId AS DocumentId, "
                       "DocumentMultimedia.BeginParagraphOrdinal AS BeginParagraphOrdinal "
                       "FROM Multimedia INNER JOIN DocumentMultimedia ON "
                       "Multimedia.MultimediaId = DocumentMultimedia.MultimediaId "
                       "WHERE Multimedia.DataType = 2 AND DocumentMultimedia.DocumentId = "
                       ":doc ORDER BY DocumentMultimedia.BeginParagraphOrdinal LIMIT 2"));
    query.bindValue(QStringLiteral(":doc"), documentId);
    if (!query.exec()) {
        return rows;
    }
    while (query.next()) {
        rows.push_back(readMediaRow(query));
    }
    return rows;
}

std::vector<PublicationSongExtract>
PublicationDatabase::weekendSongExtracts(qint64 documentId) const
{
    std::vector<PublicationSongExtract> extracts;
    if (!m_db.isOpen() || !m_db.tableExists(QStringLiteral("Extract")) ||
        !m_db.tableExists(QStringLiteral("DocumentExtract")) ||
        !m_db.tableColumns(QStringLiteral("Extract"))
             .contains(QStringLiteral("RefMepsDocumentClass"))) {
        return extracts;
    }
    QSqlQuery query(m_db.database());
    query.prepare(QStringLiteral(
        "SELECT Extract.ExtractId, Extract.Link, DocumentExtract.BeginParagraphOrdinal "
        "FROM Extract INNER JOIN DocumentExtract ON Extract.ExtractId = DocumentExtract.ExtractId "
        "WHERE Extract.RefMepsDocumentClass = 31 AND DocumentExtract.DocumentId = :doc "
        "ORDER BY Extract.ExtractId LIMIT 2"));
    query.bindValue(QStringLiteral(":doc"), documentId);
    if (!query.exec()) {
        return extracts;
    }
    while (query.next()) {
        extracts.push_back(PublicationSongExtract{
            query.value(0).toLongLong(), query.value(1).toString(), query.value(2).toInt()});
    }
    std::stable_sort(extracts.begin(), extracts.end(),
                     [](const PublicationSongExtract& a, const PublicationSongExtract& b) {
                         return a.beginParagraphOrdinal < b.beginParagraphOrdinal;
                     });
    return extracts;
}

std::vector<PublicationVideoMarker> PublicationDatabase::videoMarkers(qint64 multimediaId) const
{
    std::vector<PublicationVideoMarker> markers;
    if (!m_db.isOpen() || multimediaId <= 0 || !m_db.tableExists(QStringLiteral("VideoMarker"))) {
        return markers;
    }
    QSqlQuery query(m_db.database());
    query.prepare(QStringLiteral(
        "SELECT VideoMarkerId, Label, StartTimeTicks, DurationTicks, EndTransitionDurationTicks "
        "FROM VideoMarker WHERE MultimediaId = :id ORDER BY StartTimeTicks"));
    query.bindValue(QStringLiteral(":id"), multimediaId);
    if (!query.exec()) {
        return markers;
    }
    while (query.next()) {
        markers.push_back(PublicationVideoMarker{
            query.value(0).toLongLong(), query.value(1).toString(), query.value(2).toLongLong(),
            query.value(3).toLongLong(), query.value(4).toLongLong()});
    }
    return markers;
}

std::vector<PublicationLanguageRow> PublicationDatabase::mediaLanguages() const
{
    std::vector<PublicationLanguageRow> languages;
    if (!m_db.isOpen()) {
        return languages;
    }
    for (const QString& table : {QStringLiteral("Multimedia"), QStringLiteral("Extract")}) {
        const QSet<QString> columns = m_db.tableColumns(table);
        if (!columns.contains(QStringLiteral("MepsLanguageIndex")) ||
            !columns.contains(QStringLiteral("KeySymbol"))) {
            continue;
        }
        QSqlQuery query(m_db.database());
        if (!query.exec(
                QStringLiteral("SELECT DISTINCT KeySymbol, %1, %2, MepsLanguageIndex FROM %3 "
                               "WHERE KeySymbol IS NOT NULL")
                    .arg(columns.contains(QStringLiteral("Track")) ? QStringLiteral("Track")
                                                                   : QStringLiteral("0"),
                         columns.contains(QStringLiteral("IssueTagNumber"))
                             ? QStringLiteral("IssueTagNumber")
                             : QStringLiteral("0"),
                         table))) {
            continue;
        }
        while (query.next()) {
            languages.push_back(
                PublicationLanguageRow{query.value(0).toString(), query.value(1).toInt(),
                                       query.value(2).toInt(), query.value(3).toInt()});
        }
    }
    return languages;
}

std::vector<PublicationMediaRow> PublicationDatabase::memorialMedia() const
{
    // The Memorial invitation publication carries one background picture (category 26) and
    // the introduction videos that are not tied to a paragraph.
    std::vector<PublicationMediaRow> rows;
    if (!m_db.isOpen() || !m_db.tableExists(QStringLiteral("Multimedia"))) {
        return rows;
    }
    const bool hasDocumentMedia = m_db.tableExists(QStringLiteral("DocumentMultimedia"));
    const QString join = hasDocumentMedia ? QStringLiteral(" INNER JOIN DocumentMultimedia ON "
                                                           "Multimedia.MultimediaId = "
                                                           "DocumentMultimedia.MultimediaId")
                                          : QString();
    const QStringList filters = {
        QStringLiteral("Multimedia.CategoryType = 26"),
        QStringLiteral("Multimedia.CategoryType = -1") +
            (hasDocumentMedia
                 ? QStringLiteral(" AND DocumentMultimedia.BeginParagraphOrdinal IS NULL")
                 : QString()),
    };
    for (const QString& filter : filters) {
        QSqlQuery query(m_db.database());
        if (!query.exec(QStringLiteral("SELECT DISTINCT ") + mediaColumnList(m_db) +
                        QStringLiteral(" FROM Multimedia") + join + QStringLiteral(" WHERE ") +
                        filter)) {
            continue;
        }
        while (query.next()) {
            rows.push_back(readMediaRow(query));
        }
    }
    return rows;
}

std::vector<PublicationMediaRow>
PublicationDatabase::dedupeLinkedMultimedia(std::vector<PublicationMediaRow> rows)
{
    // A picture with a LinkMultimediaId and the picture it points to are two crops of the same
    // image: keep the one tied to the paragraph, adopt the linked file, drop the other. Videos
    // keep their link, which points at their cover picture.
    QHash<qint64, size_t> indexById;
    for (size_t i = 0; i < rows.size(); ++i) {
        indexById.insert(rows[i].multimediaId, i);
    }
    for (PublicationMediaRow& row : rows) {
        if (row.linkMultimediaId == 0 || row.categoryType == -1) {
            continue;
        }
        const auto linked = indexById.constFind(row.linkMultimediaId);
        if (linked == indexById.constEnd()) {
            continue;
        }
        PublicationMediaRow& target = rows[linked.value()];
        if (target.filePath.isEmpty()) {
            continue;
        }
        row.filePath = target.filePath;
        row.linkMultimediaId = 0;
        target.linkMultimediaId = target.multimediaId;
    }
    std::vector<PublicationMediaRow> kept;
    kept.reserve(rows.size());
    for (PublicationMediaRow& row : rows) {
        if (row.categoryType == -1 || row.linkMultimediaId == 0) {
            kept.push_back(std::move(row));
        }
    }
    return kept;
}

std::vector<PublicationDatabase::SongOrdinal>
PublicationDatabase::songExtractOrdinals(qint64 documentId) const
{
    std::vector<SongOrdinal> ordinals;
    if (!m_db.tableExists(QStringLiteral("DocumentExtract")) ||
        !m_db.tableExists(QStringLiteral("Extract")) ||
        !m_db.tableExists(QStringLiteral("RefPublication")) ||
        !m_db.tableColumns(QStringLiteral("DocumentExtract"))
             .contains(QStringLiteral("SortPosition"))) {
        return ordinals;
    }
    QSqlQuery query(m_db.database());
    query.prepare(QStringLiteral(
        "SELECT DocumentExtract.BeginParagraphOrdinal, DocumentExtract.EndParagraphOrdinal, "
        "DocumentExtract.SortPosition FROM DocumentExtract "
        "INNER JOIN Extract ON DocumentExtract.ExtractId = Extract.ExtractId "
        "INNER JOIN RefPublication ON Extract.RefPublicationId = RefPublication.RefPublicationId "
        "WHERE DocumentExtract.DocumentId = :doc AND RefPublication.UndatedSymbol = 'sjj' "
        "ORDER BY DocumentExtract.SortPosition"));
    query.bindValue(QStringLiteral(":doc"), documentId);
    if (!query.exec()) {
        return ordinals;
    }
    while (query.next()) {
        ordinals.push_back(
            SongOrdinal{query.value(0).toInt(), query.value(1).toInt(), query.value(2).toInt()});
    }
    return ordinals;
}

void PublicationDatabase::alignSongOrdinals(std::vector<PublicationMediaRow>& rows,
                                            qint64 documentId, bool paragraphColumnsExist) const
{
    // Song videos can carry unreliable paragraph ordinals: take them from the song extracts in
    // order, then renumber the media found between two songs so the meeting order holds.
    std::vector<size_t> songs;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].keySymbol.contains(QStringLiteral("sjj"))) {
            songs.push_back(i);
        }
    }
    if (songs.empty()) {
        return;
    }
    const std::vector<SongOrdinal> ordinals = songExtractOrdinals(documentId);
    if (ordinals.empty()) {
        return;
    }

    QHash<qint64, int> originalOrdinal;
    for (const PublicationMediaRow& row : rows) {
        originalOrdinal.insert(row.multimediaId, row.beginParagraphOrdinal);
    }
    for (size_t i = 0; i < songs.size() && i < ordinals.size(); ++i) {
        PublicationMediaRow& song = rows[songs[i]];
        song.beginParagraphOrdinal = ordinals[i].beginParagraphOrdinal;
        song.endParagraphOrdinal = ordinals[i].endParagraphOrdinal;
        song.beginPosition = ordinals[i].sortPosition;
        song.hasBeginPosition = true;
    }

    const QSet<size_t> songSet(songs.begin(), songs.end());
    for (size_t s = 0; s + 1 < songs.size(); ++s) {
        const PublicationMediaRow& current = rows[songs[s]];
        const int currentOriginal = originalOrdinal.value(current.multimediaId);
        const int nextOriginal = originalOrdinal.value(rows[songs[s + 1]].multimediaId);
        std::vector<size_t> between;
        for (size_t i = 0; i < rows.size(); ++i) {
            const int original = originalOrdinal.value(rows[i].multimediaId);
            if (!songSet.contains(i) && original > currentOriginal && original < nextOriginal) {
                between.push_back(i);
            }
        }
        std::stable_sort(between.begin(), between.end(), [&](size_t a, size_t b) {
            return originalOrdinal.value(rows[a].multimediaId) <
                   originalOrdinal.value(rows[b].multimediaId);
        });
        int paragraph = current.beginParagraphOrdinal;
        int position = current.beginPosition;
        for (const size_t index : between) {
            ++paragraph;
            ++position;
            rows[index].beginParagraphOrdinal = paragraph;
            rows[index].endParagraphOrdinal = paragraph;
            rows[index].beginPosition = position;
            rows[index].hasBeginPosition = true;
        }
    }

    if (paragraphColumnsExist) {
        std::stable_sort(rows.begin(), rows.end(),
                         [](const PublicationMediaRow& a, const PublicationMediaRow& b) {
                             return a.beginParagraphOrdinal < b.beginParagraphOrdinal;
                         });
    }
}

} // namespace kmedia
