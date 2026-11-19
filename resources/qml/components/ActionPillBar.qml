pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Item {
    id: root

    property bool compactMode: false
    signal openCommandPalette()
    signal openBoothSettings()

    implicitHeight: 36

    component PillAction: Rectangle {
        id: actionBtn

        property string iconName: ""
        property string label: ""
        property string tooltipText: ""
        property color dotColor: "transparent"
        property bool showDot: false
        property bool activeState: false
        signal triggered()

        Layout.preferredHeight: 28
        implicitWidth: pillContent.implicitWidth + (root.compactMode ? 14 : 18)
        radius: 14
        color: actionBtn.activeState ? Theme.accentSoft
             : (pillMouse.containsMouse ? Theme.cardHover : "transparent")
        border.width: actionBtn.activeState ? 1 : 0
        border.color: actionBtn.activeState ? Theme.accentBorder : "transparent"

        Behavior on color {
            ColorAnimation {
                duration: Theme.animFast
            }
        }

        RowLayout {
            id: pillContent

            anchors.centerIn: parent
            spacing: 5

            Rectangle {
                visible: actionBtn.showDot
                Layout.preferredWidth: 6
                Layout.preferredHeight: 6
                radius: 3
                color: actionBtn.dotColor
            }

            StudioIcon {
                name: actionBtn.iconName
                size: 13
                color: actionBtn.activeState ? Theme.accentPrimary : Theme.textSecondary
            }

            Text {
                visible: !root.compactMode && actionBtn.label.length > 0
                text: actionBtn.label
                color: actionBtn.activeState ? Theme.accentPrimary : Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                font.weight: actionBtn.activeState ? Font.DemiBold : Font.Medium
            }
        }

        ToolTip.visible: pillMouse.containsMouse && actionBtn.tooltipText.length > 0
        ToolTip.delay: 350
        ToolTip.text: actionBtn.tooltipText

        MouseArea {
            id: pillMouse

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: actionBtn.triggered()
        }
    }

    StudioCard {
        anchors.centerIn: parent
        width: Math.min(parent.width, pillRow.implicitWidth + 12)
        height: 36
        cornerRadius: 18
        surfaceColor: Theme.panelSurface
        strokeColor: Theme.borderStrong

        RowLayout {
            id: pillRow

            anchors.centerIn: parent
            spacing: 4

            PillAction {
                iconName: "monitor"
                label: qsTr("Display")
                tooltipText: qsTr("Toggle secondary presentation display (Ctrl+P)")
                showDot: true
                activeState: ApplicationController.displayManager.presentationWindowVisible
                dotColor: ApplicationController.displayManager.presentationWindowVisible
                          ? Theme.statusReady
                          : (ApplicationController.displayManager.hasSecondaryDisplay ? Theme.statusStandby : Theme.statusOff)
                onTriggered: ApplicationController.displayManager.presentationWindowVisible = !ApplicationController.displayManager.presentationWindowVisible
            }

            PillAction {
                iconName: "eye-off"
                label: qsTr("Standby")
                tooltipText: qsTr("Toggle standby blackout overlay")
                showDot: true
                activeState: ApplicationController.playback.standbyActive
                dotColor: ApplicationController.playback.standbyActive ? Theme.statusStandby : Theme.statusOff
                onTriggered: ApplicationController.playback.toggleStandby()
            }

            PillAction {
                iconName: "audio-lines"
                label: qsTr("Music")
                tooltipText: qsTr("Toggle entrance background music")
                showDot: true
                activeState: ApplicationController.backgroundMusic.playing
                dotColor: ApplicationController.backgroundMusic.playing ? Theme.statusReady : Theme.statusOff
                onTriggered: {
                    if (ApplicationController.backgroundMusic.playing) {
                        ApplicationController.backgroundMusic.fadeOutAndStop(4000)
                    } else {
                        ApplicationController.backgroundMusic.play()
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 16
                color: Theme.borderHairline
            }

            PillAction {
                iconName: "radio"
                label: "OBS"
                tooltipText: qsTr("OBS WebSocket status and booth settings")
                showDot: true
                activeState: ApplicationController.obsClient.connected
                dotColor: ApplicationController.obsClient.connected
                          ? Theme.statusReady
                          : (ApplicationController.processMonitor.obsRunning ? Theme.statusStandby : Theme.statusOff)
                onTriggered: root.openBoothSettings()
            }

            PillAction {
                iconName: "video"
                label: "Zoom"
                tooltipText: qsTr("Launch congregation Zoom meeting")
                showDot: true
                activeState: ApplicationController.zoomBridge.zoomRunning
                dotColor: ApplicationController.zoomBridge.zoomRunning ? Theme.statusReady : Theme.statusOff
                onTriggered: ApplicationController.zoomBridge.joinMeeting()
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 16
                color: Theme.borderHairline
            }

            PillAction {
                iconName: "command"
                label: root.compactMode ? "" : "Ctrl+K"
                tooltipText: qsTr("Open quick command palette (Ctrl+K)")
                onTriggered: root.openCommandPalette()
            }
        }
    }
}
