pragma ComponentBehavior: Bound
import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Item {
    id: root

    property bool compactMode: false
    property string activePopover: ""
    signal openCommandPalette()
    signal openBoothSettings()

    implicitHeight: root.compactMode ? 48 : 54

    function togglePopover(anchorItem, kind) {
        if (popover.opened && root.activePopover === kind) {
            popover.close()
            return
        }
        root.activePopover = kind
        const mapped = anchorItem.mapToItem(root, 0, 0)
        const targetWidth = popover.width > 0 ? popover.width : 300
        const desiredX = mapped.x + (anchorItem.width - targetWidth) / 2
        const minX = 4
        const maxX = Math.max(minX, root.width - targetWidth - 4)
        popover.x = Math.max(minX, Math.min(maxX, desiredX))
        popover.y = -(popover.implicitHeight > 0 ? popover.implicitHeight : 180) - 10
        popover.open()
    }

    function popoverComponentFor(kind) {
        if (kind === "display") {
            return displayPopoverComp
        }
        if (kind === "music") {
            return musicPopoverComp
        }
        if (kind === "timer") {
            return timerPopoverComp
        }
        if (kind === "obs") {
            return obsPopoverComp
        }
        if (kind === "zoom") {
            return zoomPopoverComp
        }
        if (kind === "downloads") {
            return downloadsPopoverComp
        }
        return null
    }

    component PillButton: Rectangle {
        id: btn

        property string iconName: ""
        property string iconCategory: "lucide"
        property int iconSize: root.compactMode ? 17 : 20
        property string badgeText: ""
        property string tooltipText: ""
        property color dotColor: "transparent"
        property bool showDot: false
        property bool activeState: false
        property bool liveHighlight: false
        property color customIconColor: "transparent"
        property bool useCustomIconColor: false
        signal triggered()

        readonly property int baseWidth: root.compactMode ? 34 : 46
        readonly property int pillHeight: root.compactMode ? 36 : 40

        Layout.preferredHeight: btn.pillHeight
        Layout.preferredWidth: btn.badgeText.length > 0
                               ? Math.max(btn.baseWidth, btnContent.implicitWidth + (root.compactMode ? 12 : 20))
                               : btn.baseWidth
        radius: btn.pillHeight / 2
        color: btn.activeState
               ? Theme.accentSoft
               : (btnMouse.containsMouse ? Theme.cardHover : "transparent")
        border.width: btn.activeState ? 1 : 0
        border.color: btn.activeState ? Theme.accentBorder : "transparent"

        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: btn.tooltipText

        Behavior on color {
            ColorAnimation {
                duration: Theme.animFast
            }
        }

        Keys.onReturnPressed: btn.triggered()
        Keys.onSpacePressed: btn.triggered()

        FocusRing {
            active: btn.activeFocus
            targetRadius: btn.radius
        }

        RowLayout {
            id: btnContent

            anchors.centerIn: parent
            spacing: 6

            StudioIcon {
                name: btn.iconName
                category: btn.iconCategory
                size: btn.iconSize
                color: btn.useCustomIconColor
                       ? btn.customIconColor
                       : (btn.activeState
                          ? Theme.accentPrimary
                          : (btn.liveHighlight ? Theme.statusReady : Theme.textSecondary))
            }

            Text {
                visible: btn.badgeText.length > 0
                text: btn.badgeText
                color: btn.activeState
                       ? Theme.accentPrimary
                       : (btn.liveHighlight ? Theme.statusReady : Theme.textPrimary)
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                font.weight: Font.DemiBold
            }
        }

        Rectangle {
            visible: btn.showDot
            width: 8
            height: 8
            radius: 4
            color: btn.dotColor
            border.width: 1.5
            border.color: Theme.panelSurface
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: root.compactMode ? 4 : 5
            anchors.rightMargin: root.compactMode ? 4 : 6
        }

        ToolTip.visible: btnMouse.containsMouse && btn.tooltipText.length > 0
        ToolTip.delay: 320
        ToolTip.text: btn.tooltipText

        MouseArea {
            id: btnMouse

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: btn.triggered()
        }
    }

    Component {
        id: displayPopoverComp

        PillDisplayPopover {
            onCloseRequested: popover.close()
        }
    }

    Component {
        id: musicPopoverComp

        PillMusicPopover {
            onCloseRequested: popover.close()
        }
    }

    Component {
        id: timerPopoverComp

        PillTimerPopover {
            onSaveReportRequested: (titles, headers, heading) => {
                reportDialog.titles = titles
                reportDialog.headers = headers
                reportDialog.heading = heading
                popover.close()
                reportDialog.open()
            }
        }
    }

    Component {
        id: obsPopoverComp

        PillObsPopover {
            onOpenSettingsRequested: {
                popover.close()
                root.openBoothSettings()
            }
        }
    }

    Component {
        id: zoomPopoverComp

        PillZoomPopover {
            onCloseRequested: popover.close()
            onOpenSettingsRequested: {
                popover.close()
                root.openBoothSettings()
            }
        }
    }

    Component {
        id: downloadsPopoverComp

        PillDownloadsPopover {}
    }

    FileDialog {
        id: reportDialog

        property var titles: ({})
        property var headers: []
        property string heading: ""

        title: qsTr("Save timing report")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PDF document (*.pdf)"), qsTr("Spreadsheet (*.csv)")]
        defaultSuffix: "pdf"
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        currentFile: currentFolder + "/" + qsTr("timing-report") + "-" + ApplicationController.meetingTimer.dateIso + ".pdf"
        onAccepted: {
            const saved = ApplicationController.meetingTimer.saveTimingReport(
                reportDialog.selectedFile, reportDialog.titles, reportDialog.headers, reportDialog.heading)
            ApplicationController.showToast(saved ? qsTr("Timing report saved") : qsTr("The timing report could not be saved"),
                                            saved ? "success" : "warning")
        }
    }

    Popup {
        id: popover

        width: Math.min(316, Math.max(248, root.width - 12))
        padding: 12
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onClosed: {
            if (root.activePopover.length > 0) {
                root.activePopover = ""
            }
        }

        onImplicitHeightChanged: {
            if (popover.opened) {
                popover.y = -popover.implicitHeight - 10
            }
        }

        background: StudioCard {
            surfaceColor: Theme.cardElevated
            strokeColor: Theme.borderStrong
            cornerRadius: Theme.radiusLarge
        }

        contentItem: Loader {
            active: popover.opened && root.activePopover.length > 0
            sourceComponent: root.popoverComponentFor(root.activePopover)
        }
    }

    StudioCard {
        anchors.centerIn: parent
        width: Math.min(parent.width, pillRow.implicitWidth + (root.compactMode ? 12 : 16))
        height: root.compactMode ? 46 : 52
        cornerRadius: root.compactMode ? 23 : 26
        surfaceColor: Theme.panelSurface
        strokeColor: Theme.borderStrong

        RowLayout {
            id: pillRow

            anchors.centerIn: parent
            spacing: root.compactMode ? 2 : 6

            PillButton {
                id: displayBtn

                iconName: "monitor"
                tooltipText: qsTr("Presentation Display (Ctrl+P)")
                showDot: true
                liveHighlight: ApplicationController.displayManager.presentationWindowVisible
                activeState: root.activePopover === "display"
                dotColor: ApplicationController.displayManager.presentationWindowVisible
                          ? Theme.statusReady
                          : (ApplicationController.displayManager.hasSecondaryDisplay
                             ? Theme.statusStandby : Theme.statusOff)
                onTriggered: root.togglePopover(displayBtn, "display")
            }

            PillButton {
                iconName: "eye-off"
                tooltipText: qsTr("Toggle Standby Blackout")
                showDot: true
                activeState: ApplicationController.playback.standbyActive
                dotColor: ApplicationController.playback.standbyActive
                          ? Theme.statusStandby : Theme.statusOff
                onTriggered: ApplicationController.playback.toggleStandby()
            }

            PillButton {
                iconName: "captions"
                tooltipText: ApplicationController.playback.subtitlesVisible
                             ? qsTr("Subtitles: On (click to hide)")
                             : qsTr("Subtitles: Off (click to show)")
                showDot: ApplicationController.playback.subtitleTracks.length > 0
                activeState: ApplicationController.playback.subtitlesVisible
                             && ApplicationController.playback.subtitleTracks.length > 0
                dotColor: ApplicationController.playback.subtitlesVisible
                          ? Theme.statusReady : Theme.statusOff
                onTriggered: ApplicationController.playback.toggleSubtitles()
            }

            Rectangle {
                visible: !root.compactMode
                Layout.preferredWidth: 1
                Layout.preferredHeight: 22
                color: Theme.borderHairline
            }

            PillButton {
                id: musicBtn

                iconName: "audio-lines"
                tooltipText: qsTr("Background Music Controls")
                showDot: true
                liveHighlight: ApplicationController.backgroundMusic.playing
                activeState: root.activePopover === "music" || ApplicationController.backgroundMusic.playing
                dotColor: ApplicationController.backgroundMusic.fading
                          ? Theme.statusStandby
                          : (ApplicationController.backgroundMusic.playing ? Theme.statusReady : Theme.statusOff)
                onTriggered: root.togglePopover(musicBtn, "music")
            }

            PillButton {
                id: timerBtn

                iconName: "clock"
                badgeText: (!root.compactMode && ApplicationController.meetingTimer.running)
                           ? ApplicationController.meetingTimer.formattedTime
                           : ""
                tooltipText: qsTr("Meeting Timer")
                showDot: true
                liveHighlight: ApplicationController.meetingTimer.running && !ApplicationController.meetingTimer.overtime
                activeState: root.activePopover === "timer" || ApplicationController.meetingTimer.overtime
                useCustomIconColor: ApplicationController.meetingTimer.overtime
                customIconColor: Theme.statusLive
                dotColor: ApplicationController.meetingTimer.overtime
                          ? Theme.statusLive
                          : (ApplicationController.meetingTimer.running ? Theme.statusReady : Theme.statusOff)
                onTriggered: root.togglePopover(timerBtn, "timer")
            }

            PillButton {
                id: obsBtn

                iconName: "obs-studio"
                iconCategory: "custom"
                iconSize: root.compactMode ? 18 : 22
                tooltipText: qsTr("OBS Studio Integration")
                showDot: true
                liveHighlight: ApplicationController.obsClient.connected
                activeState: root.activePopover === "obs"
                dotColor: ApplicationController.obsClient.connected
                          ? Theme.statusReady
                          : (ApplicationController.processMonitor.obsRunning ? Theme.statusStandby : Theme.statusOff)
                onTriggered: root.togglePopover(obsBtn, "obs")
            }

            PillButton {
                id: zoomBtn

                iconName: "zoom"
                iconCategory: "custom"
                iconSize: root.compactMode ? 18 : 22
                tooltipText: qsTr("Zoom Booth Controls")
                showDot: true
                liveHighlight: ApplicationController.zoomBridge.zoomRunning
                activeState: root.activePopover === "zoom"
                dotColor: ApplicationController.zoomBridge.zoomRunning ? Theme.statusReady : Theme.statusOff
                onTriggered: root.togglePopover(zoomBtn, "zoom")
            }

            Rectangle {
                visible: !root.compactMode
                Layout.preferredWidth: 1
                Layout.preferredHeight: 22
                color: Theme.borderHairline
            }

            PillButton {
                id: downloadsBtn

                iconName: "download"
                badgeText: (!root.compactMode && (ApplicationController.downloads.activeCount + ApplicationController.downloads.queuedCount) > 0)
                           ? ("" + (ApplicationController.downloads.activeCount + ApplicationController.downloads.queuedCount))
                           : ""
                tooltipText: qsTr("Transfer Queue & Downloads")
                showDot: (ApplicationController.downloads.activeCount + ApplicationController.downloads.queuedCount) > 0
                activeState: root.activePopover === "downloads" || ApplicationController.downloads.activeCount > 0
                dotColor: ApplicationController.downloads.activeCount > 0 ? Theme.accentPrimary : Theme.statusStandby
                onTriggered: root.togglePopover(downloadsBtn, "downloads")
            }

            PillButton {
                iconName: "command"
                tooltipText: qsTr("Command Palette (Ctrl+K)")
                onTriggered: root.openCommandPalette()
            }
        }
    }
}
