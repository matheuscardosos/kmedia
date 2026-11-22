#include "core/DatabaseManager.h"
#include "core/DatedFolderLayout.h"
#include "media/MediaAutoExport.h"
#include "models/PlaylistModel.h"
#include "network/WatchedFolderMedia.h"
#include "parsers/CloudSyncWatcher.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace {

QString writePicture(const QString& folder, const QString& name)
{
    (void)QDir().mkpath(folder);
    const QString path = QDir(folder).filePath(name);
    QImage image(16, 9, QImage::Format_ARGB32);
    image.fill(Qt::darkCyan);
    return image.save(path, "PNG") ? path : QString();
}

QString meetingKindFor(const QDate& date)
{
    if (date.dayOfWeek() == 3) {
        return QStringLiteral("midweek");
    }
    return date.dayOfWeek() == 7 ? QStringLiteral("weekend") : QStringLiteral("custom");
}

QHash<QString, QString> watchedSections(kmedia::DatabaseManager& db, const QString& dateIso,
                                        const QString& kind)
{
    QHash<QString, QString> sections;
    const auto playlist = db.findPlaylistByDateAndType(dateIso, kind);
    if (playlist.has_value()) {
        for (const auto& row : db.listPlaylistItemRows(playlist->id)) {
            if (row.entry.source == QStringLiteral("watched")) {
                sections.insert(row.entry.customTitle, row.entry.sectionId);
            }
        }
    }
    return sections;
}

} // namespace

TEST_CASE("WatchedFolderMedia places shared folder media in its day", "[watched][folders]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString shared = dir.filePath(QStringLiteral("shared"));
    const QString sunday = QDir(shared).filePath(QStringLiteral("2026-11-22"));
    REQUIRE_FALSE(writePicture(sunday, QStringLiteral("a.png")).isEmpty());
    REQUIRE_FALSE(writePicture(sunday, QStringLiteral("b.png")).isEmpty());
    REQUIRE_FALSE(
        writePicture(sunday, QStringLiteral("02 Watchtower Study - 01 Picture.png")).isEmpty());
    REQUIRE_FALSE(
        writePicture(QDir(shared).filePath(QStringLiteral("2026-11-25")), QStringLiteral("c.png"))
            .isEmpty());
    REQUIRE(kmedia::DatedFolderLayout::mergeSectionOrder(
        sunday,
        {{QStringLiteral("a.png"), kmedia::DatedFilePlacement{QStringLiteral("wt"), 2001, true}}}));

    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("profile.db"))));
    kmedia::PlaylistModel playlist;
    playlist.attachDatabase(&db);
    kmedia::CloudSyncWatcher watcher;
    kmedia::WatchedFolderMedia media(&watcher, &playlist);
    media.setToday(QDate(2026, 11, 21));
    media.setScheduleProviders(meetingKindFor, [](const QDate&) { return false; });
    media.setEnabled(true);
    media.setFolder(shared);
    media.setDatabase(&db);
    REQUIRE(media.isActive());

    // Placements first, then exported names, then the default section of the day.
    QHash<QString, QString> sections =
        watchedSections(db, QStringLiteral("2026-11-22"), QStringLiteral("weekend"));
    REQUIRE(sections.size() == 3);
    REQUIRE(sections.value(QStringLiteral("a")) == QStringLiteral("wt"));
    REQUIRE(sections.value(QStringLiteral("b")) == QStringLiteral("pt"));
    REQUIRE(sections.value(QStringLiteral("Picture")) == QStringLiteral("wt"));
    REQUIRE(watchedSections(db, QStringLiteral("2026-11-25"), QStringLiteral("midweek"))
                .value(QStringLiteral("c")) == QStringLiteral("lac"));

    // A file removed from the folder leaves its day.
    REQUIRE(QFile::remove(QDir(sunday).filePath(QStringLiteral("b.png"))));
    media.syncDay(QDate(2026, 11, 22));
    sections = watchedSections(db, QStringLiteral("2026-11-22"), QStringLiteral("weekend"));
    REQUIRE(sections.size() == 2);
    REQUIRE_FALSE(sections.contains(QStringLiteral("b")));

    // Moving an item in KMedia writes its placement back for the other computers.
    REQUIRE(playlist.openWorkspaceForDate(QStringLiteral("2026-11-22"), QStringLiteral("weekend")));
    int rowOfA = -1;
    for (int row = 0; row < playlist.count(); ++row) {
        if (playlist.data(playlist.index(row, 0), kmedia::PlaylistModel::TitleRole).toString() ==
            QStringLiteral("a")) {
            rowOfA = row;
        }
    }
    REQUIRE(rowOfA >= 0);
    REQUIRE(playlist.assignItemToSection(rowOfA, QStringLiteral("pt")));
    media.writePlacements();
    REQUIRE(kmedia::DatedFolderLayout::readSectionOrder(sunday)
                .value(QStringLiteral("a.png"))
                .sectionId == QStringLiteral("pt"));

    // Upcoming meeting days get their folder.
    media.prepareDayFolders({QDate(2026, 11, 29)});
    REQUIRE(QDir(QDir(shared).filePath(QStringLiteral("2026-11-29"))).exists());

    // Turning the shared folder off takes its media out of the days.
    media.setEnabled(false);
    REQUIRE(watchedSections(db, QStringLiteral("2026-11-22"), QStringLiteral("weekend")).isEmpty());
    REQUIRE(watchedSections(db, QStringLiteral("2026-11-25"), QStringLiteral("midweek")).isEmpty());
}

