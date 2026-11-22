import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import ".."

T.Menu {
    id: root

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    margins: 8
    padding: 6
    overlap: 1

    delegate: StudioMenuItem {}

    contentItem: ListView {
        implicitHeight: contentHeight
        model: root.contentModel
        interactive: Window.window ? contentHeight + root.topPadding + root.bottomPadding > Window.window.height : false
        clip: true
        currentIndex: root.currentIndex

        ScrollIndicator.vertical: ScrollIndicator {}
    }

    background: Rectangle {
        implicitWidth: 220
        implicitHeight: 40
        radius: Theme.radiusMedium
        color: Theme.cardElevated
        border.width: 1
        border.color: Theme.borderStrong
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.animFast
        }
    }
}
