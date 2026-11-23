#include "core/DatabaseManager.h"
#include "media/PlaybackController.h"
#include "models/MeetingDayModel.h"
#include "models/PlaylistModel.h"

#include <QAbstractItemModelTester>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace {

using kmedia::MeetingDayModel;
using kmedia::PlaylistModel;

// One "type:section" token per row, so a whole layout reads as a single comparable list.
QStringList layoutOf(const MeetingDayModel& day)
{
    QStringList tokens;
    for (int row = 0; row < day.rowCount(); ++row) {
        const QModelIndex index = day.index(row);
        const auto type = static_cast<MeetingDayModel::RowType>(
            day.data(index, MeetingDayModel::RowTypeRole).toInt());
        const QString section = day.data(index, MeetingDayModel::RowSectionIdRole).toString();
        switch (type) {
        case MeetingDayModel::SectionHeaderRow:
            tokens << QStringLiteral("header:") + section;
            break;
        case MeetingDayModel::SectionNoticeRow:
            tokens << QStringLiteral("notice:") + section;
            break;
        case MeetingDayModel::MediaRow:
            tokens << QStringLiteral("media:") +
                          day.data(index, PlaylistModel::TitleRole).toString();
            break;
        }
    }
    return tokens;
}

int sourceRowOf(const PlaylistModel& model, const QString& title)
{
    for (int row = 0; row < model.count(); ++row) {
        if (model.data(model.index(row, 0), PlaylistModel::TitleRole).toString() == title) {
            return row;
        }
    }
    return -1;
}

QString writeImage(const QTemporaryDir& dir, const QString& name)
{
    const QString path = dir.filePath(name);
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::darkGreen);
    return image.save(path, "PNG") ? path : QString();
}

} // namespace

