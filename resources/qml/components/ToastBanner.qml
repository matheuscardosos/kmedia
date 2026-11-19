pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import ".."
import "../controls"

Rectangle {
    id: root

    property string message: ""
    property string level: "info"
    property bool showing: false

    function displayToast(text, severity) {
        message = text
        level = severity || "info"
        showing = true
        hideTimer.restart()
    }

    visible: opacity > 0.01
    opacity: showing ? 1.0 : 0.0
    implicitWidth: Math.min(460, toastRow.implicitWidth + 28)
    implicitHeight: 40
    radius: Theme.radiusMedium
    color: Theme.cardElevated
    border.width: 1
    border.color: level === "warning" ? Theme.statusStandby : Theme.borderStrong

    Behavior on opacity {
        NumberAnimation {
            duration: Theme.animFast
        }
    }

    Timer {
        id: hideTimer

        interval: 2800
        repeat: false
        onTriggered: root.showing = false
    }

    RowLayout {
        id: toastRow

        anchors.centerIn: parent
        spacing: 8

        StudioIcon {
            name: root.level === "warning" ? "triangle-exclamation-point"
                : (root.level === "success" ? "circle-check" : "info")
            size: 15
            color: root.level === "warning" ? Theme.statusStandby
                 : (root.level === "success" ? Theme.statusReady : Theme.accentPrimary)
        }

        Text {
            text: root.message
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            font.weight: Font.Medium
            elide: Text.ElideRight
            Layout.maximumWidth: 380
        }
    }
}
