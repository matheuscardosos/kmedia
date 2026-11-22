#pragma once

#include "parsers/SqlitePackageUtils.h"

#include <QDate>
#include <QString>
#include <QtGlobal>

#include <optional>
#include <vector>

namespace kmedia {

struct PublicationVideoMarker
{
    qint64 videoMarkerId{0};
    QString label;
    qint64 startTimeTicks{0};
    qint64 durationTicks{0};
    qint64 endTransitionDurationTicks{0};
};

/** One media row of a publication database, as the meeting queries return it. */
struct PublicationMediaRow
{
    qint64 multimediaId{0};
    qint64 documentId{0};
    qint64 mepsDocumentId{0};
    QString keySymbol;
    int track{0};
    int issueTagNumber{0};
    int mepsLanguageIndex{-1};
    int mepsLanguageAlternativeIndex{-1};
    QString mimeType;
    int categoryType{0};
    QString label;
    QString caption;
    QString filePath;
    qint64 linkMultimediaId{0};
    QString linkedPreviewFilePath;
    int beginParagraphOrdinal{0};
    int endParagraphOrdinal{0};
    int beginPosition{0};
    bool hasBeginPosition{false};
    QString targetParagraphNumberLabel;
    int isInNumberedParagraphs{-1};
    QString extractCaption;
    QString extractSymbol;
    std::vector<PublicationVideoMarker> videoMarkers;
    // Filled while syncing: where the file lives (or will be downloaded), the stream URL for
    // long media, its duration, and a language other than the congregation's when one applies.
    QString streamUrl;
    double durationSeconds{0.0};
    QString languageOverride;
};

/** A DocumentExtract that points at another publication. */
struct PublicationExtractRow
{
    int beginParagraphOrdinal{0};
    int endParagraphOrdinal{0};
    qint64 documentId{0};
    qint64 refMepsDocumentId{0};
    QString uniqueEnglishSymbol;
    int issueTagNumber{0};
    int refBeginParagraphOrdinal{0};
    int refEndParagraphOrdinal{0};
    QString link;
    QString caption;
};

/** A song extract of the weekend study, used to keep the songs' language override. */
struct PublicationSongExtract
{
    qint64 extractId{0};
    QString link;
    int beginParagraphOrdinal{0};
};

struct PublicationLanguageRow
{
    QString keySymbol;
    int track{0};
    int issueTagNumber{0};
    int mepsLanguageIndex{-1};
};

struct WatchtowerStudyDocument
{
    qint64 documentId{-1};
    QString title;
};

struct PublicationDocumentEntry
{
    qint64 documentId{-1};
    QString title;
};

struct PublicationSummary
{
    QString title;
    QString symbol;
    int year{0};
    int versionNumber{0};
};

/**
 * Read-only queries over an extracted publication database, matching what the meeting media
 * sync needs: the document of a meeting week, its media and paragraph labels, the extracts
 * that point at other publications, song positions, and the language of each media item.
 */
class PublicationDatabase
{
public:
    static constexpr int kFootnoteParagraph = 1500;
    static constexpr int kLastSongOrdinal = 2000;
    static constexpr int kSongsMovedToVideosIssue = 20230200;

    struct MediaSelector
    {
        qint64 documentId{-1};
        qint64 mepsDocumentId{0};
        bool hasParagraphRange{false};
        int beginParagraphOrdinal{0};
        int endParagraphOrdinal{0};
    };

    [[nodiscard]] bool open(const QString& dbFilePath, QString* error = nullptr);
    void close();
    [[nodiscard]] bool isOpen() const;

    [[nodiscard]] qint64 midweekDocumentId(const QDate& weekMonday) const;
    [[nodiscard]] std::optional<WatchtowerStudyDocument>
    watchtowerStudyForWeek(const QDate& weekMonday) const;

    [[nodiscard]] std::vector<PublicationMediaRow> documentMultimedia(const MediaSelector& selector,
                                                                      bool includePrinted) const;
    [[nodiscard]] std::vector<PublicationExtractRow> documentExtracts(qint64 documentId,
                                                                      bool excludeTh) const;
    [[nodiscard]] std::vector<PublicationMediaRow> weekendVideos(qint64 documentId) const;
    [[nodiscard]] std::vector<PublicationMediaRow>
    weekendMediaWithoutVideos(qint64 documentId, const QString& songbookSymbol) const;
    [[nodiscard]] std::vector<PublicationMediaRow> legacyWeekendSongs(qint64 documentId) const;
    [[nodiscard]] std::vector<PublicationSongExtract> weekendSongExtracts(qint64 documentId) const;
    [[nodiscard]] std::vector<PublicationVideoMarker> videoMarkers(qint64 multimediaId) const;
    [[nodiscard]] std::vector<PublicationLanguageRow> mediaLanguages() const;
    [[nodiscard]] std::vector<PublicationMediaRow> memorialMedia() const;
    [[nodiscard]] std::vector<PublicationDocumentEntry> documentsWithMedia() const;
    [[nodiscard]] PublicationSummary summary() const;

    [[nodiscard]] static std::vector<PublicationMediaRow>
    dedupeLinkedMultimedia(std::vector<PublicationMediaRow> rows);

private:
    struct SongOrdinal
    {
        int beginParagraphOrdinal{0};
        int endParagraphOrdinal{0};
        int sortPosition{0};
    };

    [[nodiscard]] std::vector<SongOrdinal> songExtractOrdinals(qint64 documentId) const;
    void alignSongOrdinals(std::vector<PublicationMediaRow>& rows, qint64 documentId,
                           bool paragraphColumnsExist) const;

    ScopedReadOnlySqliteDb m_db;
};

} // namespace kmedia
