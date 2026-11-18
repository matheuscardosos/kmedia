import QtQuick
import QtQuick.Controls.Basic

import KMedia.UI

Button {
    id: root

    required property string labelText
    property bool active: false

    implicitHeight: 36
    hoverEnabled: true
    checkable: false

    Accessible.name: root.labelText
    Accessible.role: Accessible.Button

    background: Rectangle {
        radius: Theme.radiusSmall
        color: {
            if (root.active) {
                return Theme.accentSoft
            }
            if (root.hovered) {
                return Theme.cardSurface
            }
            return "transparent"
        }
        border.width: root.active ? 1 : 0
        border.color: root.active ? Theme.accentPrimary : "transparent"

        Rectangle {
            width: 3
            height: 18
            radius: 2
            visible: root.active
            color: Theme.accentPrimary
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    contentItem: Text {
        text: root.labelText
        color: root.active ? Theme.textPrimary : Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeMd
        font.weight: root.active ? Font.DemiBold : Font.Normal
        verticalAlignment: Text.AlignVCenter
        leftPadding: 12
        elide: Text.ElideRight
    }
}
