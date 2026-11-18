import QtQuick
import QtQuick.Layouts

import KMedia.UI

RowLayout {
    id: root

    required property string labelText
    required property color indicatorColor

    spacing: Theme.spacingSm

    Rectangle {
        Layout.preferredWidth: 8
        Layout.preferredHeight: 8
        radius: 4
        color: root.indicatorColor
    }

    Text {
        Layout.fillWidth: true
        text: root.labelText
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
        elide: Text.ElideRight
    }
}
