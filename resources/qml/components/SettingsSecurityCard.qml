pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

ColumnLayout {
    id: secRoot

    spacing: Theme.spacingMd

    StudioCard {
        Layout.fillWidth: true
        implicitHeight: pinCol.implicitHeight + 28
        surfaceColor: Theme.panelSurface

        ColumnLayout {
            id: pinCol

            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                StudioIcon {
                    name: ApplicationController.boothPinEnabled ? "lock" : "unlock"
                    size: 17
                    color: Theme.accentPrimary
                }

                Text {
                    text: qsTr("Booth Settings PIN Lock")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLg
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }

                StudioBadge {
                    text: ApplicationController.boothPinEnabled ? qsTr("Enabled") : qsTr("Disabled")
                    showDot: true
                    dotColor: ApplicationController.boothPinEnabled ? Theme.statusReady : Theme.statusOff
                    badgeColor: Theme.cardSurface
                    textColor: Theme.textSecondary
                    hasBorder: true
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: newPinField

                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: ApplicationController.boothPinEnabled
                                     ? qsTr("Current PIN (to remove) or new PIN (4-12 chars)")
                                     : qsTr("Set booth PIN (4-12 chars)...")
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
                    iconName: "lock"
                    text: ApplicationController.boothPinEnabled ? qsTr("Update PIN") : qsTr("Enable PIN")
                    enabled: newPinField.text.trim().length >= 4
                    onClicked: {
                        if (ApplicationController.enableBoothPin(newPinField.text)) {
                            newPinField.text = ""
                            ApplicationController.showToast(qsTr("Booth PIN updated"), "success")
                        } else {
                            ApplicationController.showToast(qsTr("PIN must be 4-12 letters or digits"), "warning")
                        }
                    }
                }

                StudioButton {
                    visible: ApplicationController.boothPinEnabled
                    variant: "secondary"
                    text: qsTr("Disable")
                    enabled: newPinField.text.trim().length >= 4
                    onClicked: {
                        if (ApplicationController.disableBoothPin(newPinField.text)) {
                            newPinField.text = ""
                            ApplicationController.showToast(qsTr("Booth PIN disabled"), "info")
                        } else {
                            ApplicationController.showToast(qsTr("Incorrect current PIN"), "warning")
                        }
                    }
                }
            }
        }
    }

    StudioCard {
        Layout.fillWidth: true
        implicitHeight: shortcutsCol.implicitHeight + 24
        surfaceColor: Theme.panelSurface

        ColumnLayout {
            id: shortcutsCol

            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                StudioIcon {
                    name: "command"
                    size: 16
                    color: Theme.accentPrimary
                }

                Text {
                    text: qsTr("Keyboard Shortcuts & Diagnostics")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeMd
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
            }

            Text {
                Layout.fillWidth: true
                text: "Ctrl+K  |  Ctrl+O  |  Ctrl+D  |  Ctrl+P  |  Space  |  B  |  Alt+Left / Alt+Right"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.borderHairline
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
