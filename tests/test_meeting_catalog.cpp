#include "network/CatalogClient.h"
#include "network/DownloadService.h"
#include "network/MeetingCatalogService.h"

#include <QSignalSpy>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("MeetingCatalogService resolves weekly dates and issue tags", "[network][catalog]")
{
    using MCS = kmedia::MeetingCatalogService;

    REQUIRE(MCS::weekMondayForDate(QDate(2026, 11, 19)) == QDate(2026, 11, 16));
    REQUIRE(MCS::midweekIssueTagForDate(QDate(2026, 11, 19)) == 202611);
    REQUIRE(MCS::midweekIssueTagForDate(QDate(2026, 12, 7)) == 202611);
    REQUIRE(MCS::midweekIssueTagForDate(QDate(2026, 2, 5)) == 202601);
    REQUIRE(MCS::watchtowerIssueTagForDate(QDate(2026, 11, 19)) == 202609);
    REQUIRE(MCS::watchtowerIssueTagForDate(QDate(2026, 1, 12)) == 202511);
    REQUIRE(MCS::formatWeekRange(QDate(2026, 11, 19)) == QStringLiteral("2026-11-16 - 2026-11-22"));
}

TEST_CASE("MeetingCatalogService switches languages and DownloadService exposes task variants",
          "[network][catalog][downloads]")
{
    kmedia::CatalogClient catalogClient;
    kmedia::DownloadService downloads;
    kmedia::MeetingCatalogService service(&catalogClient);

    REQUIRE_FALSE(service.languagesVariant().isEmpty());
    REQUIRE(service.selectedLanguageCode() == QStringLiteral("E"));

    QSignalSpy langSpy(&service, &kmedia::MeetingCatalogService::selectedLanguageChanged);
    service.setSelectedLanguageCode(QStringLiteral("T"));
    REQUIRE(service.selectedLanguageCode() == QStringLiteral("T"));
    REQUIRE(langSpy.count() == 1);

    const QString invalidId =
        downloads.enqueue(QUrl(), QString(), QString(), kmedia::DownloadService::Priority::Normal,
                          QStringLiteral("bad-1"), QStringLiteral("Bad Item"));
    REQUIRE(invalidId == QStringLiteral("bad-1"));
    REQUIRE(downloads.finishedCount() == 1);
    REQUIRE(downloads.tasksVariant().size() == 1);
    REQUIRE(downloads.tasksVariant().first().toMap().value(QStringLiteral("title")).toString() ==
            QStringLiteral("Bad Item"));

    downloads.clearFinished();
    REQUIRE(downloads.finishedCount() == 0);
    REQUIRE(downloads.tasksVariant().isEmpty());
}

