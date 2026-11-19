import QtQuick
import QtQuick.Templates as T
import ".."

T.Slider {
    id: root

    property int trackHeight: 4
    property int handleDiameter: 12

    implicitWidth: 96
    implicitHeight: 20
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        implicitWidth: 60
        implicitHeight: root.trackHeight
        width: root.availableWidth
        height: root.trackHeight
        radius: height / 2
        color: Theme.borderHairline

        Rectangle {
            width: Math.max(0, Math.min(parent.width, root.visualPosition * parent.width))
            height: parent.height
            radius: height / 2
            color: root.enabled ? Theme.accentPrimary : Theme.textMuted
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + (root.availableHeight - height) / 2
        implicitWidth: root.handleDiameter
        implicitHeight: root.handleDiameter
        width: root.pressed || root.hovered ? root.handleDiameter + 2 : root.handleDiameter
        height: width
        radius: width / 2
        color: Theme.textOnAccent
        border.width: 2
        border.color: Theme.accentPrimary

        Behavior on width {
            NumberAnimation {
                duration: Theme.animFast
            }
        }

        FocusRing {
            active: root.visualFocus
            targetRadius: 8
        }
    }
}
