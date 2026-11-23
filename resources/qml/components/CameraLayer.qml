import QtQuick
import QtMultimedia

import KMedia.UI

// The chosen camera as the background of the presentation screen. Kept in its own file and
// loaded on demand, so Qt Multimedia only starts when a camera is shown.
VideoOutput {
    id: cameraOutput

    fillMode: VideoOutput.PreserveAspectFit

    Component.onCompleted: ApplicationController.camera.bindVideoOutput(cameraOutput)
    Component.onDestruction: ApplicationController.camera.bindVideoOutput(null)
}
