#include "media/CameraController.h"
#include "media/MpvRendererItem.h"
#include "media/PdfViewerController.h"
#include "media/PlaybackController.h"

#include <QColor>
#include <QElapsedTimer>
#include <QFile>
#include <QPainter>
#include <QPdfWriter>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVideoFrame>
#include <QVideoSink>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

void createMultiPagePdf(const QString& path, int pages)
{
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    QPainter painter(&writer);
    for (int i = 0; i < pages; ++i) {
        if (i > 0) {
            REQUIRE(writer.newPage());
        }
        painter.fillRect(QRect(0, 0, writer.width(), writer.height()),
                         (i % 2 == 0) ? QColor(0x14, 0x17, 0x1C) : QColor(0x1B, 0x1F, 0x26));
        painter.setPen(QColor(0xFF, 0x6B, 0x00));
        painter.drawText(QRect(100, 100, 600, 200), Qt::AlignLeft,
                         QStringLiteral("Page %1").arg(i + 1));
    }
    painter.end();
}

} // namespace

TEST_CASE("PlaybackController detects kinds, normalizes trims, and preserves state in standby",
          "[playback][media]")
{
    using Kind = kmedia::PlaybackController::MediaKind;
    REQUIRE(kmedia::PlaybackController::detectMediaKind(
                QUrl::fromLocalFile(QStringLiteral("/tmp/talk.mp4"))) == Kind::Video);
    REQUIRE(kmedia::PlaybackController::detectMediaKind(
                QUrl::fromLocalFile(QStringLiteral("/tmp/song.mp3"))) == Kind::Audio);
    REQUIRE(kmedia::PlaybackController::detectMediaKind(
                QUrl::fromLocalFile(QStringLiteral("/tmp/slide.webp"))) == Kind::Image);
    REQUIRE(kmedia::PlaybackController::detectMediaKind(
                QUrl::fromLocalFile(QStringLiteral("/tmp/notes.pdf"))) == Kind::Pdf);

    const auto validTrim = kmedia::PlaybackController::normalizeTrimRange(2500, 18000);
    REQUIRE(validTrim.first == 2500);
    REQUIRE(validTrim.second == 18000);

    const auto invalidEnd = kmedia::PlaybackController::normalizeTrimRange(9000, 4000);
    REQUIRE(invalidEnd.first == 9000);
    REQUIRE(invalidEnd.second == 0);

    REQUIRE(MpvRendererItem::zoomScaleToMpvLog2(1.0) == Catch::Approx(0.0));
    REQUIRE(MpvRendererItem::zoomScaleToMpvLog2(2.0) == Catch::Approx(1.0));
    REQUIRE(MpvRendererItem::zoomScaleToMpvLog2(4.0) == Catch::Approx(2.0));
    REQUIRE(MpvRendererItem::zoomScaleToMpvLog2(32.0) == Catch::Approx(3.0));

    kmedia::PlaybackController controller;
    REQUIRE_FALSE(controller.isActivePresentation());
    REQUIRE(controller.audioDevice() == QStringLiteral("auto"));

    QSignalSpy audioSpy(&controller, &kmedia::PlaybackController::audioChanged);
    controller.setAudioDevice(QStringLiteral("wasapi/{0.0.0.00000000}.{abc}"));
    REQUIRE(controller.audioDevice() == QStringLiteral("wasapi/{0.0.0.00000000}.{abc}"));
    REQUIRE(audioSpy.count() == 1);

    const QUrl imageUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/illustration.png"));
    controller.loadMedia(imageUrl, QStringLiteral("Illustration"), 1000, 5000);

    REQUIRE(controller.mediaKind() == Kind::Image);
    REQUIRE(controller.source() == imageUrl);
    REQUIRE(controller.trimStartMs() == 1000);
    REQUIRE(controller.trimEndMs() == 5000);
    REQUIRE(controller.isPaused());
    REQUIRE(controller.isActivePresentation());

    const QUrl videoUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/talk.mp4"));
    controller.loadMedia(videoUrl, QStringLiteral("Talk Video"), 0, 0);
    REQUIRE(controller.mediaKind() == Kind::Video);
    REQUIRE(controller.isPaused());
    REQUIRE_FALSE(controller.isActivePresentation());
    controller.play();
    REQUIRE_FALSE(controller.isPaused());
    REQUIRE(controller.isActivePresentation());
    controller.stop();
    REQUIRE_FALSE(controller.isActivePresentation());

    controller.loadMedia(imageUrl, QStringLiteral("Illustration"), 1000, 5000);

    controller.setViewport(12.0, 2.0, -2.0);
    REQUIRE(controller.zoomScale() == Catch::Approx(8.0));
    REQUIRE(controller.panX() == Catch::Approx(0.5));
    REQUIRE(controller.panY() == Catch::Approx(-0.5));

    controller.toggleStandby();
    REQUIRE(controller.isStandbyActive());
    REQUIRE_FALSE(controller.isBlackoutActive());
    REQUIRE(controller.source() == imageUrl);

    // Blackout is a standby without the picture, and leaving standby ends it.
    controller.blackout();
    REQUIRE(controller.isStandbyActive());
    REQUIRE(controller.isBlackoutActive());
    controller.toggleStandby();
    REQUIRE_FALSE(controller.isStandbyActive());
    REQUIRE_FALSE(controller.isBlackoutActive());
    controller.toggleStandby();
    REQUIRE_FALSE(controller.isBlackoutActive());

    QSignalSpy subSpy(&controller, &kmedia::PlaybackController::subtitlesChanged);
    REQUIRE(controller.areSubtitlesVisible());
    controller.toggleSubtitles();
    REQUIRE_FALSE(controller.areSubtitlesVisible());
    REQUIRE(subSpy.count() == 1);
    controller.setSubtitlesVisible(true);
    REQUIRE(controller.areSubtitlesVisible());
    REQUIRE(subSpy.count() == 2);

    controller.resetViewport();
    REQUIRE(controller.zoomScale() == Catch::Approx(1.0));
    REQUIRE(controller.panX() == Catch::Approx(0.0));
    REQUIRE(controller.panY() == Catch::Approx(0.0));
}

