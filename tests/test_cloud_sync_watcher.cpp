#include "TestZipFixtures.h"
#include "parsers/CloudSyncWatcher.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("CloudSyncWatcher classifies cloud providers, media types, and detects standard folders",
          "[cloud][watcher]")
{
    REQUIRE(kmedia::CloudSyncWatcher::classifyProvider(QStringLiteral(
                "/home/user/Dropbox/Meeting")) == kmedia::CloudSyncWatcher::CloudProvider::Dropbox);
    REQUIRE(
        kmedia::CloudSyncWatcher::classifyProvider(QStringLiteral("C:/Users/Op/OneDrive/Media")) ==
        kmedia::CloudSyncWatcher::CloudProvider::OneDrive);
    REQUIRE(kmedia::CloudSyncWatcher::classifyProvider(QStringLiteral("/Users/op/Google Drive")) ==
            kmedia::CloudSyncWatcher::CloudProvider::GoogleDrive);
    REQUIRE(kmedia::CloudSyncWatcher::classifyProvider(QStringLiteral("/home/op/Nextcloud")) ==
            kmedia::CloudSyncWatcher::CloudProvider::Nextcloud);

    REQUIRE(kmedia::CloudSyncWatcher::classifyMediaFile(QStringLiteral("talk.mp4")) ==
            kmedia::CloudSyncWatcher::MediaCategory::Video);
    REQUIRE(kmedia::CloudSyncWatcher::classifyMediaFile(QStringLiteral("song.mp3")) ==
            kmedia::CloudSyncWatcher::MediaCategory::Audio);
    REQUIRE(kmedia::CloudSyncWatcher::classifyMediaFile(QStringLiteral("slide.webp")) ==
            kmedia::CloudSyncWatcher::MediaCategory::Image);
    REQUIRE(kmedia::CloudSyncWatcher::classifyMediaFile(QStringLiteral("outline.pdf")) ==
            kmedia::CloudSyncWatcher::MediaCategory::Pdf);
    REQUIRE(kmedia::CloudSyncWatcher::classifyMediaFile(QStringLiteral("mwb.jwpub")) ==
            kmedia::CloudSyncWatcher::MediaCategory::Jwpub);
    REQUIRE(kmedia::CloudSyncWatcher::classifyMediaFile(QStringLiteral("sunday.jwlplaylist")) ==
            kmedia::CloudSyncWatcher::MediaCategory::JwlPlaylist);
    REQUIRE(kmedia::CloudSyncWatcher::classifyMediaFile(QStringLiteral("notes.txt")) ==
            kmedia::CloudSyncWatcher::MediaCategory::Unknown);

    const QTemporaryDir fakeHome;
    REQUIRE(fakeHome.isValid());
    REQUIRE(QDir(fakeHome.path()).mkpath(QStringLiteral("Dropbox")));
    REQUIRE(QDir(fakeHome.path()).mkpath(QStringLiteral("Nextcloud")));

    const auto detected = kmedia::CloudSyncWatcher::detectLocalCloudFolders(fakeHome.path());
    REQUIRE(detected.size() == 2);
}

TEST_CASE(
    "CloudSyncWatcher indexes recursive folders via OS events with debounce and stability guards",
    "[cloud][watcher]")
{
    const QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString dropboxRoot = tempRoot.filePath(QStringLiteral("Dropbox/Congregation"));
    REQUIRE(QDir().mkpath(dropboxRoot + QStringLiteral("/ExistingSubdir")));

    const QString existingMedia = dropboxRoot + QStringLiteral("/ExistingSubdir/preexisting.jpg");
    {
        QFile preFile(existingMedia);
        REQUIRE(preFile.open(QIODevice::WriteOnly));
        REQUIRE(preFile.write("PRE-EXISTING-IMAGE") > 0);
    }

    kmedia::CloudSyncWatcher watcher;
    watcher.setDebounceMs(50);

    QSignalSpy discoveredSpy(&watcher, &kmedia::CloudSyncWatcher::mediaDiscovered);
    QSignalSpy updatedSpy(&watcher, &kmedia::CloudSyncWatcher::mediaUpdated);
    QSignalSpy removedSpy(&watcher, &kmedia::CloudSyncWatcher::mediaRemoved);
    QSignalSpy issueSpy(&watcher, &kmedia::CloudSyncWatcher::syncIssueDetected);

    REQUIRE(watcher.addWatchedFolder(dropboxRoot));
    REQUIRE(watcher.watchedDirectories().size() >= 2);
    REQUIRE(discoveredSpy.isEmpty());

    // Manual scan emits pre-existing baseline items cleanly.
    REQUIRE(watcher.scanAndEmitExisting(dropboxRoot) == 1);
    REQUIRE(discoveredSpy.count() == 1);

    // Create a new nested folder and add a valid video file.
    const QString sundayDir = dropboxRoot + QStringLiteral("/Sunday");
    REQUIRE(QDir().mkpath(sundayDir));
    const QString videoPath = sundayDir + QStringLiteral("/symposium.mp4");
    {
        QFile vid(videoPath);
        REQUIRE(vid.open(QIODevice::WriteOnly));
        REQUIRE(vid.write("FAKE-MP4-DATA") > 0);
    }

    REQUIRE(discoveredSpy.wait(3000));
    REQUIRE(discoveredSpy.count() == 2);
    REQUIRE(discoveredSpy.at(1).at(0).toString() == QDir::cleanPath(videoPath));
    REQUIRE(discoveredSpy.at(1).at(1).value<kmedia::CloudSyncWatcher::MediaCategory>() ==
            kmedia::CloudSyncWatcher::MediaCategory::Video);
    REQUIRE(discoveredSpy.at(1).at(3).value<kmedia::CloudSyncWatcher::CloudProvider>() ==
            kmedia::CloudSyncWatcher::CloudProvider::Dropbox);

    // Drop a valid jwlplaylist archive -> discovered cleanly after zip validation.
    const QString validPkg = dropboxRoot + QStringLiteral("/list.jwlplaylist");
    REQUIRE(kmedia::test_helpers::writeZipFile(
        validPkg, {{QStringLiteral("manifest.json"), QByteArrayLiteral("{}")}}));
    REQUIRE(discoveredSpy.wait(3000));
    REQUIRE(discoveredSpy.at(2).at(1).value<kmedia::CloudSyncWatcher::MediaCategory>() ==
            kmedia::CloudSyncWatcher::MediaCategory::JwlPlaylist);

    // Deleting the file triggers mediaRemoved cleanly.
    REQUIRE(QFile::remove(videoPath));
    REQUIRE(removedSpy.wait(3000));
    REQUIRE(removedSpy.at(0).at(0).toString() == QDir::cleanPath(videoPath));

    // A corrupt/truncated cloud placeholder triggers syncIssueDetected rather than emitting media.
    const QString corruptJwpub = dropboxRoot + QStringLiteral("/broken.jwpub");
    {
        QFile bad(corruptJwpub);
        REQUIRE(bad.open(QIODevice::WriteOnly));
        REQUIRE(bad.write("NOT-A-VALID-ZIP") > 0);
    }
    REQUIRE(issueSpy.wait(3000));
    REQUIRE(issueSpy.at(0).at(0).toString() == QDir::cleanPath(corruptJwpub));
    REQUIRE(updatedSpy.isEmpty());
}
