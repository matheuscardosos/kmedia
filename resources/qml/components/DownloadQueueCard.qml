pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

StudioCard {
    id: root

    property bool compactMode: false

    implicitHeight: queueCol.implicitHeight + 28
    surfaceColor: Theme.panelSurface

    function statusColorFor(statusInt) {
        if (statusInt === 1) {
            return Theme.accentPrimary
        }
        if (statusInt === 2) {
            return Theme.statusStandby
        }
        if (statusInt === 3 || statusInt === 4) {
            return Theme.statusReady
        }
        if (statusInt === 6) {
            return Theme.statusLive
        }
        return Theme.statusOff
    }

    ColumnLayout {
        id: queueCol

        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioIcon {
                name: "download"
                size: 18
                color: ApplicationController.downloads.activeCount > 0 ? Theme.accentPrimary : Theme.textSecondary
            }

            Text {
                text: qsTr("Transfers (%1 active, %2 queued)")
                      .arg(ApplicationController.downloads.activeCount)
                      .arg(ApplicationController.downloads.queuedCount)
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            StudioButton {
                visible: ApplicationController.downloads.activeCount > 0 || ApplicationController.downloads.queuedCount > 0
                variant: "secondary"
                iconName: "pause"
                text: root.compactMode ? "" : qsTr("Pause All")
                implicitHeight: 28
                leftPadding: root.compactMode ? 8 : 10
                rightPadding: root.compactMode ? 8 : 10
                onClicked: ApplicationController.downloads.pauseAll()
            }

            StudioButton {
                visible: ApplicationController.downloads.finishedCount > 0
                variant: "ghost"
                iconName: "trash"
                text: root.compactMode ? "" : qsTr("Clear")
                implicitHeight: 28
                leftPadding: root.compactMode ? 8 : 10
                rightPadding: root.compactMode ? 8 : 10
                onClicked: ApplicationController.downloads.clearFinished()
            }

            RowLayout {
                spacing: 4

                StudioButton {
                    variant: "secondary"
                    text: "-"
                    implicitWidth: 28
                    implicitHeight: 28
                    leftPadding: 0
                    rightPadding: 0
                    enabled: ApplicationController.downloads.maxConcurrent > 1
                    onClicked: ApplicationController.downloads.maxConcurrent = ApplicationController.downloads.maxConcurrent - 1
                }

                Text {
                    text: ApplicationController.downloads.maxConcurrent + "x"
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: Font.DemiBold
                }

                StudioButton {
                    variant: "secondary"
                    text: "+"
                    implicitWidth: 28
                    implicitHeight: 28
                    leftPadding: 0
                    rightPadding: 0
                    enabled: ApplicationController.downloads.maxConcurrent < 6
                    onClicked: ApplicationController.downloads.maxConcurrent = ApplicationController.downloads.maxConcurrent + 1
                }
            }
        }

        Text {
            visible: ApplicationController.downloads.tasks.length === 0
            Layout.fillWidth: true
            text: qsTr("No active or recent transfers.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
        }

        Repeater {
            model: ApplicationController.downloads.tasks

            delegate: Rectangle {
                id: taskRow

                required property var modelData

                Layout.fillWidth: true
                Layout.preferredHeight: taskInner.implicitHeight + 16
                radius: Theme.radiusSmall
                color: Theme.cardSurface
                border.width: 1
                border.color: Theme.borderHairline

                ColumnLayout {
                    id: taskInner

                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: taskRow.modelData.title
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            visible: !root.compactMode
                            text: taskRow.modelData.sizeText
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        StudioBadge {
                            text: taskRow.modelData.statusLabel
                            showDot: true
                            dotColor: root.statusColorFor(taskRow.modelData.status)
                            hasBorder: true
                        }

                        StudioButton {
                            visible: Boolean(taskRow.modelData.canPause)
                            variant: "ghost"
                            iconName: "pause"
                            implicitWidth: 28
                            implicitHeight: 26
                            leftPadding: 0
                            rightPadding: 0
                            onClicked: ApplicationController.downloads.pause(taskRow.modelData.taskId)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Pause")
                        }

                        StudioButton {
                            visible: Boolean(taskRow.modelData.canResume)
                            variant: "ghost"
                            iconName: "play"
                            implicitWidth: 28
                            implicitHeight: 26
                            leftPadding: 0
                            rightPadding: 0
                            onClicked: ApplicationController.downloads.resume(taskRow.modelData.taskId)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Resume")
                        }

                        StudioButton {
                            visible: Boolean(taskRow.modelData.canRetry)
                            variant: "ghost"
                            iconName: "refresh-cw"
                            implicitWidth: 28
                            implicitHeight: 26
                            leftPadding: 0
                            rightPadding: 0
                            onClicked: ApplicationController.downloads.retryTask(taskRow.modelData.taskId)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Retry")
                        }

                        StudioButton {
                            visible: Boolean(taskRow.modelData.canCancel)
                            variant: "ghost"
                            iconName: "x"
                            implicitWidth: 28
                            implicitHeight: 26
                            leftPadding: 0
                            rightPadding: 0
                            onClicked: ApplicationController.downloads.cancel(taskRow.modelData.taskId)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Cancel")
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 4
                        radius: 2
                        color: Theme.borderHairline

                        Rectangle {
                            width: Math.max(0, Math.min(parent.width, parent.width * taskRow.modelData.progress))
                            height: parent.height
                            radius: 2
                            color: root.statusColorFor(taskRow.modelData.status)
                        }
                    }

                    Text {
                        visible: taskRow.modelData.errorMessage.length > 0
                        Layout.fillWidth: true
                        text: taskRow.modelData.errorMessage
                        color: Theme.statusLive
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
