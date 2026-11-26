pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// Modal popup to create, switch, rename, and delete congregation profiles.
Popup {
    id: root

    property string editingProfileId: ""
    property string confirmDeleteId: ""
    property string newMediaLanguage: "E"

    function languageLabel(code) {
        const languages = ApplicationController.catalog.languages
        for (let i = 0; i < languages.length; ++i) {
            if (languages[i].code === code) {
                return languages[i].label
            }
        }
        return code
    }

    function createCongregation() {
        const name = newNameField.text.trim()
        if (name.length === 0) {
            return
        }
        const createdId = ApplicationController.createProfile(name, root.newMediaLanguage, true)
        if (createdId.length > 0) {
            newNameField.text = ""
            root.editingProfileId = ""
            root.confirmDeleteId = ""
            ApplicationController.showToast(qsTr("Created congregation: %1").arg(name), "success")
        } else {
            ApplicationController.showToast(qsTr("Could not create congregation"), "warning")
        }
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(520, Overlay.overlay ? Overlay.overlay.width - 32 : 520)
    modal: true
    focus: true
    padding: 16
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle {
        color: Theme.scrimOverlay
    }

    onOpened: {
        root.editingProfileId = ""
        root.confirmDeleteId = ""
        newNameField.text = ""
        root.newMediaLanguage = ApplicationController.catalog.selectedLanguageCode.length > 0
            ? ApplicationController.catalog.selectedLanguageCode
            : "E"
    }

    background: StudioCard {
        surfaceColor: Theme.cardElevated
        strokeColor: Theme.borderStrong
        cornerRadius: Theme.radiusLarge
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingMd

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioIcon {
                name: "users"
                size: 18
                color: Theme.accentPrimary
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Manage Congregations")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            StudioButton {
                variant: "ghost"
                iconName: "x"
                leftPadding: 0
                rightPadding: 0
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                onClicked: root.close()
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Each congregation keeps its own schedule, media language, and downloaded meeting files.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            wrapMode: Text.WordWrap
        }

        ListView {
            id: profileList

            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(248, Math.max(58, count * 56))
            clip: true
            spacing: 6
            model: ApplicationController.profiles

            ScrollBar.vertical: StudioScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: profileRow

                required property var modelData
                readonly property bool isCurrent: profileRow.modelData.id === ApplicationController.activeProfileId
                readonly property bool isEditing: root.editingProfileId === profileRow.modelData.id
                readonly property bool isConfirmingDelete: root.confirmDeleteId === profileRow.modelData.id

                function commitRename() {
                    const renamed = editNameField.text.trim()
                    if (renamed.length === 0) {
                        return
                    }
                    if (ApplicationController.renameProfile(profileRow.modelData.id, renamed)) {
                        root.editingProfileId = ""
                        ApplicationController.showToast(qsTr("Congregation renamed to %1").arg(renamed), "success")
                    } else {
                        ApplicationController.showToast(qsTr("Could not rename congregation"), "warning")
                    }
                }

                width: profileList.width
                height: 50
                radius: Theme.radiusSmall
                color: profileRow.isCurrent ? Theme.accentSoft : (rowMouse.containsMouse ? Theme.cardHover : Theme.cardSurface)
                border.width: 1
                border.color: profileRow.isCurrent ? Theme.accentPrimary : Theme.borderHairline

                MouseArea {
                    id: rowMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !profileRow.isEditing && !profileRow.isConfirmingDelete && !profileRow.isCurrent
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        if (ApplicationController.switchProfile(profileRow.modelData.id)) {
                            ApplicationController.showToast(qsTr("Switched congregation to %1").arg(profileRow.modelData.name), "info")
                        }
                    }
                }

                RowLayout {
                    visible: !profileRow.isEditing && !profileRow.isConfirmingDelete
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 8

                    StudioIcon {
                        visible: profileRow.isCurrent
                        name: "check"
                        size: 15
                        color: Theme.accentPrimary
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            Layout.fillWidth: true
                            text: profileRow.modelData.name
                            color: profileRow.isCurrent ? Theme.accentPrimary : Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: profileRow.isCurrent ? Font.DemiBold : Font.Medium
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.languageLabel(profileRow.modelData.mediaLanguage)
                                + " (" + profileRow.modelData.mediaLanguage + ") - "
                                + profileRow.modelData.midweekSchedule + " / "
                                + profileRow.modelData.weekendSchedule
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            elide: Text.ElideRight
                        }
                    }

                    StudioBadge {
                        visible: profileRow.isCurrent
                        text: qsTr("Active")
                        badgeColor: Theme.accentSoft
                        textColor: Theme.accentPrimary
                        hasBorder: false
                    }

                    StudioButton {
                        variant: "ghost"
                        text: qsTr("Rename")
                        Layout.preferredHeight: 28
                        onClicked: {
                            root.confirmDeleteId = ""
                            root.editingProfileId = profileRow.modelData.id
                            editNameField.text = profileRow.modelData.name
                            editNameField.forceActiveFocus()
                        }
                    }

                    StudioButton {
                        variant: "ghost"
                        iconName: "trash"
                        leftPadding: 0
                        rightPadding: 0
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        enabled: ApplicationController.profiles.length > 1
                        ToolTip.visible: hovered
                        ToolTip.text: ApplicationController.profiles.length > 1
                            ? qsTr("Delete congregation")
                            : qsTr("At least one congregation is required")
                        onClicked: {
                            root.editingProfileId = ""
                            root.confirmDeleteId = profileRow.modelData.id
                        }
                    }
                }

                RowLayout {
                    visible: profileRow.isEditing
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    TextField {
                        id: editNameField

                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        placeholderText: qsTr("Congregation name")
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        onAccepted: profileRow.commitRename()
                        background: StudioCard {
                            surfaceColor: Theme.windowBase
                            cornerRadius: Theme.radiusSmall
                        }
                    }

                    StudioButton {
                        variant: "primary"
                        iconName: "check"
                        text: qsTr("Save")
                        Layout.preferredHeight: 32
                        enabled: editNameField.text.trim().length > 0
                        onClicked: profileRow.commitRename()
                    }

                    StudioButton {
                        variant: "ghost"
                        iconName: "x"
                        leftPadding: 0
                        rightPadding: 0
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 32
                        onClicked: root.editingProfileId = ""
                    }
                }

                RowLayout {
                    visible: profileRow.isConfirmingDelete
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Delete \"%1\" and its local media?").arg(profileRow.modelData.name)
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        elide: Text.ElideRight
                    }

                    StudioButton {
                        variant: "primary"
                        text: qsTr("Delete")
                        Layout.preferredHeight: 30
                        onClicked: {
                            const name = profileRow.modelData.name
                            if (ApplicationController.deleteProfile(profileRow.modelData.id)) {
                                root.confirmDeleteId = ""
                                ApplicationController.showToast(qsTr("Removed congregation: %1").arg(name), "info")
                            }
                        }
                    }

                    StudioButton {
                        variant: "ghost"
                        text: qsTr("Cancel")
                        Layout.preferredHeight: 30
                        onClicked: root.confirmDeleteId = ""
                    }
                }
            }
        }

        StudioCard {
            Layout.fillWidth: true
            implicitHeight: addColumn.implicitHeight + 20
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: addColumn

                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text {
                    text: qsTr("Add Congregation")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    TextField {
                        id: newNameField

                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        placeholderText: qsTr("Congregation name")
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        onAccepted: root.createCongregation()
                        background: StudioCard {
                            surfaceColor: Theme.windowBase
                            cornerRadius: Theme.radiusSmall
                        }
                    }

                    StudioButton {
                        id: newLanguageButton

                        variant: "secondary"
                        iconName: "globe"
                        text: root.languageLabel(root.newMediaLanguage)
                        Layout.preferredHeight: 34
                        onClicked: newLanguagePopup.open()
                    }

                    StudioButton {
                        variant: "primary"
                        iconName: "plus"
                        text: qsTr("Add")
                        Layout.preferredHeight: 34
                        enabled: newNameField.text.trim().length > 0
                        onClicked: root.createCongregation()
                    }
                }
            }
        }
    }

    Popup {
        id: newLanguagePopup

        width: 280
        padding: 8
        focus: true
        modal: true
        anchors.centerIn: Overlay.overlay
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Overlay.modal: Rectangle {
            color: Theme.scrimOverlay
        }

        onAboutToShow: newLanguageFilter.text = ""

        background: StudioCard {
            surfaceColor: Theme.cardElevated
            strokeColor: Theme.borderStrong
        }

        contentItem: ColumnLayout {
            spacing: 8

            TextField {
                id: newLanguageFilter

                Layout.fillWidth: true
                placeholderText: qsTr("Filter languages...")
                color: Theme.textPrimary
                placeholderTextColor: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                background: StudioCard {
                    surfaceColor: Theme.windowBase
                    cornerRadius: Theme.radiusSmall
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                clip: true
                model: {
                    const languages = ApplicationController.catalog.languages
                    const query = newLanguageFilter.text.trim().toLowerCase()
                    if (query.length === 0) {
                        return languages
                    }
                    return languages.filter(language => language.label.toLowerCase().indexOf(query) >= 0
                                                        || language.code.toLowerCase() === query)
                }

                ScrollBar.vertical: StudioScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Rectangle {
                    id: langItem

                    required property var modelData
                    readonly property bool selected: langItem.modelData.code === root.newMediaLanguage

                    width: ListView.view ? ListView.view.width : 260
                    height: 32
                    radius: Theme.radiusSmall
                    color: langMouse.containsMouse ? Theme.cardHover
                                                   : (langItem.selected ? Theme.accentSoft : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: langItem.modelData.label + " (" + langItem.modelData.code + ")"
                            color: langItem.selected ? Theme.accentPrimary : Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: langItem.selected ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }

                        StudioIcon {
                            visible: langItem.selected
                            name: "check"
                            size: 14
                            color: Theme.accentPrimary
                        }
                    }

                    MouseArea {
                        id: langMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.newMediaLanguage = langItem.modelData.code
                            newLanguagePopup.close()
                        }
                    }
                }
            }
        }
    }
}
