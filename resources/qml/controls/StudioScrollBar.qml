import QtQuick
import QtQuick.Templates as T
import ".."

T.ScrollBar {
    id: root

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: 2
    visible: root.policy !== T.ScrollBar.AlwaysOff && root.size < 1.0

    contentItem: Rectangle {
        implicitWidth: root.orientation === Qt.Vertical ? 5 : 40
        implicitHeight: root.orientation === Qt.Vertical ? 40 : 5
        radius: 2.5
        color: root.pressed ? Theme.accentPrimary
             : (root.hovered ? Theme.textSecondary : Theme.borderStrong)
        opacity: root.active || root.hovered || root.pressed ? 0.9 : 0.45

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.animFast
            }
        }
    }
}
