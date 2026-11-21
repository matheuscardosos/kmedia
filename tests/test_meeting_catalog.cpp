#include "network/CatalogClient.h"
#include "network/DownloadService.h"
#include "network/MeetingCatalogService.h"

#include <QSignalSpy>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("MeetingCatalogService resolves weekly dates, issue tags, sections, and video quality",
          "[network][catalog]")
{
    using MCS = kmedia::MeetingCatalogService;

    REQUIRE(MCS::weekMondayForDate(QDate(2026, 11, 19)) == QDate(2026, 11, 16));
    REQUIRE(MCS::midweekIssueTagForDate(QDate(2026, 11, 19)) == 202611);
    REQUIRE(MCS::midweekIssueTagForDate(QDate(2026, 12, 7)) == 202611);
    REQUIRE(MCS::midweekIssueTagForDate(QDate(2026, 2, 5)) == 202601);
    REQUIRE(MCS::watchtowerIssueTagForDate(QDate(2026, 11, 19)) == 202609);
    REQUIRE(MCS::watchtowerIssueTagForDate(QDate(2026, 1, 12)) == 202511);
    REQUIRE(MCS::formatWeekRange(QDate(2026, 11, 19)) == QStringLiteral("2026-11-16 - 2026-11-22"));

    REQUIRE(MCS::inferMeetingSection(QStringLiteral("mwb"),
                                     QStringLiteral("Treasures From God's Word"),
                                     {}) == QStringLiteral("tgw"));
    REQUIRE(MCS::inferMeetingSection(QStringLiteral("mwb"), QStringLiteral("Nossa Vida Cristã"),
                                     {}) == QStringLiteral("lac"));
    REQUIRE(MCS::inferMeetingSection(QStringLiteral("w"), QStringLiteral("Lesson 10"), {}) ==
            QStringLiteral("wt"));
    REQUIRE(MCS::inferMeetingSection(QStringLiteral("S-34"), QStringLiteral("Talk 42"), {}) ==
            QStringLiteral("pt"));

    std::vector<kmedia::CatalogMediaAsset> raw;
    kmedia::CatalogMediaAsset vid360;
    vid360.keySymbol = QStringLiteral("mwb");
    vid360.issueTagNumber = 202611;
    vid360.track = 1;
    vid360.fileFormat = QStringLiteral("MP4");
    vid360.label = QStringLiteral("360p");
    vid360.url = QStringLiteral("https://cdn.example/video_r360P.mp4");
    raw.push_back(vid360);

    kmedia::CatalogMediaAsset vid720 = vid360;
    vid720.label = QStringLiteral("720p");
    vid720.url = QStringLiteral("https://cdn.example/video_r720P.mp4");
    raw.push_back(vid720);

    kmedia::CatalogMediaAsset vid240 = vid360;
    vid240.label = QStringLiteral("240p");
    vid240.url = QStringLiteral("https://cdn.example/video_r240P.mp4");
    raw.push_back(vid240);

    kmedia::CatalogMediaAsset jwpub;
    jwpub.keySymbol = QStringLiteral("mwb");
    jwpub.issueTagNumber = 202611;
    jwpub.fileFormat = QStringLiteral("JWPUB");
    jwpub.url = QStringLiteral("https://cdn.example/mwb_E_202611.jwpub");
    raw.push_back(jwpub);

    const std::vector<kmedia::CatalogMediaAsset> dedup = MCS::deduplicateAssets(raw);
    REQUIRE(dedup.size() == 2);
    REQUIRE(dedup.at(0).label == QStringLiteral("720p"));
    REQUIRE(dedup.at(1).fileFormat == QStringLiteral("JWPUB"));
}

TEST_CASE(
    "MeetingCatalogService switches weeks/languages and DownloadService exposes task variants",
    "[network][catalog][downloads]")
{
    kmedia::CatalogClient catalogClient;
    kmedia::DownloadService downloads;
    kmedia::MeetingCatalogService service(&catalogClient, &downloads);

    REQUIRE_FALSE(service.languagesVariant().isEmpty());
    REQUIRE(service.selectedLanguageCode() == QStringLiteral("E"));

    QSignalSpy langSpy(&service, &kmedia::MeetingCatalogService::selectedLanguageChanged);
    service.setSelectedLanguageCode(QStringLiteral("T"));
    REQUIRE(service.selectedLanguageCode() == QStringLiteral("T"));
    REQUIRE(langSpy.count() == 1);

    const QDate initialWeek = service.targetWeekMonday();
    service.nextWeek();
    REQUIRE(service.targetWeekMonday() == initialWeek.addDays(7));
    service.previousWeek();
    REQUIRE(service.targetWeekMonday() == initialWeek);

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

TEST_CASE("CatalogClient & MeetingCatalogService support Mediator, Pinyin, Watchtower probe, and "
          "hub congregations",
          "[network][catalog]")
{
    const std::vector<int> wtTags =
        kmedia::MeetingCatalogService::watchtowerCandidateIssueTags(QDate(2026, 11, 22));
    REQUIRE(wtTags.size() >= 2);
    REQUIRE(std::find(wtTags.begin(), wtTags.end(), 202609) != wtTags.end());

    REQUIRE(kmedia::CatalogClient::pinyinSongFilename(18, 720) ==
            QStringLiteral("sjjm_s-Pi_CHS_018_r720P.mp4"));

    const QUrl mediatorUrl = kmedia::CatalogClient::mediatorMediaUrl(
        QStringLiteral("pub-mwbv_202611_1_VIDEO"), QStringLiteral("T"));
    REQUIRE(mediatorUrl.toString().contains(
        QStringLiteral("v1/media-items/T/pub-mwbv_202611_1_VIDEO")));

    const QByteArray mediatorJson = QByteArrayLiteral(
        R"({"media":[{"title":"Full Convention Session","naturalKey":"pub-co-r26_1_VIDEO","duration":2400.0,"files":[{"progressiveDownloadURL":"https://cdn.example/long.mp4","label":"720p","mimetype":"video/mp4","filesize":500000000}]}]})");
    const auto medAssets = kmedia::CatalogClient::parseMediatorMediaJson(mediatorJson);
    REQUIRE(medAssets.size() == 1);
    REQUIRE(medAssets.at(0).streamOnly);
    REQUIRE(medAssets.at(0).fileFormat == QStringLiteral("MP4"));

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
