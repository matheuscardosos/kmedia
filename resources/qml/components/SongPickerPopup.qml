pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Popup {
    id: root

    property string sectionId: ""
    signal songChosen(int number, string sectionId)

    function openFor(targetSectionId) {
        root.sectionId = targetSectionId
        numberField.text = ""
        root.open()
        numberField.forceActiveFocus()
    }

    function accept() {
        if (!numberField.acceptableInput) {
            return
        }
        root.songChosen(Number(numberField.text), root.sectionId)
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

        RowLayout {
            spacing: 8

            StudioIcon {
                name: "music"
                size: 16
                color: Theme.accentPrimary
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Choose a song")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                visible: root.sectionId.length > 0
                text: ApplicationController.playlist.sectionLabel(root.sectionId)
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                elide: Text.ElideRight
                Layout.maximumWidth: 140
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            TextField {
                id: numberField

                Layout.fillWidth: true
                placeholderText: qsTr("Song number")
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator {
                    bottom: 1
                    top: 999
                }
                color: Theme.textPrimary
                placeholderTextColor: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                onAccepted: root.accept()
                background: StudioCard {
                    surfaceColor: Theme.windowBase
                    cornerRadius: Theme.radiusSmall
                }
            }

            StudioButton {
                variant: "primary"
                text: qsTr("Add")
                enabled: numberField.acceptableInput
                onClicked: root.accept()
            }
        }
    }
}
