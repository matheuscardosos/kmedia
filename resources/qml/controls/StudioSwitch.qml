import QtQuick
import QtQuick.Templates as T
import ".."

T.Switch {
    id: root

    implicitWidth: 40
    implicitHeight: 22
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    indicator: Rectangle {
        implicitWidth: 40
        implicitHeight: 22
        radius: 11
        color: root.checked
               ? (root.hovered ? Theme.accentHover : Theme.accentPrimary)
               : (root.hovered ? Theme.borderStrong : Theme.borderHairline)

        Behavior on color {
            ColorAnimation {
                duration: Theme.animFast
            }
        }

        Rectangle {
            x: root.checked ? parent.width - width - 3 : 3
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            radius: 8
            color: Theme.textOnAccent

            Behavior on x {
                NumberAnimation {
                    duration: Theme.animFast
                    easing.type: Easing.OutCubic
                }
            }
        }

        FocusRing {
            active: root.visualFocus
            targetRadius: 11
        }
    }
}
