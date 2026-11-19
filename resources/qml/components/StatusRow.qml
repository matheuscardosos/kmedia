pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../controls"

Item {
    id: root

    property string iconName: "radio"
    property string label: ""
    property string valueText: ""
    property color statusColor: Theme.statusOff
    property bool collapsed: false

    implicitHeight: root.collapsed ? 28 : 34

    RowLayout {
        anchors.fill: parent
        spacing: 10

        Rectangle {
            Layout.alignment: root.collapsed ? Qt.AlignCenter : Qt.AlignVCenter
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            radius: 11
            color: Qt.rgba(root.statusColor.r, root.statusColor.g, root.statusColor.b, 0.14)

            StudioIcon {
                anchors.centerIn: parent
                name: root.iconName
                size: 12
                color: root.statusColor
            }
        }

        ColumnLayout {
            visible: !root.collapsed
            Layout.fillWidth: true
            spacing: 1

            Text {
                text: root.label
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                font.weight: Font.Medium
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Text {
                text: root.valueText
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    ToolTip.visible: root.collapsed && tipHover.hovered
    ToolTip.delay: 300
    ToolTip.text: root.label + ": " + root.valueText

    HoverHandler {
        id: tipHover
    }
}
