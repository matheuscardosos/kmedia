pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Popup {
    id: root

    property int targetIndex: -1
    property string selectedTagType: "para"

    function openFor(index, currentType, currentValue) {
        root.targetIndex = index
        root.selectedTagType = currentType.length > 0 ? currentType : "para"
        tagValueField.text = currentValue
        root.open()
        tagValueField.forceActiveFocus()
    }

    function save() {
        ApplicationController.playlist.updateItemTag(root.targetIndex, root.selectedTagType,
                                                     tagValueField.text.trim())
        root.close()
    }

    width: Math.min(300, parent ? parent.width - 24 : 300)
    padding: 14
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay

    Overlay.modal: Rectangle {
        color: Theme.scrimOverlay
    }

    background: StudioCard {
        surfaceColor: Theme.cardElevated
        strokeColor: Theme.borderStrong
    }

    contentItem: ColumnLayout {
        spacing: 10

        Text {
            text: qsTr("Edit Media Tag")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: [
                    { type: "song", label: qsTr("Song") },
                    { type: "para", label: qsTr("Paragraph") },
                    { type: "custom", label: qsTr("Custom") }
                ]

                delegate: StudioButton {
                    id: typeButton

                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    variant: root.selectedTagType === typeButton.modelData.type ? "primary" : "secondary"
                    text: typeButton.modelData.label
                    onClicked: root.selectedTagType = typeButton.modelData.type
                }
            }
        }

        TextField {
            id: tagValueField

            Layout.fillWidth: true
            placeholderText: root.selectedTagType === "song" ? qsTr("Song number")
                           : (root.selectedTagType === "para" ? qsTr("Paragraph number") : qsTr("Label"))
            color: Theme.textPrimary
            placeholderTextColor: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            onAccepted: root.save()
            background: StudioCard {
                surfaceColor: Theme.windowBase
                cornerRadius: Theme.radiusSmall
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            StudioButton {
                variant: "ghost"
                text: qsTr("Clear")
                onClicked: {
                    ApplicationController.playlist.updateItemTag(root.targetIndex, "", "")
                    root.close()
                }
            }

            Item {
                Layout.fillWidth: true
            }

            StudioButton {
                variant: "primary"
                text: qsTr("Save")
                onClicked: root.save()
            }
        }
    }
}
