pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

import KMedia.UI
import "../controls"

// A small live preview that floats over the window while a picture, PDF page, or video is on the
// presentation screen, inspired by Meeting Media Manager. Drag it to move it, drag a corner to
// resize it, and click it to see it large in the middle of the window.
Item {
    id: root

    // Set by the window: whether the preview should be on screen now.
    property bool shown: false
    property bool expanded: false
    // The bars at the bottom of the window the collapsed preview keeps clear of.
    property real bottomInset: 0

    readonly property real areaHeight: Math.max(0, root.height - root.bottomInset)
    readonly property bool narrow: root.width < 600
    readonly property real edgeMargin: 8
    readonly property real maxBoxWidth: Math.min(root.width * 0.6, 520,
                                                 root.width - 2 * root.edgeMargin,
                                                 (root.areaHeight - 2 * root.edgeMargin) * 16 / 9)
    // The minimum gives way in a window too small for it.
    readonly property real minBoxWidth: Math.max(48, Math.min(root.narrow ? 132 : 180, root.maxBoxWidth))

    // Where the collapsed preview sits, measured from the bottom right corner so it follows a
    // resized window.
    property real boxWidth: root.narrow ? Math.min(root.width * 0.44, 220) : Math.min(root.width * 0.28, 320)
    property real rightOffset: root.narrow ? 8 : 16
    property real bottomOffset: root.narrow ? 8 : 16

    readonly property real collapsedWidth: Math.max(root.minBoxWidth, Math.min(root.boxWidth, root.maxBoxWidth))
    readonly property real collapsedHeight: root.collapsedWidth * 9 / 16
    readonly property real collapsedX: root.clamp(root.width - root.rightOffset - root.collapsedWidth,
                                                  root.edgeMargin,
                                                  root.width - root.edgeMargin - root.collapsedWidth)
    readonly property real collapsedY: root.clamp(root.areaHeight - root.bottomOffset - root.collapsedHeight,
                                                  root.edgeMargin,
                                                  root.areaHeight - root.edgeMargin - root.collapsedHeight)
    readonly property real expandedWidth: Math.min(root.width * 0.9, root.height * 0.9 * 16 / 9)

    function clamp(value, low, high) {
        return Math.max(low, Math.min(value, Math.max(low, high)))
    }

    function placeBox(x, y, width) {
        root.boxWidth = width
        root.rightOffset = root.width - x - width
        root.bottomOffset = root.areaHeight - y - width * 9 / 16
    }

    // Where keyboard focus was before the large view took it.
    property Item focusBeforeExpand: null

    onShownChanged: {
        if (!root.shown)
            root.expanded = false
    }
    onExpandedChanged: {
        if (root.expanded) {
            root.focusBeforeExpand = root.Window.activeFocusItem
            box.forceActiveFocus()
        } else {
            if (root.focusBeforeExpand)
                root.focusBeforeExpand.forceActiveFocus()
            root.focusBeforeExpand = null
        }
    }

    // Behind the large preview: dims the window and closes it on a click.
    Rectangle {
        anchors.fill: parent
        color: "#80000000"
        opacity: root.expanded && root.shown ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: 250
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.expanded = false
        }
    }

    Rectangle {
        id: box

        width: root.expanded ? root.expandedWidth : root.collapsedWidth
        height: width * 9 / 16
        x: root.expanded ? (root.width - width) / 2 : root.collapsedX
        // Slides up a little as it fades in.
        property real slide: root.shown ? 0 : 8

        y: (root.expanded ? (root.height - height) / 2 : root.collapsedY) + box.slide
        radius: 8
        color: "#000000"
        border.width: 1
        border.color: Theme.borderHairline
        opacity: root.shown ? 1.0 : 0.0
        visible: opacity > 0.0
        clip: true

        Accessible.role: Accessible.Graphic
        Accessible.name: qsTr("Media preview")

        Behavior on opacity {
            NumberAnimation {
                duration: 250
            }
        }

        Behavior on slide {
            NumberAnimation {
                duration: 250
            }
        }

        // Any key closes the large preview, before the window shortcuts can act on it.
        Keys.onShortcutOverride: (event) => {
            if (root.expanded)
                event.accepted = true
        }
        Keys.onPressed: (event) => {
            if (root.expanded) {
                root.expanded = false
                event.accepted = true
            }
        }

        LivePreviewSurface {
            anchors.fill: parent
            anchors.margins: 1
        }

        // Drags move the collapsed preview; a click without a drag toggles the large view.
        MouseArea {
            id: moveArea

            property point pressPoint
            property real pressRight
            property real pressBottom
            property bool dragging: false

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: root.expanded ? Qt.PointingHandCursor : (moveArea.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor)

            onPressed: (mouse) => {
                moveArea.pressPoint = mapToItem(root, mouse.x, mouse.y)
                moveArea.pressRight = root.width - root.collapsedX - root.collapsedWidth
                moveArea.pressBottom = root.areaHeight - root.collapsedY - root.collapsedHeight
                moveArea.dragging = false
            }
            onPositionChanged: (mouse) => {
                if (!moveArea.pressed || root.expanded)
                    return
                const point = mapToItem(root, mouse.x, mouse.y)
                const dx = point.x - moveArea.pressPoint.x
                const dy = point.y - moveArea.pressPoint.y
                if (!moveArea.dragging && Math.abs(dx) <= 3 && Math.abs(dy) <= 3)
                    return
                moveArea.dragging = true
                root.rightOffset = moveArea.pressRight - dx
                root.bottomOffset = moveArea.pressBottom - dy
            }
            onReleased: {
                if (moveArea.dragging) {
                    // Keep what was dragged past an edge inside the window.
                    root.placeBox(root.collapsedX, root.collapsedY, root.collapsedWidth)
                } else {
                    root.expanded = !root.expanded
                }
                moveArea.dragging = false
            }

            ToolTip.visible: moveArea.containsMouse && !moveArea.pressed && !root.expanded
            ToolTip.delay: 1000
            ToolTip.text: qsTr("Media preview")
        }

        // Resize grips on the corners, shown on hover; the opposite corner stays put.
        Repeater {
            model: [
                { sx: -1, sy: -1 },
                { sx: 1, sy: -1 },
                { sx: -1, sy: 1 },
                { sx: 1, sy: 1 }
            ]

            delegate: MouseArea {
                id: grip

                required property var modelData

                property point pressPoint
                property real fixedX
                property real fixedY
                property real pressWidth

                width: 22
                height: 22
                x: grip.modelData.sx < 0 ? 0 : box.width - width
                y: grip.modelData.sy < 0 ? 0 : box.height - height
                visible: !root.expanded
                hoverEnabled: true
                cursorShape: grip.modelData.sx === grip.modelData.sy ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                opacity: moveArea.containsMouse || grip.containsMouse || grip.pressed ? 1.0 : 0.0

                onPressed: (mouse) => {
                    grip.pressPoint = mapToItem(root, mouse.x, mouse.y)
                    grip.pressWidth = root.collapsedWidth
                    grip.fixedX = grip.modelData.sx > 0 ? root.collapsedX : root.collapsedX + root.collapsedWidth
                    grip.fixedY = grip.modelData.sy > 0 ? root.collapsedY : root.collapsedY + root.collapsedHeight
                }
                onPositionChanged: (mouse) => {
                    if (!grip.pressed)
                        return
                    const point = mapToItem(root, mouse.x, mouse.y)
                    const growX = grip.modelData.sx * (point.x - grip.pressPoint.x)
                    const growY = grip.modelData.sy * (point.y - grip.pressPoint.y) * 16 / 9
                    const roomX = grip.modelData.sx > 0 ? root.width - root.edgeMargin - grip.fixedX
                                                        : grip.fixedX - root.edgeMargin
                    const roomY = (grip.modelData.sy > 0 ? root.areaHeight - root.edgeMargin - grip.fixedY
                                                         : grip.fixedY - root.edgeMargin) * 16 / 9
                    const width = root.clamp(grip.pressWidth + Math.max(growX, growY), root.minBoxWidth,
                                             Math.min(root.maxBoxWidth, roomX, roomY))
                    root.placeBox(grip.modelData.sx > 0 ? grip.fixedX : grip.fixedX - width,
                                  grip.modelData.sy > 0 ? grip.fixedY : grip.fixedY - width * 9 / 16,
                                  width)
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: 150
                    }
                }

                // A small white bracket in the corner.
                Rectangle {
                    x: grip.modelData.sx < 0 ? 6 : grip.width - 16
                    y: grip.modelData.sy < 0 ? 6 : grip.height - 8
                    width: 10
                    height: 2
                    color: "#FFFFFF"
                }

                Rectangle {
                    x: grip.modelData.sx < 0 ? 6 : grip.width - 8
                    y: grip.modelData.sy < 0 ? 6 : grip.height - 16
                    width: 2
                    height: 10
                    color: "#FFFFFF"
                }
            }
        }

        // How far a video has played, in the large preview.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            height: 18
            radius: 9
            color: "#66000000"
            visible: root.expanded && ApplicationController.playback.mediaKind === PlaybackController.Video
                     && ApplicationController.playback.durationMs > 0

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                height: 2
                color: "#59FFFFFF"

                Rectangle {
                    height: parent.height
                    width: parent.width * Math.max(0, Math.min(1, ApplicationController.playback.positionMs
                                                                   / ApplicationController.playback.durationMs))
                    color: "#FFFFFF"
                }
            }
        }

        StudioButton {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 10
            visible: root.expanded
            variant: "secondary"
            iconName: "x"
            leftPadding: 0
            rightPadding: 0
            width: 40
            height: 40
            onClicked: root.expanded = false
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Close")
        }
    }
}
