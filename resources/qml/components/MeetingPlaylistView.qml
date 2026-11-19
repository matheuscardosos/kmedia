pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Item {
    id: root

    property bool compactMode: false
    property bool showPreviewToggle: false
    property bool previewOpen: false
    signal addMediaRequested()
    signal previewToggled()

    function sectionColor(title) {
        const _watch = ApplicationController.playlist.sections
        return ApplicationController.playlist.colorForSection(title)
    }

    CustomSectionPopup {
        id: sectionPopup
    }

    function formatMsToClock(ms) {
        if (ms <= 0) {
            return "--:--"
        }
        const totalSec = Math.floor(ms / 1000)
        const hrs = Math.floor(totalSec / 3600)
        const mins = Math.floor((totalSec % 3600) / 60)
        const secs = totalSec % 60
        const pad = (n) => (n < 10 ? "0" + n : "" + n)
        if (hrs > 0) {
            return hrs + ":" + pad(mins) + ":" + pad(secs)
        }
        return pad(mins) + ":" + pad(secs)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingMd

        // Top Header Card: Playlist identity + week/meeting switcher + summary CTA bar
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: headerCol.implicitHeight + 24
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: headerCol

                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                // Row 1: Title + Week/Playlist switcher
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: Theme.radiusSmall
                        color: Theme.cardSurface
                        border.width: 1
                        border.color: Theme.borderHairline

                        StudioIcon {
                            anchors.centerIn: parent
                            name: "calendar"
                            size: 17
                            color: Theme.accentPrimary
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            text: ApplicationController.playlist.playlistName
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTitle
                            font.weight: Font.Bold
                            font.letterSpacing: -0.2
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: ApplicationController.playlist.playlistSubtitle
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    // Playlist / Week switcher
                    RowLayout {
                        visible: !root.compactMode
                        spacing: 4

                        StudioButton {
                            variant: "secondary"
                            iconName: "chevron-left"
                            iconSize: 15
                            implicitWidth: 32
                            implicitHeight: 32
                            leftPadding: 0
                            rightPadding: 0
                            enabled: ApplicationController.playlist.availablePlaylistNames.length > 1
                            onClicked: ApplicationController.playlist.switchAdjacentPlaylist(-1)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Previous meeting playlist")
                        }

                        StudioButton {
                            id: playlistSwitchBtn

                            variant: "secondary"
                            text: ApplicationController.playlist.playlistName
                            trailingIconName: "chevron-down"
                            iconSize: 13
                            implicitHeight: 32
                            onClicked: playlistMenuPopup.open()

                            Popup {
                                id: playlistMenuPopup

                                x: 0
                                y: parent.height + 4
                                width: Math.max(210, parent.width)
                                padding: 4

                                background: StudioCard {
                                    surfaceColor: Theme.cardElevated
                                    strokeColor: Theme.borderStrong
                                }

                                contentItem: ColumnLayout {
                                    spacing: 2

                                    Repeater {
                                        model: ApplicationController.playlist.availablePlaylistNames

                                        delegate: Rectangle {
                                            id: plOption

                                            required property string modelData
                                            required property int index

                                            readonly property bool isCurrent: index === ApplicationController.playlist.activePlaylistIndex

                                            Layout.fillWidth: true
                                            implicitHeight: 32
                                            radius: Theme.radiusSmall
                                            color: plHover.containsMouse ? Theme.cardHover
                                                 : (plOption.isCurrent ? Theme.accentSoft : "transparent")

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8

                                                Text {
                                                    text: plOption.modelData
                                                    color: plOption.isCurrent ? Theme.accentPrimary : Theme.textPrimary
                                                    font.family: Theme.fontFamily
                                                    font.pixelSize: Theme.fontSizeSm
                                                    font.weight: plOption.isCurrent ? Font.DemiBold : Font.Normal
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }

                                                StudioIcon {
                                                    visible: plOption.isCurrent
                                                    name: "check"
                                                    size: 14
                                                    color: Theme.accentPrimary
                                                }
                                            }

                                            MouseArea {
                                                id: plHover

                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    ApplicationController.playlist.switchPlaylistByIndex(plOption.index)
                                                    playlistMenuPopup.close()
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        StudioButton {
                            variant: "secondary"
                            iconName: "chevron-right"
                            iconSize: 15
                            implicitWidth: 32
                            implicitHeight: 32
                            leftPadding: 0
                            rightPadding: 0
                            enabled: ApplicationController.playlist.availablePlaylistNames.length > 1
                            onClicked: ApplicationController.playlist.switchAdjacentPlaylist(1)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Next meeting playlist")
                        }
                    }

                    StudioButton {
                        visible: root.showPreviewToggle
                        variant: root.previewOpen ? "primary" : "secondary"
                        iconName: "monitor"
                        text: root.compactMode ? "" : (root.previewOpen ? qsTr("Hide Preview") : qsTr("Preview"))
                        implicitHeight: 32
                        leftPadding: root.compactMode ? 8 : 10
                        rightPadding: root.compactMode ? 8 : 10
                        onClicked: root.previewToggled()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Toggle live preview and inspector")
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.borderHairline
                }

                // Row 2: Media count + total duration + Broadcast Orange primary CTA
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    RowLayout {
                        spacing: 6

                        StudioIcon {
                            name: "layers"
                            size: 14
                            color: Theme.textSecondary
                        }

                        Text {
                            text: qsTr("%1 items").arg(ApplicationController.playlist.count)
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Font.Medium
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: 14
                        color: Theme.borderHairline
                    }

                    RowLayout {
                        spacing: 6

                        StudioIcon {
                            name: "clock"
                            size: 14
                            color: Theme.textSecondary
                        }

                        Text {
                            text: qsTr("%1 duration").arg(ApplicationController.playlist.formattedTotalDuration)
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Font.Medium
                        }
                    }

                    StudioBadge {
                        visible: !root.compactMode && ApplicationController.catalog.weekEventLabel.length > 0
                        text: ApplicationController.catalog.weekEventLabel
                        showDot: true
                        dotColor: ApplicationController.catalog.weekEventType === "co_visit"
                                  ? "#3F7F5F" : "#6B4E9B"
                        badgeColor: Theme.accentSoft
                        textColor: Theme.accentPrimary
                        hasBorder: true
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StudioButton {
                        id: addSectionBtn

                        variant: "secondary"
                        iconName: "folder-plus"
                        text: root.compactMode ? "" : qsTr("Section")
                        leftPadding: root.compactMode ? 0 : 10
                        rightPadding: root.compactMode ? 0 : 10
                        Layout.preferredWidth: root.compactMode ? 34 : addSectionBtn.implicitWidth
                        Layout.preferredHeight: 34
                        onClicked: sectionPopup.openCreate()
                        ToolTip.visible: addSectionBtn.hovered
                        ToolTip.text: qsTr("Create custom meeting section")
                    }

                    StudioButton {
                        variant: "secondary"
                        iconName: "refresh-cw"
                        text: root.compactMode ? qsTr("Sync") : qsTr("Sync Week")
                        implicitHeight: 34
                        enabled: !ApplicationController.catalog.busy
                        onClicked: ApplicationController.catalog.syncCurrentWeekToPlaylist()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Fetch weekly workbook and Watchtower media automatically")
                    }

                    StudioButton {
                        variant: "primary"
                        iconName: "plus"
                        text: root.compactMode ? qsTr("Add") : qsTr("Add Media")
                        implicitHeight: 34
                        onClicked: root.addMediaRequested()
                    }
                }

                // Custom section chips strip for quick color, reorder, and section creation
                Flow {
                    visible: !root.compactMode && ApplicationController.playlist.sections.length > 0
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: ApplicationController.playlist.sections

                        delegate: Rectangle {
                            id: secChip

                            required property var modelData

                            width: secChipRow.implicitWidth + 16
                            height: 24
                            radius: 12
                            color: secChipHover.containsMouse ? Theme.cardHover : Theme.cardSurface
                            border.width: 1
                            border.color: Theme.borderHairline

                            RowLayout {
                                id: secChipRow

                                anchors.centerIn: parent
                                spacing: 6

                                Rectangle {
                                    Layout.preferredWidth: 8
                                    Layout.preferredHeight: 8
                                    radius: 4
                                    color: secChip.modelData.color
                                }

                                Text {
                                    text: secChip.modelData.name + " (" + secChip.modelData.itemCount + ")"
                                    color: Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    font.weight: Font.Medium
                                }
                            }

                            MouseArea {
                                id: secChipHover

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: sectionPopup.openEdit(
                                    secChip.modelData.name,
                                    secChip.modelData.color,
                                    secChip.modelData.itemCount === 0)
                            }
                        }
                    }
                }
            }
        }

        // Structured Playlist Table Card
        StudioCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 4

                // Table Column Header Row
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    Layout.leftMargin: 8
                    Layout.rightMargin: 18
                    spacing: 10

                    Item {
                        visible: !root.compactMode
                        Layout.preferredWidth: 16
                    }

                    Text {
                        Layout.preferredWidth: 22
                        text: "#"
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Media")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.preferredWidth: 56
                        text: qsTr("Duration")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignRight
                    }

                    Text {
                        Layout.preferredWidth: 46
                        text: qsTr("Status")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.borderHairline
                }

                // Playlist Items ListView
                ListView {
                    id: playlistView

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    topMargin: 4
                    bottomMargin: 4
                    model: ApplicationController.playlist
                    currentIndex: ApplicationController.playlist.activeIndex

                    ScrollBar.vertical: StudioScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    // Meeting section dividers (Treasures, Apply Yourself, Living as Christians,
                    // Public Talk, Watchtower Study, Circuit Overseer, custom). Each section gets
                    // its own color avatar with a generic list glyph, like the M3 demo layout.
                    section.property: "section"
                    section.criteria: ViewSection.FullString
                    section.delegate: Item {
                        id: sectionHeader

                        required property string section

                        readonly property string resolvedColorHex: {
                            const _watch = ApplicationController.playlist.sections
                            return ApplicationController.playlist.colorForSection(sectionHeader.section)
                        }

                        width: playlistView.width - 10
                        height: 40

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            anchors.topMargin: 6
                            anchors.bottomMargin: 4
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                Layout.alignment: Qt.AlignVCenter
                                radius: 12
                                color: sectionHeader.resolvedColorHex

                                StudioIcon {
                                    anchors.centerIn: parent
                                    name: "list-video"
                                    size: 13
                                    color: "#FFFFFF"
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: sectionHeader.section.length > 0 ? sectionHeader.section : qsTr("Imported Media")
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Font.Bold
                                font.letterSpacing: 0.6
                                font.capitalization: Font.AllUppercase
                                elide: Text.ElideRight
                            }

                            StudioButton {
                                id: editSecBtn

                                variant: "ghost"
                                iconName: "sliders-horizontal"
                                iconSize: 12
                                leftPadding: 0
                                rightPadding: 0
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                onClicked: sectionPopup.openEdit(sectionHeader.section, sectionHeader.resolvedColorHex, false)
                                ToolTip.visible: editSecBtn.hovered
                                ToolTip.text: qsTr("Customize or reorder section")
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            height: 1
                            color: sectionHeader.resolvedColorHex
                            opacity: 0.45
                        }
                    }

                    // Empty state placeholder when playlist has zero items
                    Item {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 32, 360)
                        height: emptyCol.implicitHeight
                        visible: playlistView.count === 0

                        ColumnLayout {
                            id: emptyCol

                            anchors.fill: parent
                            spacing: 10

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                                radius: 24
                                color: Theme.cardSurface
                                border.width: 1
                                border.color: Theme.borderHairline

                                StudioIcon {
                                    anchors.centerIn: parent
                                    name: "folder-plus"
                                    size: 22
                                    color: Theme.accentPrimary
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("No media queued for this meeting")
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLg
                                font.weight: Font.DemiBold
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("Drop video, audio, images, PDFs, .jwpub, or .jwlplaylist files anywhere, or press Ctrl+O.")
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    delegate: Rectangle {
                        id: rowDelegate

                        required property int index
                        required property string title
                        required property string subtitle
                        required property string section
                        required property string thumbUrl
                        required property string kindLabel
                        required property int mediaKind
                        required property string formattedDuration
                        required property int trimStart
                        required property int trimEnd
                        required property bool fileExists
                        required property bool isActive

                        readonly property bool isVideoVisual: kindLabel === "VID" || subtitle.indexOf("Visual") >= 0
                        readonly property string typeIconName: {
                            if (kindLabel === "AUD") return "audio-lines"
                            if (kindLabel === "PDF") return "file-text"
                            if (isVideoVisual) return "film"
                            return "image"
                        }

                        width: playlistView.width - 10
                        height: root.compactMode ? 54 : 60
                        radius: Theme.radiusMedium
                        color: isActive ? Theme.rowSelected
                             : (rowHover.containsMouse ? Theme.cardHover : "transparent")
                        border.width: isActive ? 1 : 0
                        border.color: isActive ? Theme.accentPrimary : "transparent"

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.animFast
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 10

                            // Column 1: Drag handle
                            Item {
                                visible: !root.compactMode
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 32

                                StudioIcon {
                                    anchors.centerIn: parent
                                    name: "grip-vertical"
                                    size: 14
                                    color: dragMouse.pressed ? Theme.accentPrimary : Theme.textMuted
                                }

                                MouseArea {
                                    id: dragMouse

                                    property int startRow: -1
                                    property real pressSceneY: 0

                                    anchors.fill: parent
                                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                    onPressed: (mouse) => {
                                        startRow = rowDelegate.index
                                        pressSceneY = mapToItem(playlistView, 0, mouse.y).y
                                        mouse.accepted = true
                                    }
                                    onPositionChanged: (mouse) => {
                                        if (!pressed || startRow < 0) {
                                            return
                                        }
                                        const curY = mapToItem(playlistView, 0, mouse.y).y
                                        const rowStep = rowDelegate.height + playlistView.spacing
                                        const delta = Math.round((curY - pressSceneY) / rowStep)
                                        const target = Math.max(0, Math.min(playlistView.count - 1, startRow + delta))
                                        if (target !== rowDelegate.index) {
                                            ApplicationController.playlist.moveItem(rowDelegate.index, target)
                                            startRow = target
                                            pressSceneY = curY
                                        }
                                    }
                                }
                            }

                            // Column 2: 1-based item index
                            Text {
                                Layout.preferredWidth: 22
                                text: "" + (rowDelegate.index + 1)
                                color: rowDelegate.isActive ? Theme.accentPrimary : Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                font.weight: rowDelegate.isActive ? Font.Bold : Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                            }

                            // Column 3: 16:9 Thumbnail + Title & Subtitle
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: root.compactMode ? 54 : 72
                                    Layout.preferredHeight: root.compactMode ? 32 : 42
                                    radius: Theme.radiusSmall
                                    color: Theme.cardSurface
                                    border.width: 1
                                    border.color: rowDelegate.isActive ? Theme.accentBorder : Theme.borderHairline
                                    clip: true

                                    Image {
                                        anchors.fill: parent
                                        source: rowDelegate.thumbUrl
                                        visible: rowDelegate.thumbUrl.length > 0
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        smooth: true
                                        mipmap: true
                                    }

                                    StudioIcon {
                                        anchors.centerIn: parent
                                        visible: rowDelegate.thumbUrl.length === 0
                                        name: rowDelegate.typeIconName
                                        size: 16
                                        color: Theme.textSecondary
                                    }

                                    Rectangle {
                                        visible: rowDelegate.thumbUrl.length > 0 && rowDelegate.isVideoVisual
                                        anchors.centerIn: parent
                                        width: 20
                                        height: 20
                                        radius: 10
                                        color: "#8A0D0F12"

                                        StudioIcon {
                                            anchors.centerIn: parent
                                            anchors.horizontalCenterOffset: 1
                                            name: "play"
                                            size: 9
                                            color: "#FFFFFF"
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Text {
                                            text: rowDelegate.title
                                            color: Theme.textPrimary
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeMd
                                            font.weight: rowDelegate.isActive ? Font.DemiBold : Font.Medium
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        StudioBadge {
                                            visible: rowDelegate.trimEnd > rowDelegate.trimStart && !root.compactMode
                                            implicitHeight: 18
                                            text: qsTr("TRIM")
                                            badgeColor: Theme.accentSoft
                                            textColor: Theme.accentPrimary
                                            hasBorder: true
                                            strokeColor: Theme.accentBorder
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 5

                                        StudioIcon {
                                            name: rowDelegate.typeIconName
                                            size: 11
                                            color: Theme.textMuted
                                        }

                                        Text {
                                            text: rowDelegate.subtitle.length > 0 ? rowDelegate.subtitle : rowDelegate.section
                                            color: Theme.textSecondary
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeXs
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }

                            // Column 4: Tabular Duration
                            Text {
                                Layout.preferredWidth: 56
                                text: rowDelegate.formattedDuration
                                color: rowDelegate.isActive ? Theme.textPrimary : Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                font.weight: rowDelegate.isActive ? Font.DemiBold : Font.Normal
                                horizontalAlignment: Text.AlignRight
                            }

                            // Column 5: Readiness Status Icon
                            Item {
                                Layout.preferredWidth: 46
                                Layout.preferredHeight: 28

                                StudioIcon {
                                    anchors.centerIn: parent
                                    name: rowDelegate.fileExists ? "circle-check" : "triangle-exclamation-point"
                                    size: 16
                                    color: rowDelegate.fileExists ? Theme.statusReady : Theme.statusStandby
                                }
                            }
                        }

                        MouseArea {
                            id: rowHover

                            anchors.fill: parent
                            anchors.leftMargin: root.compactMode ? 0 : 24
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            cursorShape: Qt.PointingHandCursor
                            onClicked: (mouse) => {
                                if (mouse.button === Qt.RightButton) {
                                    ApplicationController.playlist.activateIndex(rowDelegate.index)
                                    rowContextMenu.popup()
                                } else {
                                    ApplicationController.playlist.activateIndex(rowDelegate.index)
                                }
                            }
                            onDoubleClicked: {
                                ApplicationController.playlist.activateIndex(rowDelegate.index)
                                ApplicationController.playback.play()
                            }
                        }

                        Menu {
                            id: rowContextMenu

                            MenuItem {
                                text: qsTr("Play Now")
                                onTriggered: {
                                    ApplicationController.playlist.activateIndex(rowDelegate.index)
                                    ApplicationController.playback.play()
                                }
                            }
                            MenuItem {
                                text: qsTr("Move Up")
                                enabled: rowDelegate.index > 0
                                onTriggered: ApplicationController.playlist.moveItem(rowDelegate.index, rowDelegate.index - 1)
                            }
                            MenuItem {
                                text: qsTr("Move Down")
                                enabled: rowDelegate.index < playlistView.count - 1
                                onTriggered: ApplicationController.playlist.moveItem(rowDelegate.index, rowDelegate.index + 1)
                            }
                            MenuItem {
                                text: qsTr("Move to Section...")
                                onTriggered: sectionPopup.openAssign(rowDelegate.index, rowDelegate.section)
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: qsTr("Remove from Playlist")
                                onTriggered: ApplicationController.playlist.removeAt(rowDelegate.index)
                            }
                        }
                    }
                }
            }
        }
    }
}
