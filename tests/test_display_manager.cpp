#include "integrations/DisplayManager.h"

#include <QSignalSpy>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DisplayManager selects secondary display automatically and honors overrides",
          "[display][integrations]")
{
    REQUIRE(kmedia::DisplayManager::selectTargetScreenIndex({}, -1) == -1);

    const std::vector<kmedia::DisplayDescriptor> singleScreen = {
        {QStringLiteral("HDMI-1"), QRect(0, 0, 1920, 1080), true},
    };
    REQUIRE(kmedia::DisplayManager::selectTargetScreenIndex(singleScreen, -1) == 0);

    const std::vector<kmedia::DisplayDescriptor> dualScreens = {
        {QStringLiteral("eDP-1"), QRect(0, 0, 1366, 768), true},
        {QStringLiteral("HDMI-1"), QRect(1366, 0, 1920, 1080), false},
    };
    REQUIRE(kmedia::DisplayManager::selectTargetScreenIndex(dualScreens, -1) == 1);
    REQUIRE(kmedia::DisplayManager::selectTargetScreenIndex(dualScreens, 0) == 0);
    REQUIRE(kmedia::DisplayManager::selectTargetScreenIndex(dualScreens, 99) == 1);

    kmedia::DisplayManager manager;
    REQUIRE(manager.screenCount() >= 1);
    REQUIRE(manager.targetGeometry().isValid());

    QSignalSpy visibilitySpy(&manager, &kmedia::DisplayManager::visibilityChanged);
    manager.setPresentationWindowVisible(true);
    REQUIRE(manager.isPresentationWindowVisible());
    REQUIRE(visibilitySpy.count() >= 1);

    manager.resetAutoSelection();
    REQUIRE(manager.manualScreenIndex() == -1);
}
