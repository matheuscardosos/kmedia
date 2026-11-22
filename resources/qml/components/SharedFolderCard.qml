pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// Settings > Integrations: media dropped into `<folder>/YYYY-MM-DD/` shows up in that day.
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
                name: "folder-open"
                size: 17
                color: Theme.accentPrimary
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Shared folder")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            StudioSwitch {
                checked: ApplicationController.watchedFolder.enabled
                onToggled: ApplicationController.watchedFolder.enabled = checked
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Media in its date folders (YYYY-MM-DD) shows up in that day.")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            wrapMode: Text.WordWrap
        }

        FolderPathField {
            visible: ApplicationController.watchedFolder.enabled
            Layout.fillWidth: true
            path: ApplicationController.watchedFolder.folder
            dialogTitle: qsTr("Shared folder")
            onFolderChosen: (path) => ApplicationController.watchedFolder.folder = path
        }

        RowLayout {
            visible: ApplicationController.watchedFolder.enabled
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: qsTr("Delete the folders of past weeks")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                wrapMode: Text.WordWrap
            }

            StudioSwitch {
                checked: ApplicationController.watchedFolder.deletePastFolders
                onToggled: ApplicationController.watchedFolder.deletePastFolders = checked
            }
        }
    }
}
