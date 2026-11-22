pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// A folder path that can be typed or picked with the system folder dialog.
RowLayout {
    id: root

    property string path: ""
    property string dialogTitle: ""
    signal folderChosen(string path)

    spacing: 6

    TextField {
        id: pathField

        Layout.fillWidth: true
        text: root.path
        placeholderText: qsTr("Choose a folder")
        color: Theme.textPrimary
        placeholderTextColor: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
        onEditingFinished: {
            if (pathField.text.trim() !== root.path) {
                root.folderChosen(pathField.text.trim())
            }
        }
        background: StudioCard {
            surfaceColor: Theme.windowBase
            cornerRadius: Theme.radiusSmall
        }
    }

    StudioButton {
        variant: "secondary"
        iconName: "folder-open"
        text: qsTr("Browse")
        Layout.preferredHeight: 34
        onClicked: folderDialog.open()
    }

    FolderDialog {
        id: folderDialog

        title: root.dialogTitle
        onAccepted: root.folderChosen(folderDialog.selectedFolder.toString())
    }
}
