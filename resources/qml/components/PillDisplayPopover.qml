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

    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        StudioIcon {
            name: "monitor"
            size: 17
            color: ApplicationController.displayManager.presentationWindowVisible
                   ? Theme.statusReady : Theme.accentPrimary
        }

        Text {
            text: qsTr("Presentation Display")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
            Layout.fillWidth: true
        }

        StudioSwitch {
            checked: ApplicationController.displayManager.presentationWindowVisible
            onToggled: ApplicationController.displayManager.presentationWindowVisible = checked
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Theme.borderHairline
    }

    Repeater {
        model: ApplicationController.displayManager.screens

        delegate: Rectangle {
            id: screenRow

            required property var modelData
            readonly property bool isTarget: screenRow.modelData.index === ApplicationController.displayManager.targetScreenIndex

            Layout.fillWidth: true
            Layout.preferredHeight: 38
            radius: Theme.radiusSmall
            color: screenRow.isTarget
                   ? Theme.accentSoft
                   : (screenMouse.containsMouse ? Theme.cardHover : Theme.cardSurface)
            border.width: 1
            border.color: screenRow.isTarget ? Theme.accentBorder : Theme.borderHairline

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    radius: 3.5
                    color: screenRow.isTarget ? Theme.accentPrimary : Theme.statusOff
                }

                Text {
                    text: screenRow.modelData.name
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: screenRow.isTarget ? Font.DemiBold : Font.Medium
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    text: screenRow.modelData.width + "x" + screenRow.modelData.height
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }

                StudioBadge {
                    visible: Boolean(screenRow.modelData.isPrimary)
                    text: qsTr("Primary")
                    badgeColor: Theme.panelSurface
                    textColor: Theme.textMuted
                    hasBorder: true
                }
            }

            MouseArea {
                id: screenMouse

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    ApplicationController.displayManager.manualScreenIndex = screenRow.modelData.index
                    ApplicationController.displayManager.presentationWindowVisible = true
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        StudioButton {
            variant: "secondary"
            iconName: "rotate-ccw"
            text: qsTr("Auto Select")
            Layout.fillWidth: true
            enabled: ApplicationController.displayManager.manualScreenIndex >= 0
            onClicked: ApplicationController.displayManager.resetAutoSelection()
        }

        StudioButton {
            variant: ApplicationController.displayManager.presentationWindowVisible ? "danger" : "primary"
            iconName: "monitor"
            text: ApplicationController.displayManager.presentationWindowVisible ? qsTr("Hide") : qsTr("Show")
            Layout.fillWidth: true
            onClicked: {
                ApplicationController.displayManager.presentationWindowVisible = !ApplicationController.displayManager.presentationWindowVisible
                root.closeRequested()
            }
        }
    }
}
