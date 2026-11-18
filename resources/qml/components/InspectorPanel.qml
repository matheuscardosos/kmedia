import QtQuick
import QtQuick.Layouts

import KMedia.UI

Rectangle {
    id: root

    color: Theme.panelSurface

    Rectangle {
        width: 1
        color: Theme.borderHairline
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: qsTr("Live Preview")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
            }

            Rectangle {
                implicitWidth: statusLabel.implicitWidth + Theme.spacingMd
                implicitHeight: 22
                radius: Theme.radiusSmall
                color: previewRenderer.mediaLoaded ? Theme.statusLive : Theme.cardSurface
                border.width: 1
                border.color: previewRenderer.mediaLoaded ? Theme.statusLive : Theme.borderHairline

                Text {
                    id: statusLabel

                    anchors.centerIn: parent
                    text: previewRenderer.mediaLoaded ? qsTr("ON AIR") : qsTr("STANDBY")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: Font.Bold
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(width * 9 / 16)
            radius: Theme.radiusMedium
            color: "#000000"
            border.width: 1
            border.color: Theme.borderHairline

            MpvRendererItem {
                id: previewRenderer

                anchors.fill: parent
                anchors.margins: 1
            }

            Text {
                anchors.centerIn: parent
                visible: !previewRenderer.mediaLoaded
                text: qsTr("Hardware Decoder: %1").arg(previewRenderer.hardwareDecoder)
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderHairline
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Output Routing")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Select a media item to configure trim markers, zoom/pan viewport, or page controls.")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            wrapMode: Text.WordWrap
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
