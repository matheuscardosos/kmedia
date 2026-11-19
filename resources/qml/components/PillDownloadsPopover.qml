pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

ColumnLayout {
    id: root

    signal openDownloadsRequested()

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
        spacing: 8

        Text {
            text: qsTr("Queued: %1").arg(ApplicationController.downloads.queuedCount)
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            Layout.fillWidth: true
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
            text: "" + ApplicationController.downloads.maxConcurrent
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

    StudioButton {
        variant: "primary"
        iconName: "download"
        text: qsTr("Open Downloads")
        Layout.fillWidth: true
        onClicked: root.openDownloadsRequested()
    }
}
