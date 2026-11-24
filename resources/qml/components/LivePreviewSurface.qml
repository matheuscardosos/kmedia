import QtQuick

import KMedia.UI

// What the presentation screen shows for the active media, drawn small for the operator: live
// frames of a playing video, or the picture or PDF page with the same zoom and pan.
Item {
    id: surface

    readonly property int mediaKind: ApplicationController.playback.mediaKind
    readonly property bool standby: ApplicationController.playback.standbyActive

    clip: true

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
}
