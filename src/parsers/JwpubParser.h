#pragma once

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
    QString featureTitle;
    int classCode{0};
};

struct JwpubMediaItem
{
    qint64 multimediaId{0};
    qint64 documentId{0};
    QString documentTitle;
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
    int beginParagraphOrdinal{0};
    int endParagraphOrdinal{0};
    std::vector<JwpubVideoMarker> videoMarkers;
};

struct JwpubPackage
{
    bool valid{false};
    QString archivePath;
    JwpubManifest manifest;
    JwpubPublicationInfo publication;
    std::vector<JwpubDocument> documents;
    std::vector<JwpubMediaItem> mediaItems;
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

    [[nodiscard]] static bool extractMediaAssets(const QString& jwpubPath, const QString& destDir,
                                                 const std::vector<QString>& requestedArchivePaths,
                                                 std::vector<QString>* extractedPaths = nullptr,
                                                 QString* error = nullptr);
};

} // namespace kmedia
