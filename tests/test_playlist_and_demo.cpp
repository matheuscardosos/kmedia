#include "TestZipFixtures.h"
#include "core/DatabaseManager.h"
#include "media/PlaybackController.h"
#include "models/PlaylistModel.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTranslator>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("PlaylistModel supports drag-and-drop moves, jwlplaylist import, trims, and demo seeding",
          "[models][playlist][demo]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    kmedia::DatabaseManager db;
    REQUIRE(db.open(tempDir.filePath(QStringLiteral("playlist.db"))));

    kmedia::PlaybackController playback;
    kmedia::PlaylistModel model;
    model.setPackageExtractDir(tempDir.filePath(QStringLiteral("extracted")));
    model.attachDatabase(&db);
    model.attachPlaybackController(&playback);

    const QString slideA = tempDir.filePath(QStringLiteral("slideA.png"));
    const QString slideB = tempDir.filePath(QStringLiteral("slideB.png"));
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(Qt::darkGray);
    REQUIRE(img.save(slideA, "PNG"));
    REQUIRE(img.save(slideB, "PNG"));

    REQUIRE(model.importLocalFile(slideA, QStringLiteral("tgw")));
    REQUIRE(model.importLocalFile(slideB, QStringLiteral("lac")));
    REQUIRE(model.count() == 2);

    REQUIRE(model.activateIndex(0));
    REQUIRE(model.activeIndex() == 0);
    REQUIRE(model.activeTitle() == QStringLiteral("slideA"));

    // Move active index 0 down to index 1 and ensure active index tracks the item cleanly.
    REQUIRE(model.moveItem(0, 1));
    REQUIRE(model.activeIndex() == 1);
    REQUIRE(model.data(model.index(0, 0), kmedia::PlaylistModel::TitleRole).toString() ==
            QStringLiteral("slideB"));

    model.updateActiveTrim(1200, 9500);
    REQUIRE(model.activeTrimStartMs() == 1200);
    REQUIRE(model.activeTrimEndMs() == 9500);
    REQUIRE(playback.trimStartMs() == 1200);
    REQUIRE(playback.trimEndMs() == 9500);

    // Populate deterministic offline demo workspace grouped by meeting sections.
    model.populateDemoWorkspace(tempDir.filePath(QStringLiteral("demo-assets")));
    REQUIRE(model.count() == 8);
    REQUIRE(model.activeIndex() == 0);
    REQUIRE(QFileInfo::exists(QUrl(model.activeThumbnailUrl()).toLocalFile()));
    REQUIRE(model.data(model.index(0, 0), kmedia::PlaylistModel::SectionTitleRole).toString() ==
            QStringLiteral("tgw"));
    REQUIRE(model.data(model.index(3, 0), kmedia::PlaylistModel::SectionTitleRole).toString() ==
            QStringLiteral("ayfm"));
    REQUIRE(model.data(model.index(7, 0), kmedia::PlaylistModel::SectionTitleRole).toString() ==
            QStringLiteral("lac"));
    REQUIRE(model.data(model.index(7, 0), kmedia::PlaylistModel::SectionLabelRole).toString() ==
            QStringLiteral("Living as Christians"));

    // Enriched metadata used by the inspector and playlist table.
    REQUIRE(model.totalDurationMs() == 2127000);
    REQUIRE(model.formattedTotalDuration().isEmpty() == false);
    REQUIRE(model.activeFormatLabel() == QStringLiteral("PNG"));
    REQUIRE(model.activeResolutionLabel() == QStringLiteral("960 x 540 (16:9)"));
    REQUIRE(model.data(model.index(0, 0), kmedia::PlaylistModel::FileExistsRole).toBool());

    // Demo seeds a second weekend playlist that the header switcher can cycle to.
    REQUIRE(model.availablePlaylistNames().size() >= 2);
    const int startPlaylist = model.activePlaylistIndex();
    REQUIRE(model.switchAdjacentPlaylist(1));
    REQUIRE(model.activePlaylistIndex() != startPlaylist);
    REQUIRE(model.switchAdjacentPlaylist(-1));
    REQUIRE(model.activePlaylistIndex() == startPlaylist);
}

