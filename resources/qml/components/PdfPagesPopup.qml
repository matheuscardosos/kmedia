pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// A PDF with several pages: pick the pages to add as pictures, or add the whole document.
Popup {
    id: root

    property string sectionId: ""
    property var picked: []

    function openFor(fileUrl, targetSectionId) {
        root.sectionId = targetSectionId
        root.picked = []
        ApplicationController.pdfPages.open(fileUrl)
        root.open()
    }

    function togglePage(page) {
        const next = root.picked.slice()
        const at = next.indexOf(page)
        if (at >= 0) {
            next.splice(at, 1)
        } else {
            next.push(page)
        }
        next.sort((a, b) => a - b)
        root.picked = next
    }

    width: Math.min(820, parent ? parent.width - 24 : 820)
    height: Math.min(640, parent ? parent.height - 48 : 640)
    padding: 14
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    onClosed: ApplicationController.pdfPages.close()

    Overlay.modal: Rectangle {
        color: Theme.scrimOverlay
    }

    background: StudioCard {
        surfaceColor: Theme.cardElevated
        strokeColor: Theme.borderStrong
        cornerRadius: Theme.radiusLarge
    }

    Connections {
        target: ApplicationController.pdfPages

        function onPagesAdded(count) {
            if (count > 0) {
                root.close()
            } else {
                ApplicationController.showToast(qsTr("These pages could not be added"), "warning")
            }
        }
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
                text: ApplicationController.pdfPages.fileName
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
            }

            StudioButton {
                variant: "ghost"
                text: root.picked.length === ApplicationController.pdfPages.pageCount
                      ? qsTr("Select none") : qsTr("Select all")
                onClicked: {
                    if (root.picked.length === ApplicationController.pdfPages.pageCount) {
                        root.picked = []
                    } else {
                        const all = []
                        for (let page = 1; page <= ApplicationController.pdfPages.pageCount; ++page) {
                            all.push(page)
                        }
                        root.picked = all
                    }
                }
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

        GridView {
            id: pageGrid

            readonly property int columns: Math.max(1, Math.floor(pageGrid.width / 150))

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: Math.floor(pageGrid.width / pageGrid.columns)
            cellHeight: Math.round(pageGrid.cellWidth * 1.3) + 24
            boundsBehavior: Flickable.StopAtBounds
            model: ApplicationController.pdfPages.pages

            ScrollBar.vertical: StudioScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Item {
                id: pageCell

                required property var modelData
                readonly property bool chosen: root.picked.indexOf(pageCell.modelData.page) >= 0

                width: pageGrid.cellWidth
                height: pageGrid.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    radius: Theme.radiusMedium
                    color: pageCell.chosen ? Theme.accentSoft : Theme.cardSurface
                    border.width: pageCell.chosen ? 2 : 1
                    border.color: pageCell.chosen ? Theme.accentPrimary : Theme.borderHairline

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4

                        Image {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            source: pageCell.modelData.thumbUrl
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: String(pageCell.modelData.page)
                            horizontalAlignment: Text.AlignHCenter
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.togglePage(pageCell.modelData.page)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioButton {
                variant: "secondary"
                text: qsTr("Add the whole document")
                onClicked: ApplicationController.pdfPages.addDocument(root.sectionId)
            }

            Item {
                Layout.fillWidth: true
            }

            StudioButton {
                variant: "primary"
                iconName: "image"
                text: qsTr("Add %n page(s) as pictures", "", root.picked.length)
                enabled: root.picked.length > 0 && !ApplicationController.pdfPages.busy
                onClicked: ApplicationController.pdfPages.addPages(root.picked, root.sectionId)
            }
        }
    }
}
