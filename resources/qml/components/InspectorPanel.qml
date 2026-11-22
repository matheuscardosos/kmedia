pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Rectangle {
    id: root

    property bool drawerMode: false
    signal closeRequested()

    readonly property int mediaKind: ApplicationController.playback.mediaKind
    readonly property int effectiveDurationMs: ApplicationController.playback.durationMs > 0
        ? ApplicationController.playback.durationMs
        : ApplicationController.playlist.activeDurationMs

    function formatClock(ms) {
        if (ms <= 0) {
            return "00:00"
        }
        const totalSec = Math.floor(ms / 1000)
        const mins = Math.floor(totalSec / 60)
        const secs = totalSec % 60
        return (mins < 10 ? "0" : "") + mins + ":" + (secs < 10 ? "0" : "") + secs
    }

    function formatPrecisionMs(ms) {
        if (ms <= 0) {
            return "00:00.000"
        }
        const totalSec = Math.floor(ms / 1000)
        const mins = Math.floor(totalSec / 60)
        const secs = totalSec % 60
        const remMs = ms % 1000
        const pad2 = (n) => (n < 10 ? "0" + n : "" + n)
        const pad3 = (n) => (n < 10 ? "00" + n : (n < 100 ? "0" + n : "" + n))
        return pad2(mins) + ":" + pad2(secs) + "." + pad3(remMs)
    }

    color: Theme.windowBase

    Rectangle {
        visible: root.drawerMode
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 1
        color: Theme.borderHairline
    }

    ScrollView {
        anchors.fill: parent
        anchors.topMargin: Theme.spacingMd
        anchors.bottomMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingMd
        anchors.leftMargin: root.drawerMode ? Theme.spacingMd : 0
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical: StudioScrollBar {
            policy: ScrollBar.AsNeeded
        }

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingMd

            // Card 1: Live Preview Monitor & Quick Stage Output Controls
            StudioCard {
                Layout.fillWidth: true
                implicitHeight: previewCardCol.implicitHeight + 24
                surfaceColor: Theme.panelSurface

                ColumnLayout {
                    id: previewCardCol

                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StudioIcon {
                            name: "monitor"
                            size: 16
                            color: Theme.textSecondary
                        }

                        Text {
                            text: qsTr("Live Preview")
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMd
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }

                        StudioButton {
                            visible: root.drawerMode
                            variant: "ghost"
                            iconName: "x"
                            iconSize: 15
                            implicitWidth: 28
                            implicitHeight: 28
                            leftPadding: 0
                            rightPadding: 0
                            onClicked: root.closeRequested()
                        }
                    }

                    // 16:9 Live Preview Canvas with overlaid status & Standby/Blackout pills
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.round(width * 9 / 16)
                        radius: Theme.radiusSmall
                        color: "#000000"
                        border.width: 1
                        border.color: Theme.borderHairline
                        clip: true

                        Image {
                            anchors.fill: parent
                            visible: (root.mediaKind === PlaybackController.Image || root.mediaKind === PlaybackController.Pdf) && !ApplicationController.playback.standbyActive
                            source: root.mediaKind === PlaybackController.Pdf
                                    ? ApplicationController.pdf.currentPageImageUrl
                                    : ApplicationController.playback.source
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                            scale: ApplicationController.playback.zoomScale
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: root.mediaKind === PlaybackController.None || ApplicationController.playback.standbyActive
                            color: "#090B0E"

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 6

                                StudioIcon {
                                    Layout.alignment: Qt.AlignHCenter
                                    name: ApplicationController.playback.standbyActive ? "eye-off" : "monitor"
                                    size: 22
                                    color: ApplicationController.playback.standbyActive ? Theme.statusStandby : Theme.textMuted
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: ApplicationController.playback.standbyActive
                                          ? qsTr("STANDBY ACTIVE")
                                          : qsTr("NO ACTIVE MEDIA")
                                    color: ApplicationController.playback.standbyActive ? Theme.statusStandby : Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    font.weight: Font.Bold
                                    font.letterSpacing: 0.6
                                }
                            }
                        }

                        // Top-left live stage badge
                        StudioBadge {
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.margins: 8
                            text: ApplicationController.playback.standbyActive
                                  ? qsTr("STANDBY")
                                  : ((root.mediaKind !== PlaybackController.None && !ApplicationController.playback.paused)
                                     ? qsTr("ON AIR")
                                     : qsTr("READY"))
                            badgeColor: ApplicationController.playback.standbyActive
                                        ? "#D97706"
                                        : ((root.mediaKind !== PlaybackController.None && !ApplicationController.playback.paused)
                                           ? Theme.statusLive
                                           : "#CC181C23")
                            textColor: "#FFFFFF"
                            showDot: true
                            dotColor: (root.mediaKind !== PlaybackController.None && !ApplicationController.playback.paused && !ApplicationController.playback.standbyActive)
                                      ? "#FFFFFF"
                                      : (ApplicationController.playback.standbyActive ? "#FEF3C7" : Theme.statusReady)
                        }

                        // Top-right Standby & Blackout floating glass pills
                        RowLayout {
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: 8
                            spacing: 6

                            Rectangle {
                                implicitWidth: standbyPillRow.implicitWidth + 14
                                implicitHeight: 24
                                radius: Theme.radiusSmall
                                color: ApplicationController.playback.standbyActive
                                       && !ApplicationController.playback.blackoutActive
                                       ? Theme.statusStandby
                                       : "#B816191F"
                                border.width: 1
                                border.color: "#38FFFFFF"

                                RowLayout {
                                    id: standbyPillRow

                                    anchors.centerIn: parent
                                    spacing: 4

                                    StudioIcon {
                                        name: "pause"
                                        size: 11
                                        color: "#FFFFFF"
                                    }

                                    Text {
                                        text: qsTr("Standby")
                                        color: "#FFFFFF"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeXs
                                        font.weight: Font.DemiBold
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: ApplicationController.playback.toggleStandby()
                                }
                            }

                            Rectangle {
                                implicitWidth: blackoutPillRow.implicitWidth + 14
                                implicitHeight: 24
                                radius: Theme.radiusSmall
                                color: ApplicationController.playback.blackoutActive
                                       ? Theme.statusStandby
                                       : "#B816191F"
                                border.width: 1
                                border.color: "#38FFFFFF"

                                RowLayout {
                                    id: blackoutPillRow

                                    anchors.centerIn: parent
                                    spacing: 4

                                    StudioIcon {
                                        name: "square"
                                        size: 10
                                        color: "#FFFFFF"
                                    }

                                    Text {
                                        text: qsTr("Blackout")
                                        color: "#FFFFFF"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeXs
                                        font.weight: Font.DemiBold
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: ApplicationController.playback.blackout()
                                }
                            }
                        }
                    }

                    // Mini preview footer bar
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        StudioButton {
                            variant: "ghost"
                            iconName: ApplicationController.playback.paused ? "play" : "pause"
                            iconSize: 13
                            implicitWidth: 26
                            implicitHeight: 26
                            leftPadding: 0
                            rightPadding: 0
                            onClicked: ApplicationController.playback.togglePause()
                        }

                        Text {
                            text: ApplicationController.playback.formattedPosition + " / " + root.formatClock(root.effectiveDurationMs)
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        StudioSlider {
                            Layout.fillWidth: true
                            implicitHeight: 16
                            trackHeight: 3
                            handleDiameter: 9
                            from: 0
                            to: Math.max(1, root.effectiveDurationMs)
                            value: Math.min(to, ApplicationController.playback.positionMs)
                            enabled: root.effectiveDurationMs > 0
                            onMoved: ApplicationController.playback.seekMs(Math.round(value))
                        }

                        StudioButton {
                            variant: "ghost"
                            iconName: ApplicationController.playback.muted ? "volume-x" : "volume-2"
                            iconSize: 14
                            implicitWidth: 26
                            implicitHeight: 26
                            leftPadding: 0
                            rightPadding: 0
                            onClicked: ApplicationController.playback.muted = !ApplicationController.playback.muted
                        }

                        StudioButton {
                            variant: ApplicationController.displayManager.presentationWindowVisible ? "primary" : "ghost"
                            iconName: ApplicationController.displayManager.presentationWindowVisible ? "minimize-2" : "maximize-2"
                            iconSize: 14
                            implicitWidth: 28
                            implicitHeight: 26
                            leftPadding: 0
                            rightPadding: 0
                            onClicked: ApplicationController.displayManager.presentationWindowVisible = !ApplicationController.displayManager.presentationWindowVisible
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Toggle secondary presentation output window")
                        }
                    }
                }
            }

            // Card 2: File Information
            StudioCard {
                Layout.fillWidth: true
                implicitHeight: infoCardCol.implicitHeight + 24
                surfaceColor: Theme.panelSurface

                ColumnLayout {
                    id: infoCardCol

                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StudioIcon {
                            name: "info"
                            size: 15
                            color: Theme.textSecondary
                        }

                        Text {
                            text: qsTr("File Information")
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMd
                            font.weight: Font.DemiBold
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 6

                        Text { text: qsTr("Name"); color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSm }
                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            text: ApplicationController.playlist.activeFileName.length > 0 ? ApplicationController.playlist.activeFileName : "--"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Font.Medium
                            elide: Text.ElideMiddle
                        }

                        Text { text: qsTr("Format"); color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSm }
                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            text: ApplicationController.playlist.activeFormatLabel.length > 0 ? ApplicationController.playlist.activeFormatLabel : "--"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }

                        Text { text: qsTr("Resolution"); color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSm }
                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            text: ApplicationController.playlist.activeResolutionLabel.length > 0 ? ApplicationController.playlist.activeResolutionLabel : "--"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }

                        Text { text: qsTr("Duration"); color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSm }
                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            text: root.formatClock(root.effectiveDurationMs)
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }

                        Text { text: qsTr("File Size"); color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSm }
                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            text: ApplicationController.playlist.activeFileSizeFormatted.length > 0 ? ApplicationController.playlist.activeFileSizeFormatted : "--"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }
                    }
                }
            }

            // Card 3: Outputs & Live Routing Status
            StudioCard {
                Layout.fillWidth: true
                implicitHeight: routingCol.implicitHeight + 24
                surfaceColor: Theme.panelSurface

                ColumnLayout {
                    id: routingCol

                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StudioIcon {
                            name: "cast"
                            size: 15
                            color: Theme.textSecondary
                        }

                        Text {
                            text: qsTr("Outputs / Routing")
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMd
                            font.weight: Font.DemiBold
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        StudioIcon { name: "monitor"; size: 14; color: Theme.textMuted }
                        Text { text: qsTr("Projector / 2nd Display"); color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSm; Layout.fillWidth: true }
                        RowLayout {
                            Layout.preferredWidth: 112
                            spacing: 6
                            Rectangle {
                                Layout.preferredWidth: 7; Layout.preferredHeight: 7; radius: 3.5
                                color: ApplicationController.displayManager.presentationWindowVisible
                                       ? Theme.statusReady
                                       : (ApplicationController.displayManager.hasSecondaryDisplay ? Theme.statusStandby : Theme.statusOff)
                            }
                            Text {
                                Layout.fillWidth: true
                                text: ApplicationController.displayManager.presentationWindowVisible ? qsTr("Active")
                                    : (ApplicationController.displayManager.hasSecondaryDisplay ? qsTr("Ready") : qsTr("Off"))
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        StudioIcon { name: "radio"; size: 14; color: Theme.textMuted }
                        Text { text: qsTr("OBS Scene Sync"); color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSm; Layout.fillWidth: true }
                        RowLayout {
                            Layout.preferredWidth: 112
                            spacing: 6
                            Rectangle {
                                Layout.preferredWidth: 7; Layout.preferredHeight: 7; radius: 3.5
                                color: ApplicationController.obsClient.connected ? Theme.statusReady : Theme.statusOff
                            }
                            Text {
                                Layout.fillWidth: true
                                text: ApplicationController.obsClient.connected ? qsTr("Connected") : qsTr("Offline")
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        StudioIcon { name: "volume-2"; size: 14; color: Theme.textMuted }
                        Text { text: qsTr("Audio Output"); color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSm; Layout.fillWidth: true }
                        RowLayout {
                            Layout.preferredWidth: 112
                            spacing: 6
                            Rectangle {
                                Layout.preferredWidth: 7; Layout.preferredHeight: 7; radius: 3.5
                                color: ApplicationController.playback.muted ? Theme.statusStandby : Theme.statusReady
                            }
                            Text {
                                Layout.fillWidth: true
                                text: ApplicationController.playback.muted ? qsTr("Muted") : qsTr("Active (Stereo)")
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                            }
                        }
                    }
                }
            }

            // Card 4: Non-Destructive Trim, Zoom & PDF Page Controls
            StudioCard {
                Layout.fillWidth: true
                implicitHeight: editCol.implicitHeight + 24
                surfaceColor: Theme.panelSurface

                ColumnLayout {
                    id: editCol

                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StudioIcon {
                            name: "scissors"
                            size: 15
                            color: Theme.textSecondary
                        }

                        Text {
                            text: qsTr("Edit (Non-Destructive)")
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMd
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }

                        StudioButton {
                            variant: "ghost"
                            iconName: "rotate-ccw"
                            iconSize: 13
                            implicitWidth: 26
                            implicitHeight: 26
                            leftPadding: 0
                            rightPadding: 0
                            onClicked: {
                                ApplicationController.playlist.resetActiveTrim()
                                ApplicationController.playback.resetViewport()
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Reset trim markers and zoom")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: qsTr("In")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }

                        StudioCard {
                            Layout.fillWidth: true
                            implicitHeight: 32
                            surfaceColor: Theme.cardSurface

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 6

                                Text {
                                    text: root.formatPrecisionMs(ApplicationController.playback.trimStartMs)
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    Layout.fillWidth: true
                                }

                                StudioButton {
                                    variant: "ghost"
                                    iconName: "clock"
                                    iconSize: 12
                                    implicitWidth: 22
                                    implicitHeight: 22
                                    leftPadding: 0
                                    rightPadding: 0
                                    onClicked: ApplicationController.playlist.updateActiveTrim(
                                                   ApplicationController.playback.positionMs,
                                                   ApplicationController.playback.trimEndMs)
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Set trim start at current position")
                                }
                            }
                        }

                        Text {
                            text: qsTr("Out")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }

                        StudioCard {
                            Layout.fillWidth: true
                            implicitHeight: 32
                            surfaceColor: Theme.cardSurface

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 6

                                Text {
                                    text: root.formatPrecisionMs(ApplicationController.playback.trimEndMs)
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    Layout.fillWidth: true
                                }

                                StudioButton {
                                    variant: "ghost"
                                    iconName: "clock"
                                    iconSize: 12
                                    implicitWidth: 22
                                    implicitHeight: 22
                                    leftPadding: 0
                                    rightPadding: 0
                                    onClicked: ApplicationController.playlist.updateActiveTrim(
                                                   ApplicationController.playback.trimStartMs,
                                                   ApplicationController.playback.positionMs > 0
                                                   ? ApplicationController.playback.positionMs
                                                   : root.effectiveDurationMs)
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Set trim end at current position")
                                }
                            }
                        }
                    }

                    // Interactive image/video zoom control
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.mediaKind === PlaybackController.Video || root.mediaKind === PlaybackController.Image

                        StudioIcon {
                            name: "zoom-in"
                            size: 14
                            color: Theme.textSecondary
                        }

                        Text {
                            text: ApplicationController.playback.zoomScale.toFixed(1) + "x"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            Layout.preferredWidth: 32
                        }

                        StudioSlider {
                            Layout.fillWidth: true
                            from: 1.0
                            to: 4.0
                            stepSize: 0.1
                            value: ApplicationController.playback.zoomScale
                            onMoved: ApplicationController.playlist.updateActiveViewport(
                                         value, ApplicationController.playback.panX,
                                         ApplicationController.playback.panY)
                        }
                    }

                    // PDF navigation row when viewing PDF document
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: root.mediaKind === PlaybackController.Pdf && ApplicationController.pdf.loaded

                        Text {
                            text: qsTr("Page %1 of %2")
                                  .arg(ApplicationController.pdf.currentPage + 1)
                                  .arg(Math.max(1, ApplicationController.pdf.pageCount))
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            Layout.fillWidth: true
                        }

                        StudioButton {
                            variant: "secondary"
                            iconName: "chevron-left"
                            implicitWidth: 28
                            implicitHeight: 28
                            leftPadding: 0
                            rightPadding: 0
                            enabled: ApplicationController.pdf.currentPage > 0
                            onClicked: ApplicationController.pdf.previousPage()
                        }

                        StudioButton {
                            variant: "secondary"
                            iconName: "chevron-right"
                            implicitWidth: 28
                            implicitHeight: 28
                            leftPadding: 0
                            rightPadding: 0
                            enabled: ApplicationController.pdf.currentPage + 1 < ApplicationController.pdf.pageCount
                            onClicked: ApplicationController.pdf.nextPage()
                        }
                    }
                }
            }
        }
    }
}
