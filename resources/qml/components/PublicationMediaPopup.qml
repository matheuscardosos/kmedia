pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// The media of one document of a publication: from JW.org by symbol and issue, or from a .jwpub
// added from the computer.
Popup {
    id: root

    property string sectionId: ""
    property bool fromFile: false
    readonly property var filteredDocuments: {
        const query = filterField.text.trim().toLowerCase()
        const documents = ApplicationController.publicationMedia.documents
        if (query.length === 0) {
            return documents
        }
        return documents.filter(document => document.title.toLowerCase().indexOf(query) >= 0)
    }

    function openForJwOrg(targetSectionId) {
        root.sectionId = targetSectionId
        root.fromFile = false
        ApplicationController.publicationMedia.clear()
        filterField.text = ""
        root.open()
        symbolField.forceActiveFocus()
    }

    function openForFile(fileUrl, targetSectionId) {
        root.sectionId = targetSectionId
        root.fromFile = true
        filterField.text = ""
        ApplicationController.publicationMedia.openFile(fileUrl)
        root.open()
    }

    function openPublication() {
        if (symbolField.text.trim().length > 0) {
            ApplicationController.publicationMedia.openPublication(symbolField.text, issueField.text)
        }
    }

    width: Math.min(480, parent ? parent.width - 24 : 480)
    height: Math.min(580, parent ? parent.height - 48 : 580)
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

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioIcon {
                name: "file-text"
                size: 18
                color: Theme.accentPrimary
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Publication media")
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
            visible: !root.fromFile
            Layout.fillWidth: true
            spacing: 6

            TextField {
                id: symbolField

                Layout.fillWidth: true
                placeholderText: qsTr("Symbol, for example lff")
                color: Theme.textPrimary
                placeholderTextColor: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                onAccepted: root.openPublication()
                background: StudioCard {
                    surfaceColor: Theme.windowBase
                    cornerRadius: Theme.radiusSmall
                }
            }

            TextField {
                id: issueField

                Layout.preferredWidth: 130
                placeholderText: qsTr("Issue (2026-11)")
                color: Theme.textPrimary
                placeholderTextColor: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                onAccepted: root.openPublication()
                background: StudioCard {
                    surfaceColor: Theme.windowBase
                    cornerRadius: Theme.radiusSmall
                }
            }

            StudioButton {
                variant: "primary"
                text: qsTr("Open")
                enabled: symbolField.text.trim().length > 0 && !ApplicationController.publicationMedia.busy
                Layout.preferredHeight: 34
                onClicked: root.openPublication()
            }
        }

        Text {
            visible: ApplicationController.publicationMedia.busy
                     || ApplicationController.publicationMedia.title.length > 0
            Layout.fillWidth: true
            text: ApplicationController.publicationMedia.busy ? qsTr("Opening the publication...")
                                                               : ApplicationController.publicationMedia.title
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            elide: Text.ElideRight
        }

        TextField {
            id: filterField

            visible: ApplicationController.publicationMedia.documents.length > 0
            Layout.fillWidth: true
            placeholderText: qsTr("Search")
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
            id: documentList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            model: root.filteredDocuments

            ScrollBar.vertical: StudioScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: documentRow

                required property var modelData

                width: documentList.width
                height: 40
                radius: Theme.radiusSmall
                color: documentMouse.containsMouse ? Theme.cardHover : "transparent"

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    verticalAlignment: Text.AlignVCenter
                    text: documentRow.modelData.title
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    elide: Text.ElideRight
                }

                MouseArea {
                    id: documentMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        ApplicationController.publicationMedia.addDocument(documentRow.modelData.documentId,
                                                                           root.sectionId)
                        root.close()
                    }
                }
            }
        }
    }
}
