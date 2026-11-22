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

    function openFor(index, currentTitle) {
        root.targetIndex = index
        titleField.text = currentTitle
        root.open()
        titleField.forceActiveFocus()
        titleField.selectAll()
    }

    function save() {
        const title = titleField.text.trim()
        if (title.length > 0) {
            ApplicationController.playlist.setItemTitle(root.targetIndex, title)
        }
        root.close()
    }

    width: Math.min(360, parent ? parent.width - 24 : 360)
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
            text: qsTr("Rename")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }

        TextField {
            id: titleField

            Layout.fillWidth: true
            color: Theme.textPrimary
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

            Item {
                Layout.fillWidth: true
            }

            StudioButton {
                variant: "ghost"
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            StudioButton {
                variant: "primary"
                text: qsTr("Save")
                enabled: titleField.text.trim().length > 0
                onClicked: root.save()
            }
        }
    }
}
