#include "TestZipFixtures.h"
#include "core/DatabaseManager.h"
#include "core/NetworkManager.h"
#include "models/PlaylistModel.h"
#include "network/CatalogClient.h"
#include "network/DocumentMediaImporter.h"
#include "network/DownloadService.h"
#include "network/PublicTalkMedia.h"
#include "network/PublicationMediaPicker.h"
#include "network/PublicationStore.h"

#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include <catch2/catch_test_macros.hpp>

namespace {

QByteArray pngBytes(Qt::GlobalColor color)
{
    QImage image(32, 18, QImage::Format_ARGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    (void)buffer.open(QIODevice::WriteOnly);
    (void)image.save(&buffer, "PNG");
    return bytes;
}

QString writeS34(const QTemporaryDir& dir)
{
    const QByteArray dbBytes = kmedia::test_helpers::buildSqliteBytes(
        dir.path(),
        {QStringLiteral("CREATE TABLE Publication (Year INTEGER, VersionNumber INTEGER, "
                        "UndatedSymbol TEXT, IssueTagNumber INTEGER, MepsLanguageIndex INTEGER)"),
         QStringLiteral("INSERT INTO Publication VALUES (2026, 5, 'S-34mp', 0, 0)"),
         QStringLiteral("CREATE TABLE Document (DocumentId INTEGER PRIMARY KEY, MepsDocumentId "
                        "INTEGER, Class INTEGER, Title TEXT)"),
         QStringLiteral("INSERT INTO Document VALUES (1, 1001, 0, '12. Respect for Authority'), "
                        "(2, 1002, 0, 'No. 3 - Serving With the Organization'), "
                        "(3, 1003, 0, '99. Talk Without Media')"),
         QStringLiteral("CREATE TABLE Multimedia (MultimediaId INTEGER PRIMARY KEY, KeySymbol "
                        "TEXT, Track INTEGER, IssueTagNumber INTEGER, MepsLanguageIndex INTEGER, "
                        "MimeType TEXT, CategoryType INTEGER, Label TEXT, Caption TEXT, FilePath "
                        "TEXT)"),
         QStringLiteral("INSERT INTO Multimedia VALUES "
                        "(10, NULL, 0, 0, 0, 'image/png', 8, 'Opening picture', '', 'a.png'), "
                        "(11, NULL, 0, 0, 0, 'image/png', 8, 'Closing picture', '', 'b.png'), "
                        "(20, NULL, 0, 0, 0, 'image/png', 8, 'Unity picture', '', 'c.png')"),
         QStringLiteral("CREATE TABLE DocumentMultimedia (DocumentMultimediaId INTEGER PRIMARY "
                        "KEY, DocumentId INTEGER, MultimediaId INTEGER, BeginParagraphOrdinal "
                        "INTEGER, EndParagraphOrdinal INTEGER)"),
         QStringLiteral("INSERT INTO DocumentMultimedia VALUES (1, 1, 10, 1, 1), (2, 1, 11, 5, 5), "
                        "(3, 2, 20, 2, 2)"),
         QStringLiteral("CREATE TABLE DocumentParagraph (DocumentParagraphId INTEGER PRIMARY KEY, "
                        "DocumentId INTEGER, ParagraphIndex INTEGER, ParagraphNumberLabel TEXT, "
                        "BeginPosition INTEGER, EndPosition INTEGER)")});
    const QString path = dir.filePath(QStringLiteral("S-34mp_E.jwpub"));
    const bool written = kmedia::test_helpers::writeJwpubFixture(
        path, dir.path(), QByteArrayLiteral("{}"), dbBytes, QStringLiteral("S-34mp_E.db"),
        {{QStringLiteral("a.png"), pngBytes(Qt::darkBlue)},
         {QStringLiteral("b.png"), pngBytes(Qt::darkGreen)},
         {QStringLiteral("c.png"), pngBytes(Qt::darkRed)}});
    return written && !dbBytes.isEmpty() ? path : QString();
}

} // namespace

TEST_CASE("PublicTalkMedia remembers the S-34mp and adds a talk's media to the public talk",
          "[network][publictalks]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString s34 = writeS34(dir);
    REQUIRE_FALSE(s34.isEmpty());

    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("profile.db"))));
    kmedia::PlaylistModel playlist;
    playlist.attachDatabase(&db);
    REQUIRE(playlist.openWorkspaceForDate(QStringLiteral("2026-11-22"), QStringLiteral("weekend")));

    kmedia::PublicationStore store(nullptr, nullptr);
    kmedia::DocumentMediaImporter importer(&store, &playlist);
    const QString folder = dir.filePath(QStringLiteral("Publications/S-34"));
    {
        kmedia::PublicTalkMedia talks(&store, &importer);
        talks.setFolder(folder);
        REQUIRE_FALSE(talks.isAvailable());

        QSignalSpy loaded(&talks, &kmedia::PublicTalkMedia::talksChanged);
        talks.importFile(QUrl::fromLocalFile(s34));
        REQUIRE(loaded.wait(10000));
        REQUIRE(talks.isAvailable());
        REQUIRE(talks.label() == QStringLiteral("S-34mp 2026 v5"));

        // Talks with media only, by talk number.
        const QVariantList list = talks.talks();
        REQUIRE(list.size() == 2);
        REQUIRE(list.at(0).toMap().value(QStringLiteral("number")).toInt() == 3);
        REQUIRE(list.at(1).toMap().value(QStringLiteral("number")).toInt() == 12);

        QSignalSpy added(&talks, &kmedia::PublicTalkMedia::talkAdded);
        talks.addTalk(list.at(1).toMap().value(QStringLiteral("documentId")).toLongLong(), {});
        REQUIRE(added.count() == 1);
        REQUIRE(added.at(0).at(0).toInt() == 2);
    }

    REQUIRE(playlist.count() == 2);
    REQUIRE(playlist.data(playlist.index(0, 0), kmedia::PlaylistModel::TitleRole).toString() ==
            QStringLiteral("Opening picture"));
    REQUIRE(playlist.data(playlist.index(1, 0), kmedia::PlaylistModel::TitleRole).toString() ==
            QStringLiteral("Closing picture"));
    REQUIRE(
        playlist.data(playlist.index(0, 0), kmedia::PlaylistModel::SectionTitleRole).toString() ==
        QStringLiteral("pt"));
    REQUIRE(playlist.data(playlist.index(0, 0), kmedia::PlaylistModel::SourceRole).toString() ==
            QStringLiteral("additional"));
    bool subtitleSet = false;
    for (const QVariant& section : playlist.sections()) {
        const QVariantMap map = section.toMap();
        if (map.value(QStringLiteral("id")).toString() == QStringLiteral("pt")) {
            subtitleSet = map.value(QStringLiteral("documentTitle")).toString() ==
                          QStringLiteral("12. Respect for Authority");
        }
    }
    REQUIRE(subtitleSet);

    // The next start finds the S-34mp already unpacked.
    kmedia::PublicTalkMedia reopened(&store, &importer);
    reopened.setFolder(folder);
    REQUIRE(reopened.isAvailable());
    REQUIRE(reopened.talks().size() == 2);
}

