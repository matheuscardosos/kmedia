#include "integrations/ProcessMonitor.h"

#include <QSignalSpy>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ProcessMonitor classifies OBS Studio and Zoom executables without false positives",
          "[integrations][process]")
{
    const auto noneFound = kmedia::ProcessMonitor::classifyProcessNames(
        {QStringLiteral("explorer.exe"), QStringLiteral("obsidian.exe"),
         QStringLiteral("zoom_helper_tool"), QStringLiteral("kmedia.exe")});
    REQUIRE_FALSE(noneFound.first);
    REQUIRE_FALSE(noneFound.second);

    const auto winBoth = kmedia::ProcessMonitor::classifyProcessNames(
        {QStringLiteral("svchost.exe"),
         QStringLiteral("C:/Program Files/obs-studio/bin/64bit/obs64.exe"),
         QStringLiteral("Zoom.exe")});
    REQUIRE(winBoth.first);
    REQUIRE(winBoth.second);

    const auto unixBoth = kmedia::ProcessMonitor::classifyProcessNames(
        {QStringLiteral("/usr/bin/obs"), QStringLiteral("zoom.us")});
    REQUIRE(unixBoth.first);
    REQUIRE(unixBoth.second);

    const auto screenShareProc =
        kmedia::ProcessMonitor::classifyProcessNames({QStringLiteral("CptHost.exe")});
    REQUIRE_FALSE(screenShareProc.first);
    REQUIRE(screenShareProc.second);
}

TEST_CASE("ProcessMonitor queries OS process snapshot natively and emits only on state transitions",
          "[integrations][process]")
{
    kmedia::ProcessMonitor monitor;
    QSignalSpy statusSpy(&monitor, &kmedia::ProcessMonitor::statusChanged);

    monitor.setIntervalMs(2500);
    REQUIRE(monitor.intervalMs() == 2500);

    const QStringList running = kmedia::ProcessMonitor::queryRunningProcessNames();
    REQUIRE_FALSE(running.isEmpty());

    monitor.applySimulatedState(true, false);
    REQUIRE(monitor.isObsRunning());
    REQUIRE_FALSE(monitor.isZoomRunning());
    REQUIRE(statusSpy.count() == 1);

    // Repeating the same state must not emit redundant notifications.
    monitor.applySimulatedState(true, false);
    REQUIRE(statusSpy.count() == 1);

    monitor.applySimulatedState(true, true);
    REQUIRE(statusSpy.count() == 2);
}
