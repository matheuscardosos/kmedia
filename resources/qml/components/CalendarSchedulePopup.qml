pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Popup {
    id: popupRoot

    property int selectedMidweekDay: ApplicationController.calendar.midweekDay
    property int selectedWeekendDay: ApplicationController.calendar.weekendDay

    readonly property var midweekOptions: [
        { day: 1, label: qsTr("Mon") },
        { day: 2, label: qsTr("Tue") },
        { day: 3, label: qsTr("Wed") },
        { day: 4, label: qsTr("Thu") },
        { day: 5, label: qsTr("Fri") }
    ]

    readonly property var weekendOptions: [
        { day: 6, label: qsTr("Sat") },
        { day: 7, label: qsTr("Sun") }
    ]

    width: Math.min(340, parent ? parent.width - 24 : 340)
    padding: 16
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle {
        color: Theme.scrimOverlay
    }

    onAboutToShow: {
        popupRoot.selectedMidweekDay = ApplicationController.calendar.midweekDay
        popupRoot.selectedWeekendDay = ApplicationController.calendar.weekendDay
        midTimeField.text = ApplicationController.calendar.midweekTime
        endTimeField.text = ApplicationController.calendar.weekendTime
    }

    background: StudioCard {
        surfaceColor: Theme.cardElevated
        strokeColor: Theme.borderStrong
        cornerRadius: Theme.radiusLarge
    }

    contentItem: ColumnLayout {
        id: scheduleForm

        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioIcon {
                name: "clock"
                size: 16
                color: Theme.accentPrimary
            }

            Text {
                text: qsTr("Congregation Meeting Schedule")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("Midweek Meeting")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                font.weight: Font.DemiBold
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Repeater {
                    model: popupRoot.midweekOptions

                    delegate: StudioButton {
                        id: midBtn

                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        variant: popupRoot.selectedMidweekDay === midBtn.modelData.day ? "primary" : "secondary"
                        text: midBtn.modelData.label
                        onClicked: popupRoot.selectedMidweekDay = midBtn.modelData.day
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: qsTr("Start time (HH:mm)")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    Layout.fillWidth: true
                }

                TextField {
                    id: midTimeField

                    Layout.preferredWidth: 88
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    placeholderText: "19:30"
                    background: StudioCard {
                        surfaceColor: Theme.windowBase
                        cornerRadius: Theme.radiusSmall
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderHairline
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("Weekend Meeting")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                font.weight: Font.DemiBold
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Repeater {
                    model: popupRoot.weekendOptions

                    delegate: StudioButton {
                        id: endBtn

                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        variant: popupRoot.selectedWeekendDay === endBtn.modelData.day ? "primary" : "secondary"
                        text: endBtn.modelData.label
                        onClicked: popupRoot.selectedWeekendDay = endBtn.modelData.day
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: qsTr("Start time (HH:mm)")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    Layout.fillWidth: true
                }

                TextField {
                    id: endTimeField

                    Layout.preferredWidth: 88
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    placeholderText: "09:30"
                    background: StudioCard {
                        surfaceColor: Theme.windowBase
                        cornerRadius: Theme.radiusSmall
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            StudioButton {
                variant: "ghost"
                text: qsTr("Cancel")
                onClicked: popupRoot.close()
            }

            StudioButton {
                variant: "primary"
                iconName: "check"
                text: qsTr("Save")
                onClicked: {
                    ApplicationController.calendar.setScheduleDays(
                        popupRoot.selectedMidweekDay,
                        midTimeField.text,
                        popupRoot.selectedWeekendDay,
                        endTimeField.text
                    )
                    popupRoot.close()
                }
            }
        }
    }
}
