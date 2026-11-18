import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import KMedia.UI

ColumnLayout {
    id: root

    required property string sectionTitle

    spacing: 0

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.headerHeight
        color: Theme.windowBase

        Rectangle {
            height: 1
            color: Theme.borderHairline
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacingLg
            anchors.rightMargin: Theme.spacingLg

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: root.sectionTitle
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLg
                    font.weight: Font.DemiBold
                }

                Text {
                    text: qsTr("Native Qt %1 + libmpv %2 workspace").arg(ApplicationController.qtVersion).arg(ApplicationController.mpvVersion)
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }
            }

            Button {
                id: addMediaButton

                text: qsTr("+ Add Media")
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: addMediaButton.pressed
                        ? Theme.accentPressed
                        : (addMediaButton.hovered ? Theme.accentHover : Theme.accentPrimary)
                }
                contentItem: Text {
                    text: addMediaButton.text
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: Theme.spacingMd
                    rightPadding: Theme.spacingMd
                }
            }
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - Theme.spacingXl * 2, 440)
            height: emptyColumn.implicitHeight + Theme.spacingXl * 2
            radius: Theme.radiusLarge
            color: Theme.cardSurface
            border.width: 1
            border.color: Theme.borderHairline

            ColumnLayout {
                id: emptyColumn

                anchors.centerIn: parent
                width: parent.width - Theme.spacingXl * 2
                spacing: Theme.spacingSm

                Text {
                    Layout.fillWidth: true
                    text: qsTr("No media items queued")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLg
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Drag and drop local video, audio, image, PDF, JWPUB, or JWLPLAYLIST files here to begin.")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.transportBarHeight
        color: Theme.panelSurface

        Rectangle {
            height: 1
            color: Theme.borderHairline
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacingLg
            anchors.rightMargin: Theme.spacingLg
            spacing: Theme.spacingMd

            Text {
                text: qsTr("00:00 / 00:00")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.features: ({ "tnum": 1 })
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("Standby")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
            }
        }
    }
}
