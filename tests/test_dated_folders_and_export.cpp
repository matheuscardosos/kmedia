#include "core/DatedFolderLayout.h"
#include "media/PdfViewerController.h"
#include "media/PlaylistExporter.h"
#include "parsers/CloudSyncWatcher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QPdfWriter>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace {

void writeFile(const QString& path, const QByteArray& content)
{
    REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write(content) == content.size());
}

QStringList fileNamesIn(const QString& dir)
{
    QStringList names = QDir(dir).entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    names.sort();
    return names;
}

} // namespace

TEST_CASE("DatedFolderLayout recognizes day folders and prunes weeks that are over",
          "[dated][folders]")
{
    using kmedia::DatedFolderLayout;

    REQUIRE(DatedFolderLayout::isDayFolderName(QStringLiteral("2026-11-25")));
    REQUIRE_FALSE(DatedFolderLayout::isDayFolderName(QStringLiteral("2026-13-01")));
    REQUIRE_FALSE(DatedFolderLayout::isDayFolderName(QStringLiteral("20261125")));
    REQUIRE_FALSE(DatedFolderLayout::isDayFolderName(QStringLiteral("Sunday")));
    REQUIRE(DatedFolderLayout::dayFolderName(QDate(2026, 3, 4)) == QStringLiteral("2026-03-04"));
    REQUIRE(DatedFolderLayout::dateForFile(QStringLiteral("/share/2026-11-25/slide.jpg")) ==
            QDate(2026, 11, 25));
    REQUIRE_FALSE(
        DatedFolderLayout::dateForFile(QStringLiteral("/share/misc/slide.jpg")).isValid());

    const QDate today(2026, 11, 25); // Wednesday
    REQUIRE(DatedFolderLayout::isLongPastDayFolder(QStringLiteral("2026-11-17"), today));
    REQUIRE_FALSE(DatedFolderLayout::isLongPastDayFolder(QStringLiteral("2026-11-18"), today));

    QTemporaryDir root;
    REQUIRE(root.isValid());
    const QStringList folders = {
        QStringLiteral("2026-11-15"), // Sunday of last week: removed
        QStringLiteral("2026-11-23"), // Monday of this week: kept
        QStringLiteral("20261116"),   // compact name of last week: removed
        QStringLiteral("2026-12-02"), // future: kept
        QStringLiteral("Archive"),    // not a date: kept
    };
    for (const QString& folder : folders) {
        writeFile(QDir(root.path()).filePath(folder + QStringLiteral("/file.mp4")), "x");
    }

    REQUIRE(DatedFolderLayout::pruneDayFolders(root.path(), today) == 2);
    QStringList remaining = QDir(root.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    remaining.sort();
    REQUIRE(remaining == QStringList{QStringLiteral("2026-11-23"), QStringLiteral("2026-12-02"),
                                     QStringLiteral("Archive")});
}

TEST_CASE("DatedFolderLayout reads, merges, and removes section order placements",
          "[dated][folders]")
{
    using kmedia::DatedFilePlacement;
    using kmedia::DatedFolderLayout;

    QTemporaryDir root;
    REQUIRE(root.isValid());
    const QString dayFolder = QDir(root.path()).filePath(QStringLiteral("2026-11-25"));

    REQUIRE(DatedFolderLayout::readSectionOrder(dayFolder).isEmpty());

    QHash<QString, DatedFilePlacement> first;
    first.insert(QStringLiteral("map.jpg"), DatedFilePlacement{QStringLiteral("tgw"), 1002, true});
    REQUIRE(DatedFolderLayout::mergeSectionOrder(dayFolder, first));

    QHash<QString, DatedFilePlacement> second;
    second.insert(QStringLiteral("talk.mp4"),
                  DatedFilePlacement{QStringLiteral("lac"), 3001.5, true});
    REQUIRE(DatedFolderLayout::mergeSectionOrder(dayFolder, second));

    const auto placements = DatedFolderLayout::readSectionOrder(dayFolder);
    REQUIRE(placements.size() == 2);
    REQUIRE(placements.value(QStringLiteral("map.jpg")).sectionId == QStringLiteral("tgw"));
    REQUIRE(placements.value(QStringLiteral("map.jpg")).order == 1002);
    REQUIRE(placements.value(QStringLiteral("talk.mp4")).order == 3001.5);

    REQUIRE(DatedFolderLayout::removeFromSectionOrder(dayFolder, QStringLiteral("map.jpg")));
    REQUIRE(DatedFolderLayout::readSectionOrder(dayFolder).size() == 1);

    // A placement file caught half synced reads as empty instead of failing.
    const QString orderPath = QDir(dayFolder).filePath(DatedFolderLayout::sectionOrderFileName());
    REQUIRE(QFile::remove(orderPath));
    writeFile(orderPath, "{\"talk.mp4\": {\"order\": 30");
    REQUIRE(DatedFolderLayout::readSectionOrder(dayFolder).isEmpty());
}

TEST_CASE("DatedFolderLayout builds and parses exported file names", "[dated][export]")
{
    using kmedia::DatedFolderLayout;

    const QString name = DatedFolderLayout::exportedFileName(
        2, QStringLiteral("Apply Yourself to the Field Ministry"), 3, 4,
        QStringLiteral("Song 12 - "), QStringLiteral("Start: A Conversation?"),
        QStringLiteral(".mp4"));
    REQUIRE(name == QStringLiteral("02 Apply Yourself to the Field Ministry - 03 Song 12 - Start A "
                                   "Conversation.mp4"));
    REQUIRE(DatedFolderLayout::exportedFileName(1, QStringLiteral("Watchtower Study"), 7, 120, {},
                                                QStringLiteral("Image"), QStringLiteral(".jpg")) ==
            QStringLiteral("01 Watchtower Study - 007 Image.jpg"));

    const auto parts = DatedFolderLayout::parseExportedFileName(name);
    REQUIRE(parts.has_value());
    REQUIRE(parts->sectionNumber == 2);
    REQUIRE(parts->sectionLabel == QStringLiteral("Apply Yourself to the Field Ministry"));
    REQUIRE(parts->itemNumber == 3);
    REQUIRE(parts->order() == 2003);
    REQUIRE_FALSE(DatedFolderLayout::parseExportedFileName(QStringLiteral("holiday.jpg")));

    REQUIRE(DatedFolderLayout::legacySectionId(QStringLiteral("Section-lac - video.mp4")) ==
            QStringLiteral("lac"));
    REQUIRE(DatedFolderLayout::legacySectionId(QStringLiteral("video.mp4")).isEmpty());

    REQUIRE(DatedFolderLayout::sanitizeExportName(QStringLiteral("a<b>c:d\"e/f\\g|h?i*j. ")) ==
            QStringLiteral("abcdefghij"));
    REQUIRE(DatedFolderLayout::sanitizeExportName(QStringLiteral("CON")).isEmpty());

    const QString longPath = QStringLiteral("/share/2026-11-25/") + QString(300, QLatin1Char('x')) +
                             QStringLiteral(".mp4");
    const QString fitted = DatedFolderLayout::fitPathToByteLimit(longPath);
    REQUIRE(fitted.toUtf8().size() <= DatedFolderLayout::kMaxExportPathBytes);
    REQUIRE(fitted.endsWith(QStringLiteral(".mp4")));
    REQUIRE(fitted == DatedFolderLayout::fitPathToByteLimit(longPath));
}

TEST_CASE("PlaylistExporter writes numbered day folders and keeps them in sync", "[export][media]")
{
    using kmedia::DayExportOptions;
    using kmedia::ExportMediaItem;
    using kmedia::ExportSection;
    using kmedia::PlaylistExporter;

    QTemporaryDir sources;
    QTemporaryDir exportRoot;
    REQUIRE(sources.isValid());
    REQUIRE(exportRoot.isValid());

    const QString song = sources.filePath(QStringLiteral("sjjm_E_012.mp4"));
    const QString picture = sources.filePath(QStringLiteral("picture.jpg"));
    const QString video = sources.filePath(QStringLiteral("talk.mp4"));
    const QString hidden = sources.filePath(QStringLiteral("hidden.png"));
    writeFile(song, "song");
    writeFile(picture, "picture");
    writeFile(video, "video");
    writeFile(hidden, "hidden");

    std::vector<ExportSection> sections = {
        {QStringLiteral("Treasures From God's Word"),
         {{song, QStringLiteral("Song 12.mp4"), QStringLiteral("12"), false},
          {picture, QStringLiteral("Picture"), {}, false}}},
        {QStringLiteral("Empty Section"), {{hidden, QStringLiteral("Hidden"), {}, true}}},
        {QStringLiteral("Living as Christians"), {{video, QStringLiteral("Talk"), {}, false}}},
    };

    DayExportOptions options;
    options.date = QDate(2026, 11, 25);
    options.exportRoot = exportRoot.path();

    const auto first = PlaylistExporter::exportDay(sections, options);
    REQUIRE(first.success);
    REQUIRE(first.copiedCount == 3);
    const QString dayFolder = QDir(exportRoot.path()).filePath(QStringLiteral("2026-11-25"));
    REQUIRE(first.dayFolder == dayFolder);
    REQUIRE(fileNamesIn(dayFolder) ==
            QStringList{QStringLiteral("01 Treasures From God's Word - 01 Song 12 - Song 12.mp4"),
                        QStringLiteral("01 Treasures From God's Word - 02 Picture.jpg"),
                        QStringLiteral("02 Living as Christians - 01 Talk.mp4")});

    const auto second = PlaylistExporter::exportDay(sections, options);
    REQUIRE(second.success);
    REQUIRE(second.copiedCount == 0);
    REQUIRE(second.skippedCount == 3);

    sections.at(0).items.pop_back();
    const auto third = PlaylistExporter::exportDay(sections, options);
    REQUIRE(third.removedCount == 1);
    REQUIRE(fileNamesIn(dayFolder).size() == 2);

    DayExportOptions invalid;
    invalid.exportRoot = exportRoot.path();
    REQUIRE_FALSE(PlaylistExporter::exportDay(sections, invalid).success);
}

TEST_CASE("PlaylistExporter converts only images and audio into MP4", "[export][media]")
{
    using kmedia::PlaylistExporter;

    REQUIRE(PlaylistExporter::shouldConvertToMp4(QStringLiteral("slide.JPG")));
    REQUIRE(PlaylistExporter::shouldConvertToMp4(QStringLiteral("song.mp3")));
    REQUIRE_FALSE(PlaylistExporter::shouldConvertToMp4(QStringLiteral("video.mkv")));
    REQUIRE_FALSE(PlaylistExporter::shouldConvertToMp4(QStringLiteral("outline.pdf")));

    const QStringList imageArgs =
        PlaylistExporter::conversionArguments(QStringLiteral("in.png"), QStringLiteral("out.mp4"));
    REQUIRE(imageArgs.contains(QStringLiteral("-loop")));
    REQUIRE(imageArgs.at(imageArgs.indexOf(QStringLiteral("-t")) + 1) == QStringLiteral("5"));
    REQUIRE(imageArgs.last() == QStringLiteral("out.mp4"));

    const QStringList audioArgs =
        PlaylistExporter::conversionArguments(QStringLiteral("in.mp3"), QStringLiteral("out.mp4"));
    REQUIRE(audioArgs.contains(QStringLiteral("-vn")));
    REQUIRE_FALSE(audioArgs.contains(QStringLiteral("-loop")));

    QString error;
    REQUIRE(PlaylistExporter::convertToMp4({}, QStringLiteral("in.png"), {}, &error).isEmpty());
    REQUIRE_FALSE(error.isEmpty());
}

TEST_CASE("PdfViewerController parses page ranges and exports pages as images", "[pdf][export]")
{
    using kmedia::PdfViewerController;

    REQUIRE(PdfViewerController::parsePageRange(QStringLiteral("all"), 3) ==
            std::vector<int>{1, 2, 3});
    REQUIRE(PdfViewerController::parsePageRange(QStringLiteral("3-1, 2, 9, x"), 5) ==
            std::vector<int>{1, 2, 3});
    REQUIRE(PdfViewerController::parsePageRange(QStringLiteral("4,2-3"), 5) ==
            std::vector<int>{4, 2, 3});
    REQUIRE(PdfViewerController::parsePageRange(QStringLiteral("1"), 0).empty());

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString pdfPath = dir.filePath(QStringLiteral("outline.pdf"));
    {
        QPdfWriter writer(pdfPath);
        QPainter painter(&writer);
        painter.drawText(QPoint(100, 100), QStringLiteral("Page 1"));
        REQUIRE(writer.newPage());
        painter.drawText(QPoint(100, 100), QStringLiteral("Page 2"));
        painter.end();
    }

    QString error;
    const QStringList pages = PdfViewerController::exportPagesToPng(
        pdfPath, dir.filePath(QStringLiteral("pages")), QStringLiteral("2"), 800, &error);
    REQUIRE(error.isEmpty());
    REQUIRE(pages.size() == 1);
    REQUIRE(pages.first().endsWith(QStringLiteral("outline_p002.png")));
    REQUIRE(QFileInfo(pages.first()).size() > 0);
}

TEST_CASE("CloudSyncWatcher day folder mode only reports media inside dated folders",
          "[cloud][watcher][dated]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    const QString dayDir = QDir(root.path()).filePath(today);
    const QString otherDir = QDir(root.path()).filePath(QStringLiteral("Other"));
    REQUIRE(QDir().mkpath(dayDir));
    REQUIRE(QDir().mkpath(otherDir));
    writeFile(QDir(dayDir).filePath(QStringLiteral("existing.jpg")), "existing");
    writeFile(QDir(otherDir).filePath(QStringLiteral("ignored.jpg")), "ignored");

    kmedia::CloudSyncWatcher watcher;
    watcher.setDebounceMs(50);
    watcher.setDayFoldersOnly(true);
    QSignalSpy datedSpy(&watcher, &kmedia::CloudSyncWatcher::datedMediaDiscovered);

    REQUIRE(watcher.addWatchedFolder(root.path()));
    REQUIRE(watcher.scanAndEmitExisting(QDir::cleanPath(root.path())) == 1);
    REQUIRE(datedSpy.count() == 1);
    REQUIRE(datedSpy.at(0).at(1).toDate() == QDate::currentDate());

    const QString added = QDir(dayDir).filePath(QStringLiteral("added.mp4"));
    writeFile(added, "video");
    REQUIRE(datedSpy.wait(3000));
    REQUIRE(datedSpy.count() == 2);
    REQUIRE(datedSpy.at(1).at(0).toString() == QDir::cleanPath(added));

    writeFile(QDir(otherDir).filePath(QStringLiteral("late.mp4")), "video");
    REQUIRE_FALSE(datedSpy.wait(400));
}
