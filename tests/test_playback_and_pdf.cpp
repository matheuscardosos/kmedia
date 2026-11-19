#include "media/MpvRendererItem.h"
#include "media/PdfViewerController.h"
#include "media/PlaybackController.h"

#include <QColor>
#include <QPainter>
#include <QPdfWriter>
#include <QSignalSpy>
#include <QTemporaryDir>

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
    const QUrl imageUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/illustration.png"));
    controller.loadMedia(imageUrl, QStringLiteral("Illustration"), 1000, 5000);

    REQUIRE(controller.mediaKind() == Kind::Image);
    REQUIRE(controller.source() == imageUrl);
    REQUIRE(controller.trimStartMs() == 1000);
    REQUIRE(controller.trimEndMs() == 5000);

    controller.setViewport(12.0, 2.0, -2.0);
    REQUIRE(controller.zoomScale() == Catch::Approx(8.0));
    REQUIRE(controller.panX() == Catch::Approx(0.5));
    REQUIRE(controller.panY() == Catch::Approx(-0.5));

    controller.toggleStandby();
    REQUIRE(controller.isStandbyActive());
    REQUIRE(controller.source() == imageUrl);

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
