pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// Settings > Meetings: every day with media is copied into `<folder>/YYYY-MM-DD/`.
StudioCard {
    id: root

    implicitHeight: column.implicitHeight + 28
    surfaceColor: Theme.panelSurface

    ColumnLayout {
        id: column

        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioIcon {
                name: "download"
                size: 17
                color: Theme.accentPrimary
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Media export")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            StudioSwitch {
                checked: ApplicationController.autoExport.enabled
                onToggled: ApplicationController.autoExport.enabled = checked
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Each day with media is copied into a date folder, in meeting order.")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            wrapMode: Text.WordWrap
        }

        FolderPathField {
            visible: ApplicationController.autoExport.enabled
            Layout.fillWidth: true
            path: ApplicationController.autoExport.folder
            dialogTitle: qsTr("Media export")
            onFolderChosen: (path) => ApplicationController.autoExport.folder = path
        }

        RowLayout {
            visible: ApplicationController.autoExport.enabled
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Convert images and audio to MP4")
                    color: ApplicationController.autoExport.ffmpegAvailable ? Theme.textPrimary : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    wrapMode: Text.WordWrap
                }

                Text {
                    visible: !ApplicationController.autoExport.ffmpegAvailable
                    Layout.fillWidth: true
                    text: qsTr("Needs FFmpeg installed on this computer.")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    wrapMode: Text.WordWrap
                }
            }

            StudioSwitch {
                enabled: ApplicationController.autoExport.ffmpegAvailable
                checked: ApplicationController.autoExport.convertToMp4
                onToggled: ApplicationController.autoExport.convertToMp4 = checked
            }
        }
    }
}
