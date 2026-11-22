pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

ColumnLayout {
    id: root

    signal closeRequested()
    signal openSettingsRequested()

    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        StudioIcon {
            name: "zoom"
            category: "custom"
            size: 20
            color: ApplicationController.zoomBridge.zoomRunning ? Theme.statusReady : Theme.textSecondary
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            Text {
                text: "Zoom"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            Text {
                text: ApplicationController.zoomBridge.meetingId.length > 0
                      ? qsTr("ID: %1").arg(ApplicationController.zoomBridge.meetingId)
                      : qsTr("Meeting ID not set")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                Layout.fillWidth: true
            }
        }

        StudioBadge {
            text: ApplicationController.zoomBridge.zoomRunning ? qsTr("Running") : qsTr("Standby")
            showDot: true
            dotColor: ApplicationController.zoomBridge.zoomRunning ? Theme.statusReady : Theme.statusOff
            hasBorder: true
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: zoomMetaCol.implicitHeight + 16
        radius: Theme.radiusSmall
        color: Theme.cardSurface
        border.width: 1
        border.color: Theme.borderHairline

        ColumnLayout {
            id: zoomMetaCol

            anchors.fill: parent
            anchors.margins: 8
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: qsTr("Share Shortcut:")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    Layout.fillWidth: true
                }

                Text {
                    text: ApplicationController.zoomBridge.shareShortcutHint
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: Font.DemiBold
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: qsTr("Audio Output:")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }

                Text {
                    text: ApplicationController.playback.audioDeviceDescription.length > 0
                          ? ApplicationController.playback.audioDeviceDescription
                          : qsTr("System default")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        StudioButton {
            variant: "primary"
            iconName: "external-link"
            text: qsTr("Join Meeting")
            Layout.fillWidth: true
            onClicked: {
                if (!ApplicationController.zoomBridge.joinMeeting()) {
                    ApplicationController.showToast(qsTr("Set a valid Zoom Meeting ID in Settings first"), "warning")
                }
                root.closeRequested()
            }
        }

        StudioButton {
            variant: "ghost"
            iconName: "settings"
            implicitWidth: 34
            leftPadding: 0
            rightPadding: 0
            onClicked: root.openSettingsRequested()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Zoom settings")
        }
    }
}
