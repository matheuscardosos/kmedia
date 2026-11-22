pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// Public talk media from the congregation's S-34mp: pick the file once, then pick a talk.
Popup {
    id: root

    property string sectionId: ""
    readonly property var filteredTalks: {
        const query = filterField.text.trim().toLowerCase()
        const talks = ApplicationController.publicTalks.talks
        if (query.length === 0) {
            return talks
        }
        return talks.filter(talk => talk.title.toLowerCase().indexOf(query) >= 0
                                    || ("" + talk.number) === query)
    }

    function openFor(targetSectionId) {
        root.sectionId = targetSectionId
        filterField.text = ""
        root.open()
    }

    width: Math.min(460, parent ? parent.width - 24 : 460)
    height: Math.min(560, parent ? parent.height - 48 : 560)
    padding: 14
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay

    Overlay.modal: Rectangle {
        color: Theme.scrimOverlay
    }

    background: StudioCard {
        surfaceColor: Theme.cardElevated
        strokeColor: Theme.borderStrong
        cornerRadius: Theme.radiusLarge
    }

    FileDialog {
        id: s34Dialog

        title: qsTr("Select the S-34mp file")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("JW Library publication (*.jwpub)")]
        onAccepted: ApplicationController.publicTalks.importFile(selectedFile)
    }

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioIcon {
                name: "section-public-talk"
                category: "custom"
                size: 18
                color: Theme.accentPrimary
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Public talk media")
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: ApplicationController.publicTalks.busy ? qsTr("Opening the S-34mp...")
                    : (ApplicationController.publicTalks.available ? ApplicationController.publicTalks.label
                                                                   : qsTr("Select the S-34mp file"))
                color: ApplicationController.publicTalks.available ? Theme.textSecondary : Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                elide: Text.ElideRight
            }

            StudioButton {
                variant: ApplicationController.publicTalks.available ? "secondary" : "primary"
                iconName: "folder-open"
                text: ApplicationController.publicTalks.available ? qsTr("Replace") : qsTr("Browse")
                enabled: !ApplicationController.publicTalks.busy
                Layout.preferredHeight: 32
                onClicked: s34Dialog.open()
            }
        }

        TextField {
            id: filterField

            visible: ApplicationController.publicTalks.available
            Layout.fillWidth: true
            placeholderText: qsTr("Talk number or title")
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
            id: talkList

            visible: ApplicationController.publicTalks.available
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            model: root.filteredTalks

            ScrollBar.vertical: StudioScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: talkRow

                required property var modelData

                width: talkList.width
                height: 40
                radius: Theme.radiusSmall
                color: talkMouse.containsMouse ? Theme.cardHover : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 24
                        radius: Theme.radiusSmall
                        visible: talkRow.modelData.number > 0
                        color: Theme.accentSoft

                        Text {
                            anchors.centerIn: parent
                            text: "" + talkRow.modelData.number
                            color: Theme.accentPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: talkRow.modelData.title
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: talkMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        ApplicationController.publicTalks.addTalk(talkRow.modelData.documentId, root.sectionId)
                        root.close()
                    }
                }
            }
        }

        Item {
            visible: !ApplicationController.publicTalks.available
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
