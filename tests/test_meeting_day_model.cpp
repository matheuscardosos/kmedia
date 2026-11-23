#include "core/DatabaseManager.h"
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
