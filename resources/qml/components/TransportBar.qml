pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

StudioCard {
    id: root

    property bool compactMode: false

    readonly property int effectiveDurationMs: ApplicationController.playback.durationMs > 0
        ? ApplicationController.playback.durationMs
        : ApplicationController.playlist.activeDurationMs

    function formatClockMs(ms) {
        if (ms <= 0) {
            return "00:00"
        }
        const totalSec = Math.floor(ms / 1000)
        const mins = Math.floor(totalSec / 60)
        const secs = totalSec % 60
        return (mins < 10 ? "0" : "") + mins + ":" + (secs < 10 ? "0" : "") + secs
    }

    surfaceColor: Theme.panelSurface
    implicitHeight: root.compactMode ? 96 : Theme.transportBarHeight

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 8
        anchors.bottomMargin: 6
        spacing: 4

        // Top row: Active thumbnail + Transport cluster + Background Music & Volume
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            // Left active media summary
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: root.compactMode ? 110 : 120
                Layout.preferredWidth: root.compactMode ? -1 : 165
                Layout.maximumWidth: root.compactMode ? 1000 : 190
                spacing: 8

                Rectangle {
                    visible: !root.compactMode
                    Layout.preferredWidth: 46
                    Layout.preferredHeight: 28
                    radius: Theme.radiusSmall
                    color: Theme.cardSurface
                    border.width: 1
                    border.color: Theme.borderHairline
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: ApplicationController.playlist.activeThumbnailUrl
                        visible: source.toString().length > 0
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        mipmap: true
                    }

                    StudioIcon {
                        anchors.centerIn: parent
                        visible: ApplicationController.playlist.activeThumbnailUrl.length === 0
                        name: "film"
                        size: 14
                        color: Theme.textSecondary
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text: ApplicationController.playlist.activeTitle.length > 0
                              ? ApplicationController.playlist.activeTitle
                              : qsTr("Ready for Playback")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        text: ApplicationController.playback.formattedPosition + " / " + root.formatClockMs(root.effectiveDurationMs)
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                    }
                }
            }

            // Center Transport Cluster: Previous, Prominent Circular Play/Pause, Next
            RowLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: 6

                StudioButton {
                    variant: "ghost"
                    iconName: "skip-back"
                    iconSize: 15
                    implicitWidth: 30
                    implicitHeight: 30
                    leftPadding: 0
                    rightPadding: 0
                    onClicked: ApplicationController.playlist.activatePrevious()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Previous item")
                }

                Rectangle {
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    radius: 19
                    color: playHover.containsMouse ? Theme.accentHover : Theme.accentPrimary

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.animFast
                        }
                    }

                    StudioIcon {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: ApplicationController.playback.paused ? 1 : 0
                        name: ApplicationController.playback.paused ? "play" : "pause"
                        size: 17
                        color: Theme.textOnAccent
                    }

                    MouseArea {
                        id: playHover

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: ApplicationController.playback.togglePause()
                    }
                }

                StudioButton {
                    variant: "ghost"
                    iconName: "skip-forward"
                    iconSize: 15
                    implicitWidth: 30
                    implicitHeight: 30
                    leftPadding: 0
                    rightPadding: 0
                    onClicked: ApplicationController.playlist.activateNext()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Next item")
                }
            }

            // Right Background Music switch + Fade + Output Volume
            RowLayout {
                visible: !root.compactMode
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight
                spacing: 7

                Item {
                    Layout.fillWidth: true
                }

                StudioIcon {
                    name: "audio-lines"
                    size: 14
                    color: ApplicationController.backgroundMusic.playing ? Theme.accentPrimary : Theme.textSecondary
                }

                Text {
                    text: qsTr("Background Music")
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: Font.Medium
                    visible: root.width > 580
                }

                StudioSwitch {
                    checked: ApplicationController.backgroundMusic.playing
                    onToggled: {
                        if (checked) {
                            ApplicationController.backgroundMusic.play()
                        } else {
                            ApplicationController.backgroundMusic.fadeOutAndStop(4000)
                        }
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Toggle congregation entrance background music")
                }

                StudioButton {
                    visible: ApplicationController.backgroundMusic.playing
                    variant: "ghost"
                    iconName: "sliders-horizontal"
                    iconSize: 14
                    implicitWidth: 26
                    implicitHeight: 26
                    leftPadding: 0
                    rightPadding: 0
                    onClicked: ApplicationController.backgroundMusic.fadeOutAndStop(4000)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Fade out background music")
                }

                StudioButton {
                    variant: "ghost"
                    iconName: ApplicationController.playback.muted ? "volume-x" : "volume-2"
                    iconSize: 15
                    implicitWidth: 26
                    implicitHeight: 26
                    leftPadding: 0
                    rightPadding: 0
                    onClicked: ApplicationController.playback.muted = !ApplicationController.playback.muted
                    ToolTip.visible: hovered
                    ToolTip.text: ApplicationController.playback.muted ? qsTr("Unmute audio") : qsTr("Mute audio")
                }

                StudioSlider {
                    Layout.preferredWidth: 70
                    Layout.maximumWidth: 76
                    implicitWidth: 70
                    from: 0
                    to: 100
                    value: ApplicationController.playback.volume
                    onMoved: ApplicationController.playback.volume = Math.round(value)
                }
            }
        }

        // Bottom Tier: Full-width Precision Timeline Scrub Bar
        StudioSlider {
            Layout.fillWidth: true
            implicitHeight: 18
            trackHeight: 4
            handleDiameter: 11
            from: 0
            to: Math.max(1, root.effectiveDurationMs)
            value: Math.min(to, ApplicationController.playback.positionMs)
            enabled: root.effectiveDurationMs > 0
            onMoved: ApplicationController.playback.seekMs(Math.round(value))
        }
    }
}
