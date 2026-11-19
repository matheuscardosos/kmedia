#include "network/CatalogClient.h"
#include "network/DownloadService.h"
#include "network/MeetingCatalogService.h"

#include <QSignalSpy>

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
                                     {}) == QStringLiteral("Treasures From God's Word"));
    REQUIRE(MCS::inferMeetingSection(QStringLiteral("mwb"), QStringLiteral("Nossa Vida Cristã"),
                                     {}) == QStringLiteral("Living as Christians"));
    REQUIRE(MCS::inferMeetingSection(QStringLiteral("w"), QStringLiteral("Lesson 10"), {}) ==
            QStringLiteral("Watchtower Study"));
    REQUIRE(MCS::inferMeetingSection(QStringLiteral("S-34"), QStringLiteral("Talk 42"), {}) ==
            QStringLiteral("Public Talk"));

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
