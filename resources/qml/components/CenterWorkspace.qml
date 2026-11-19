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

        // Compact secondary header only shown for non-Meeting views
        StudioCard {
            visible: root.activeTabIndex !== 0
            Layout.fillWidth: true
            implicitHeight: 52
            surfaceColor: Theme.panelSurface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Text {
                    text: root.sectionTitle
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLg
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                StudioButton {
                    visible: root.showPreviewToggle
                    variant: root.previewOpen ? "primary" : "secondary"
                    iconName: "monitor"
                    text: root.compactMode ? "" : (root.previewOpen ? qsTr("Hide Preview") : qsTr("Preview"))
                    implicitHeight: 32
                    onClicked: root.previewToggled()
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.activeTabIndex

            MeetingPlaylistView {
                compactMode: root.compactMode
                showPreviewToggle: root.showPreviewToggle
                previewOpen: root.previewOpen
                onAddMediaRequested: root.addMediaRequested()
                onPreviewToggled: root.previewToggled()
            }

            MediaLibraryView {
                compactMode: root.compactMode
                onAddFilesRequested: root.addMediaRequested()
            }

            PlaylistsView {
                compactMode: root.compactMode
            }

            DownloadsView {
                compactMode: root.compactMode
            }

            SettingsView {
                compactMode: root.compactMode
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
