pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

ColumnLayout {
    id: root

    signal closeRequested()

    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        StudioIcon {
            name: "audio-lines"
            size: 17
            color: ApplicationController.backgroundMusic.playing ? Theme.accentPrimary : Theme.textSecondary
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            Text {
                text: qsTr("Background Music")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            Text {
                text: ApplicationController.backgroundMusic.trackCount > 0
                      ? ApplicationController.backgroundMusic.currentTrackTitle
                      : qsTr("No ambient tracks queued")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        StudioBadge {
            text: ApplicationController.backgroundMusic.fading
                  ? qsTr("Fading")
                  : (ApplicationController.backgroundMusic.playing ? qsTr("Live") : qsTr("Idle"))
            showDot: true
            dotColor: ApplicationController.backgroundMusic.fading
                      ? Theme.statusStandby
                      : (ApplicationController.backgroundMusic.playing ? Theme.statusReady : Theme.statusOff)
            hasBorder: true
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        StudioButton {
            variant: "secondary"
            iconName: "skip-back"
            implicitWidth: 36
            leftPadding: 0
            rightPadding: 0
            enabled: ApplicationController.backgroundMusic.trackCount > 0
            onClicked: ApplicationController.backgroundMusic.previous()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Previous track")
        }

        StudioButton {
            variant: "primary"
            iconName: ApplicationController.backgroundMusic.playing ? "pause" : "play"
            text: ApplicationController.backgroundMusic.playing ? qsTr("Pause") : qsTr("Play")
            Layout.fillWidth: true
            onClicked: {
                if (ApplicationController.backgroundMusic.playing) {
                    ApplicationController.backgroundMusic.pause()
                } else {
                    ApplicationController.backgroundMusic.play()
                }
            }
        }

        StudioButton {
            variant: "secondary"
            iconName: "skip-forward"
            implicitWidth: 36
            leftPadding: 0
            rightPadding: 0
            enabled: ApplicationController.backgroundMusic.trackCount > 0
            onClicked: ApplicationController.backgroundMusic.next()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Next track")
        }

        StudioButton {
            variant: "secondary"
            iconName: "square"
            implicitWidth: 36
            leftPadding: 0
            rightPadding: 0
            enabled: ApplicationController.backgroundMusic.playing
            onClicked: ApplicationController.backgroundMusic.stop()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Stop music")
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        StudioIcon {
            name: "volume-2"
            size: 15
            color: Theme.textSecondary
        }

        StudioSlider {
            Layout.fillWidth: true
            from: 0
            to: 100
            stepSize: 1
            value: ApplicationController.backgroundMusic.targetVolume
            onMoved: ApplicationController.backgroundMusic.targetVolume = Math.round(value)
        }

        Text {
            text: ApplicationController.backgroundMusic.targetVolume + "%"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            Layout.preferredWidth: 32
            horizontalAlignment: Text.AlignRight
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        RowLayout {
            spacing: 6

            Text {
                text: qsTr("Shuffle")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
            }

            StudioSwitch {
                checked: ApplicationController.backgroundMusic.shuffle
                onToggled: ApplicationController.backgroundMusic.shuffle = checked
            }
        }

        Item {
            Layout.fillWidth: true
        }

        StudioButton {
            variant: "secondary"
            iconName: "audio-lines"
            text: qsTr("Fade Out")
            enabled: ApplicationController.backgroundMusic.playing && !ApplicationController.backgroundMusic.fading
            onClicked: {
                ApplicationController.backgroundMusic.fadeOutAndStop(-1)
                root.closeRequested()
            }
        }
    }
}
