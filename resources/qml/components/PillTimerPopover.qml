pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

ColumnLayout {
    id: root

    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        StudioIcon {
            name: "clock"
            size: 17
            color: ApplicationController.meetingTimer.overtime
                   ? Theme.statusLive
                   : (ApplicationController.meetingTimer.running ? Theme.statusReady : Theme.accentPrimary)
        }

        Text {
            text: qsTr("Meeting Timer")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
            Layout.fillWidth: true
        }

        StudioBadge {
            text: ApplicationController.meetingTimer.overtime ? qsTr("Overtime") : qsTr("On Schedule")
            showDot: true
            dotColor: ApplicationController.meetingTimer.overtime
                      ? Theme.statusLive
                      : (ApplicationController.meetingTimer.running ? Theme.statusReady : Theme.statusOff)
            hasBorder: true
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 62
        radius: Theme.radiusMedium
        color: Theme.cardSurface
        border.width: 1
        border.color: ApplicationController.meetingTimer.overtime ? Theme.statusLive : Theme.borderHairline

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: qsTr("Elapsed")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }

                Text {
                    text: ApplicationController.meetingTimer.formattedElapsed
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: 22
                    font.weight: Font.Bold
                }
            }

            ColumnLayout {
                spacing: 2
                Layout.alignment: Qt.AlignRight

                Text {
                    text: ApplicationController.meetingTimer.overtime ? qsTr("Over") : qsTr("Remaining")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    Layout.alignment: Qt.AlignRight
                }

                Text {
                    text: ApplicationController.meetingTimer.formattedRemaining
                    color: ApplicationController.meetingTimer.overtime ? Theme.statusLive : Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignRight
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        StudioButton {
            variant: "primary"
            iconName: ApplicationController.meetingTimer.running ? "pause" : "play"
            text: ApplicationController.meetingTimer.running ? qsTr("Pause") : qsTr("Start")
            Layout.fillWidth: true
            onClicked: ApplicationController.meetingTimer.toggle()
        }

        StudioButton {
            variant: "secondary"
            iconName: "rotate-ccw"
            text: qsTr("Reset")
            onClicked: ApplicationController.meetingTimer.reset()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        Text {
            text: qsTr("Target: %1 min").arg(ApplicationController.meetingTimer.plannedMinutes)
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            Layout.fillWidth: true
        }

        StudioButton {
            variant: "secondary"
            text: "45m"
            implicitHeight: 26
            leftPadding: 8
            rightPadding: 8
            onClicked: ApplicationController.meetingTimer.plannedMinutes = 45
        }

        StudioButton {
            variant: "secondary"
            text: "60m"
            implicitHeight: 26
            leftPadding: 8
            rightPadding: 8
            onClicked: ApplicationController.meetingTimer.plannedMinutes = 60
        }

        StudioButton {
            variant: "secondary"
            text: "105m"
            implicitHeight: 26
            leftPadding: 8
            rightPadding: 8
            onClicked: ApplicationController.meetingTimer.plannedMinutes = 105
        }
    }
}
