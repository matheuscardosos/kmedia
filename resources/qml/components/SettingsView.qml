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

        // Appearance, Language & Profile Card
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: profileCol.implicitHeight + 28
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: profileCol

                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StudioIcon {
                        name: "globe"
                        size: 17
                        color: Theme.accentPrimary
                    }

                    Text {
                        text: qsTr("Appearance & Interface Language")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLg
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    StudioIcon {
                        name: Theme.isDark ? "moon" : "sun"
                        size: 15
                        color: Theme.textSecondary
                    }

                    Text {
                        text: qsTr("Dark Booth Theme (Studio Carbon)")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                    }

                    StudioSwitch {
                        checked: ApplicationController.darkTheme
                        onToggled: ApplicationController.darkTheme = checked
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.borderHairline
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: qsTr("Active Congregation: %1").arg(ApplicationController.activeProfileName)
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    StudioButton {
                        variant: ApplicationController.uiLanguage === "en" ? "primary" : "secondary"
                        text: qsTr("English")
                        implicitHeight: 30
                        onClicked: ApplicationController.setUiLanguage("en")
                    }

                    StudioButton {
                        variant: ApplicationController.uiLanguage === "pt" ? "primary" : "secondary"
                        text: qsTr("Português")
                        implicitHeight: 30
                        onClicked: ApplicationController.setUiLanguage("pt")
                    }
                }
            }
        }

        // OBS Studio & Zoom Booth Integrations Card
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: boothCol.implicitHeight + 28
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: boothCol

                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StudioIcon {
                        name: "radio"
                        size: 17
                        color: Theme.accentPrimary
                    }

                    Text {
                        text: qsTr("OBS Studio WebSocket v5 & Zoom Bridge")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLg
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        id: obsHostField

                        Layout.fillWidth: true
                        placeholderText: qsTr("OBS Host (127.0.0.1)")
                        text: ApplicationController.obsClient.host
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        background: StudioCard {
                            surfaceColor: Theme.windowBase
                            cornerRadius: Theme.radiusSmall
                        }
                    }

                    TextField {
                        id: obsPortField

                        implicitWidth: 82
                        placeholderText: "4455"
                        text: String(ApplicationController.obsClient.port)
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        background: StudioCard {
                            surfaceColor: Theme.windowBase
                            cornerRadius: Theme.radiusSmall
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        id: zoomIdField

                        Layout.fillWidth: true
                        placeholderText: qsTr("Zoom Meeting ID")
                        text: ApplicationController.zoomBridge.meetingId
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        background: StudioCard {
                            surfaceColor: Theme.windowBase
                            cornerRadius: Theme.radiusSmall
                        }
                    }

                    StudioButton {
                        variant: "primary"
                        text: qsTr("Save & Connect")
                        onClicked: {
                            ApplicationController.obsClient.host = obsHostField.text
                            const parsedPort = parseInt(obsPortField.text, 10)
                            if (!isNaN(parsedPort)) {
                                ApplicationController.obsClient.port = parsedPort
                            }
                            ApplicationController.obsClient.enabled = true
                            ApplicationController.zoomBridge.meetingId = zoomIdField.text
                            ApplicationController.saveObsAndZoomSettings()
                            ApplicationController.obsClient.connectToObs()
                            ApplicationController.showToast(qsTr("Booth integration settings saved"), "success")
                        }
                    }
                }

                StudioButton {
                    variant: "secondary"
                    iconName: "video"
                    text: qsTr("Launch Zoom Meeting")
                    onClicked: ApplicationController.zoomBridge.joinMeeting()
                }
            }
        }

        // Cloud Watched Folder Card
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: cloudFolderCol.implicitHeight + 28
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: cloudFolderCol

                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StudioIcon {
                        name: "folder-open"
                        size: 17
                        color: Theme.accentPrimary
                    }

                    Text {
                        text: qsTr("Watched Cloud Sync Folder")
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLg
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        id: cloudFolderField

                        Layout.fillWidth: true
                        placeholderText: qsTr("Local Dropbox / OneDrive / Nextcloud path")
                        text: ApplicationController.cloudWatcher.watchedFolders.join(";")
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        background: StudioCard {
                            surfaceColor: Theme.windowBase
                            cornerRadius: Theme.radiusSmall
                        }
                    }

                    StudioButton {
                        variant: "secondary"
                        text: qsTr("Watch")
                        onClicked: {
                            ApplicationController.saveCloudFolder(cloudFolderField.text)
                            ApplicationController.showToast(qsTr("Updated watched folders"), "info")
                        }
                    }
                }
            }
        }

        // Runtime Diagnostics Card
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: diagRow.implicitHeight + 22
            surfaceColor: Theme.panelSurface

            RowLayout {
                id: diagRow

                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                StudioIcon {
                    name: "info"
                    size: 15
                    color: Theme.textMuted
                }

                Text {
                    text: qsTr("KMedia %1 | Qt %2 | libmpv %3 | Memory %4")
                          .arg(ApplicationController.appVersion)
                          .arg(ApplicationController.qtVersion)
                          .arg(ApplicationController.mpvVersion)
                          .arg(ApplicationController.memoryUsage)
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }
    }
}