TEST_CASE("PublicationMediaPicker adds one document of a local publication",
          "[network][publicationmedia]")
{
    using kmedia::PublicationMediaPicker;
    REQUIRE(PublicationMediaPicker::issueTagFrom(QStringLiteral("2026-11")) == 202611);
    REQUIRE(PublicationMediaPicker::issueTagFrom(QStringLiteral("20261100")) == 202611);
    REQUIRE(PublicationMediaPicker::issueTagFrom(QStringLiteral("202611")) == 202611);
    REQUIRE(PublicationMediaPicker::issueTagFrom({}) == 0);

    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString publication = writeS34(dir);
    REQUIRE_FALSE(publication.isEmpty());

    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("profile.db"))));
    kmedia::PlaylistModel playlist;
    playlist.attachDatabase(&db);
    REQUIRE(playlist.openWorkspaceForDate(QStringLiteral("2026-11-20"), QStringLiteral("custom")));

    // A .jwpub added by hand asks for the document list instead of adding every picture.
    QSignalSpy requested(&playlist, &kmedia::PlaylistModel::publicationFileRequested);
    REQUIRE(playlist.importUrls({QUrl::fromLocalFile(publication)}, {}) == 0);
    REQUIRE(requested.count() == 1);
    REQUIRE(playlist.count() == 0);

    kmedia::PublicationStore store(nullptr, nullptr);
    store.setCacheRoot(dir.path());
    kmedia::DocumentMediaImporter importer(&store, &playlist);
    PublicationMediaPicker picker(&store, &importer);
    QSignalSpy loaded(&picker, &PublicationMediaPicker::documentsChanged);
    picker.openFile(requested.at(0).at(0).toUrl());
    REQUIRE(loaded.wait(10000));
    REQUIRE(picker.documents().size() == 2);

    QSignalSpy added(&picker, &PublicationMediaPicker::documentAdded);
    picker.addDocument(
        picker.documents().at(0).toMap().value(QStringLiteral("documentId")).toLongLong(), {});
    REQUIRE(added.count() == 1);
    REQUIRE(added.at(0).at(0).toInt() == 2);
    REQUIRE(playlist.count() == 2);
    REQUIRE(
        playlist.data(playlist.index(0, 0), kmedia::PlaylistModel::SectionTitleRole).toString() ==
        QStringLiteral("imported-media"));
}

TEST_CASE("PublicationMediaPicker opens a JW.org publication", "[.network][publicationmedia]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString language = qEnvironmentVariable("KMEDIA_NETWORK_LANG", QStringLiteral("E"));

    kmedia::DatabaseManager db;
    REQUIRE(db.open(dir.filePath(QStringLiteral("profile.db"))));
    kmedia::PlaylistModel playlist;
    playlist.attachDatabase(&db);
    REQUIRE(playlist.openWorkspaceForDate(QStringLiteral("2026-11-20"), QStringLiteral("custom")));

    kmedia::NetworkManager network;
    kmedia::CatalogClient catalog(&network, &db);
    kmedia::DownloadService downloads(&network, &db);
    kmedia::PublicationStore store(&catalog, &downloads);
    store.setCacheRoot(dir.path());
    kmedia::DocumentMediaImporter importer(&store, &playlist);
    importer.setLanguages({language});
    kmedia::PublicationMediaPicker picker(&store, &importer);
    picker.setLanguages({language});

    QSignalSpy loaded(&picker, &kmedia::PublicationMediaPicker::documentsChanged);
    picker.openPublication(QStringLiteral("lff"), {});
    REQUIRE(loaded.wait(5 * 60 * 1000));
    INFO(picker.title().toStdString());
    REQUIRE(picker.documents().size() > 10);

    QSignalSpy added(&picker, &kmedia::PublicationMediaPicker::documentAdded);
    picker.addDocument(
        picker.documents().at(5).toMap().value(QStringLiteral("documentId")).toLongLong(), {});
    if (added.isEmpty()) {
        REQUIRE(added.wait(5 * 60 * 1000));
    }
    WARN(QStringLiteral("%1: %2 media from \"%3\"")
             .arg(picker.title())
             .arg(added.at(0).at(0).toInt())
             .arg(picker.documents().at(5).toMap().value(QStringLiteral("title")).toString())
             .toStdString());
    REQUIRE(added.at(0).at(0).toInt() > 0);
}