TEST_CASE("Embedded Qt Linguist Portuguese catalog translates QML interface strings cleanly",
          "[i18n][translations]")
{
    Q_INIT_RESOURCE(kmedia_translations);
    QTranslator translator;
    REQUIRE(translator.load(QStringLiteral(":/i18n/kmedia_pt.qm")));
    REQUIRE(translator.translate("Main", "Meeting Workspace") ==
            QStringLiteral("Espaço da Reunião"));
    REQUIRE(translator.translate("SettingsView", "Appearance & Interface Language") ==
            QStringLiteral("Aparência e Idioma da Interface"));
    REQUIRE(translator.translate("ActionPillBar", "Background Music Controls") ==
            QStringLiteral("Controles da Música Ambiente"));
    REQUIRE(translator.translate("PillTimerPopover", "Meeting Timer") ==
            QStringLiteral("Cronômetro da Reunião"));
}

TEST_CASE("PlaylistModel supports custom section builder, color customization, and persistence",
          "[models][playlist][sections]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("sections.db"));

    const QString mediaA = tempDir.filePath(QStringLiteral("talkA.png"));
    const QString mediaB = tempDir.filePath(QStringLiteral("talkB.png"));
    QString localNeeds;
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::darkBlue);
    REQUIRE(img.save(mediaA, "PNG"));
    REQUIRE(img.save(mediaB, "PNG"));

    {
        kmedia::DatabaseManager db;
        REQUIRE(db.open(dbPath));

        kmedia::PlaylistModel model;
        model.attachDatabase(&db);
        REQUIRE(model.importLocalFile(mediaA, QStringLiteral("tgw")));
        REQUIRE(model.importLocalFile(mediaB, QStringLiteral("lac")));

        localNeeds = model.createSection(QStringLiteral("Local Needs"), QStringLiteral("#1E88E5"));
        REQUIRE(localNeeds.startsWith(QStringLiteral("custom-")));
        REQUIRE(model.createSection(QStringLiteral("local needs"), QStringLiteral("#000000"))
                    .isEmpty());
        REQUIRE(model.setSectionColor(localNeeds, QStringLiteral("#2E7D32")));
        REQUIRE(model.colorForSection(localNeeds) == QStringLiteral("#2E7D32"));

        REQUIRE(model.assignItemToSection(0, localNeeds));
        REQUIRE(model.data(model.index(1, 0), kmedia::PlaylistModel::SectionTitleRole).toString() ==
                localNeeds);

        // Renaming changes only the label; items keep pointing at the same section id.
        REQUIRE(model.renameSection(localNeeds, QStringLiteral("Special Talk")));
        REQUIRE(model.sectionLabel(localNeeds) == QStringLiteral("Special Talk"));
        REQUIRE(model.data(model.index(1, 0), kmedia::PlaylistModel::SectionLabelRole).toString() ==
                QStringLiteral("Special Talk"));
        REQUIRE(model.colorForSection(localNeeds) == QStringLiteral("#2E7D32"));

        // Meeting sections keep their translated names and cannot be removed.
        REQUIRE_FALSE(model.renameSection(QStringLiteral("tgw"), QStringLiteral("Other")));
        REQUIRE_FALSE(model.removeSection(QStringLiteral("lac")));
        REQUIRE(model.sectionLabel(QStringLiteral("tgw")) ==
                QStringLiteral("Treasures From God's Word"));

        // Move Special Talk ahead of Living as Christians and verify ordering updates items.
        REQUIRE(model.moveSection(localNeeds, -1));
        REQUIRE(model.data(model.index(0, 0), kmedia::PlaylistModel::SectionTitleRole).toString() ==
                localNeeds);
        REQUIRE(model.data(model.index(0, 0), kmedia::PlaylistModel::TitleRole).toString() ==
                QStringLiteral("talkA"));

        // Removing a custom section keeps its media under the added media section.
        const QString temporary =
            model.createSection(QStringLiteral("Temp"), QStringLiteral("#8E24AA"));
        REQUIRE(model.assignItemToSection(1, temporary));
        REQUIRE(model.removeSection(temporary));
        REQUIRE(model.data(model.index(1, 0), kmedia::PlaylistModel::SectionTitleRole).toString() ==
                QStringLiteral("imported-media"));
    }

    // Reopen database to verify custom section order and hex color survive restarts.
    kmedia::DatabaseManager reopened;
    REQUIRE(reopened.open(dbPath));
    kmedia::PlaylistModel reloaded;
    reloaded.attachDatabase(&reopened);
    REQUIRE(reloaded.count() == 2);
    REQUIRE(reloaded.colorForSection(localNeeds) == QStringLiteral("#2E7D32"));
    REQUIRE(reloaded.sectionLabel(localNeeds) == QStringLiteral("Special Talk"));
    REQUIRE(
        reloaded.data(reloaded.index(0, 0), kmedia::PlaylistModel::SectionTitleRole).toString() ==
        localNeeds);
}

