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
    property bool gridViewMode: true
    property int filterKind: 0
    signal addFilesRequested()

    readonly property var filterOptions: [
        { kind: 0, label: qsTr("All"), icon: "layers" },
        { kind: 1, label: qsTr("Video"), icon: "film" },
        { kind: 2, label: qsTr("Audio"), icon: "audio-lines" },
        { kind: 3, label: qsTr("Image"), icon: "image" },
        { kind: 4, label: qsTr("PDF"), icon: "file-text" }
    ]

    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical: StudioScrollBar {
        policy: ScrollBar.AsNeeded
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacingMd

        // Compact icon-driven media library toolbar
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: toolbarCol.implicitHeight + 24
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: toolbarCol

                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StudioIcon {
                        name: "jwpub"
                        category: "custom"
                        size: 18
                        color: Theme.accentPrimary
                    }

                    Text {
                        text: qsTr("Media Library")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLg
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    StudioBadge {
                        visible: !root.compactMode
                        text: qsTr("%1 watched").arg(ApplicationController.cloudWatcher.watchedFolders.length)
                        showDot: ApplicationController.cloudWatcher.watchedFolders.length > 0
                        dotColor: Theme.statusReady
                        badgeColor: Theme.cardSurface
                        textColor: Theme.textSecondary
                        hasBorder: true

                        MouseArea {
                            id: cloudHover

                            anchors.fill: parent
                            hoverEnabled: true
                            ToolTip.visible: cloudHover.containsMouse
                            ToolTip.text: qsTr("Watched Dropbox, OneDrive, iCloud Drive, and Nextcloud folders auto-import media.")
                        }
                    }

                    StudioBadge {
                        text: "" + ApplicationController.playlist.count
                        badgeColor: Theme.cardSurface
                        textColor: Theme.textSecondary
                        hasBorder: true
                    }

                    StudioButton {
                        id: gridModeBtn

                        variant: root.gridViewMode ? "primary" : "secondary"
                        iconName: "layout-grid"
                        leftPadding: 0
                        rightPadding: 0
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        ToolTip.visible: gridModeBtn.hovered
                        ToolTip.text: qsTr("Thumbnail grid view")
                        onClicked: root.gridViewMode = true
                    }

                    StudioButton {
                        id: listModeBtn

                        variant: !root.gridViewMode ? "primary" : "secondary"
                        iconName: "list"
                        leftPadding: 0
                        rightPadding: 0
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        ToolTip.visible: listModeBtn.hovered
                        ToolTip.text: qsTr("Compact list view")
                        onClicked: root.gridViewMode = false
                    }

                    StudioButton {
                        variant: "primary"
                        iconName: "plus"
                        text: root.compactMode ? qsTr("Add") : qsTr("Add Media")
                        Layout.preferredHeight: 32
                        onClicked: root.addFilesRequested()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StudioCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        surfaceColor: Theme.windowBase
                        cornerRadius: Theme.radiusSmall

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            StudioIcon {
                                name: "search"
                                size: 14
                                color: Theme.textMuted
                            }

                            TextField {
                                id: searchField

                                Layout.fillWidth: true
                                placeholderText: qsTr("Search media...")
                                color: Theme.textPrimary
                                placeholderTextColor: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                leftPadding: 0
                                rightPadding: 0
                                background: Item {}
                            }
                        }
                    }

                    Repeater {
                        model: root.filterOptions

                        delegate: StudioButton {
                            id: filterBtn

                            required property var modelData

                            variant: root.filterKind === filterBtn.modelData.kind ? "primary" : "secondary"
                            iconName: filterBtn.modelData.icon
                            text: root.compactMode ? "" : filterBtn.modelData.label
                            leftPadding: root.compactMode ? 0 : 10
                            rightPadding: root.compactMode ? 0 : 10
                            Layout.preferredWidth: root.compactMode ? 32 : filterBtn.implicitWidth
                            Layout.preferredHeight: 32
                            ToolTip.visible: filterBtn.hovered
                            ToolTip.text: filterBtn.modelData.label
                            onClicked: root.filterKind = filterBtn.modelData.kind
                        }
                    }
                }
            }
        }

        // Visual thumbnail grid and list browser
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: galleryCol.implicitHeight + 24
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: galleryCol

                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                // Empty library state
                ColumnLayout {
                    visible: ApplicationController.playlist.count === 0
                    Layout.fillWidth: true
                    Layout.topMargin: 24
                    Layout.bottomMargin: 24
                    spacing: 10

                    StudioIcon {
                        Layout.alignment: Qt.AlignHCenter
                        name: "folder-plus"
                        size: 28
                        color: Theme.accentPrimary
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("No media in library")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeMd
                        font.weight: Font.DemiBold
                    }

                    StudioButton {
                        Layout.alignment: Qt.AlignHCenter
                        variant: "primary"
                        iconName: "plus"
                        text: qsTr("Add Media")
                        onClicked: root.addFilesRequested()
                    }
                }

                // Visual 16:9 thumbnail grid mode
                Flow {
                    id: gridFlow

                    readonly property int columns: Math.max(1, Math.floor((gridFlow.width + 10) / 196))
                    readonly property real cardWidth: Math.max(120, Math.floor((gridFlow.width - (gridFlow.columns - 1) * 10) / gridFlow.columns))

                    visible: root.gridViewMode && ApplicationController.playlist.count > 0
                    Layout.fillWidth: true
                    spacing: 10

                    Repeater {
                        model: ApplicationController.playlist

                        delegate: Rectangle {
                            id: gridCard

                            required property int index
                            required property string title
                            required property string subtitle
                            required property string thumbUrl
                            required property string kindLabel
                            required property int mediaKind
                            required property string formatLabel
                            required property string resolutionLabel
                            required property string formattedDuration
                            required property bool isActive

                            readonly property string queryText: searchField.text.trim().toLowerCase()
                            readonly property bool matchesFilter: (root.filterKind === 0 || gridCard.mediaKind === root.filterKind)
                                && (gridCard.queryText.length === 0
                                    || gridCard.title.toLowerCase().indexOf(gridCard.queryText) >= 0
                                    || gridCard.formatLabel.toLowerCase().indexOf(gridCard.queryText) >= 0)
                            readonly property string typeIconName: {
                                if (gridCard.mediaKind === 1) return "film"
                                if (gridCard.mediaKind === 2) return "audio-lines"
                                if (gridCard.mediaKind === 4) return "file-text"
                                return "image"
                            }

                            visible: gridCard.matchesFilter
                            width: gridFlow.cardWidth
                            height: 158
                            radius: Theme.radiusMedium
                            color: gridCard.isActive
                                   ? Theme.rowSelected
                                   : (gridMouse.containsMouse ? Theme.cardHover : Theme.cardSurface)
                            border.width: gridCard.isActive ? 2 : 1
                            border.color: gridCard.isActive ? Theme.accentPrimary : Theme.borderHairline

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 6

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 96
                                    radius: Theme.radiusSmall
                                    color: Theme.windowBase
                                    clip: true

                                    Image {
                                        anchors.fill: parent
                                        source: gridCard.thumbUrl
                                        visible: gridCard.thumbUrl.length > 0
                                        sourceSize.width: 320
                                        sourceSize.height: 180
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        smooth: true
                                        mipmap: true
                                    }

                                    StudioIcon {
                                        anchors.centerIn: parent
                                        visible: gridCard.thumbUrl.length === 0
                                        name: gridCard.typeIconName
                                        size: 26
                                        color: Theme.textSecondary
                                    }

                                    Rectangle {
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.margins: 6
                                        width: kindText.implicitWidth + 10
                                        height: 18
                                        radius: 4
                                        color: "#B80D0F12"

                                        Text {
                                            id: kindText

                                            anchors.centerIn: parent
                                            text: gridCard.kindLabel
                                            color: "#FFFFFF"
                                            font.family: Theme.fontFamily
                                            font.pixelSize: 10
                                            font.weight: Font.Bold
                                        }
                                    }

                                    Rectangle {
                                        visible: gridCard.formattedDuration.length > 0 && gridCard.formattedDuration !== "--:--"
                                        anchors.bottom: parent.bottom
                                        anchors.right: parent.right
                                        anchors.margins: 6
                                        width: durText.implicitWidth + 10
                                        height: 18
                                        radius: 4
                                        color: "#B80D0F12"

                                        Text {
                                            id: durText

                                            anchors.centerIn: parent
                                            text: gridCard.formattedDuration
                                            color: "#FFFFFF"
                                            font.family: Theme.fontFamily
                                            font.pixelSize: 10
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: gridCard.title
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Text {
                                        text: gridCard.formatLabel
                                        color: Theme.textMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeXs
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    StudioIcon {
                                        visible: gridCard.isActive
                                        name: "play"
                                        size: 11
                                        color: Theme.accentPrimary
                                    }
                                }
                            }

                            MouseArea {
                                id: gridMouse

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: ApplicationController.playlist.activateIndex(gridCard.index)
                                onDoubleClicked: {
                                    ApplicationController.playlist.activateIndex(gridCard.index)
                                    ApplicationController.playback.play()
                                }
                            }
                        }
                    }
                }

                // Compact list mode with miniature thumbnails
                ColumnLayout {
                    visible: !root.gridViewMode && ApplicationController.playlist.count > 0
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: ApplicationController.playlist

                        delegate: Rectangle {
                            id: libRow

                            required property int index
                            required property string title
                            required property string thumbUrl
                            required property string kindLabel
                            required property int mediaKind
                            required property string formatLabel
                            required property string resolutionLabel
                            required property string formattedDuration
                            required property bool isActive

                            readonly property string queryText: searchField.text.trim().toLowerCase()
                            readonly property bool matchesFilter: (root.filterKind === 0 || libRow.mediaKind === root.filterKind)
                                && (libRow.queryText.length === 0
                                    || libRow.title.toLowerCase().indexOf(libRow.queryText) >= 0
                                    || libRow.formatLabel.toLowerCase().indexOf(libRow.queryText) >= 0)
                            readonly property string typeIconName: {
                                if (libRow.mediaKind === 1) return "film"
                                if (libRow.mediaKind === 2) return "audio-lines"
                                if (libRow.mediaKind === 4) return "file-text"
                                return "image"
                            }

                            visible: libRow.matchesFilter
                            Layout.fillWidth: true
                            Layout.preferredHeight: 46
                            radius: Theme.radiusSmall
                            color: libRow.isActive
                                   ? Theme.rowSelected
                                   : (rowMouse.containsMouse ? Theme.cardHover : Theme.cardSurface)
                            border.width: 1
                            border.color: libRow.isActive ? Theme.accentPrimary : Theme.borderHairline

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 10
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 52
                                    Layout.preferredHeight: 30
                                    radius: 4
                                    color: Theme.windowBase
                                    clip: true

                                    Image {
                                        anchors.fill: parent
                                        source: libRow.thumbUrl
                                        visible: libRow.thumbUrl.length > 0
                                        sourceSize.width: 104
                                        sourceSize.height: 60
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        smooth: true
                                    }

                                    StudioIcon {
                                        anchors.centerIn: parent
                                        visible: libRow.thumbUrl.length === 0
                                        name: libRow.typeIconName
                                        size: 14
                                        color: Theme.textSecondary
                                    }
                                }

                                Text {
                                    text: libRow.title
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    font.weight: libRow.isActive ? Font.DemiBold : Font.Medium
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                StudioBadge {
                                    visible: !root.compactMode
                                    text: libRow.formatLabel
                                    badgeColor: Theme.panelSurface
                                    textColor: Theme.textSecondary
                                    hasBorder: true
                                }

                                Text {
                                    text: libRow.formattedDuration
                                    color: Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                }
                            }

                            MouseArea {
                                id: rowMouse

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: ApplicationController.playlist.activateIndex(libRow.index)
                                onDoubleClicked: {
                                    ApplicationController.playlist.activateIndex(libRow.index)
                                    ApplicationController.playback.play()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
