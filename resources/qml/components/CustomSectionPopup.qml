pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Popup {
    id: popupRoot

    // mode: "create" | "edit" | "assign"
    property string mode: "create"
    property string targetSectionId: ""
    property string selectedColor: "#945EB5"
    property bool canRemove: false
    property bool canRename: true
    property int assignItemIndex: -1

    readonly property var colorSwatches: [
        "#3C7F8B",
        "#D68F00",
        "#BF2F13",
        "#945EB5",
        "#3E6EA8",
        "#3F7F5F",
        "#8C3B66",
        "#5F6B73",
        "#6B4E9B"
    ]

    function isCustomSection(sectionId) {
        return sectionId === "imported-media" || sectionId.startsWith("custom-")
    }

    function openCreate() {
        popupRoot.mode = "create"
        popupRoot.targetSectionId = ""
        popupRoot.selectedColor = "#945EB5"
        popupRoot.canRemove = false
        popupRoot.canRename = true
        popupRoot.assignItemIndex = -1
        sectionNameField.text = ""
        popupRoot.open()
    }

    function openEdit(sectionId, colorHex) {
        popupRoot.mode = "edit"
        popupRoot.targetSectionId = sectionId
        popupRoot.selectedColor = colorHex
        popupRoot.canRename = popupRoot.isCustomSection(sectionId)
        popupRoot.canRemove = sectionId.startsWith("custom-")
        popupRoot.assignItemIndex = -1
        sectionNameField.text = ApplicationController.playlist.sectionLabel(sectionId)
        popupRoot.open()
    }

    // Moves every selected item instead of a single one.
    function openAssignSelection(currentSectionId) {
        popupRoot.openAssign(-1, currentSectionId)
    }

    function openAssign(rowIndex, currentSectionId) {
        popupRoot.mode = "assign"
        popupRoot.assignItemIndex = rowIndex
        popupRoot.targetSectionId = currentSectionId
        popupRoot.selectedColor = "#945EB5"
        popupRoot.canRemove = false
        popupRoot.canRename = false
        sectionNameField.text = ""
        popupRoot.open()
    }

    width: Math.min(352, parent ? parent.width - 24 : 352)
    padding: 16
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle {
        color: Theme.scrimOverlay
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
                name: popupRoot.mode === "create" ? "folder-plus" : "layers"
                size: 16
                color: Theme.accentPrimary
            }

            Text {
                text: popupRoot.mode === "create"
                      ? qsTr("New Meeting Section")
                      : (popupRoot.mode === "assign"
                         ? qsTr("Assign Item to Section")
                         : qsTr("Customize Section"))
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
        }

        // Section list for quick item assignment
        ColumnLayout {
            visible: popupRoot.mode === "assign"
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: ApplicationController.playlist.sections

                delegate: StudioButton {
                    id: assignSecBtn

                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    variant: assignSecBtn.modelData.id === popupRoot.targetSectionId
                             ? "primary" : "secondary"
                    text: assignSecBtn.modelData.name
                    onClicked: {
                        if (popupRoot.assignItemIndex < 0) {
                            ApplicationController.playlist.assignSelectedToSection(assignSecBtn.modelData.id)
                        } else {
                            ApplicationController.playlist.assignItemToSection(
                                popupRoot.assignItemIndex, assignSecBtn.modelData.id)
                        }
                        popupRoot.close()
                    }
                }
            }
        }

        // Create / Edit section name & swatches
        ColumnLayout {
            visible: popupRoot.mode !== "assign"
            Layout.fillWidth: true
            spacing: 10

            TextField {
                id: sectionNameField

                Layout.fillWidth: true
                readOnly: !popupRoot.canRename
                placeholderText: qsTr("Section title...")
                color: Theme.textPrimary
                placeholderTextColor: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                background: StudioCard {
                    surfaceColor: Theme.windowBase
                    cornerRadius: Theme.radiusSmall
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: popupRoot.colorSwatches

                    delegate: Rectangle {
                        id: swatchRect

                        required property string modelData

                        readonly property bool isChosen: popupRoot.selectedColor.toUpperCase() === swatchRect.modelData.toUpperCase()

                        width: 26
                        height: 26
                        radius: 13
                        color: swatchRect.modelData
                        border.width: swatchRect.isChosen ? 2 : 1
                        border.color: swatchRect.isChosen ? Theme.textPrimary : Theme.borderHairline

                        StudioIcon {
                            visible: swatchRect.isChosen
                            anchors.centerIn: parent
                            name: "check"
                            size: 13
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: popupRoot.selectedColor = swatchRect.modelData
                        }
                    }
                }
            }

            RowLayout {
                visible: popupRoot.mode === "edit"
                Layout.fillWidth: true
                spacing: 6

                StudioButton {
                    variant: "secondary"
                    iconName: "chevron-up"
                    text: qsTr("Move Up")
                    Layout.fillWidth: true
                    onClicked: ApplicationController.playlist.moveSection(popupRoot.targetSectionId, -1)
                }

                StudioButton {
                    variant: "secondary"
                    iconName: "chevron-down"
                    text: qsTr("Move Down")
                    Layout.fillWidth: true
                    onClicked: ApplicationController.playlist.moveSection(popupRoot.targetSectionId, 1)
                }
            }

            StudioButton {
                visible: popupRoot.mode === "edit" && ApplicationController.playlist.activeIndex >= 0
                Layout.fillWidth: true
                variant: "secondary"
                iconName: "layers"
                text: qsTr("Move Active Media Here")
                onClicked: {
                    ApplicationController.playlist.assignItemToSection(
                        ApplicationController.playlist.activeIndex,
                        popupRoot.targetSectionId)
                    popupRoot.close()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioButton {
                visible: popupRoot.mode === "edit" && popupRoot.canRemove
                variant: "danger"
                iconName: "trash"
                leftPadding: 8
                rightPadding: 8
                onClicked: {
                    ApplicationController.playlist.removeSection(popupRoot.targetSectionId)
                    popupRoot.close()
                }
            }

            Item {
                Layout.fillWidth: true
            }

            StudioButton {
                variant: "ghost"
                text: qsTr("Close")
                onClicked: popupRoot.close()
            }

            StudioButton {
                visible: popupRoot.mode !== "assign"
                variant: "primary"
                iconName: "check"
                text: popupRoot.mode === "create" ? qsTr("Create") : qsTr("Save")
                enabled: sectionNameField.text.trim().length > 0
                onClicked: {
                    const cleanTitle = sectionNameField.text.trim()
                    if (popupRoot.mode === "create") {
                        ApplicationController.playlist.createSection(cleanTitle, popupRoot.selectedColor)
                    } else {
                        if (popupRoot.canRename &&
                            cleanTitle !== ApplicationController.playlist.sectionLabel(popupRoot.targetSectionId)) {
                            ApplicationController.playlist.renameSection(popupRoot.targetSectionId, cleanTitle)
                        }
                        ApplicationController.playlist.setSectionColor(popupRoot.targetSectionId, popupRoot.selectedColor)
                    }
                    popupRoot.close()
                }
            }
        }
    }
}
