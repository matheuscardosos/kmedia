pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Popup {
    id: popupRoot

    property string selectedType: ""

    readonly property var eventOptions: [
        { type: "", label: qsTr("Regular Week"), icon: "calendar" },
        { type: "co_visit", label: qsTr("Circuit Overseer Visit"), icon: "users" },
        { type: "memorial", label: qsTr("Memorial"), icon: "disc-3" },
        { type: "circuit_assembly", label: qsTr("Circuit Assembly"), icon: "layers" },
        { type: "regional_convention", label: qsTr("Regional Convention"), icon: "globe" }
    ]

    width: Math.min(360, parent ? parent.width - 24 : 360)
    padding: 16
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle {
        color: Theme.scrimOverlay
    }

    onAboutToShow: {
        popupRoot.selectedType = ApplicationController.calendar.selectedWeekEventType
        customLabelField.text = ApplicationController.calendar.selectedWeekEventLabel
    }

    background: StudioCard {
        surfaceColor: Theme.cardElevated
        strokeColor: Theme.borderStrong
        cornerRadius: Theme.radiusLarge
    }

    contentItem: ColumnLayout {
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioIcon {
                name: "users"
                size: 16
                color: Theme.accentPrimary
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                    text: qsTr("Week Special Event")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeMd
                    font.weight: Font.DemiBold
                }

                Text {
                    text: ApplicationController.calendar.selectedWeekLabel
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: popupRoot.eventOptions

                delegate: StudioButton {
                    id: evBtn

                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    variant: popupRoot.selectedType === evBtn.modelData.type ? "primary" : "secondary"
                    iconName: evBtn.modelData.icon
                    text: evBtn.modelData.label
                    onClicked: {
                        popupRoot.selectedType = evBtn.modelData.type
                        if (popupRoot.selectedType.length === 0) {
                            customLabelField.text = ""
                        }
                    }
                }
            }
        }

        TextField {
            id: customLabelField

            visible: popupRoot.selectedType.length > 0
            Layout.fillWidth: true
            placeholderText: qsTr("Optional custom event title...")
            color: Theme.textPrimary
            placeholderTextColor: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            background: StudioCard {
                surfaceColor: Theme.windowBase
                cornerRadius: Theme.radiusSmall
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioButton {
                variant: "ghost"
                text: qsTr("Reset")
                visible: ApplicationController.calendar.selectedWeekEventType.length > 0
                onClicked: {
                    ApplicationController.calendar.clearWeekSpecialEvent(
                        ApplicationController.calendar.selectedDateIso)
                    popupRoot.close()
                }
            }

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
                text: qsTr("Apply")
                onClicked: {
                    ApplicationController.calendar.setWeekSpecialEvent(
                        ApplicationController.calendar.selectedDateIso,
                        popupRoot.selectedType,
                        customLabelField.text
                    )
                    popupRoot.close()
                }
            }
        }
    }
}
