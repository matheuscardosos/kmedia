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

    REQUIRE(model.importLocalFile(slideA, QStringLiteral("Treasures")));
    REQUIRE(model.importLocalFile(slideB, QStringLiteral("Living")));
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
            QStringLiteral("Treasures From God's Word"));
    REQUIRE(model.data(model.index(3, 0), kmedia::PlaylistModel::SectionTitleRole).toString() ==
            QStringLiteral("Apply Yourself to the Field Ministry"));
    REQUIRE(model.data(model.index(7, 0), kmedia::PlaylistModel::SectionTitleRole).toString() ==
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
