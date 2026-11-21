#pragma once

#include "parsers/PublicationDatabase.h"

#include <QDate>
#include <QString>

#include <vector>

namespace kmedia {

/** One synced media item of a meeting day, ready to be written into the day's workspace. */
struct DayMediaItem
{
    QString uniqueKey;
    QString pubMediaId;
    QString title;
    QString sectionId;
    QString filePath;
    QString streamUrl;
    QString thumbnailPath;
    QString mimeType;
    QString tagType;
    QString tagValue;
    QString groupKey;
    QString markerRangesJson;
    int sortOrderOriginal{0};
    qint64 durationMs{0};
    bool hidden{false};
    bool isCbs{false};
};

struct MeetingMediaContext
{
    QDate date;
    bool isMidweek{false};
    bool isWeekend{false};
    bool isCoWeek{false};
    bool excludeCbsVideos{false};
};

struct WeekendMediaFilters
{
    bool excludeWtParagraphVideos{false};
    bool excludeFootnotes{false};
};

/**
 * Turns the media rows of a meeting week into the ordered items of a meeting day: section by
 * paragraph position, Congregation Bible Study detection, paragraph and song tags, extract
 * groups, and the Circuit Overseer week rules.
 */
class MeetingMediaBuilder
{
public:
    MeetingMediaBuilder() = delete;

    // Paragraph ordinal where the Apply Yourself to the Field Ministry part starts.
    static constexpr int kApplyYourselfFirstParagraph = 18;

    [[nodiscard]] static std::vector<DayMediaItem> buildDay(std::vector<PublicationMediaRow> rows,
                                                            const MeetingMediaContext& context);

    [[nodiscard]] static std::vector<PublicationMediaRow>
    orderWeekendMedia(const std::vector<PublicationMediaRow>& mediaWithoutVideos,
                      const std::vector<PublicationMediaRow>& videosInParagraphs,
                      const std::vector<PublicationMediaRow>& videosNotInParagraphs,
                      const WeekendMediaFilters& filters);
    [[nodiscard]] static std::vector<PublicationMediaRow>
    selectWeekendSongs(int issueTagNumber, const std::vector<PublicationMediaRow>& legacySongs,
                       const std::vector<PublicationMediaRow>& videosNotInParagraphs);
    static void placeWeekendSongs(std::vector<PublicationMediaRow>& allMedia,
                                  std::vector<PublicationMediaRow> songs,
                                  const std::vector<PublicationSongExtract>& songExtracts);

    [[nodiscard]] static QString paragraphNumbers(const QString& paragraphLabel,
                                                  const QString& caption);
    [[nodiscard]] static bool isSong(const PublicationMediaRow& row);
    [[nodiscard]] static bool isVideo(const PublicationMediaRow& row);
    [[nodiscard]] static QString pubMediaId(const PublicationMediaRow& row);
    [[nodiscard]] static QString languageFromExtractLink(const QString& link);
};

} // namespace kmedia