TEST_CASE(
    "PlaylistModel supports date-keyed workspaces, dividers, tags, groups, hide, and reset order",
    "[models][playlist][workspace]")
{
    const QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString dbPath = tempDir.filePath(QStringLiteral("workspace.db"));

    {
        kmedia::DatabaseManager db;
        REQUIRE(db.open(dbPath));
        kmedia::PlaylistModel model;
        model.attachDatabase(&db);

        REQUIRE(model.openWorkspaceForDate(QStringLiteral("2026-11-25"), QStringLiteral("midweek"),
                                           QStringLiteral("Midweek Nov 25"), false));
        REQUIRE(model.sections().size() == 3);

        const QString imgA = tempDir.filePath(QStringLiteral("slideA.png"));
        const QString imgB = tempDir.filePath(QStringLiteral("slideB.png"));
        QImage(16, 9, QImage::Format_RGB32).save(imgA);
        QImage(16, 9, QImage::Format_RGB32).save(imgB);

        kmedia::PlaylistEntryView a;
        a.title = QStringLiteral("Opening Song 126");
        a.sectionId = QStringLiteral("tgw");
        a.filePath = imgA;
        a.tagType = QStringLiteral("song");
        a.tagValue = QStringLiteral("Song 126");
        a.groupKey = QStringLiteral("grp_intro");
        REQUIRE(model.appendEntry(a));

        kmedia::PlaylistEntryView b;
        b.title = QStringLiteral("Gems Image");
        b.sectionId = QStringLiteral("tgw");
        b.filePath = imgB;
        b.groupKey = QStringLiteral("grp_intro");
        REQUIRE(model.appendEntry(b));

        REQUIRE(model.addDivider(QStringLiteral("Transition Break"), QStringLiteral("tgw"),
                                 QStringLiteral("#D47B2A"), 0));
        REQUIRE(model.count() == 3);
        REQUIRE(model.data(model.index(1, 0), kmedia::PlaylistModel::ItemKindRole).toString() ==
                QStringLiteral("divider"));

        REQUIRE(model.toggleItemHidden(2));
        REQUIRE(model.hiddenCount() == 1);
        REQUIRE_FALSE(
            model.data(model.index(2, 0), kmedia::PlaylistModel::RowVisibleRole).toBool());
        model.setShowHidden(true);
        REQUIRE(model.data(model.index(2, 0), kmedia::PlaylistModel::RowVisibleRole).toBool());

        REQUIRE(model.toggleGroupExpanded(QStringLiteral("grp_intro")));
        REQUIRE(model.updateItemTag(0, QStringLiteral("para"), QStringLiteral("Para. 2")));
        REQUIRE(model.setItemSlideInterval(0, 30000));
        REQUIRE(model.setSectionRepeatable(QStringLiteral("tgw"), true));

        // Switch to another date and back to confirm isolation per date.
        REQUIRE(model.openWorkspaceForDate(QStringLiteral("2026-11-29"), QStringLiteral("weekend"),
                                           QStringLiteral("Weekend Nov 29"), false));
        REQUIRE(model.count() == 0);
        REQUIRE(model.sections().size() == 2);

        REQUIRE(
            model.openWorkspaceForDate(QStringLiteral("2026-11-25"), QStringLiteral("midweek")));
        REQUIRE(model.count() == 3);
        REQUIRE(model.data(model.index(0, 0), kmedia::PlaylistModel::TagValueRole).toString() ==
                QStringLiteral("Para. 2"));
        REQUIRE(model.data(model.index(0, 0), kmedia::PlaylistModel::SlideIntervalMsRole).toInt() ==
                30000);
    }
}
