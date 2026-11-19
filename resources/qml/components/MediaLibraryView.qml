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
    signal addFilesRequested()

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
            implicitHeight: pkgCol.implicitHeight + 28
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: pkgCol

                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    StudioIcon {
                        name: "jwpub"
                        category: "custom"
                        size: 20
                        color: Theme.accentPrimary
                    }

                    Text {
                        text: qsTr("Import Local Media or Publication Packages")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLg
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Supports native hardware-accelerated video (MP4, MKV, WEBM), high-DPI images, PDF pages, and extracted .jwpub / .jwlplaylist bundles with preserved sort order.")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    spacing: 8

                    StudioButton {
                        variant: "primary"
                        iconName: "folder-open"
                        text: qsTr("Browse Files or Packages")
                        onClicked: root.addFilesRequested()
                    }
                }
            }
        }

        StudioCard {
            Layout.fillWidth: true
            implicitHeight: cloudCol.implicitHeight + 28
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: cloudCol

                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StudioIcon {
                        name: "folder-plus"
                        size: 17
                        color: Theme.accentPrimary
                    }

                    Text {
                        text: qsTr("Watched Cloud Sync Folders")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLg
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }

                    StudioBadge {
                        text: qsTr("%1 folders").arg(ApplicationController.cloudWatcher.watchedFolders.length)
                        badgeColor: Theme.cardSurface
                        textColor: Theme.textSecondary
                        hasBorder: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Any media added to configured Dropbox, OneDrive, iCloud Drive, or Nextcloud folders appears in your workspace automatically.")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    wrapMode: Text.WordWrap
                }
            }
        }

        StudioCard {
            Layout.fillWidth: true
            implicitHeight: libListCol.implicitHeight + 28
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: libListCol

                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StudioIcon {
                        name: "jwlplaylist"
                        category: "custom"
                        size: 18
                        color: Theme.textSecondary
                    }

                    Text {
                        text: qsTr("Queued Media Items (%1)").arg(ApplicationController.playlist.count)
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeMd
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                }

                Repeater {
                    model: ApplicationController.playlist

                    delegate: Rectangle {
                        id: libRow

                        required property int index
                        required property string title
                        required property string formatLabel
                        required property string resolutionLabel
                        required property string formattedDuration

                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: Theme.radiusSmall
                        color: Theme.cardSurface
                        border.width: 1
                        border.color: Theme.borderHairline

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            Text {
                                text: "" + (libRow.index + 1)
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                Layout.preferredWidth: 18
                            }

                            Text {
                                text: libRow.title
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: libRow.formatLabel + (libRow.resolutionLabel.length > 0 ? (" | " + libRow.resolutionLabel) : "")
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                visible: !root.compactMode
                            }

                            Text {
                                text: libRow.formattedDuration
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }
                        }
                    }
                }
            }
        }
    }
}
