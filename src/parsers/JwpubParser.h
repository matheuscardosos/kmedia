#pragma once

#include <QDate>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <vector>

namespace kmedia {

struct JwpubManifest
{
    QString name;
    QString symbol;
    QString shortTitle;
    int issueTagNumber{0};
    int mepsLanguageIndex{0};
    QString hash;
    QString timestamp;
};

struct JwpubPublicationInfo
{
    QString title;
    QString symbol;
    QString undatedSymbol;
    QString uniqueEnglishSymbol;
    int issueTagNumber{0};
    int mepsLanguageIndex{0};
    int year{0};
    int versionNumber{0};
};

struct JwpubVideoMarker
{
    qint64 videoMarkerId{0};
    QString label;
    qint64 startTimeTicks{0};
    qint64 durationTicks{0};
    qint64 endTransitionDurationTicks{0};
};

struct JwpubDocument
{
    qint64 documentId{0};
    qint64 mepsDocumentId{0};
    QString title;
    QString subtitle;
    QString featureTitle;
    int classCode{0};
    int firstDateOffset{0};
    int lastDateOffset{0};
    int maxParagraphOrdinal{0};
};

struct JwpubMediaItem
{
    qint64 multimediaId{0};
    qint64 documentId{0};
    QString documentTitle;
    QString documentSubtitle;
    QString filePath;
    QString label;
    QString caption;
    QString mimeType;
    int majorType{0};
    int minorType{0};
    int categoryType{0};
    int track{0};
    QString keySymbol;
    int issueTagNumber{0};
    int mepsLanguageIndex{0};
    int documentClass{0};
    int mepsDocumentId{0};
    qint64 linkMultimediaId{0};
    bool suppressZoom{false};
    int beginParagraphOrdinal{0};
    int endParagraphOrdinal{0};
    int maxParagraphOrdinal{0};
    bool isFootnote{false};
    bool isPrinted{false};
    bool isCbsCandidate{false};
    QString paragraphLabel;
    QString groupCaption;
    QString tagType;
    QString tagValue;
    QString refPubSymbol;
    int refIssueTagNumber{0};
    qint64 refMepsDocumentId{0};
    int songNumber{0};
    std::vector<JwpubVideoMarker> videoMarkers;
};

struct JwpubExtractRef
{
    qint64 documentId{0};
    qint64 extractId{0};
    int beginParagraphOrdinal{0};
    int endParagraphOrdinal{0};
    QString caption;
    QString refPubSymbol;
    int refIssueTagNumber{0};
    int refMepsLanguageIndex{0};
    qint64 refMepsDocumentId{0};
    int refBeginParagraphOrdinal{0};
    int refEndParagraphOrdinal{0};
    int songNumber{0};
    bool isTh{false};
    bool isCbsSource{false};
};

struct JwpubWeekFilterOptions
{
    QDate targetDate;
    QString pubSymbolHint;
    bool includePrinted{false};
    bool excludeFootnotes{true};
    bool excludeWtParagraphVideos{false};
    bool excludeTh{false};
    bool excludeCbsPubs{false};
};

struct JwpubPackage
{
    bool valid{false};
    QString archivePath;
    JwpubManifest manifest;
    JwpubPublicationInfo publication;
    std::vector<JwpubDocument> documents;
    std::vector<JwpubMediaItem> mediaItems;
    std::vector<JwpubExtractRef> extracts;
    QStringList archiveFiles;
    QString errorMessage;
};

/**
 * Native C++ parser and selective extractor for `.jwpub` archives using minizip-ng and Qt SQLite.
 */
class JwpubParser
{
public:
    [[nodiscard]] static JwpubPackage parse(const QString& jwpubPath);

    [[nodiscard]] static JwpubPackage parseForWeek(const QString& jwpubPath,
                                                   const JwpubWeekFilterOptions& options);

    [[nodiscard]] static JwpubPackage filterPackageForWeek(const JwpubPackage& package,
                                                           const JwpubWeekFilterOptions& options);

    [[nodiscard]] static bool extractMediaAssets(const QString& jwpubPath, const QString& destDir,
                                                 const std::vector<QString>& requestedArchivePaths,
                                                 std::vector<QString>* extractedPaths = nullptr,
                                                 QString* error = nullptr);
};

} // namespace kmedia
