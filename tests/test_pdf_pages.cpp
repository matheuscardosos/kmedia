#include "core/DatabaseManager.h"
#include "media/PdfPageImporter.h"
#include "models/PlaylistModel.h"

#include <QPainter>
#include <QPdfWriter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

namespace {

QString writePdf(const QTemporaryDir& dir, const QString& name, int pages)
{
    const QString path = dir.filePath(name);
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    QPainter painter(&writer);
    for (int page = 0; page < pages; ++page) {
        if (page > 0) {
            writer.newPage();
        }
        painter.fillRect(QRect(0, 0, writer.width(), writer.height()), QColor(20 * page, 80, 120));
    }
    painter.end();
    return path;
}

} // namespace

TEST_CASE("A PDF with several pages asks which pages to add as pictures", "[pdf][media]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("pdf.db"))));
    kmedia::PlaylistModel model;
    model.attachDatabase(&db);
    REQUIRE(model.openWorkspaceForDate(QStringLiteral("2026-11-25"), QStringLiteral("custom")));

    const QString threePages = writePdf(dir, QStringLiteral("outline.pdf"), 3);
    REQUIRE(kmedia::PdfPageImporter::countPages(threePages) == 3);

    // Several pages: nothing is added until the pages are picked.
    QSignalSpy requested(&model, &kmedia::PlaylistModel::pdfPagesRequested);
    REQUIRE_FALSE(model.importLocalFile(threePages));
    REQUIRE(requested.count() == 1);
    REQUIRE(model.count() == 0);

    // One page goes straight in, and the whole document can still be added as one item.
    REQUIRE(model.importLocalFile(writePdf(dir, QStringLiteral("single.pdf"), 1)));
    REQUIRE(model.importPdfDocument(threePages));
    REQUIRE(model.count() == 2);

    kmedia::PdfPageImporter importer(&model);
    importer.setCacheRoot(dir.path());
    importer.open(QUrl::fromLocalFile(threePages));
    REQUIRE(importer.pageCount() == 3);
    REQUIRE(importer.fileName() == QStringLiteral("outline.pdf"));
    REQUIRE(QTest::qWaitFor(
        [&importer]() {
            const QVariantList pages = importer.pages();
            return std::all_of(pages.begin(), pages.end(), [](const QVariant& page) {
                return !page.toMap().value(QStringLiteral("thumbUrl")).toString().isEmpty();
            });
        },
        10000));

    QSignalSpy added(&importer, &kmedia::PdfPageImporter::pagesAdded);
    importer.addPages({1, 3}, QString());
    REQUIRE(added.wait(10000));
    REQUIRE(added.first().first().toInt() == 2);
    REQUIRE(model.count() == 4);
    REQUIRE(model.data(model.index(3, 0), kmedia::PlaylistModel::TitleRole)
                .toString()
                .contains(QStringLiteral("p003")));
}
