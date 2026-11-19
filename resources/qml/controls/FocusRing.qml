import QtQuick
import ".."

Rectangle {
    id: root

    property bool active: false
    property int targetRadius: Theme.radiusSmall
    property int radiusOffset: 2

    anchors.fill: parent
    anchors.margins: -3
    radius: root.targetRadius + root.radiusOffset
    color: "transparent"
    border.width: 2
    border.color: Theme.accentPrimary
    visible: root.active
    opacity: root.active ? 0.9 : 0.0

    Behavior on opacity {
        NumberAnimation {
            duration: Theme.animFast
        }
    }
}
