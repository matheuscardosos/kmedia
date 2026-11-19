pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

ScrollView {
    id: root

    property bool compactMode: false

    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical: StudioScrollBar {
        policy: ScrollBar.AsNeeded
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacingMd

        StudioCard {
            Layout.fillWidth: true
            implicitHeight: plHeaderCol.implicitHeight + 28
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: plHeaderCol

                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    StudioIcon {
                        name: "layers"
                        size: 18
                        color: Theme.accentPrimary
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: qsTr("Meeting Playlists (%1)").arg(ApplicationController.playlist.availablePlaylistNames.length)
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLg
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: qsTr("Active: %1 (%2 items, %3)")
                                  .arg(ApplicationController.playlist.playlistName)
                                  .arg(ApplicationController.playlist.count)
                                  .arg(ApplicationController.playlist.formattedTotalDuration)
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }
                    }
                }

                // Create new playlist row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        id: newPlaylistField

                        Layout.fillWidth: true
                        placeholderText: qsTr("New playlist name...")
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        background: StudioCard {
                            surfaceColor: Theme.windowBase
                            cornerRadius: Theme.radiusSmall
                        }
                    }

                    StudioButton {
                        variant: "primary"
                        iconName: "plus"
                        text: qsTr("Create")
                        enabled: newPlaylistField.text.trim().length > 0
                        onClicked: {
                            const created = newPlaylistField.text.trim()
                            if (ApplicationController.playlist.createNewPlaylist(created)) {
                                newPlaylistField.text = ""
                                ApplicationController.showToast(qsTr("Created playlist %1").arg(created), "success")
                            }
                        }
                    }
                }
            }
        }

        // Saved playlists list
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: savedCol.implicitHeight + 24
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: savedCol

                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                Repeater {
                    model: ApplicationController.playlist.availablePlaylistNames

                    delegate: Rectangle {
                        id: plCardRow

                        required property string modelData
                        required property int index

                        readonly property bool isCurrent: index === ApplicationController.playlist.activePlaylistIndex

                        Layout.fillWidth: true
                        implicitHeight: 42
                        radius: Theme.radiusSmall
                        color: plCardRow.isCurrent ? Theme.rowSelected : (itemHover.containsMouse ? Theme.cardHover : Theme.cardSurface)
                        border.width: 1
                        border.color: plCardRow.isCurrent ? Theme.accentPrimary : Theme.borderHairline

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            StudioIcon {
                                name: "calendar"
                                size: 15
                                color: plCardRow.isCurrent ? Theme.accentPrimary : Theme.textSecondary
                            }

                            Text {
                                text: plCardRow.modelData
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMd
                                font.weight: plCardRow.isCurrent ? Font.DemiBold : Font.Medium
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            StudioBadge {
                                visible: plCardRow.isCurrent
                                text: qsTr("ACTIVE")
                                badgeColor: Theme.accentSoft
                                textColor: Theme.accentPrimary
                                hasBorder: true
                                strokeColor: Theme.accentBorder
                            }
                        }

                        MouseArea {
                            id: itemHover

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: ApplicationController.playlist.switchPlaylistByIndex(plCardRow.index)
                        }
                    }
                }
            }
        }
    }
}