TEST_CASE("MediaAutoExport copies each day into a numbered date folder", "[export][folders]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("media"));
    const QString talk = writePicture(source, QStringLiteral("talk.png"));
    const QString study = writePicture(source, QStringLiteral("study.png"));
    const QString hidden = writePicture(source, QStringLiteral("hidden.png"));
    REQUIRE_FALSE(hidden.isEmpty());

    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("profile.db"))));
    kmedia::PlaylistModel playlist;
    playlist.attachDatabase(&db);
    REQUIRE(playlist.openWorkspaceForDate(QStringLiteral("2026-11-22"), QStringLiteral("weekend")));
    REQUIRE(playlist.importLocalFile(study, QStringLiteral("wt")));
    REQUIRE(playlist.importLocalFile(talk, QStringLiteral("pt")));
    REQUIRE(playlist.importLocalFile(hidden, QStringLiteral("wt")));
    REQUIRE(playlist.setItemHidden(2, true));

    const QString exportRoot = dir.filePath(QStringLiteral("export"));
    REQUIRE(QDir().mkpath(QDir(exportRoot).filePath(QStringLiteral("2026-11-08"))));
    kmedia::MediaAutoExport exporter;
    exporter.setToday(QDate(2026, 11, 21));
    exporter.setScheduleProviders(meetingKindFor, [](const QDate&) { return false; });
    exporter.setDatabase(&db);
    exporter.setEnabled(true);
    exporter.setFolder(exportRoot);
    REQUIRE(exporter.isActive());

    // Sections follow the meeting: Public Talk before Watchtower Study.
    const std::vector<kmedia::ExportSection> sections =
        exporter.sectionsForDay(QDate(2026, 11, 22));
    REQUIRE(sections.size() == 2);
    REQUIRE(sections.at(0).label == QStringLiteral("Public Talk"));
    REQUIRE(sections.at(1).items.size() == 2);

    exporter.exportDays({QDate(2026, 11, 22)});
    exporter.waitForIdle();
    const QStringList exported =
        QDir(QDir(exportRoot).filePath(QStringLiteral("2026-11-22"))).entryList(QDir::Files);
    // Hidden media is left out.
    REQUIRE(exported.size() == 2);
    REQUIRE(exported.at(0).contains(QStringLiteral("Public Talk")));
    REQUIRE(exported.at(1).contains(QStringLiteral("Watchtower Study")));
    // Folders of weeks that are over are removed.
    REQUIRE_FALSE(QDir(QDir(exportRoot).filePath(QStringLiteral("2026-11-08"))).exists());
}
