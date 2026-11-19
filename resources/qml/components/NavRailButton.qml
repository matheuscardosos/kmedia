pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Rectangle {
    id: root

    property string label: ""
    property string iconName: "circle-check"
    property bool active: false
    property bool collapsed: false
    signal clicked()

    implicitHeight: 40
    radius: Theme.radiusMedium
    color: active ? Theme.accentSoft : (mouseArea.containsMouse ? Theme.cardHover : "transparent")

    Behavior on color {
        ColorAnimation {
            duration: Theme.animFast
        }
    }

    Rectangle {
        visible: root.active && !root.collapsed
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: 20
        radius: 2
        color: Theme.accentPrimary
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.collapsed ? 0 : 12
        anchors.rightMargin: root.collapsed ? 0 : 12
        spacing: 10

        Item {
            Layout.fillWidth: root.collapsed
        }

        StudioIcon {
            name: root.iconName
            size: 17
            color: root.active ? Theme.accentPrimary : Theme.textSecondary
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            visible: !root.collapsed
            text: root.label
            color: root.active ? Theme.accentPrimary : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
            font.weight: root.active ? Font.DemiBold : Font.Medium
            elide: Text.ElideRight
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
        }

        Item {
            Layout.fillWidth: root.collapsed
        }
    }

    ToolTip.visible: root.collapsed && mouseArea.containsMouse
    ToolTip.delay: 350
    ToolTip.text: root.label

    MouseArea {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
