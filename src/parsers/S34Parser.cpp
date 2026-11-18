#include "parsers/S34Parser.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>

namespace kmedia {

int S34Parser::extractTalkNumber(const QString& title)
{
    static const QRegularExpression kNumberPattern(
        QStringLiteral(R"(^(?:No\.?\s*|#)?(\d{1,4})(?=\s*(?:[.\-:)]|\s+)))"),
        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = kNumberPattern.match(title.trimmed());
    if (!match.hasMatch()) {
        return 0;
    }

    bool ok = false;
    const int parsed = match.captured(1).toInt(&ok);
    return ok ? parsed : 0;
}

S34Package S34Parser::parse(const QString& s34JwpubPath, bool includeEmptyTalks)
{
    S34Package result;
    result.archivePath = s34JwpubPath;

    const JwpubPackage pkg = JwpubParser::parse(s34JwpubPath);
    if (!pkg.valid) {
        result.errorMessage = pkg.errorMessage;
        return result;
    }

    result.publication = pkg.publication;

    QHash<qint64, std::vector<JwpubMediaItem>> mediaByDoc;
    for (const JwpubMediaItem& item : pkg.mediaItems) {
        mediaByDoc[item.documentId].push_back(item);
    }

    QHash<qint64, bool> seenDocs;
    for (const JwpubDocument& doc : pkg.documents) {
        seenDocs.insert(doc.documentId, true);
        std::vector<JwpubMediaItem> docMedia = mediaByDoc.value(doc.documentId);
        if (!includeEmptyTalks && docMedia.empty()) {
            continue;
        }

        S34PublicTalk talk;
        talk.documentId = doc.documentId;
        talk.mepsDocumentId = doc.mepsDocumentId;
        talk.title = doc.title;
        talk.talkNumber = extractTalkNumber(doc.title);
        talk.mediaItems = std::move(docMedia);
        result.talks.push_back(std::move(talk));
    }

    // Handle minimal archives where multimedia exists without a corresponding Document row.
    for (auto it = mediaByDoc.constBegin(); it != mediaByDoc.constEnd(); ++it) {
        if (seenDocs.contains(it.key())) {
            continue;
        }
        S34PublicTalk talk;
        talk.documentId = it.key();
        talk.mediaItems = it.value();
        if (!talk.mediaItems.empty()) {
            talk.title = talk.mediaItems.front().documentTitle;
            talk.talkNumber = extractTalkNumber(talk.title);
        }
        result.talks.push_back(std::move(talk));
    }

    std::sort(result.talks.begin(), result.talks.end(),
              [](const S34PublicTalk& a, const S34PublicTalk& b) {
                  if (a.talkNumber != b.talkNumber && a.talkNumber > 0 && b.talkNumber > 0) {
                      return a.talkNumber < b.talkNumber;
                  }
                  return a.documentId < b.documentId;
              });

    result.valid = true;
    return result;
}

bool S34Parser::extractTalkMedia(const S34Package& package, qint64 documentId,
                                 const QString& destDir, std::vector<QString>* extractedPaths,
                                 QString* error)
{
    for (const S34PublicTalk& talk : package.talks) {
        if (talk.documentId != documentId) {
            continue;
        }

        std::vector<QString> requested;
        requested.reserve(talk.mediaItems.size());
        for (const JwpubMediaItem& item : talk.mediaItems) {
            if (!item.filePath.isEmpty()) {
                requested.push_back(item.filePath);
            }
        }
        return JwpubParser::extractMediaAssets(package.archivePath, destDir, requested,
                                               extractedPaths, error);
    }

    if (error != nullptr) {
        *error =
            QStringLiteral("Public talk DocumentId %1 not found in S-34 package").arg(documentId);
    }
    return false;
}

} // namespace kmedia
