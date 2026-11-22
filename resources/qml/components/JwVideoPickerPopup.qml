pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// The JW.org video library, newest first, with a search over the titles.
Popup {
    id: root

    property string sectionId: ""
    readonly property var filteredVideos: {
        const query = filterField.text.trim().toLowerCase()
        const videos = ApplicationController.videos.videos
        if (query.length === 0) {
            return videos
        }
        return videos.filter(video => video.title.toLowerCase().indexOf(query) >= 0)
    }

    function openFor(targetSectionId) {
        root.sectionId = targetSectionId
        filterField.text = ""
        ApplicationController.videos.load()
        root.open()
        filterField.forceActiveFocus()
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

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioIcon {
                name: "film"
                size: 18
                color: Theme.accentPrimary
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Videos")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            StudioButton {
                variant: "ghost"
                iconName: "refresh-cw"
                enabled: !ApplicationController.videos.busy
                leftPadding: 0
                rightPadding: 0
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                onClicked: ApplicationController.videos.reload()
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

        TextField {
            id: filterField

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

        GridView {
            id: videoGrid

            readonly property int columns: Math.max(1, Math.floor(videoGrid.width / 190))

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: Math.floor(videoGrid.width / videoGrid.columns)
            cellHeight: Math.round((videoGrid.cellWidth - 10) * 9 / 16) + 56
            boundsBehavior: Flickable.StopAtBounds
            model: root.filteredVideos

            ScrollBar.vertical: StudioScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Item {
                id: videoCell

                required property var modelData

                width: videoGrid.cellWidth
                height: videoGrid.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    radius: Theme.radiusMedium
                    color: videoMouse.containsMouse ? Theme.cardHover : "transparent"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 4

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.round(width * 9 / 16)
                            radius: Theme.radiusSmall
                            color: Theme.cardSurface
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: videoCell.modelData.imageUrl
                                sourceSize.width: 320
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                smooth: true
                            }

                            Rectangle {
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                width: durationLabel.implicitWidth + 10
                                height: 18
                                radius: 9
                                color: "#B3000000"

                                Text {
                                    id: durationLabel

                                    anchors.centerIn: parent
                                    text: videoCell.modelData.duration
                                    color: "#FFFFFF"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: videoCell.modelData.title
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: videoMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            ApplicationController.videos.addVideo(videoCell.modelData.key, root.sectionId)
                            root.close()
                        }
                    }
                }
            }
        }

        Text {
            visible: ApplicationController.videos.busy
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Loading videos...")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
        }
    }
}
