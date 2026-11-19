pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

StudioCard {
    id: gateRoot

    property string errorMessage: ""

    function attemptUnlock() {
        if (ApplicationController.unlockBoothSettings(pinInput.text)) {
            gateRoot.errorMessage = ""
            pinInput.text = ""
        } else {
            gateRoot.errorMessage = qsTr("Incorrect PIN")
            pinInput.selectAll()
        }
    }

    implicitHeight: gateCol.implicitHeight + 32
    surfaceColor: Theme.panelSurface

    ColumnLayout {
        id: gateCol

        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: 18
                color: Theme.accentSoft
                border.width: 1
                border.color: Theme.accentBorder

                StudioIcon {
                    anchors.centerIn: parent
                    name: "lock"
                    size: 17
                    color: Theme.accentPrimary
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: qsTr("Booth Settings Locked")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLg
                    font.weight: Font.DemiBold
                }

                Text {
                    text: qsTr("Enter the congregation booth PIN to modify settings.")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: pinInput

                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: qsTr("Enter booth PIN...")
                color: Theme.textPrimary
                placeholderTextColor: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                Keys.onReturnPressed: gateRoot.attemptUnlock()
                background: StudioCard {
                    surfaceColor: Theme.windowBase
                    cornerRadius: Theme.radiusSmall
                }
            }

            StudioButton {
                variant: "primary"
                iconName: "unlock"
                text: qsTr("Unlock")
                enabled: pinInput.text.trim().length >= 4
                onClicked: gateRoot.attemptUnlock()
            }
        }

        Text {
            visible: gateRoot.errorMessage.length > 0
            text: gateRoot.errorMessage
            color: Theme.statusLive
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
        }
    }
}
