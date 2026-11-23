pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// The pictures and videos of the Study Bible: a book, a chapter, then the media to add.
Popup {
    id: root

    property string sectionId: ""
    property int book: 0
    property string bookName: ""
    property var chapters: []
    property int chapter: 0
    property var media: []
    property var picked: []

    function openFor(targetSectionId) {
        root.sectionId = targetSectionId
        root.book = 0
        root.chapter = 0
        root.picked = []
        ApplicationController.studyBible.open()
        root.open()
    }

    function openChapter(chapterNumber) {
        root.chapter = chapterNumber
        root.picked = []
        root.media = ApplicationController.studyBible.chapterMedia(root.book, chapterNumber)
    }

    function togglePicked(multimediaId) {
        const next = root.picked.slice()
        const at = next.indexOf(multimediaId)
        if (at >= 0) {
            next.splice(at, 1)
        } else {
            next.push(multimediaId)
        }
        root.picked = next
    }

    width: Math.min(820, parent ? parent.width - 24 : 820)
    height: Math.min(640, parent ? parent.height - 48 : 640)
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

    Connections {
        target: ApplicationController.studyBible

        function onMediaAdded(count) {
            if (count > 0) {
                root.close()
            } else {
                ApplicationController.showToast(qsTr("This media could not be added"), "warning")
            }
        }

        function onOpenFailed() {
            ApplicationController.showToast(qsTr("The Study Bible could not be downloaded"), "warning")
        }
    }

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioButton {
                visible: root.book > 0
                variant: "ghost"
                iconName: "chevron-left"
                leftPadding: 0
                rightPadding: 0
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                onClicked: {
                    if (root.chapter > 0) {
                        root.chapter = 0
                        root.picked = []
                    } else {
                        root.book = 0
                    }
                }
            }

            StudioIcon {
                visible: root.book === 0
                name: "file-text"
                size: 18
                color: Theme.accentPrimary
            }

            Text {
                Layout.fillWidth: true
                text: root.book === 0 ? qsTr("Study Bible")
                    : (root.chapter === 0 ? root.bookName : root.bookName + " " + root.chapter)
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
            visible: ApplicationController.studyBible.busy
            text: qsTr("Downloading the Study Bible...")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
        }

        ScrollView {
            visible: root.chapter === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            Flow {
                width: parent.width
                spacing: 6

                Repeater {
                    model: root.book === 0 ? ApplicationController.studyBible.books : []

                    delegate: StudioButton {
                        id: bookButton

                        required property var modelData

                        variant: "secondary"
                        text: bookButton.modelData.name
                        implicitWidth: 150
                        implicitHeight: 34
                        onClicked: {
                            root.book = bookButton.modelData.number
                            root.bookName = bookButton.modelData.name
                            root.chapters = bookButton.modelData.chapters
                        }
                    }
                }

                Repeater {
                    model: root.book > 0 ? root.chapters : []

                    delegate: StudioButton {
                        required property int modelData

                        variant: "secondary"
                        text: String(modelData)
                        leftPadding: 0
                        rightPadding: 0
                        implicitWidth: 44
                        implicitHeight: 34
                        onClicked: root.openChapter(modelData)
                    }
                }
            }
        }

        GridView {
            id: mediaGrid

            readonly property int columns: Math.max(1, Math.floor(mediaGrid.width / 190))

            visible: root.chapter > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: Math.floor(mediaGrid.width / mediaGrid.columns)
            cellHeight: Math.round((mediaGrid.cellWidth - 10) * 9 / 16) + 52
            boundsBehavior: Flickable.StopAtBounds
            model: root.media

            ScrollBar.vertical: StudioScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Item {
                id: mediaCell

                required property var modelData
                readonly property bool chosen: root.picked.indexOf(mediaCell.modelData.multimediaId) >= 0

                width: mediaGrid.cellWidth
                height: mediaGrid.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    radius: Theme.radiusMedium
                    color: mediaCell.chosen ? Theme.accentSoft : Theme.cardSurface
                    border.width: 1
                    border.color: mediaCell.chosen ? Theme.accentPrimary : Theme.borderHairline

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: Theme.radiusSmall
                            color: Theme.windowBase
                            clip: true

                            Image {
                                anchors.fill: parent
                                visible: mediaCell.modelData.previewUrl.length > 0
                                source: mediaCell.modelData.previewUrl
                                sourceSize.width: 320
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }

                            StudioIcon {
                                anchors.centerIn: parent
                                visible: mediaCell.modelData.isVideo
                                name: "film"
                                size: 22
                                color: "#FFFFFF"
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: mediaCell.modelData.verse + "  " + mediaCell.modelData.title
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.togglePicked(mediaCell.modelData.multimediaId)
                    }
                }
            }
        }

        RowLayout {
            visible: root.chapter > 0
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            StudioButton {
                variant: "primary"
                iconName: "plus"
                text: qsTr("Add %n", "", root.picked.length)
                enabled: root.picked.length > 0
                onClicked: ApplicationController.studyBible.addMedia(root.book, root.chapter,
                                                                     root.picked, root.sectionId)
            }
        }
    }
}
