pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// Background music: the songbook in the media language, timed to end when a meeting starts.
ColumnLayout {
    id: root

    readonly property var engine: ApplicationController.backgroundMusic
    readonly property var music: ApplicationController.music

    signal closeRequested()

    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        StudioIcon {
            name: "audio-lines"
            size: 17
            color: root.engine.playing ? Theme.accentPrimary : Theme.textSecondary
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
                text: root.music.loading
                      ? qsTr("Loading the songbook...")
                      : (root.engine.playing && root.engine.trackCount > 0
                         ? root.engine.currentTrackTitle
                         : qsTr("Songs in the media language"))
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        StudioBadge {
            text: root.engine.fading
                  ? qsTr("Fading")
                  : (root.engine.playing ? qsTr("Live") : qsTr("Idle"))
            showDot: true
            dotColor: root.engine.fading
                      ? Theme.statusStandby
                      : (root.engine.playing ? Theme.statusReady : Theme.statusOff)
            hasBorder: true
        }
    }

    Text {
        visible: root.engine.playing && !isNaN(root.music.stopTime.getTime())
        text: qsTr("Ends at %1, when the meeting starts").arg(Qt.formatTime(root.music.stopTime, "HH:mm"))
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
        elide: Text.ElideRight
        Layout.fillWidth: true
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
            enabled: root.engine.playing
            onClicked: root.engine.previous()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Previous track")
        }

        StudioButton {
            variant: "primary"
            iconName: root.engine.playing ? "square" : "play"
            text: root.engine.playing ? qsTr("Stop") : qsTr("Play")
            enabled: !root.music.loading && !root.engine.fading
            Layout.fillWidth: true
            onClicked: root.music.toggle()
        }

        StudioButton {
            variant: "secondary"
            iconName: "skip-forward"
            implicitWidth: 36
            leftPadding: 0
            rightPadding: 0
            enabled: root.engine.playing
            onClicked: root.engine.next()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Next track")
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
            value: root.engine.targetVolume
            onMoved: root.engine.targetVolume = Math.round(value)
        }

        Text {
            text: root.engine.targetVolume + "%"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            Layout.preferredWidth: 32
            horizontalAlignment: Text.AlignRight
        }
    }
}
