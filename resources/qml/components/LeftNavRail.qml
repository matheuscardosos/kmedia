pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Rectangle {
    id: root

    property int activeTabIndex: 0
    property bool collapsed: false
    signal tabSelected(int tabIndex)

    readonly property var navItems: [
        { label: qsTr("Meeting"), icon: "calendar" },
        { label: qsTr("Settings"), icon: "settings" }
    ]

    color: Theme.panelSurface

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: Theme.borderHairline
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.collapsed ? Theme.spacingSm : Theme.spacingMd
        spacing: Theme.spacingMd

        // Top branding bar with real project logo and Light/Dark theme switch
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 2
            spacing: 8

            Item {
                visible: root.collapsed
                Layout.fillWidth: true
            }

            Image {
                source: "qrc:/qt/qml/KMedia/UI/icons/logo.svg"
                sourceSize.width: 56
                sourceSize.height: 56
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }

            Text {
                visible: !root.collapsed
                text: "KMedia"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 17
                font.weight: Font.Bold
                font.letterSpacing: -0.3
                Layout.fillWidth: true
            }

            StudioButton {
                visible: !root.collapsed
                variant: "ghost"
                iconName: Theme.isDark ? "sun" : "moon"
                iconSize: 15
                implicitWidth: 30
                implicitHeight: 30
                leftPadding: 0
                rightPadding: 0
                cornerRadius: Theme.radiusSmall
                onClicked: ApplicationController.toggleDarkTheme()
                ToolTip.visible: hovered
                ToolTip.text: Theme.isDark ? qsTr("Switch to light theme") : qsTr("Switch to dark theme")
            }

            Item {
                visible: root.collapsed
                Layout.fillWidth: true
            }
        }

        // Compact theme button when rail is collapsed
        StudioButton {
            visible: root.collapsed
            Layout.alignment: Qt.AlignHCenter
            variant: "ghost"
            iconName: Theme.isDark ? "sun" : "moon"
            iconSize: 15
            implicitWidth: 36
            implicitHeight: 30
            leftPadding: 0
            rightPadding: 0
            onClicked: ApplicationController.toggleDarkTheme()
            ToolTip.visible: hovered
            ToolTip.text: Theme.isDark ? qsTr("Switch to light theme") : qsTr("Switch to dark theme")
        }

        // Congregation profile selector card
        StudioCard {
            visible: !root.collapsed
            Layout.fillWidth: true
            implicitHeight: 50
            surfaceColor: profileHover.containsMouse ? Theme.cardHover : Theme.cardSurface
            strokeColor: profilePopup.visible ? Theme.accentPrimary : Theme.borderHairline

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 9

                StudioIcon {
                    name: "users"
                    size: 16
                    color: Theme.textSecondary
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text: qsTr("Congregation")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                    }

                    Text {
                        text: ApplicationController.activeProfileName
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                StudioIcon {
                    name: "chevron-down"
                    size: 14
                    color: Theme.textSecondary
                }
            }

            MouseArea {
                id: profileHover

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: profilePopup.open()
            }

            Popup {
                id: profilePopup

                x: 0
                y: parent.height + 4
                width: parent.width
                padding: 4

                background: StudioCard {
                    surfaceColor: Theme.cardElevated
                    strokeColor: Theme.borderStrong
                }

                contentItem: ColumnLayout {
                    spacing: 2

                    Repeater {
                        model: ApplicationController.availableProfileNames

                        delegate: Rectangle {
                            id: profileRow

                            required property string modelData
                            required property int index

                            readonly property bool isCurrent: modelData === ApplicationController.activeProfileName

                            Layout.fillWidth: true
                            implicitHeight: 32
                            radius: Theme.radiusSmall
                            color: itemHover.containsMouse ? Theme.cardHover
                                 : (profileRow.isCurrent ? Theme.accentSoft : "transparent")

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 6

                                Text {
                                    text: profileRow.modelData
                                    color: profileRow.isCurrent ? Theme.accentPrimary : Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    font.weight: profileRow.isCurrent ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                StudioIcon {
                                    visible: profileRow.isCurrent
                                    name: "check"
                                    size: 14
                                    color: Theme.accentPrimary
                                }
                            }

                            MouseArea {
                                id: itemHover

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    const ids = ApplicationController.availableProfileIds
                                    if (profileRow.index >= 0 && profileRow.index < ids.length) {
                                        ApplicationController.switchProfile(ids[profileRow.index])
                                        ApplicationController.showToast(qsTr("Switched congregation to %1").arg(profileRow.modelData), "info")
                                    }
                                    profilePopup.close()
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.borderHairline
                    }

                    StudioButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        variant: "ghost"
                        iconName: "users"
                        text: qsTr("Manage congregations...")
                        onClicked: {
                            profilePopup.close()
                            congregationManagerPopup.open()
                        }
                    }
                }
            }

            CongregationManagerPopup {
                id: congregationManagerPopup
            }
        }

        // Primary navigation destinations
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: root.navItems

                delegate: NavRailButton {
                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    label: modelData.label
                    iconName: modelData.icon
                    active: root.activeTabIndex === index
                    collapsed: root.collapsed
                    onClicked: root.tabSelected(index)
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        // Bottom live hardware, OBS, Zoom, and storage status panel
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: statusCol.implicitHeight + 18

            ColumnLayout {
                id: statusCol

                anchors.fill: parent
                anchors.margins: root.collapsed ? 6 : 10
                spacing: 8

                StatusRow {
                    Layout.fillWidth: true
                    collapsed: root.collapsed
                    iconName: "radio"
                    label: "OBS"
                    valueText: ApplicationController.obsClient.connected ? qsTr("Connected")
                             : (ApplicationController.processMonitor.obsRunning ? qsTr("Detected")
                                                                              : qsTr("Offline"))
                    statusColor: ApplicationController.obsClient.connected ? Theme.statusReady
                               : (ApplicationController.processMonitor.obsRunning ? Theme.statusStandby
                                                                                : Theme.statusOff)
                }

                StatusRow {
                    Layout.fillWidth: true
                    collapsed: root.collapsed
                    iconName: "monitor"
                    label: qsTr("2nd Display")
                    valueText: ApplicationController.displayManager.presentationWindowVisible
                             ? qsTr("Active")
                             : (ApplicationController.displayManager.hasSecondaryDisplay
                                ? qsTr("Ready")
                                : qsTr("Single"))
                    statusColor: ApplicationController.displayManager.presentationWindowVisible
                               ? Theme.statusReady
                               : (ApplicationController.displayManager.hasSecondaryDisplay
                                  ? Theme.statusStandby
                                  : Theme.statusOff)
                }

                StatusRow {
                    Layout.fillWidth: true
                    collapsed: root.collapsed
                    iconName: "video"
                    label: "Zoom"
                    valueText: ApplicationController.zoomBridge.zoomRunning ? qsTr("Active")
                                                                          : qsTr("Not running")
                    statusColor: ApplicationController.zoomBridge.zoomRunning ? Theme.statusReady
                                                                              : Theme.statusOff
                }

                StatusRow {
                    Layout.fillWidth: true
                    collapsed: root.collapsed
                    iconName: "hard-drive"
                    label: qsTr("Storage")
                    valueText: qsTr("%1 free").arg(ApplicationController.storageAvailableText)
                    statusColor: Theme.statusReady
                }
            }
        }
    }
}
