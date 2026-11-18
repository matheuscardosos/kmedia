#pragma once

#include "parsers/JwpubParser.h"

#include <QString>

#include <vector>

namespace kmedia {

struct S34PublicTalk
{
    int talkNumber{0};
    qint64 documentId{0};
    qint64 mepsDocumentId{0};
    QString title;
    std::vector<JwpubMediaItem> mediaItems;
};

struct S34Package
{
    bool valid{false};
    QString archivePath;
    JwpubPublicationInfo publication;
    std::vector<S34PublicTalk> talks;
    QString errorMessage;
};

/**
 * Parser for S-34 / S-34mp public talk outline media `.jwpub` packages.
 * Reuses JwpubParser with zero temporary directory leakage or duplicate archive extraction.
 */
class S34Parser
{
public:
    [[nodiscard]] static S34Package parse(const QString& s34JwpubPath,
                                          bool includeEmptyTalks = false);

    [[nodiscard]] static bool extractTalkMedia(const S34Package& package, qint64 documentId,
                                               const QString& destDir,
                                               std::vector<QString>* extractedPaths = nullptr,
                                               QString* error = nullptr);

    [[nodiscard]] static int extractTalkNumber(const QString& title);
};

} // namespace kmedia
