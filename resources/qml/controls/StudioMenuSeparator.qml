import QtQuick
import QtQuick.Templates as T
import ".."

T.MenuSeparator {
    id: root

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: visible ? Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                       implicitContentHeight + topPadding + bottomPadding) : 0
    height: visible ? implicitHeight : 0
    topPadding: 4
    bottomPadding: 4

    contentItem: Rectangle {
        implicitWidth: 180
        implicitHeight: 1
        color: Theme.borderHairline
    }
}
