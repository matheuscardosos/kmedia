pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

ColumnLayout {
    id: root

    signal openSettingsRequested()

    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        StudioIcon {
            name: "obs-studio"
            category: "custom"
            size: 20
            color: ApplicationController.obsClient.connected
                   ? Theme.statusReady
                   : (ApplicationController.processMonitor.obsRunning ? Theme.statusStandby : Theme.textSecondary)
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            Text {
                text: "OBS Studio"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            Text {
                text: ApplicationController.obsClient.host + ":" + ApplicationController.obsClient.port
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                Layout.fillWidth: true
            }
        }

        StudioBadge {
            text: ApplicationController.obsClient.connected
                  ? qsTr("Connected")
                  : (ApplicationController.processMonitor.obsRunning ? qsTr("Detected") : qsTr("Offline"))
            showDot: true
            dotColor: ApplicationController.obsClient.connected
                      ? Theme.statusReady
                      : (ApplicationController.processMonitor.obsRunning ? Theme.statusStandby : Theme.statusOff)
            hasBorder: true
        }
    }

    Text {
        visible: ApplicationController.obsClient.lastError.length > 0 && !ApplicationController.obsClient.connected
        Layout.fillWidth: true
        text: ApplicationController.obsClient.lastError
        color: Theme.statusLive
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
        wrapMode: Text.WordWrap
    }

    Flow {
        visible: ApplicationController.obsClient.availableScenes.length > 0
        Layout.fillWidth: true
        spacing: 6

        Repeater {
            model: ApplicationController.obsClient.availableScenes

            delegate: StudioButton {
                id: sceneBtn

                required property string modelData
                readonly property bool isCurrent: sceneBtn.modelData === ApplicationController.obsClient.currentProgramScene

                variant: sceneBtn.isCurrent ? "primary" : "secondary"
                text: sceneBtn.modelData
                implicitHeight: 28
                leftPadding: 10
                rightPadding: 10
                onClicked: ApplicationController.obsClient.setCurrentProgramScene(sceneBtn.modelData)
            }
        }
    }

    Text {
        visible: ApplicationController.obsClient.availableScenes.length === 0
        Layout.fillWidth: true
        text: ApplicationController.obsClient.connected
              ? qsTr("No scenes returned by OBS WebSocket.")
              : qsTr("Connect to switch OBS scenes live.")
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
        wrapMode: Text.WordWrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        StudioButton {
            variant: ApplicationController.obsClient.connected ? "secondary" : "primary"
            text: ApplicationController.obsClient.connected ? qsTr("Disconnect") : qsTr("Connect")
            Layout.fillWidth: true
            onClicked: {
                if (ApplicationController.obsClient.connected) {
                    ApplicationController.obsClient.disconnectFromObs()
                } else {
                    ApplicationController.obsClient.connectToObs()
                }
            }
        }

        StudioButton {
            visible: ApplicationController.obsClient.connected
            variant: ApplicationController.obsClient.recordingActive ? "danger" : "secondary"
            iconName: "disc"
            text: ApplicationController.obsClient.recordingActive ? qsTr("Stop Rec") : qsTr("Record")
            onClicked: {
                if (ApplicationController.obsClient.recordingActive) {
                    ApplicationController.obsClient.stopRecording()
                } else {
                    ApplicationController.obsClient.startRecording()
                }
            }
        }

        StudioButton {
            variant: "ghost"
            iconName: "settings"
            implicitWidth: 34
            leftPadding: 0
            rightPadding: 0
            onClicked: root.openSettingsRequested()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("OBS settings")
        }
    }
}
