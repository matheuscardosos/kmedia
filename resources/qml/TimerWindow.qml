import QtQuick
import QtQuick.Shapes
import QtQuick.Window

import KMedia.UI

// The meeting timer for the chairman or the speaker, inspired by Meeting Media Manager: on a screen
// of its own, away from the main and the presentation screens, or in a normal window without one.
Window {
    id: timerWindow

    // The screen of the main window, kept clear of the timer.
    property string mainScreenName: ""

    readonly property var timer: ApplicationController.meetingTimer
    // A screen that is neither the main nor the presentation one.
    readonly property var freeScreen: {
        const screens = ApplicationController.displayManager.screens
        for (let i = 0; i < screens.length; ++i) {
            const name = screens[i].name
            if (name !== timerWindow.mainScreenName
                    && !(ApplicationController.displayManager.presentationWindowVisible
                         && name === ApplicationController.displayManager.targetScreenName))
                return screens[i]
        }
        return null
    }
    readonly property bool dedicated: timerWindow.freeScreen !== null
    readonly property var currentPart: timerWindow.timer.running && timerWindow.timer.currentIndex >= 0
                                       ? timerWindow.timer.parts[timerWindow.timer.currentIndex] : null
    readonly property real remainingMs: timerWindow.timer.targetMs - timerWindow.timer.elapsedMs
    readonly property bool lastMinute: timerWindow.timer.running && timerWindow.timer.targetMs > 0
                                       && timerWindow.remainingMs > 0 && timerWindow.remainingMs <= 60000
    property date now: new Date()
    // The five minutes before the meeting count down to its start.
    readonly property real startsInMs: timerWindow.timer.meetingStart
                                       ? timerWindow.timer.meetingStart.getTime() - timerWindow.now.getTime()
                                       : -1
    readonly property bool startingSoon: !timerWindow.timer.running
                                         && timerWindow.startsInMs > 0 && timerWindow.startsInMs <= 5 * 60000

    // Same titles as in the timer popover.
    function partTitle(part) {
        if (!part)
            return ""
        if (part.custom)
            return part.title.length > 0 ? part.title : qsTr("Untitled part")
        const id = part.id
        if (id.startsWith("ayfm-"))
            return qsTr("Apply Yourself %1").arg(id.substring(5))
        if (id.startsWith("lac-"))
            return qsTr("Living as Christians %1").arg(id.substring(4))
        switch (id) {
        case "opening-song":
        case "closing-song":
            return qsTr("Song and prayer")
        case "middle-song":
            return qsTr("Song")
        case "introduction":
            return qsTr("Opening comments")
        case "treasures":
            return qsTr("Treasures From God's Word")
        case "gems":
            return qsTr("Spiritual gems")
        case "bible-reading":
            return qsTr("Bible reading")
        case "cbs":
            return qsTr("Congregation Bible Study")
        case "concluding-comments":
            return qsTr("Concluding comments")
        case "co-service-talk":
            return qsTr("Service talk")
        case "public-talk":
            return qsTr("Public talk")
        case "wt":
        case "abbreviated-wt":
            return qsTr("Watchtower Study")
        case "co-final-talk":
            return qsTr("Circuit overseer's talk")
        }
        return id
    }

    function twoDigits(value) {
        return (value < 10 ? "0" : "") + value
    }

    title: qsTr("KMedia - Timer")
    color: "#000000"
    visible: ApplicationController.timerWindowVisible
    flags: Qt.Window | (timerWindow.dedicated ? Qt.FramelessWindowHint : 0)
    x: timerWindow.dedicated ? timerWindow.freeScreen.x : x
    y: timerWindow.dedicated ? timerWindow.freeScreen.y : y
    width: timerWindow.dedicated ? timerWindow.freeScreen.width : 640
    height: timerWindow.dedicated ? timerWindow.freeScreen.height : 360
    minimumWidth: 240
    minimumHeight: 135

    // Closing the window turns it off, like the switch in the timer.
    onClosing: ApplicationController.timerWindowVisible = false

    Timer {
        interval: 1000
        repeat: true
        running: timerWindow.visible
        triggeredOnStart: true
        onTriggered: timerWindow.now = new Date()
    }

    Column {
        anchors.centerIn: parent
        width: parent.width * 0.9
        spacing: timerWindow.height * 0.03

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            visible: text.length > 0
            text: timerWindow.currentPart
                  ? timerWindow.partTitle(timerWindow.currentPart)
                  : (timerWindow.startingSoon ? qsTr("Meeting starts in") : "")
            color: "#B0B6C0"
            font.family: Theme.fontFamily
            font.pixelSize: Math.max(16, Math.min(timerWindow.width * 0.045, timerWindow.height * 0.08))
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Item {
            id: dial

            readonly property real size: Math.min(timerWindow.width * 0.9, timerWindow.height * 0.7)
            readonly property real progress: timerWindow.timer.targetMs > 0
                                             ? Math.min(1, timerWindow.timer.elapsedMs / timerWindow.timer.targetMs)
                                             : 0
            readonly property color tone: timerWindow.timer.overtime
                                          ? "#EF4444"
                                          : (timerWindow.lastMinute ? "#F59E0B" : "#22C55E")

            width: parent.width
            height: dial.size

            // How much of the part has gone, filling as it runs.
            Shape {
                id: ring

                anchors.centerIn: parent
                width: dial.size
                height: dial.size
                visible: timerWindow.timer.running && timerWindow.timer.targetMs > 0
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: "#1F242C"
                    strokeWidth: dial.size * 0.035
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap

                    PathAngleArc {
                        centerX: ring.width / 2
                        centerY: ring.height / 2
                        radiusX: ring.width / 2 - dial.size * 0.03
                        radiusY: ring.height / 2 - dial.size * 0.03
                        startAngle: -90
                        sweepAngle: 360
                    }
                }

                ShapePath {
                    strokeColor: dial.tone
                    strokeWidth: dial.size * 0.035
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap

                    PathAngleArc {
                        centerX: ring.width / 2
                        centerY: ring.height / 2
                        radiusX: ring.width / 2 - dial.size * 0.03
                        radiusY: ring.height / 2 - dial.size * 0.03
                        startAngle: -90
                        sweepAngle: 360 * (timerWindow.timer.overtime ? 1 : dial.progress)
                    }
                }
            }

            Text {
                id: digits

                anchors.centerIn: parent
                text: timerWindow.timer.running
                      ? timerWindow.timer.formattedTime
                      : (timerWindow.startingSoon
                         ? timerWindow.twoDigits(Math.floor(timerWindow.startsInMs / 60000)) + ":"
                           + timerWindow.twoDigits(Math.floor(timerWindow.startsInMs / 1000) % 60)
                         : timerWindow.twoDigits(timerWindow.now.getHours()) + ":"
                           + timerWindow.twoDigits(timerWindow.now.getMinutes()))
                color: timerWindow.timer.overtime
                       ? "#EF4444"
                       : (timerWindow.lastMinute ? "#F59E0B" : "#FFFFFF")
                font.family: Theme.fontFamily
                font.pixelSize: ring.visible ? dial.size * 0.26 : Math.min(timerWindow.width * 0.24, dial.size * 0.6)
                font.weight: Font.Bold
                font.features: { "tnum": 1 }

                // Blinks gently while paused or past its time.
                SequentialAnimation on opacity {
                    running: timerWindow.visible && (timerWindow.timer.paused || timerWindow.timer.overtime)
                    loops: Animation.Infinite
                    alwaysRunToEnd: true

                    NumberAnimation {
                        to: 0.35
                        duration: 600
                        easing.type: Easing.InOutQuad
                    }

                    NumberAnimation {
                        to: 1.0
                        duration: 600
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }
    }
}
