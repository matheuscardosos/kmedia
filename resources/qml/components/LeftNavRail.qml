pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts

import KMedia.UI

Rectangle {
    id: root

    property int activeTabIndex: 0

    color: Theme.panelSurface

    Rectangle {
        width: 1
        color: Theme.borderHairline
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        spacing: Theme.spacingMd

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs

            Text {
                text: qsTr("KMedia")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.Bold
            }

            Text {
                text: qsTr("Default Congregation")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderHairline
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs

            Repeater {
                model: [
                    qsTr("Meeting"),
                    qsTr("Media"),
                    qsTr("Playlists"),
                    qsTr("Downloads"),
                    qsTr("Settings")
                ]

                delegate: NavRailButton {
                    required property int index
                    required property string modelData

                    Layout.fillWidth: true
                    labelText: modelData
                    active: root.activeTabIndex === index
                    onClicked: root.activeTabIndex = index
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderHairline
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm

            StatusRow {
                Layout.fillWidth: true
                labelText: qsTr("OBS: Standby")
                indicatorColor: Theme.textMuted
            }

            StatusRow {
                Layout.fillWidth: true
                labelText: qsTr("Zoom: Standby")
                indicatorColor: Theme.textMuted
            }

            StatusRow {
                Layout.fillWidth: true
                labelText: qsTr("Display: Primary")
                indicatorColor: Theme.statusReady
            }
        }
    }
}
