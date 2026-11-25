import QtQuick
import QtQuick.Window

import KMedia.UI

Window {
    id: presentationRoot

    readonly property bool dedicated: ApplicationController.displayManager.hasSecondaryDisplay
                                      || ApplicationController.displayManager.manualScreenIndex >= 0

    title: "KMedia - Presentation"
    color: "#000000"
    visible: ApplicationController.displayManager.presentationWindowVisible
    x: presentationRoot.dedicated
       ? ApplicationController.displayManager.targetGeometry.x
       : ApplicationController.displayManager.targetGeometry.x
         + Math.max(0, Math.round((ApplicationController.displayManager.targetGeometry.width - 960) / 2))
    y: presentationRoot.dedicated
       ? ApplicationController.displayManager.targetGeometry.y
       : ApplicationController.displayManager.targetGeometry.y
         + Math.max(0, Math.round((ApplicationController.displayManager.targetGeometry.height - 540) / 2))
    width: presentationRoot.dedicated
           ? ApplicationController.displayManager.targetGeometry.width
           : Math.min(960, ApplicationController.displayManager.targetGeometry.width)
    height: presentationRoot.dedicated
            ? ApplicationController.displayManager.targetGeometry.height
            : Math.min(540, ApplicationController.displayManager.targetGeometry.height)
    flags: Qt.Window | (presentationRoot.dedicated ? Qt.FramelessWindowHint : 0)
    onClosing: ApplicationController.displayManager.presentationWindowVisible = false

    readonly property int activeKind: ApplicationController.playback.mediaKind
    readonly property bool cameraShown: ApplicationController.camera.active
                                        && !ApplicationController.playback.blackoutActive

    Item {
        id: mediaViewport

        anchors.fill: parent
        clip: true
        // Stays visible under the standby layer, which fades over it.

        MpvRendererItem {
            id: presentationMpv

            anchors.fill: parent
            // Keep the FBO renderer attached while the window is open so loading a video never races.
            visible: true
            opacity: (presentationRoot.activeKind === PlaybackController.Video ||
                presentationRoot.activeKind === PlaybackController.Audio) ? 1.0 : 0.0

            Component.onCompleted: ApplicationController.bindPresentationMpv(presentationMpv)
        }

        Item {
            id: graphicTransformContainer

            anchors.fill: parent
            visible: presentationRoot.activeKind === PlaybackController.Image ||
                presentationRoot.activeKind === PlaybackController.Pdf

            // Eases toward the zoom and pan set in the preview, but not when other media comes in.
            property bool easeViewport: true

            Connections {
                target: ApplicationController.playback
                function onMediaChanged() {
                    graphicTransformContainer.easeViewport = false
                    Qt.callLater(() => graphicTransformContainer.easeViewport = true)
                }
            }

            transform: [
                Scale {
                    origin.x: graphicTransformContainer.width * 0.5
                    origin.y: graphicTransformContainer.height * 0.5
                    xScale: ApplicationController.playback.zoomScale
                    yScale: ApplicationController.playback.zoomScale

                    Behavior on xScale {
                        enabled: graphicTransformContainer.easeViewport
                        SmoothedAnimation { duration: 180; velocity: -1 }
                    }
                    Behavior on yScale {
                        enabled: graphicTransformContainer.easeViewport
                        SmoothedAnimation { duration: 180; velocity: -1 }
                    }
                },
                Translate {
                    x: ApplicationController.playback.panX * graphicTransformContainer.width
                    y: ApplicationController.playback.panY * graphicTransformContainer.height

                    Behavior on x {
                        enabled: graphicTransformContainer.easeViewport
                        SmoothedAnimation { duration: 180; velocity: -1 }
                    }
                    Behavior on y {
                        enabled: graphicTransformContainer.easeViewport
                        SmoothedAnimation { duration: 180; velocity: -1 }
                    }
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
        // Standby and the screen without media fade in and out (inspired by Meeting Media Manager).
        readonly property bool shown: ApplicationController.playback.standbyActive ||
            presentationRoot.activeKind === PlaybackController.None

        z: 100
        color: "#000000"
        opacity: standbyOverlay.shown ? 1.0 : 0.0
        visible: standbyOverlay.opacity > 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: 300
                easing.type: Easing.InOutQuad
            }
        }

        // The chosen camera replaces the standby picture and the yeartext (inspired by Meeting
        // Media Manager).
        Loader {
            anchors.fill: parent
            active: ApplicationController.camera.cameraId.length > 0 && presentationRoot.visible
            visible: presentationRoot.cameraShown
            // By URL, so Qt Multimedia only loads with the camera.
            source: active ? Qt.resolvedUrl("components/CameraLayer.qml") : ""
        }

        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            smooth: true
            visible: ApplicationController.playback.standbyImageUrl.toString().length > 0
                     && !ApplicationController.playback.blackoutActive
                     && !presentationRoot.cameraShown
            source: ApplicationController.playback.standbyImageUrl
        }

        // Without a standby picture, the yeartext (inspired by Meeting Media Manager).
        Text {
            anchors.centerIn: parent
            width: parent.width * 0.8
            visible: ApplicationController.playback.standbyImageUrl.toString().length === 0
                     && !ApplicationController.playback.blackoutActive
                     && !presentationRoot.cameraShown
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