TEST_CASE("MeetingDayModel lays out a meeting day like the meeting media page",
          "[models][meeting-day]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("day.db"))));
    PlaylistModel model;
    model.attachDatabase(&db);
    REQUIRE(model.openWorkspaceForDate(QStringLiteral("2026-11-18"), QStringLiteral("midweek")));

    MeetingDayModel day(&model);
    const QAbstractItemModelTester tester(&day,
                                          QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy resets(&day, &QAbstractItemModel::modelReset);

    // Meeting sections show in meeting order even before any media arrives.
    REQUIRE(day.isMeetingDay());
    REQUIRE(day.sectionCount() == 3);
    REQUIRE(layoutOf(day) ==
            QStringList{QStringLiteral("header:tgw"), QStringLiteral("notice:tgw"),
                        QStringLiteral("header:ayfm"), QStringLiteral("notice:ayfm"),
                        QStringLiteral("header:lac"), QStringLiteral("notice:lac")});
    REQUIRE(day.data(day.index(0), MeetingDayModel::RowSectionLabelRole).toString() ==
            QStringLiteral("Treasures From God's Word"));

    REQUIRE(
        model.importLocalFile(writeImage(dir, QStringLiteral("song.png")), QStringLiteral("lac")));
    REQUIRE(model.importLocalFile(writeImage(dir, QStringLiteral("extra.png")),
                                  QStringLiteral("imported-media")));
    REQUIRE(layoutOf(day) ==
            QStringList{QStringLiteral("header:tgw"), QStringLiteral("notice:tgw"),
                        QStringLiteral("header:ayfm"), QStringLiteral("notice:ayfm"),
                        QStringLiteral("header:lac"), QStringLiteral("media:song"),
                        QStringLiteral("header:imported-media"), QStringLiteral("media:extra")});
    REQUIRE(day.mediaCount() == 2);
    const auto songRow = static_cast<int>(layoutOf(day).indexOf(QStringLiteral("media:song")));
    REQUIRE(day.rowForSourceRow(sourceRowOf(model, QStringLiteral("song"))) == songRow);
    REQUIRE(day.data(day.index(songRow), MeetingDayModel::IsLastInSectionRole).toBool());

    // A song tag tells the section header that the section already has its song.
    const int lacHeaderRow = day.headerRowOfSection(QStringLiteral("lac"));
    REQUIRE_FALSE(
        day.data(day.index(lacHeaderRow), MeetingDayModel::RowSectionHasSongRole).toBool());
    REQUIRE(model.updateItemTag(sourceRowOf(model, QStringLiteral("song")), QStringLiteral("song"),
                                QStringLiteral("12")));
    REQUIRE(day.data(day.index(lacHeaderRow), MeetingDayModel::RowSectionHasSongRole).toBool());

    // On a meeting day an empty extra section stays out of the way.
    const QString custom =
        model.createSection(QStringLiteral("Local Needs"), QStringLiteral("#1E88E5"));
    REQUIRE_FALSE(custom.isEmpty());
    REQUIRE(day.headerRowOfSection(custom) == -1);

    // Hidden media leaves a notice row behind until hidden media is shown again.
    REQUIRE(model.setItemHidden(sourceRowOf(model, QStringLiteral("song")), true));
    const int lacHeader = day.headerRowOfSection(QStringLiteral("lac"));
    REQUIRE(layoutOf(day).at(lacHeader + 1) == QStringLiteral("notice:lac"));
    REQUIRE(
        day.data(day.index(lacHeader + 1), MeetingDayModel::RowSectionHiddenCountRole).toInt() ==
        1);
    model.setShowHidden(true);
    REQUIRE(layoutOf(day).at(lacHeader + 1) == QStringLiteral("media:song"));
    model.setShowHidden(false);

    // A collapsed section keeps only its header.
    REQUIRE(model.toggleSectionCollapsed(QStringLiteral("imported-media")));
    REQUIRE(layoutOf(day).last() == QStringLiteral("header:imported-media"));
    REQUIRE(model.toggleSectionCollapsed(QStringLiteral("imported-media")));

    REQUIRE(resets.isEmpty());
}

TEST_CASE("MeetingDayModel keeps its rows when the day reloads", "[models][meeting-day]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("reload.db"))));
    PlaylistModel model;
    model.attachDatabase(&db);
    REQUIRE(model.openWorkspaceForDate(QStringLiteral("2026-11-22"), QStringLiteral("weekend")));
    REQUIRE(
        model.importLocalFile(writeImage(dir, QStringLiteral("talk.png")), QStringLiteral("pt")));
    REQUIRE(
        model.importLocalFile(writeImage(dir, QStringLiteral("study.png")), QStringLiteral("wt")));

    MeetingDayModel day(&model);
    const QAbstractItemModelTester tester(&day,
                                          QAbstractItemModelTester::FailureReportingMode::Fatal);
    const QStringList before = layoutOf(day);
    REQUIRE(before == QStringList{QStringLiteral("header:pt"), QStringLiteral("media:talk"),
                                  QStringLiteral("header:wt"), QStringLiteral("media:study")});

    QSignalSpy resets(&day, &QAbstractItemModel::modelReset);
    QSignalSpy removed(&day, &QAbstractItemModel::rowsRemoved);
    QSignalSpy inserted(&day, &QAbstractItemModel::rowsInserted);
    model.reloadFromDatabase();
    REQUIRE(layoutOf(day) == before);
    REQUIRE(resets.isEmpty());
    REQUIRE(removed.isEmpty());
    REQUIRE(inserted.isEmpty());

    // Moving an item to another section moves its row instead of rebuilding the list.
    REQUIRE(model.assignItemToSection(sourceRowOf(model, QStringLiteral("study")),
                                      QStringLiteral("pt")));
    REQUIRE(layoutOf(day) == QStringList{QStringLiteral("header:pt"), QStringLiteral("media:talk"),
                                         QStringLiteral("media:study"), QStringLiteral("header:wt"),
                                         QStringLiteral("notice:wt")});
    REQUIRE(resets.isEmpty());
}

TEST_CASE("MeetingDayModel shows every section on a day without a meeting", "[models][meeting-day]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("free.db"))));
    PlaylistModel model;
    model.attachDatabase(&db);
    REQUIRE(model.openWorkspaceForDate(QStringLiteral("2026-11-20"), QStringLiteral("custom")));

    MeetingDayModel day(&model);
    REQUIRE_FALSE(day.isMeetingDay());
    REQUIRE(day.rowCount() == 0);

    const QString custom =
        model.createSection(QStringLiteral("Assembly"), QStringLiteral("#945EB5"));
    REQUIRE(layoutOf(day) ==
            QStringList{QStringLiteral("header:") + custom, QStringLiteral("notice:") + custom});
    REQUIRE(day.data(day.index(0), MeetingDayModel::RowSectionIsCustomRole).toBool());
    REQUIRE(day.data(day.index(0), MeetingDayModel::RowSectionLabelRole).toString() ==
            QStringLiteral("Assembly"));
}

TEST_CASE("PlaylistModel selects several items and acts on them together",
          "[models][meeting-day][selection]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("select.db"))));
    PlaylistModel model;
    model.attachDatabase(&db);
    REQUIRE(model.openWorkspaceForDate(QStringLiteral("2026-11-18"), QStringLiteral("midweek")));
    MeetingDayModel day(&model);
    const QAbstractItemModelTester tester(&day,
                                          QAbstractItemModelTester::FailureReportingMode::Fatal);

    for (const QString& name :
         {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")}) {
        REQUIRE(model.importLocalFile(writeImage(dir, name + QStringLiteral(".png")),
                                      QStringLiteral("lac")));
    }
    kmedia::PlaylistEntryView synced;
    synced.title = QStringLiteral("synced");
    synced.filePath = writeImage(dir, QStringLiteral("synced.png"));
    synced.sectionId = QStringLiteral("lac");
    synced.source = QStringLiteral("dynamic");
    REQUIRE(model.appendEntry(synced));
    const auto row = [&model](const char* title) {
        return sourceRowOf(model, QString::fromLatin1(title));
    };

    // Ctrl+click toggles, Shift+click takes the visible range in the order shown.
    QSignalSpy selection(&model, &PlaylistModel::selectionChanged);
    model.toggleRowSelected(row("a"));
    model.toggleRowSelected(row("c"));
    REQUIRE(model.selectionCount() == 2);
    REQUIRE(model.data(model.index(row("c"), 0), PlaylistModel::SelectedRole).toBool());
    REQUIRE(
        day.data(day.index(day.rowForSourceRow(row("c"))), PlaylistModel::SelectedRole).toBool());
    model.toggleRowSelected(row("a"));
    REQUIRE(model.selectionCount() == 1);
    REQUIRE(model.setItemHidden(row("b"), true));
    model.setShowHidden(true);
    model.selectRows(day.sourceRowsBetween(row("a"), row("d")));
    REQUIRE(model.selectedRows() == QList<int>{row("a"), row("c"), row("d")});
    REQUIRE(selection.count() == 4);
    model.setShowHidden(false);

    // Dragging the selection moves it as one block, in its order, next to the target.
    REQUIRE(model.moveSelectedTo(row("synced")));
    QStringList order;
    for (int i = 0; i < model.count(); ++i) {
        order << model.data(model.index(i, 0), PlaylistModel::TitleRole).toString();
    }
    REQUIRE(order == QStringList{QStringLiteral("b"), QStringLiteral("synced"), QStringLiteral("a"),
                                 QStringLiteral("c"), QStringLiteral("d")});
    REQUIRE(model.selectionCount() == 3);
    REQUIRE_FALSE(model.moveSelectedTo(row("c")));

    // Delete removes added media and only hides synced media.
    model.selectRows({row("a"), row("synced")});
    REQUIRE(model.removeSelected() == 2);
    REQUIRE(row("a") == -1);
    REQUIRE(model.data(model.index(row("synced"), 0), PlaylistModel::HiddenRole).toBool());
    REQUIRE(model.selectionCount() == 0);

    // A reload keeps the selection; another day starts empty.
    model.selectRows({row("c"), row("d")});
    model.reloadFromDatabase();
    REQUIRE(model.selectionCount() == 2);
    REQUIRE(model.openWorkspaceForDate(QStringLiteral("2026-11-22"), QStringLiteral("weekend")));
    REQUIRE(model.selectionCount() == 0);
}

TEST_CASE("PlaylistModel lists the clips of a sign language video and plays one",
          "[models][meeting-day][clips]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("clips.db"))));
    PlaylistModel model;
    model.attachDatabase(&db);
    REQUIRE(model.openWorkspaceForDate(QStringLiteral("2026-11-18"), QStringLiteral("midweek")));

    kmedia::PlaylistEntryView video;
    video.title = QStringLiteral("signed");
    video.filePath = writeImage(dir, QStringLiteral("signed.png"));
    video.sectionId = QStringLiteral("tgw");
    video.source = QStringLiteral("dynamic");
    video.markerRangesJson =
        QStringLiteral(R"([{"label":"Matthew 5:3","startMs":12000,"durationMs":33000},)"
                       R"({"label":"Matthew 5:4","startMs":45000,"durationMs":20000},)"
                       R"({"label":"empty","startMs":70000,"durationMs":0}])");
    REQUIRE(model.appendEntry(video));
    const int row = sourceRowOf(model, QStringLiteral("signed"));

    const QVariantList clips = model.data(model.index(row, 0), PlaylistModel::ClipsRole).toList();
    REQUIRE(clips.size() == 2);
    REQUIRE(clips.at(1).toMap().value(QStringLiteral("label")).toString() ==
            QStringLiteral("Matthew 5:4"));
    REQUIRE(clips.at(1).toMap().value(QStringLiteral("endMs")).toLongLong() == 65000);

    // Picking a clip trims the item to it, and the trim survives a reload.
    REQUIRE(model.setItemTrim(row, 45000, 65000));
    model.reloadFromDatabase();
    const int reloaded = sourceRowOf(model, QStringLiteral("signed"));
    REQUIRE(model.data(model.index(reloaded, 0), PlaylistModel::TrimStartMsRole).toLongLong() ==
            45000);
    REQUIRE(model.data(model.index(reloaded, 0), PlaylistModel::TrimEndMsRole).toLongLong() ==
            65000);
    REQUIRE(model.data(model.index(reloaded, 0), PlaylistModel::ClipsRole).toList().size() == 2);

    // The whole video again.
    REQUIRE(model.setItemTrim(reloaded, 0, 0));
    REQUIRE(model.data(model.index(reloaded, 0), PlaylistModel::TrimEndMsRole).toLongLong() == 0);
}

TEST_CASE("PlaylistModel plays each item at its saved speed", "[models][meeting-day][speed]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("speed.db"))));
    PlaylistModel model;
    model.attachDatabase(&db);
    kmedia::PlaybackController playback;
    model.attachPlaybackController(&playback);
    REQUIRE(model.openWorkspaceForDate(QStringLiteral("2026-11-18"), QStringLiteral("midweek")));
    REQUIRE(model.importLocalFile(writeImage(dir, QStringLiteral("a.png")), QStringLiteral("lac")));
    REQUIRE(model.importLocalFile(writeImage(dir, QStringLiteral("b.png")), QStringLiteral("lac")));
    const int a = sourceRowOf(model, QStringLiteral("a"));
    const int b = sourceRowOf(model, QStringLiteral("b"));

    REQUIRE(model.setItemPlaybackRate(a, 1.5));
    REQUIRE(model.activateIndex(a));
    REQUIRE(playback.speed() == 1.5);
    REQUIRE(model.activateIndex(b));
    REQUIRE(playback.speed() == 1.0);

    // Changing the active item's speed applies at once, within the allowed range.
    REQUIRE(model.setItemPlaybackRate(b, 40.0));
    REQUIRE(playback.speed() == 15.0);
    model.reloadFromDatabase();
    REQUIRE(model
                .data(model.index(sourceRowOf(model, QStringLiteral("a")), 0),
                      PlaylistModel::PlaybackRateRole)
                .toDouble() == 1.5);
}
