import QtQuick
import ".."

Rectangle {
    id: root

    property color surfaceColor: Theme.cardSurface
    property color strokeColor: Theme.borderHairline
    property int cornerRadius: Theme.radiusMedium

    radius: root.cornerRadius
    color: root.surfaceColor
    border.width: 1
    border.color: root.strokeColor
}
