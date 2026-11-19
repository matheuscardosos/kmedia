pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

ScrollView {
    id: root

    property bool compactMode: false

    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical: StudioScrollBar {
        policy: ScrollBar.AsNeeded
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacingMd

        StudioCard {
            Layout.fillWidth: true
            implicitHeight: dlCard.implicitHeight + 28
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: dlCard

                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StudioIcon {
                        name: "download"
                        size: 18
                        color: Theme.accentPrimary
                    }

                    Text {
                        text: qsTr("Network Transfer Queue")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLg
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Active transfers: %1 | Queued: %2 | Max concurrency: %3")
                          .arg(ApplicationController.downloads.activeCount)
                          .arg(ApplicationController.downloads.queuedCount)
                          .arg(ApplicationController.downloads.maxConcurrent)
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    spacing: 8

                    StudioButton {
                        variant: "secondary"
                        text: "-"
                        implicitWidth: 34
                        implicitHeight: 30
                        enabled: ApplicationController.downloads.maxConcurrent > 1
                        onClicked: ApplicationController.downloads.maxConcurrent = ApplicationController.downloads.maxConcurrent - 1
                    }

                    Text {
                        text: qsTr("Limit: %1").arg(ApplicationController.downloads.maxConcurrent)
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        font.weight: Font.Medium
                    }

                    StudioButton {
                        variant: "secondary"
                        text: "+"
                        implicitWidth: 34
                        implicitHeight: 30
                        onClicked: ApplicationController.downloads.maxConcurrent = ApplicationController.downloads.maxConcurrent + 1
                    }
                }
            }
        }
    }
}