TEST_CASE("The preview drops the last video frame when the media changes", "[media]")
{
    kmedia::PlaybackController controller;
    MpvRendererItem player;
    controller.attachMpvItem(&player);
    QImage frame(16, 9, QImage::Format_RGB32);
    frame.fill(Qt::red);

    controller.loadMediaWithKind(QUrl::fromLocalFile(QStringLiteral("/tmp/first.mp4")),
                                 kmedia::PlaybackController::MediaKind::Video);
    emit player.videoFrameReady(frame);
    REQUIRE(controller.previewSerial() == 1);
    REQUIRE_FALSE(controller.previewFrame().isNull());

    // A new video starts with no frame; the player drops grabs taken for the old one.
    controller.loadMediaWithKind(QUrl::fromLocalFile(QStringLiteral("/tmp/second.mp4")),
                                 kmedia::PlaybackController::MediaKind::Video);
    REQUIRE(controller.previewSerial() == 0);
    REQUIRE(controller.previewFrame().isNull());

    emit player.videoFrameReady(frame);
    REQUIRE(controller.previewSerial() == 1);

    controller.stop();
    REQUIRE(controller.previewSerial() == 0);
    REQUIRE(controller.previewFrame().isNull());
}

TEST_CASE("The presentation camera is offered to sign languages and reports a missing camera",
          "[media]")
{
    kmedia::CameraController camera;
    QSignalSpy offeredSpy(&camera, &kmedia::CameraController::offeredChanged);
    REQUIRE_FALSE(camera.isOffered());

    camera.setSignLanguage(true);
    REQUIRE(camera.isOffered());
    REQUIRE(offeredSpy.count() == 1);

    // A chosen camera stays offered after the language changes.
    camera.setCameraId(QStringLiteral("kmedia-missing-camera"));
    camera.setSignLanguage(false);
    REQUIRE(camera.isOffered());
    REQUIRE(offeredSpy.count() == 1);

    // Nothing starts until the presentation screen hands over its video output.
    REQUIRE(camera.problem() == kmedia::CameraController::Problem::None);
    REQUIRE_FALSE(camera.isActive());

#ifndef Q_OS_MACOS
    QObject videoOutput;
    camera.bindVideoOutput(&videoOutput);
    REQUIRE(camera.problem() == kmedia::CameraController::Problem::NotConnected);
    REQUIRE_FALSE(camera.isActive());
    camera.bindVideoOutput(nullptr);
#endif

    camera.setCameraId({});
    REQUIRE(camera.problem() == kmedia::CameraController::Problem::None);
    REQUIRE_FALSE(camera.isOffered());
    REQUIRE(offeredSpy.count() == 2);
}

// Needs a connected camera, so it is hidden: run it by its tag.
TEST_CASE("The presentation camera shows the frames of a connected camera", "[.camera][media]")
{
    kmedia::CameraController camera;
    camera.refreshCameras();
    if (camera.cameras().isEmpty()) {
        SKIP("No camera is connected");
    }
    QVideoSink sink;
    QSignalSpy frameSpy(&sink, &QVideoSink::videoFrameChanged);
    camera.setCameraId(camera.cameras().first().toMap().value(QStringLiteral("id")).toString());
    camera.bindVideoOutput(&sink);

    REQUIRE(frameSpy.wait(10000));
    REQUIRE(camera.isActive());
    REQUIRE(camera.problem() == kmedia::CameraController::Problem::None);
    REQUIRE(sink.videoFrame().isValid());

    camera.setCameraId({});
    REQUIRE_FALSE(camera.isActive());
}

