pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// The download queue, opened from the pill bar: every transfer with its progress and actions.
ColumnLayout {
    id: root

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

    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        StudioIcon {
            name: "download"
            size: 17
            color: ApplicationController.downloads.activeCount > 0 ? Theme.accentPrimary : Theme.textSecondary
        }

        Text {
            text: qsTr("Transfer Queue")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
            Layout.fillWidth: true
        }

        StudioBadge {
            text: qsTr("%1 active").arg(ApplicationController.downloads.activeCount)
            showDot: true
            dotColor: ApplicationController.downloads.activeCount > 0 ? Theme.accentPrimary : Theme.statusOff
            hasBorder: true
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        Text {
            text: qsTr("Queued: %1").arg(ApplicationController.downloads.queuedCount)
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            Layout.fillWidth: true
        }

        StudioButton {
            visible: ApplicationController.downloads.activeCount > 0 || ApplicationController.downloads.queuedCount > 0
            variant: "ghost"
            iconName: "pause"
            leftPadding: 0
            rightPadding: 0
            implicitWidth: 28
            implicitHeight: 26
            onClicked: ApplicationController.downloads.pauseAll()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Pause All")
        }

        StudioButton {
            visible: ApplicationController.downloads.finishedCount > 0
            variant: "ghost"
            iconName: "trash"
            leftPadding: 0
            rightPadding: 0
            implicitWidth: 28
            implicitHeight: 26
            onClicked: ApplicationController.downloads.clearFinished()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Clear")
        }

        StudioButton {
            variant: "secondary"
            text: "-"
            implicitWidth: 28
            implicitHeight: 26
            leftPadding: 0
            rightPadding: 0
            enabled: ApplicationController.downloads.maxConcurrent > 1
            onClicked: ApplicationController.downloads.maxConcurrent = ApplicationController.downloads.maxConcurrent - 1
        }

        Text {
            text: ApplicationController.downloads.maxConcurrent + "x"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            font.weight: Font.DemiBold
        }

        StudioButton {
            variant: "secondary"
            text: "+"
            implicitWidth: 28
            implicitHeight: 26
            leftPadding: 0
            rightPadding: 0
            enabled: ApplicationController.downloads.maxConcurrent < 6
            onClicked: ApplicationController.downloads.maxConcurrent = ApplicationController.downloads.maxConcurrent + 1
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

    ListView {
        id: taskList

        visible: ApplicationController.downloads.tasks.length > 0
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(taskList.contentHeight, 300)
        clip: true
        spacing: 6
        boundsBehavior: Flickable.StopAtBounds
        model: ApplicationController.downloads.tasks

        ScrollBar.vertical: StudioScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: Rectangle {
            id: taskRow

            required property var modelData

            width: taskList.width
            height: taskInner.implicitHeight + 14
            radius: Theme.radiusSmall
            color: Theme.cardSurface
            border.width: 1
            border.color: Theme.borderHairline

            ColumnLayout {
                id: taskInner

                anchors.fill: parent
                anchors.margins: 7
                spacing: 5

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Rectangle {
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: root.statusColorFor(taskRow.modelData.status)
                    }

                    Text {
                        Layout.fillWidth: true
                        text: taskRow.modelData.title
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    StudioButton {
                        visible: Boolean(taskRow.modelData.canPause)
                        variant: "ghost"
                        iconName: "pause"
                        iconSize: 13
                        implicitWidth: 24
                        implicitHeight: 24
                        leftPadding: 0
                        rightPadding: 0
                        onClicked: ApplicationController.downloads.pause(taskRow.modelData.taskId)
                    }

                    StudioButton {
                        visible: Boolean(taskRow.modelData.canResume)
                        variant: "ghost"
                        iconName: "play"
                        iconSize: 13
                        implicitWidth: 24
                        implicitHeight: 24
                        leftPadding: 0
                        rightPadding: 0
                        onClicked: ApplicationController.downloads.resume(taskRow.modelData.taskId)
                    }

                    StudioButton {
                        visible: Boolean(taskRow.modelData.canRetry)
                        variant: "ghost"
                        iconName: "refresh-cw"
                        iconSize: 13
                        implicitWidth: 24
                        implicitHeight: 24
                        leftPadding: 0
                        rightPadding: 0
                        onClicked: ApplicationController.downloads.retryTask(taskRow.modelData.taskId)
                    }

                    StudioButton {
                        visible: Boolean(taskRow.modelData.canCancel)
                        variant: "ghost"
                        iconName: "x"
                        iconSize: 13
                        implicitWidth: 24
                        implicitHeight: 24
                        leftPadding: 0
                        rightPadding: 0
                        onClicked: ApplicationController.downloads.cancel(taskRow.modelData.taskId)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 3
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
