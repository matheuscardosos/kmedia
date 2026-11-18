#include "core/DatabaseManager.h"
#include "core/ProfileManager.h"
#include "core/SettingsStore.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ProfileManager initializes default profile and switches profiles cleanly",
          "[profiles][core]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    kmedia::ProfileManager manager;
    REQUIRE(manager.initialize(tempDir.path()));
    REQUIRE(manager.profiles().size() == 1);
    REQUIRE(manager.activeDatabase() != nullptr);
    REQUIRE(manager.activeDatabase()->isOpen());
    REQUIRE(manager.activeSettings() != nullptr);

    const QString defaultId = manager.activeProfileId();
    REQUIRE(
        manager.activeSettings()->setValue(QStringLiteral("media/language"), QStringLiteral("E")));

    const QString secondId =
        manager.createProfile(QStringLiteral("Portuguese Group"), QStringLiteral("T"));
    REQUIRE_FALSE(secondId.isEmpty());
    REQUIRE(manager.profiles().size() == 2);

    QSignalSpy switchSpy(&manager, &kmedia::ProfileManager::activeProfileChanged);
    REQUIRE(manager.switchProfile(secondId));
    REQUIRE(switchSpy.count() == 1);
    REQUIRE(manager.activeProfileId() == secondId);
    REQUIRE(manager.lastSwitchDurationMs() < 100);

    // Settings and SQLite database must be completely isolated per profile
    REQUIRE_FALSE(manager.activeSettings()->contains(QStringLiteral("media/language")));
    REQUIRE(
        manager.activeSettings()->setValue(QStringLiteral("media/language"), QStringLiteral("T")));
    REQUIRE(manager.activeDatabase()->databasePath() != QString());

    REQUIRE(manager.switchProfile(defaultId));
    REQUIRE(manager.lastSwitchDurationMs() < 100);
    REQUIRE(manager.activeSettings()->stringValue(QStringLiteral("media/language")) ==
            QStringLiteral("E"));

    REQUIRE(manager.renameProfile(secondId, QStringLiteral("Central Portuguese")));
    const auto renamed = manager.findProfile(secondId);
    REQUIRE(renamed.has_value());
    REQUIRE(renamed->name == QStringLiteral("Central Portuguese"));

    // Deleting active profile switches cleanly; deleting last profile is rejected
    REQUIRE(manager.switchProfile(secondId));
    REQUIRE(manager.deleteProfile(secondId));
    REQUIRE(manager.activeProfileId() == defaultId);
    REQUIRE_FALSE(manager.deleteProfile(defaultId));
}

TEST_CASE("ProfileManager persists active selection and settings across restarts",
          "[profiles][core]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QString createdId;
    {
        kmedia::ProfileManager firstRun;
        REQUIRE(firstRun.initialize(tempDir.path()));
        createdId = firstRun.createProfile(QStringLiteral("West Hall"), QStringLiteral("S"));
        REQUIRE(firstRun.switchProfile(createdId));
        REQUIRE(firstRun.activeSettings()->setValue(QStringLiteral("obs/port"), 4466));
    }

    kmedia::ProfileManager secondRun;
    REQUIRE(secondRun.initialize(tempDir.path()));
    REQUIRE(secondRun.profiles().size() == 2);
    REQUIRE(secondRun.activeProfileId() == createdId);
    REQUIRE(secondRun.activeSettings()->intValue(QStringLiteral("obs/port"), 0) == 4466);
}