TEST_CASE("CatalogClient supports Mediator, Pinyin, and hub congregations", "[network][catalog]")
{
    REQUIRE(kmedia::CatalogClient::pinyinSongFilename(18, 720) ==
            QStringLiteral("sjjm_s-Pi_CHS_018_r720P.mp4"));

    const QUrl mediatorUrl = kmedia::CatalogClient::mediatorMediaUrl(
        QStringLiteral("pub-mwbv_202611_1_VIDEO"), QStringLiteral("T"));
    REQUIRE(mediatorUrl.toString().contains(
        QStringLiteral("v1/media-items/T/pub-mwbv_202611_1_VIDEO")));

    const QByteArray mediatorJson = QByteArrayLiteral(
        R"({"media":[{"title":"Full Convention Session","naturalKey":"pub-co-r26_1_VIDEO","duration":2400.0,"images":{"sqr":{"sm":"https://cdn.example/sqr_sm.jpg"},"lsr":{"xs":"https://cdn.example/lsr_xs.jpg","md":"https://cdn.example/lsr_md.jpg"}},"files":[{"progressiveDownloadURL":"https://cdn.example/long.mp4","label":"720p","mimetype":"video/mp4","filesize":500000000}]}]})");
    const auto medAssets = kmedia::CatalogClient::parseMediatorMediaJson(mediatorJson);
    REQUIRE(medAssets.size() == 1);
    REQUIRE(medAssets.at(0).streamOnly);
    REQUIRE(medAssets.at(0).fileFormat == QStringLiteral("MP4"));
    // A wide picture wins over a square one, at the smallest size from "sm" up.
    REQUIRE(medAssets.at(0).imageUrl == QStringLiteral("https://cdn.example/lsr_md.jpg"));

    const QByteArray hubJson = QByteArrayLiteral(
        R"({"geoLocationList":[{"properties":{"orgName":"Central English","address":"100 Main St","lang":"E","schedule":{"midweek":{"weekday":3,"time":"19:00"},"weekend":{"weekday":7,"time":"10:00"}}}}]})");
    const auto congs = kmedia::CatalogClient::parseCongregationSearchJson(hubJson);
    REQUIRE(congs.size() == 1);
    REQUIRE(congs.at(0).orgName == QStringLiteral("Central English"));
    REQUIRE(congs.at(0).midweekDay == 3);
    REQUIRE(congs.at(0).midweekTime == QStringLiteral("19:00"));
    REQUIRE(congs.at(0).weekendDay == 7);
    REQUIRE(congs.at(0).weekendTime == QStringLiteral("10:00"));
}

TEST_CASE("CatalogClient reads JW.org video categories", "[network][catalog][videos]")
{
    const QUrl tree = kmedia::CatalogClient::mediatorCategoryUrl(
        QStringLiteral("T"), QStringLiteral("VideoOnDemand"), true);
    REQUIRE(tree.toString().contains(QStringLiteral("v1/categories/T/VideoOnDemand")));
    REQUIRE(tree.toString().contains(QStringLiteral("detailed=1")));
    REQUIRE(tree.toString().contains(QStringLiteral("mediaLimit=0")));
    REQUIRE(kmedia::CatalogClient::mediatorCategoryUrl(QStringLiteral("T"),
                                                       QStringLiteral("LatestVideos"), false)
                .toString()
                .contains(QStringLiteral("detailed=0")));

    const QByteArray json = QByteArrayLiteral(
        R"({"category":{"key":"VODStudio","name":"Studio","subcategories":[{"key":"StudioFeatured"}],)"
        R"("media":[{"naturalKey":"pub-jwbvod26_T_48_VIDEO","title":"Morning Worship",)"
        R"("firstPublished":"2026-11-20T13:42:26.166Z","duration":629.4,)"
        R"("images":{"sqr":{"sm":"https://cdn.example/sqr.jpg"},"lss":{"lg":"https://cdn.example/lss.jpg"}},)"
        R"("files":[{"label":"144p","progressiveDownloadURL":"https://cdn.example/v_r144P.3gp","checksum":"aa"},)"
        R"({"label":"720p","progressiveDownloadURL":"https://cdn.example/v_r720P.mp4","checksum":"bb","mimetype":"video/mp4"}]}]}})");
    const kmedia::JwVideoCategory category = kmedia::CatalogClient::parseMediatorCategoryJson(json);
    REQUIRE(category.key == QStringLiteral("VODStudio"));
    REQUIRE(category.subcategories == QStringList{QStringLiteral("StudioFeatured")});
    REQUIRE(category.videos.size() == 1);
    const kmedia::JwVideo& video = category.videos.front();
    REQUIRE(video.title == QStringLiteral("Morning Worship"));
    REQUIRE(video.imageUrl == QStringLiteral("https://cdn.example/sqr.jpg"));
    REQUIRE(video.files.size() == 2);
    // The low resolution 3GP copy is kept apart from the MP4 files a projector plays.
    REQUIRE(video.files.at(0).fileFormat == QStringLiteral("3GP"));
    REQUIRE(video.files.at(1).fileFormat == QStringLiteral("MP4"));
    REQUIRE(video.files.at(1).imageUrl == video.imageUrl);
}

TEST_CASE("CatalogClient reads the yeartext as plain lines", "[catalog][yeartext]")
{
    const QUrl url = kmedia::CatalogClient::yeartextUrl(QStringLiteral("T"), 2026);
    REQUIRE(url.host() == QStringLiteral("wol.jw.org"));
    REQUIRE(url.query().contains(QStringLiteral("docid=1102026800")));
    REQUIRE(url.query().contains(QStringLiteral("wtlocale=T")));

    const QByteArray json =
        R"({"content": "<p id=\"p1\" class=\"themeScrp\"><span class=\"pageNum\"></span><strong>Happy are those conscious</strong></p> <p id=\"p2\"><strong>of their spiritual need.</strong></p> <p id=\"p3\"><strong>Matthew&nbsp;5:3</strong></p>"})";
    REQUIRE(kmedia::CatalogClient::parseYeartextJson(json) ==
            QStringLiteral("Happy are those conscious\nof their spiritual need.\nMatthew 5:3"));
    REQUIRE(kmedia::CatalogClient::parseYeartextJson("{}").isEmpty());
}
