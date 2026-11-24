import QtQuick

import KMedia.UI

// What the presentation screen shows for the active media, drawn small for the operator: live
// frames of a playing video, or the picture or PDF page with the same zoom and pan.
Item {
    id: surface

    readonly property int mediaKind: ApplicationController.playback.mediaKind
    readonly property bool standby: ApplicationController.playback.standbyActive

    // Drag to pan and Ctrl or Shift with the wheel to zoom toward the pointer (inspired by Meeting
    // Media Manager); the presentation screen follows.
    property bool interactive: false
    readonly property bool zoomable: surface.interactive && !surface.standby
                                     && (surface.mediaKind === PlaybackController.Image
                                         || surface.mediaKind === PlaybackController.Pdf
                                         || surface.mediaKind === PlaybackController.Video)
    readonly property real maxZoom: 5.0

    clip: true

    // mpv pans a video in parts of the zoomed video, pictures move in parts of the screen.
    function panUnit(zoom) {
        return surface.mediaKind === PlaybackController.Video ? zoom : 1.0
    }

    // The size the media is drawn at before zooming.
    function contentSize() {
        if (surface.mediaKind !== PlaybackController.Video && picture.paintedWidth > 0)
            return Qt.size(picture.paintedWidth, picture.paintedHeight)
        return Qt.size(surface.width, surface.height)
    }

    // Sets the zoom and the offset in pixels, keeping the media over its own frame: it moves at
    // most half of what the zoom adds, so not at all at zoom 1.
    function applyViewport(zoom, offsetX, offsetY) {
        if (surface.width <= 0 || surface.height <= 0)
            return
        const z = Math.max(1.0, Math.min(surface.maxZoom, zoom))
        const size = surface.contentSize()
        const maxX = size.width * (z - 1) / 2
        const maxY = size.height * (z - 1) / 2
        const x = Math.max(-maxX, Math.min(maxX, offsetX))
        const y = Math.max(-maxY, Math.min(maxY, offsetY))
        ApplicationController.playback.setViewport(z, x / (surface.width * surface.panUnit(z)),
                                                   y / (surface.height * surface.panUnit(z)))
        persistTimer.restart()
    }

    function offsetX() {
        return ApplicationController.playback.panX * surface.width
               * surface.panUnit(ApplicationController.playback.zoomScale)
    }

    function offsetY() {
        return ApplicationController.playback.panY * surface.height
               * surface.panUnit(ApplicationController.playback.zoomScale)
    }

    // Zooms keeping the point under (x, y) in place; the middle when no point is given.
    function zoomAt(zoom, x, y) {
        const oldZoom = ApplicationController.playback.zoomScale
        const newZoom = Math.max(1.0, Math.min(surface.maxZoom, zoom))
        const pointX = (x === undefined ? surface.width / 2 : x) - surface.width / 2
        const pointY = (y === undefined ? surface.height / 2 : y) - surface.height / 2
        const growth = newZoom / oldZoom
        surface.applyViewport(newZoom,
                              pointX - (pointX - surface.offsetX()) * growth,
                              pointY - (pointY - surface.offsetY()) * growth)
    }

    // Saved with the item once the gesture settles, not on every step.
    Timer {
        id: persistTimer

        interval: 300
        onTriggered: ApplicationController.playlist.updateActiveViewport(
                         ApplicationController.playback.zoomScale,
                         ApplicationController.playback.panX,
                         ApplicationController.playback.panY)
    }

    // A playing video shows live frames grabbed from the presentation player.
    Image {
        anchors.fill: parent
        visible: surface.mediaKind === PlaybackController.Video
                 && !surface.standby
                 && ApplicationController.playback.previewSerial > 0
        source: visible ? "image://kmedia-preview/" + ApplicationController.playback.previewSerial : ""
        cache: false
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Item {
        id: graphicFrame

        anchors.fill: parent
        visible: (surface.mediaKind === PlaybackController.Image || surface.mediaKind === PlaybackController.Pdf)
                 && !surface.standby

        transform: [
            Scale {
                origin.x: graphicFrame.width * 0.5
                origin.y: graphicFrame.height * 0.5
                xScale: ApplicationController.playback.zoomScale
                yScale: ApplicationController.playback.zoomScale
            },
            Translate {
                x: ApplicationController.playback.panX * graphicFrame.width
                y: ApplicationController.playback.panY * graphicFrame.height
            }
        ]

        Image {
            id: picture

            anchors.fill: parent
            source: !graphicFrame.visible
                    ? ""
                    : (surface.mediaKind === PlaybackController.Pdf
                       ? ApplicationController.pdf.currentPageImageUrl
                       : ApplicationController.playback.source)
            fillMode: Image.PreserveAspectFit
            // PDF pages load at once so turning a page does not flash blank.
            asynchronous: surface.mediaKind !== PlaybackController.Pdf
            smooth: true
            mipmap: true
        }
    }

    MouseArea {
        id: panArea

        property point pressPoint
        property real pressOffsetX
        property real pressOffsetY
        property bool panning: false

        anchors.fill: parent
        enabled: surface.zoomable
        acceptedButtons: Qt.LeftButton
        cursorShape: ApplicationController.playback.zoomScale > 1.01
                     ? (panArea.panning ? Qt.ClosedHandCursor : Qt.OpenHandCursor)
                     : Qt.ArrowCursor

        onPressed: (mouse) => {
            panArea.pressPoint = Qt.point(mouse.x, mouse.y)
            panArea.pressOffsetX = surface.offsetX()
            panArea.pressOffsetY = surface.offsetY()
            panArea.panning = false
            // At zoom 1 there is nothing to pan, so the press goes on to what is below.
            mouse.accepted = ApplicationController.playback.zoomScale > 1.01
        }
        onPositionChanged: (mouse) => {
            const dx = mouse.x - panArea.pressPoint.x
            const dy = mouse.y - panArea.pressPoint.y
            if (!panArea.panning && Math.hypot(dx, dy) < 3)
                return
            panArea.panning = true
            surface.applyViewport(ApplicationController.playback.zoomScale,
                                  panArea.pressOffsetX + dx, panArea.pressOffsetY + dy)
        }
        onReleased: panArea.panning = false
        onCanceled: panArea.panning = false

        onWheel: (wheel) => {
            if (!(wheel.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))) {
                wheel.accepted = false
                return
            }
            // Shift with the wheel turns into a sideways scroll on some systems.
            const delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
            const step = Math.max(-0.1, Math.min(0.1, delta / 1200))
            surface.zoomAt(ApplicationController.playback.zoomScale * Math.exp(step), wheel.x, wheel.y)
        }
    }
}
