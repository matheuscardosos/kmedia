#include "parsers/MeetingMediaBuilder.h"

#include "core/MeetingSections.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cstdlib>
#include <optional>

namespace kmedia {

namespace {

constexpr qint64 kTicksPerMillisecond = 10000;

struct CaptionNumberCandidate
{
    qsizetype start{0};
    qsizetype end{0};
    QString text;
};

QString rightTrimmed(const QString& text)
{
    qsizetype end = text.size();
    while (end > 0 && text.at(end - 1).isSpace()) {
        --end;
    }
    return text.left(end);
}

std::vector<int> numbersIn(const QString& text)
{
    static const QRegularExpression kNumber(QStringLiteral("[0-9]+"));
    std::vector<int> numbers;
    auto it = kNumber.globalMatch(text);
    while (it.hasNext()) {
        numbers.push_back(it.next().captured(0).toInt());
    }
    return numbers;
}

// Numbers in a caption, each extended over a range or list it starts ("11-12", "4, 5", "4 y 5").
std::vector<CaptionNumberCandidate> captionNumberCandidates(const QString& caption)
{
    static const QRegularExpression kNumber(QStringLiteral("[0-9]+"));
    static const QRegularExpression kContinuation(
        QStringLiteral(R"(^(?:\s*[-\x{2013}\x{2014},;]\s*|\s+\p{L}{1,20}\s+)[0-9]+)"),
        QRegularExpression::UseUnicodePropertiesOption);
    std::vector<CaptionNumberCandidate> candidates;
    qsizetype consumedUntil = 0;
    auto it = kNumber.globalMatch(caption);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const qsizetype start = match.capturedStart();
        if (start < consumedUntil) {
            continue;
        }
        const qsizetype numberEnd = match.capturedEnd();
        const QRegularExpressionMatch continuation = kContinuation.match(caption.mid(numberEnd));
        const qsizetype end =
            numberEnd + (continuation.hasMatch() ? continuation.capturedLength() : 0);
        candidates.push_back({start, end, caption.mid(start, end - start).trimmed()});
        consumedUntil = end;
    }
    return candidates;
}

bool isCompatibleWithParagraphLabel(const QString& paragraphLabel,
                                    const CaptionNumberCandidate& candidate)
{
    std::vector<int> labelNumbers = numbersIn(paragraphLabel);
    std::erase_if(labelNumbers, [](int number) { return number <= 0; });
    if (labelNumbers.empty()) {
        return false;
    }
    // Media can sit on the paragraph just before the one it refers to.
    for (const int candidateNumber : numbersIn(candidate.text)) {
        for (const int labelNumber : labelNumbers) {
            if (std::abs(candidateNumber - labelNumber) <= 1) {
                return true;
            }
        }
    }
    return false;
}

struct Parenthetical
{
    QString content;
    qsizetype start{0};
    qsizetype end{0};
};

std::optional<Parenthetical> trailingParenthetical(const QString& caption)
{
    static const QRegularExpression kTrailing(
        QStringLiteral(
            R"(\([^()]*\)\s*[.!?,;:\x{FF0C}\x{3002}\x{FF01}\x{FF1F}\x{FF1B}\x{FF1A}]*$)"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QRegularExpressionMatch match = kTrailing.match(caption);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    const QString matched = match.captured(0);
    const qsizetype close = matched.lastIndexOf(QLatin1Char(')'));
    return Parenthetical{matched.mid(1, close - 1), match.capturedStart(),
                         match.capturedStart() + close};
}

bool isParagraphReference(const QString& paragraphLabel, const QString& caption,
                          const CaptionNumberCandidate& candidate)
{
    static const QRegularExpression kReferenceSymbol(
        QStringLiteral(R"((?:[^\p{L}\p{N}\s]|\p{L}\.)\s*$)"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kTrailingPunctuation(
        QStringLiteral(R"(^[\s.!?,;:\x{FF0C}\x{3002}\x{FF01}\x{FF1F}\x{FF1B}\x{FF1A})\]}\x{00BB})"
                       R"(\x{201D}'\x{2019}\x{2026}]*$)"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kLetter(QStringLiteral(R"(\p{L})"),
                                            QRegularExpression::UseUnicodePropertiesOption);

    for (const int number : numbersIn(candidate.text)) {
        if (number < 1 || number >= 100) {
            return false;
        }
    }
    const auto isColon = [](QChar ch) { return ch == QLatin1Char(':') || ch == QChar(0xFF1A); };
    if ((candidate.start > 0 && isColon(caption.at(candidate.start - 1))) ||
        (candidate.end < caption.size() && isColon(caption.at(candidate.end)))) {
        return false;
    }

    const bool hasReferenceSymbol =
        kReferenceSymbol.match(rightTrimmed(caption.left(candidate.start))).hasMatch();
    const bool compatible = isCompatibleWithParagraphLabel(paragraphLabel, candidate);
    if (const std::optional<Parenthetical> parenthetical = trailingParenthetical(caption)) {
        return candidate.start > parenthetical->start && candidate.end <= parenthetical->end &&
               kLetter.match(parenthetical->content).hasMatch() &&
               (hasReferenceSymbol || compatible);
    }
    return kTrailingPunctuation.match(caption.mid(candidate.end)).hasMatch() &&
           (hasReferenceSymbol || compatible);
}

bool hasExtension(const QString& filePath, std::initializer_list<const char*> extensions)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    for (const char* extension : extensions) {
        if (suffix == QLatin1StringView(extension)) {
            return true;
        }
    }
    return false;
}

QString markerRangesJson(const std::vector<PublicationVideoMarker>& markers)
{
    if (markers.empty()) {
        return {};
    }
    QJsonArray ranges;
    for (const PublicationVideoMarker& marker : markers) {
        QJsonObject range;
        range.insert(QStringLiteral("label"), marker.label);
        range.insert(QStringLiteral("startMs"), marker.startTimeTicks / kTicksPerMillisecond);
        range.insert(QStringLiteral("durationMs"), marker.durationTicks / kTicksPerMillisecond);
        ranges.append(range);
    }
    return QString::fromUtf8(QJsonDocument(ranges).toJson(QJsonDocument::Compact));
}

bool isCbsRow(const PublicationMediaRow& row, const MeetingMediaContext& context, int lastParagraph)
{
    // The Congregation Bible Study media sits in the last two paragraphs before the closing song.
    return context.isMidweek && row.beginParagraphOrdinal >= lastParagraph - 2 &&
           row.beginParagraphOrdinal < lastParagraph;
}

QString sectionFor(const PublicationMediaRow& row, const MeetingMediaContext& context,
                   int middleSongParagraph)
{
    if (context.isMidweek && middleSongParagraph > 0) {
        if (row.beginParagraphOrdinal >= middleSongParagraph) {
            return MeetingSections::kLivingAsChristians;
        }
        if (row.beginParagraphOrdinal >= MeetingMediaBuilder::kApplyYourselfFirstParagraph) {
            return MeetingSections::kApplyYourself;
        }
        return MeetingSections::kTreasures;
    }
    return MeetingSections::kWatchtowerStudy;
}

} // namespace

bool MeetingMediaBuilder::isVideo(const PublicationMediaRow& row)
{
    return row.mimeType.contains(QStringLiteral("video"), Qt::CaseInsensitive) ||
           hasExtension(row.filePath, {"mp4", "m4v", "mov", "mkv", "avi", "webm"});
}

bool MeetingMediaBuilder::isSong(const PublicationMediaRow& row)
{
    return row.track > 0 && row.keySymbol.contains(QStringLiteral("sjj")) && isVideo(row);
}

QString MeetingMediaBuilder::pubMediaId(const PublicationMediaRow& row)
{
    QStringList parts;
    if (!row.keySymbol.isEmpty() || row.issueTagNumber != 0) {
        parts << row.keySymbol;
        if (row.issueTagNumber != 0) {
            parts << QString::number(row.issueTagNumber);
        }
    } else {
        parts << QString::number(row.mepsDocumentId != 0 ? row.mepsDocumentId : row.multimediaId);
    }
    if (row.mepsLanguageIndex >= 0) {
        parts << QString::number(row.mepsLanguageIndex);
    }
    if (row.track > 0) {
        parts << QString::number(row.track);
    }
    parts.removeAll(QString());
    return parts.join(QLatin1Char('_'));
}

QString MeetingMediaBuilder::languageFromExtractLink(const QString& link)
{
    static const QRegularExpression kLanguage(QStringLiteral(R"(/([A-Z]{1,6}):[0-9])"));
    const QRegularExpressionMatch match = kLanguage.match(link);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString MeetingMediaBuilder::paragraphNumbers(const QString& paragraphLabel, const QString& caption)
{
    if (caption.isEmpty()) {
        return paragraphLabel;
    }
    std::vector<CaptionNumberCandidate> candidates = captionNumberCandidates(caption);
    std::reverse(candidates.begin(), candidates.end());
    for (const CaptionNumberCandidate& candidate : candidates) {
        if (isParagraphReference(paragraphLabel, caption, candidate)) {
            return candidate.text;
        }
    }
    return {};
}

std::vector<PublicationMediaRow> MeetingMediaBuilder::orderWeekendMedia(
    const std::vector<PublicationMediaRow>& mediaWithoutVideos,
    const std::vector<PublicationMediaRow>& videosInParagraphs,
    const std::vector<PublicationMediaRow>& videosNotInParagraphs,
    const WeekendMediaFilters& filters)
{
    std::vector<PublicationMediaRow> withPosition;
    std::vector<PublicationMediaRow> withoutPosition;
    for (const auto* group : {&mediaWithoutVideos, &videosInParagraphs, &videosNotInParagraphs}) {
        for (const PublicationMediaRow& row : *group) {
            (row.hasBeginPosition ? withPosition : withoutPosition).push_back(row);
        }
    }
    std::stable_sort(withPosition.begin(), withPosition.end(),
                     [](const PublicationMediaRow& a, const PublicationMediaRow& b) {
                         return a.beginPosition < b.beginPosition;
                     });

    // Media without a position goes right before the last positioned item (the closing song).
    std::vector<PublicationMediaRow> ordered;
    if (withPosition.empty()) {
        ordered = withoutPosition;
    } else {
        ordered.assign(withPosition.begin(), withPosition.end() - 1);
        ordered.insert(ordered.end(), withoutPosition.begin(), withoutPosition.end());
        ordered.push_back(withPosition.back());
    }

    std::vector<PublicationMediaRow> filtered;
    for (PublicationMediaRow row : ordered) {
        if (row.isInNumberedParagraphs == 0 && row.targetParagraphNumberLabel.isEmpty()) {
            row.targetParagraphNumberLabel =
                QString::number(PublicationDatabase::kFootnoteParagraph);
        }
        const bool isFootnote = row.targetParagraphNumberLabel ==
                                QString::number(PublicationDatabase::kFootnoteParagraph);
        if (filters.excludeWtParagraphVideos && row.isInNumberedParagraphs == 1 &&
            row.mimeType.contains(QStringLiteral("video"))) {
            continue;
        }
        if (filters.excludeFootnotes && isFootnote) {
            continue;
        }
        filtered.push_back(std::move(row));
    }
    return PublicationDatabase::dedupeLinkedMultimedia(std::move(filtered));
}

std::vector<PublicationMediaRow> MeetingMediaBuilder::selectWeekendSongs(
    int issueTagNumber, const std::vector<PublicationMediaRow>& legacySongs,
    const std::vector<PublicationMediaRow>& videosNotInParagraphs)
{
    if (issueTagNumber < PublicationDatabase::kSongsMovedToVideosIssue) {
        return legacySongs;
    }
    if (videosNotInParagraphs.empty()) {
        return {};
    }
    std::vector<PublicationMediaRow> sorted = videosNotInParagraphs;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const PublicationMediaRow& a, const PublicationMediaRow& b) {
                         return a.multimediaId < b.multimediaId;
                     });
    if (sorted.size() <= 2) {
        return sorted;
    }
    std::vector<PublicationMediaRow> songs;
    for (const PublicationMediaRow& row : sorted) {
        if (row.track > 0 && row.keySymbol.contains(QStringLiteral("sjj"))) {
            songs.push_back(row);
        }
    }
    if (songs.size() > 2) {
        return {sorted.begin(), sorted.begin() + 2};
    }
    return songs;
}

void MeetingMediaBuilder::placeWeekendSongs(std::vector<PublicationMediaRow>& allMedia,
                                            std::vector<PublicationMediaRow> songs,
                                            const std::vector<PublicationSongExtract>& songExtracts)
{
    for (size_t i = 0; i < songs.size() && i < songExtracts.size(); ++i) {
        songs[i].languageOverride = languageFromExtractLink(songExtracts[i].link);
    }
    std::stable_sort(songs.begin(), songs.end(),
                     [](const PublicationMediaRow& a, const PublicationMediaRow& b) {
                         return a.multimediaId < b.multimediaId;
                     });
    const auto place = [&allMedia](PublicationMediaRow song, int ordinal, bool atEnd) {
        song.beginParagraphOrdinal = ordinal;
        const auto existing =
            std::find_if(allMedia.begin(), allMedia.end(), [&song](const PublicationMediaRow& row) {
                return row.track == song.track && row.keySymbol == song.keySymbol;
            });
        if (existing != allMedia.end()) {
            *existing = std::move(song);
        } else if (atEnd) {
            allMedia.push_back(std::move(song));
        } else {
            allMedia.insert(allMedia.begin(), std::move(song));
        }
    };
    if (!songs.empty()) {
        place(songs[0], 0, false);
    }
    if (songs.size() > 1) {
        place(songs[1], PublicationDatabase::kLastSongOrdinal, true);
    }
}

std::vector<DayMediaItem> MeetingMediaBuilder::buildDay(std::vector<PublicationMediaRow> rows,
                                                        const MeetingMediaContext& context)
{
    std::vector<DayMediaItem> items;
    if (rows.empty()) {
        return items;
    }

    const int lastParagraph = rows.back().beginParagraphOrdinal;
    int middleSongParagraph = 0;
    if (context.isMidweek) {
        int songsSeen = 0;
        for (const PublicationMediaRow& row : rows) {
            if (isSong(row) && ++songsSeen == 2) {
                middleSongParagraph = row.beginParagraphOrdinal;
                break;
            }
        }
    }

    if (context.excludeCbsVideos) {
        std::erase_if(rows, [&](const PublicationMediaRow& row) {
            const bool hasSymbol = !row.extractSymbol.isEmpty() || !row.keySymbol.isEmpty();
            return hasSymbol && isCbsRow(row, context, lastParagraph) && isVideo(row);
        });
    }

    const QString datePart = context.date.toString(QStringLiteral("yyyyMMdd"));
    struct Entry
    {
        int sortOrder{0};
        QString groupKey;
        bool isCbs{false};
        std::vector<DayMediaItem> children;
    };
    std::vector<Entry> entries;
    QHash<QString, size_t> groupIndex;

    for (size_t index = 0; index < rows.size(); ++index) {
        const PublicationMediaRow& row = rows[index];
        const bool song = isSong(row);

        DayMediaItem item;
        item.pubMediaId = pubMediaId(row);
        item.filePath = row.filePath.isEmpty() ? QString() : QDir::cleanPath(row.filePath);
        item.streamUrl = row.streamUrl;
        item.mimeType = row.mimeType;
        item.thumbnailPath = row.linkedPreviewFilePath;
        item.durationMs = static_cast<qint64>(row.durationSeconds * 1000.0);
        item.sectionId = sectionFor(row, context, middleSongParagraph);
        item.isCbs = isCbsRow(row, context, lastParagraph);
        item.markerRangesJson = markerRangesJson(row.videoMarkers);
        item.sortOrderOriginal =
            row.beginParagraphOrdinal != 0 ? row.beginParagraphOrdinal : static_cast<int>(index);
        if (song) {
            static const QRegularExpression kSongNumberPrefix(QStringLiteral(R"(^[0-9]+\.\s*)"));
            item.title = QString(row.label).remove(kSongNumberPrefix);
            item.tagType = QStringLiteral("song");
            item.tagValue = QString::number(row.track);
        } else {
            item.title = row.label.isEmpty() ? row.caption : row.label;
            const QString paragraph = paragraphNumbers(row.targetParagraphNumberLabel, row.caption);
            if (!paragraph.isEmpty()) {
                item.tagType = QStringLiteral("para");
                item.tagValue = paragraph;
            }
        }
        if (item.title.isEmpty()) {
            item.title = QFileInfo(item.filePath).completeBaseName();
        }
        const QString source = !item.filePath.isEmpty()     ? item.filePath
                               : !item.pubMediaId.isEmpty() ? item.pubMediaId
                                                            : item.streamUrl;
        item.uniqueKey = datePart + QLatin1Char('-') + source;

        if (row.extractCaption.isEmpty()) {
            entries.push_back(Entry{item.sortOrderOriginal, QString(), item.isCbs, {item}});
            continue;
        }
        item.groupKey = row.extractCaption;
        const auto group = groupIndex.constFind(row.extractCaption);
        if (group != groupIndex.constEnd()) {
            entries[group.value()].children.push_back(item);
            continue;
        }
        groupIndex.insert(row.extractCaption, entries.size());
        entries.push_back(Entry{item.sortOrderOriginal, row.extractCaption, item.isCbs, {item}});
    }

    std::stable_sort(entries.begin(), entries.end(),
                     [](const Entry& a, const Entry& b) { return a.sortOrder < b.sortOrder; });

    if (context.isCoWeek && !entries.empty()) {
        // The Circuit Overseer gives the closing talk: hide the last song and the study media.
        for (DayMediaItem& child : entries.back().children) {
            child.hidden = true;
        }
        for (Entry& entry : entries) {
            if (entry.isCbs) {
                for (DayMediaItem& child : entry.children) {
                    child.hidden = true;
                }
            }
        }
    }

    for (Entry& entry : entries) {
        for (DayMediaItem& child : entry.children) {
            items.push_back(std::move(child));
        }
    }
    return items;
}

} // namespace kmedia
