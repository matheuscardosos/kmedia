import QtQuick
import QtQuick.Layouts
import ".."

Rectangle {
    id: root

    property string text: ""
    property color badgeColor: Theme.panelSurface
    property color textColor: Theme.textPrimary
    property color dotColor: "transparent"
    property color strokeColor: Theme.borderHairline
    property bool hasBorder: false
    property bool showDot: false

    implicitWidth: badgeRow.implicitWidth + 16
    implicitHeight: 24
    radius: Theme.radiusSmall
    color: root.badgeColor
    border.width: root.hasBorder ? 1 : 0
    border.color: root.strokeColor

    RowLayout {
        id: badgeRow

        anchors.centerIn: parent
        spacing: 5

        Rectangle {
            visible: root.showDot
            Layout.preferredWidth: 7
            Layout.preferredHeight: 7
            radius: 3.5
            color: root.dotColor
        }

        Text {
            text: root.text
            color: root.textColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            font.weight: Font.Bold
            font.letterSpacing: 0.4
        }
    }
}
