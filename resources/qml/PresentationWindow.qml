import QtQuick
import QtQuick.Window

import KMedia.UI

Window {
    id: presentationRoot

    title: "KMedia - Presentation"
    color: "#000000"
    visible: ApplicationController.displayManager.presentationWindowVisible
    x: ApplicationController.displayManager.targetGeometry.x
    y: ApplicationController.displayManager.targetGeometry.y
    width: ApplicationController.displayManager.targetGeometry.width
    height: ApplicationController.displayManager.targetGeometry.height
    flags: Qt.Window | (ApplicationController.displayManager.hasSecondaryDisplay
        ? Qt.FramelessWindowHint
        : 0)

    readonly property int activeKind: ApplicationController.playback.mediaKind

    Item {
        id: mediaViewport

        anchors.fill: parent
        clip: true
        visible: !ApplicationController.playback.standbyActive

        MpvRendererItem {
            id: presentationMpv

            anchors.fill: parent
            visible: presentationRoot.activeKind === PlaybackController.Video ||
                presentationRoot.activeKind === PlaybackController.Audio

            Component.onCompleted: ApplicationController.bindPresentationMpv(presentationMpv)
        }

        Item {
            id: graphicTransformContainer

            anchors.fill: parent
            visible: presentationRoot.activeKind === PlaybackController.Image ||
                presentationRoot.activeKind === PlaybackController.Pdf

            transform: [
                Scale {
                    origin.x: graphicTransformContainer.width * 0.5
                    origin.y: graphicTransformContainer.height * 0.5
                    xScale: ApplicationController.playback.zoomScale
                    yScale: ApplicationController.playback.zoomScale
                },
                Translate {
                    x: ApplicationController.playback.panX * graphicTransformContainer.width
                    y: ApplicationController.playback.panY * graphicTransformContainer.height
                }
            ]

            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                mipmap: true
                visible: presentationRoot.activeKind === PlaybackController.Image
                source: presentationRoot.activeKind === PlaybackController.Image
                    ? ApplicationController.playback.source
                    : ""
            }

            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                asynchronous: false
                smooth: true
                visible: presentationRoot.activeKind === PlaybackController.Pdf
                source: presentationRoot.activeKind === PlaybackController.Pdf
                    ? ApplicationController.pdf.currentPageImageUrl
                    : ""
                sourceSize.width: Math.max(640, Math.round(presentationRoot.width))
            }
        }
    }

    Rectangle {
        id: standbyOverlay

        anchors.fill: parent
        z: 100
        color: "#000000"
        visible: ApplicationController.playback.standbyActive ||
            presentationRoot.activeKind === PlaybackController.None

        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            smooth: true
            visible: ApplicationController.playback.standbyImageUrl.toString().length > 0
                     && !ApplicationController.playback.blackoutActive
            source: ApplicationController.playback.standbyImageUrl
        }

        // Without a standby picture, the yeartext (inspired by Meeting Media Manager).
        Text {
            anchors.centerIn: parent
            width: parent.width * 0.8
            visible: ApplicationController.playback.standbyImageUrl.toString().length === 0
                     && !ApplicationController.playback.blackoutActive
                     && ApplicationController.yeartext.text.length > 0
            text: ApplicationController.yeartext.text
            color: "#FFFFFF"
            font.family: Theme.fontFamily
            font.pixelSize: Math.max(18, parent.height / 16)
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            lineHeight: 1.15
        }
    }
}
