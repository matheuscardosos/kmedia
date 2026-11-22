pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Rectangle {
    id: root

    property int activeTabIndex: 0
    property bool compactMode: false
    property bool showPreviewToggle: false
    property bool previewOpen: false
    property string sectionTitle: qsTr("Meeting Workspace")
    property string sectionIcon: "settings"
    signal previewToggled()
    signal addMediaRequested()
    signal openCommandPalette()
    signal openBoothSettings()

    function openAddMediaDialog() {
        root.addMediaRequested()
    }

    color: Theme.windowBase

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compactMode ? Theme.spacingSm : Theme.spacingMd
        spacing: Theme.spacingSm

        // Pages other than Meeting share one header: icon, title, and congregation.
        StudioCard {
            visible: root.activeTabIndex !== 0
            Layout.fillWidth: true
            implicitHeight: 60
            surfaceColor: Theme.panelSurface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: Theme.radiusSmall
                    color: Theme.accentSoft

                    StudioIcon {
                        anchors.centerIn: parent
                        name: root.sectionIcon
                        size: 18
                        color: Theme.accentPrimary
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Text {
                        Layout.fillWidth: true
                        text: root.sectionTitle
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: root.compactMode ? Theme.fontSizeLg : Theme.fontSizeTitle
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: ApplicationController.activeProfileName
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        elide: Text.ElideRight
                    }
                }

                StudioButton {
                    visible: root.showPreviewToggle
                    variant: root.previewOpen ? "primary" : "ghost"
                    iconName: "monitor"
                    leftPadding: 0
                    rightPadding: 0
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    onClicked: root.previewToggled()
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.activeTabIndex

            MeetingMediaPage {
                compactMode: root.compactMode
                showPreviewToggle: root.showPreviewToggle
                previewOpen: root.previewOpen
                onPreviewToggled: root.previewToggled()
            }

            Loader {
                active: root.activeTabIndex === 1 || item !== null
                asynchronous: false

                sourceComponent: SettingsView {
                    compactMode: root.compactMode
                }
            }
        }

        // Quick-access Floating Pill Taskbar for booth toggles and shortcuts
        ActionPillBar {
            visible: root.activeTabIndex === 0
            Layout.fillWidth: true
            compactMode: root.compactMode
            onOpenCommandPalette: root.openCommandPalette()
            onOpenBoothSettings: root.openBoothSettings()
        }

        // Docked Studio Broadcast Transport Bar inside Meeting view
        TransportBar {
            visible: root.activeTabIndex === 0
            Layout.fillWidth: true
            compactMode: root.compactMode
        }
    }
}
