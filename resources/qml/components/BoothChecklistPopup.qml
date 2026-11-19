pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Popup {
    id: popupRoot

    property int activeTab: 0
    property bool audioChecked: false
    property bool displayChecked: false
    property bool slidesChecked: false
    property bool playbackStopped: false
    property bool standbyRestored: false
    property bool recordingStopped: false

    readonly property bool hasSecondaryDisplay: ApplicationController.displayManager.hasSecondaryDisplay
    readonly property bool hasQueuedMedia: ApplicationController.playlist.count > 0
    readonly property int readyCount: (popupRoot.hasSecondaryDisplay ? 1 : 0)
                                      + (popupRoot.hasQueuedMedia ? 1 : 0)
                                      + (popupRoot.audioChecked ? 1 : 0)
                                      + (popupRoot.displayChecked ? 1 : 0)
    readonly property int totalPreCount: 4

    width: Math.min(372, parent ? parent.width - 24 : 372)
    padding: 16
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle {
        color: Theme.scrimOverlay
    }

    background: StudioCard {
        surfaceColor: Theme.cardElevated
        strokeColor: Theme.borderStrong
        cornerRadius: Theme.radiusLarge
    }

    contentItem: ColumnLayout {
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioIcon {
                name: "clipboard-check"
                size: 16
                color: Theme.accentPrimary
            }

            Text {
                text: qsTr("Booth Readiness Checklists")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            StudioBadge {
                text: qsTr("%1/%2 ready").arg(popupRoot.readyCount).arg(popupRoot.totalPreCount)
                showDot: true
                dotColor: popupRoot.readyCount === popupRoot.totalPreCount
                          ? Theme.statusReady : Theme.statusStandby
                badgeColor: Theme.cardSurface
                textColor: Theme.textSecondary
                hasBorder: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            StudioButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                variant: popupRoot.activeTab === 0 ? "primary" : "secondary"
                text: qsTr("Pre-Meeting")
                onClicked: popupRoot.activeTab = 0
            }

            StudioButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                variant: popupRoot.activeTab === 1 ? "primary" : "secondary"
                text: qsTr("Post-Meeting")
                onClicked: popupRoot.activeTab = 1
            }
        }

        // Pre-meeting hardware + manual booth checklist
        ColumnLayout {
            visible: popupRoot.activeTab === 0
            Layout.fillWidth: true
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                radius: Theme.radiusSmall
                color: Theme.cardSurface
                border.width: 1
                border.color: Theme.borderHairline

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    StudioIcon {
                        name: popupRoot.hasSecondaryDisplay ? "circle-check" : "triangle-exclamation-point"
                        size: 15
                        color: popupRoot.hasSecondaryDisplay ? Theme.statusReady : Theme.statusStandby
                    }

                    Text {
                        text: popupRoot.hasSecondaryDisplay
                              ? qsTr("Presentation Display: %1").arg(ApplicationController.displayManager.targetScreenName)
                              : qsTr("Single Display Mode (check HDMI/projector)")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                radius: Theme.radiusSmall
                color: Theme.cardSurface
                border.width: 1
                border.color: Theme.borderHairline

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    StudioIcon {
                        name: popupRoot.hasQueuedMedia ? "circle-check" : "triangle-exclamation-point"
                        size: 15
                        color: popupRoot.hasQueuedMedia ? Theme.statusReady : Theme.statusStandby
                    }

                    Text {
                        text: qsTr("Meeting Queue: %1 item(s)").arg(ApplicationController.playlist.count)
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                radius: Theme.radiusSmall
                color: Theme.cardSurface
                border.width: 1
                border.color: Theme.borderHairline

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    StudioIcon {
                        name: popupRoot.audioChecked ? "circle-check" : "audio-lines"
                        size: 15
                        color: popupRoot.audioChecked ? Theme.statusReady : Theme.textMuted
                    }

                    Text {
                        text: qsTr("Sound Check & Auditorium Output Verified")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: popupRoot.audioChecked = !popupRoot.audioChecked
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                radius: Theme.radiusSmall
                color: Theme.cardSurface
                border.width: 1
                border.color: Theme.borderHairline

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    StudioIcon {
                        name: popupRoot.displayChecked ? "circle-check" : "radio"
                        size: 15
                        color: popupRoot.displayChecked ? Theme.statusReady : Theme.textMuted
                    }

                    Text {
                        text: ApplicationController.obsClient.connected
                              ? qsTr("OBS & Zoom Booth Test Passed")
                              : qsTr("Standby Slide / Screen Routing Verified")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: popupRoot.displayChecked = !popupRoot.displayChecked
                }
            }
        }

        // Post-meeting shutdown checklist
        ColumnLayout {
            visible: popupRoot.activeTab === 1
            Layout.fillWidth: true
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                radius: Theme.radiusSmall
                color: Theme.cardSurface
                border.width: 1
                border.color: Theme.borderHairline

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    StudioIcon {
                        name: popupRoot.standbyRestored ? "circle-check" : "monitor"
                        size: 15
                        color: popupRoot.standbyRestored ? Theme.statusReady : Theme.textMuted
                    }

                    Text {
                        text: qsTr("Restore Auditorium Standby Screen")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        popupRoot.standbyRestored = !popupRoot.standbyRestored
                        if (popupRoot.standbyRestored) {
                            ApplicationController.playback.standbyActive = true
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                radius: Theme.radiusSmall
                color: Theme.cardSurface
                border.width: 1
                border.color: Theme.borderHairline

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    StudioIcon {
                        name: popupRoot.playbackStopped ? "circle-check" : "square"
                        size: 15
                        color: popupRoot.playbackStopped ? Theme.statusReady : Theme.textMuted
                    }

                    Text {
                        text: qsTr("Stop Media & Ambient Music Playback")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        popupRoot.playbackStopped = !popupRoot.playbackStopped
                        if (popupRoot.playbackStopped) {
                            ApplicationController.playback.stop()
                            ApplicationController.backgroundMusic.stop()
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                radius: Theme.radiusSmall
                color: Theme.cardSurface
                border.width: 1
                border.color: Theme.borderHairline

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    StudioIcon {
                        name: popupRoot.recordingStopped ? "circle-check" : "radio"
                        size: 15
                        color: popupRoot.recordingStopped ? Theme.statusReady : Theme.textMuted
                    }

                    Text {
                        text: qsTr("Verify OBS Recording & Zoom Closed")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: popupRoot.recordingStopped = !popupRoot.recordingStopped
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            StudioButton {
                variant: "primary"
                text: qsTr("Done")
                onClicked: popupRoot.close()
            }
        }
    }
}