TEST_CASE("PdfViewerController navigates multi-page PDFs and provides crisp page images",
          "[pdf][media]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString pdfPath = tempDir.filePath(QStringLiteral("program.pdf"));
    createMultiPagePdf(pdfPath, 3);

    kmedia::PdfViewerController pdf;
    kmedia::PlaybackController playback;
    playback.attachPdfController(&pdf);

    QSignalSpy docSpy(&pdf, &kmedia::PdfViewerController::documentChanged);
    QSignalSpy pageSpy(&pdf, &kmedia::PdfViewerController::currentPageChanged);

    playback.loadMedia(QUrl::fromLocalFile(pdfPath), QStringLiteral("Program PDF"));
    REQUIRE(playback.mediaKind() == kmedia::PlaybackController::MediaKind::Pdf);
    REQUIRE(pdf.isLoaded());
    REQUIRE(pdf.pageCount() == 3);
    REQUIRE(pdf.currentPage() == 0);
    REQUIRE(docSpy.count() >= 1);

    pdf.nextPage();
    REQUIRE(pdf.currentPage() == 1);
    pdf.goToPage(99);
    REQUIRE(pdf.currentPage() == 2);
    pdf.previousPage();
    REQUIRE(pdf.currentPage() == 1);
    REQUIRE(pageSpy.count() >= 3);

    playback.setViewport(2.5, 0.25, -0.1);
    REQUIRE(pdf.zoomScale() == Catch::Approx(2.5));
    REQUIRE(pdf.panX() == Catch::Approx(0.25));
    REQUIRE(pdf.panY() == Catch::Approx(-0.1));

    kmedia::PdfPageImageProvider provider(&pdf);
    QSize renderedSize;
    const QImage pageImg = provider.requestImage(QStringLiteral("%1/1").arg(pdf.revision()),
                                                 &renderedSize, QSize(800, 0));
    REQUIRE_FALSE(pageImg.isNull());
    REQUIRE(renderedSize.width() == 800);
    REQUIRE(pageImg.width() == 800);

    playback.stop();
    REQUIRE_FALSE(pdf.isLoaded());
    REQUIRE(pdf.pageCount() == 0);
}

TEST_CASE("The presentation player hands out its current video frame for the preview",
          "[.gui][media]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    // Two seconds of a red 64x36 raw video.
    const QString clip = dir.filePath(QStringLiteral("clip.y4m"));
    QFile video(clip);
    REQUIRE(video.open(QIODevice::WriteOnly));
    video.write(QByteArrayLiteral("YUV4MPEG2 W64 H36 F10:1 Ip A1:1 C420jpeg\n"));
    for (int frame = 0; frame < 20; ++frame) {
        video.write(QByteArrayLiteral("FRAME\n"));
        video.write(QByteArray(64 * 36, static_cast<char>(81)));
        video.write(QByteArray(32 * 18, static_cast<char>(90)));
        video.write(QByteArray(32 * 18, static_cast<char>(240)));
    }
    video.close();

    // mpv renders through OpenGL, like the app sets up in main().
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow window;
    window.resize(320, 180);
    auto* player = new MpvRendererItem(window.contentItem());
    player->setSize(QSizeF(320, 180));
    QSignalSpy drawn(&window, &QQuickWindow::frameSwapped);
    window.show();
    // Like the presentation window, the player renders before anything is loaded.
    REQUIRE((!drawn.isEmpty() || drawn.wait(5000)));
    QSignalSpy frames(player, &MpvRendererItem::videoFrameReady);
    player->loadFile(QUrl::fromLocalFile(clip));
    QElapsedTimer clock;
    clock.start();
    while (frames.isEmpty() && clock.elapsed() < 10000) {
        player->requestVideoFrame(48);
        QTest::qWait(100);
    }
    REQUIRE_FALSE(frames.isEmpty());
    const QImage frame = frames.first().first().value<QImage>();
    REQUIRE_FALSE(frame.isNull());
    REQUIRE(frame.width() == 48);
    REQUIRE(QColor(frame.pixel(24, 13)).red() > 150);
}

TEST_CASE("PlaybackController defers video load until render context and hands off to preview",
          "[media][playback]")
{
    kmedia::PlaybackController playback;
    MpvRendererItem presentationMpv;
    MpvRendererItem previewMpv;

    playback.attachMpvItem(&presentationMpv);
    playback.attachPreviewMpvItem(&previewMpv);

    // Without a presentation window shown, loading a video keeps PlaybackController cleanly idle on
    // presentationMpv (waiting for an OpenGL render context) rather than leaving vid=-1 black.
    playback.setPresentationVisible(false);
    const QUrl fakeVideo = QUrl::fromLocalFile(QStringLiteral("sample_video.mp4"));
    playback.loadMedia(fakeVideo, QStringLiteral("Sample Video"));
    REQUIRE(playback.mediaTitle() == QStringLiteral("Sample Video"));
    REQUIRE(playback.mediaKind() == kmedia::PlaybackController::MediaKind::Video);

    // Showing the presentation window hands target control to presentationMpv cleanly and
    // detaching previewMpv clears without crashing.
    playback.setPresentationVisible(true);
    playback.detachPreviewMpvItem(&previewMpv);
    playback.stop();
    REQUIRE(playback.mediaKind() == kmedia::PlaybackController::MediaKind::None);
}
