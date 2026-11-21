#pragma once

#include <QDate>
#include <QHash>
#include <QString>

#include <optional>

namespace kmedia {

/** Where one file of a day folder belongs: its section id and its sort order inside the day. */
struct DatedFilePlacement
{
    QString sectionId;
    double order{0.0};
    bool hasOrder{false};
};

/** Numbers and section label decoded from an exported `"<NN> <Section> - <NN> <Title>"` name. */
struct ExportedFileNameParts
{
    int sectionNumber{0};
    QString sectionLabel;
    int itemNumber{0};

    [[nodiscard]] double order() const;
};

/**
 * Conventions shared by the watched folder and the media auto-export for `YYYY-MM-DD` day
 * folders: folder naming, the per-folder `.section-order.json` placement file, exported file
 * names, and removal of folders whose week is over.
 */
class DatedFolderLayout
{
public:
    DatedFolderLayout() = delete;

    // A dated folder older than this can never match a day of the meeting window.
    static constexpr int kPastDayFolderMaxAgeDays = 7;
    // Full destination paths are kept under this many UTF-8 bytes for cloud sync clients.
    static constexpr int kMaxExportPathBytes = 230;

    [[nodiscard]] static QString sectionOrderFileName();

    [[nodiscard]] static bool isDayFolderName(const QString& name);
    [[nodiscard]] static QDate dateFromDayFolderName(const QString& name);
    [[nodiscard]] static QString dayFolderName(const QDate& date);
    [[nodiscard]] static QDate dateForFile(const QString& filePath);
    [[nodiscard]] static bool isLongPastDayFolder(const QString& name, const QDate& today);
    static int pruneDayFolders(const QString& rootDir, const QDate& today);

    [[nodiscard]] static QHash<QString, DatedFilePlacement>
    readSectionOrder(const QString& dayFolder);
    static bool mergeSectionOrder(const QString& dayFolder,
                                  const QHash<QString, DatedFilePlacement>& placements);
    static bool removeFromSectionOrder(const QString& dayFolder, const QString& fileName);

    [[nodiscard]] static std::optional<ExportedFileNameParts>
    parseExportedFileName(const QString& fileName);
    [[nodiscard]] static QString legacySectionId(const QString& fileName);
    [[nodiscard]] static QString exportedFileName(int sectionNumber, const QString& sectionLabel,
                                                  int itemNumber, int itemCount,
                                                  const QString& songPrefix, const QString& title,
                                                  const QString& extension);
    [[nodiscard]] static QString fitPathToByteLimit(const QString& filePath,
                                                    int maxBytes = kMaxExportPathBytes);
    [[nodiscard]] static QString sanitizeExportName(const QString& raw);
};

} // namespace kmedia
