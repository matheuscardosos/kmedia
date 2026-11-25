pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import KMedia.UI
import "components"
import "controls"

ApplicationWindow {
    id: root

    width: 1280
    height: 800
    minimumWidth: 360
    minimumHeight: 520
    visible: true
    title: qsTr("KMedia")
    color: Theme.windowBase

    readonly property bool isCompact: width < Theme.compactBreakpoint
    readonly property bool isMedium: width >= Theme.compactBreakpoint && width < Theme.wideBreakpoint
    readonly property bool isWide: width >= Theme.wideBreakpoint
    readonly property bool editingTextInput: root.activeFocusItem instanceof TextInput
                                             || root.activeFocusItem instanceof TextEdit
    property int currentTabIndex: ApplicationController.startupTabIndex
    property bool inspectorDrawerOpen: false
    property bool previewDocked: true
    readonly property bool previewPanelOpen: (root.isWide && root.previewDocked) || root.inspectorDrawerOpen

    readonly property list<string> sectionTitles: [
        qsTr("Meeting Workspace"),
        qsTr("Settings")
    ]

    Connections {
        target: ApplicationController
        function onToastRequested(message, level) {
            toastBanner.displayToast(message, level)
        }
    }

    FileDialog {
        id: mediaFileDialog

        title: qsTr("Select Media or Publication Package")
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            qsTr("Supported Media (*.mp4 *.mkv *.webm *.mov *.mp3 *.aac *.flac *.ogg *.png *.jpg *.jpeg *.webp *.pdf *.jwpub *.jwlplaylist)"),
            qsTr("All Files (*)")
        ]
        onAccepted: {
            const added = ApplicationController.playlist.importUrls(selectedFiles)
            if (added > 0) {
                ApplicationController.showToast(qsTr("Added %1 media item(s)").arg(added), "success")
            }
        }
    }

    // Keyboard shortcuts for fast booth operation
    Shortcut {
        sequence: "Ctrl+K"
        onActivated: commandPalette.open()
    }
    Shortcut {
        sequence: "Ctrl+O"
        onActivated: mediaFileDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: ApplicationController.toggleDarkTheme()
    }
    Shortcut {
        sequence: "Ctrl+P"
        onActivated: ApplicationController.displayManager.presentationWindowVisible = !ApplicationController.displayManager.presentationWindowVisible
    }
    Shortcut {
        sequence: "Alt+Right"
        onActivated: ApplicationController.playlist.activateNext()
    }
    Shortcut {
        sequence: "Alt+Left"
        onActivated: ApplicationController.playlist.activatePrevious()
    }
    Shortcut {
        sequence: "Space"
        enabled: !root.editingTextInput && !commandPalette.opened
        onActivated: ApplicationController.playback.togglePause()
    }
    Shortcut {
        sequence: "B"
        enabled: !root.editingTextInput && !commandPalette.opened
        onActivated: ApplicationController.playback.toggleStandby()
    }
    Shortcut {
        sequence: "Ctrl+M"
        enabled: !commandPalette.opened
        onActivated: ApplicationController.music.toggle()
    }
    Shortcut {
        sequence: "Escape"
        enabled: !root.editingTextInput && !commandPalette.opened
                 && (ApplicationController.playback.mediaKind !== PlaybackController.None
                     || ApplicationController.playlist.selectionCount > 1)
        // A selection of several items is cleared first; Escape stops playback otherwise.
        onActivated: {
            if (ApplicationController.playlist.selectionCount > 1) {
                ApplicationController.playlist.clearSelection()
            } else {
                ApplicationController.playback.stop()
            }
        }
    }
    Shortcut {
        sequence: "Delete"
        enabled: !root.editingTextInput && !commandPalette.opened
                 && root.currentTabIndex === 0
                 && ApplicationController.playlist.activeIndex >= 0
        // Synced media is only hidden: the next sync would bring a deleted item back.
        onActivated: {
            if (ApplicationController.playlist.selectionCount > 1) {
                ApplicationController.playlist.removeSelected()
            } else if (ApplicationController.playlist.activeIsSynced) {
                ApplicationController.playlist.setItemHidden(ApplicationController.playlist.activeIndex, true)
            } else {
                ApplicationController.playlist.removeAt(ApplicationController.playlist.activeIndex)
            }
        }
    }
    Shortcut {
        sequence: "1"
        enabled: !root.editingTextInput && !commandPalette.opened
        onActivated: root.currentTabIndex = 0
    }
    Shortcut {
        sequence: "2"
        enabled: !root.editingTextInput && !commandPalette.opened
        onActivated: root.currentTabIndex = 1
    }

    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls) {
                const count = ApplicationController.playlist.importUrls(drop.urls)
                if (count > 0) {
                    ApplicationController.showToast(qsTr("Imported %1 dropped file(s)").arg(count), "success")
                }
                drop.acceptProposedAction()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            LeftNavRail {
                id: navRail

                visible: !root.isCompact
                collapsed: root.isMedium
                activeTabIndex: root.currentTabIndex
                onTabSelected: (tabIndex) => root.currentTabIndex = tabIndex
                Layout.preferredWidth: root.isMedium ? Theme.navRailCompactWidth : Theme.navRailWidth
                Layout.fillHeight: true
            }

            CenterWorkspace {
                id: centerWorkspace

                Layout.fillWidth: true
                Layout.fillHeight: true
                activeTabIndex: root.currentTabIndex
                compactMode: root.isCompact
                showPreviewToggle: true
                previewOpen: root.isWide ? root.previewDocked : root.inspectorDrawerOpen
                sectionTitle: root.sectionTitles[root.currentTabIndex] ?? qsTr("Meeting Workspace")
                onPreviewToggled: {
                    if (root.isWide) {
                        root.previewDocked = !root.previewDocked
                    } else {
                        root.inspectorDrawerOpen = !root.inspectorDrawerOpen
                    }
                }
                onAddMediaRequested: mediaFileDialog.open()
                onOpenCommandPalette: commandPalette.open()
                onOpenBoothSettings: root.currentTabIndex = 1
            }

            InspectorPanel {
                visible: root.isWide && root.previewDocked
                drawerMode: false
                hostLocalPlayer: !drawerInspectorLoader.active
                Layout.preferredWidth: Theme.inspectorPanelWidth
                Layout.fillHeight: true
            }
        }

        CompactBottomNav {
            visible: root.isCompact
            Layout.fillWidth: true
            activeTabIndex: root.currentTabIndex
            onTabSelected: (tabIndex) => root.currentTabIndex = tabIndex
        }
    }

    Loader {
        id: drawerInspectorLoader

        active: !root.isWide && root.inspectorDrawerOpen
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: Math.min(root.width, Theme.inspectorPanelWidth)
        z: 50

        sourceComponent: InspectorPanel {
            drawerMode: true
            hostLocalPlayer: true
            onCloseRequested: root.inspectorDrawerOpen = false
        }
    }

    ToastBanner {
        id: toastBanner

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 16
        z: 90
    }

    CommandPalette {
        id: commandPalette

        onOpenAddMediaRequested: mediaFileDialog.open()
        onNavigateTabRequested: (tabIndex) => root.currentTabIndex = tabIndex
    }

    // The floating preview stands in for the preview panel while the panel is closed.
    FloatingPreview {
        id: floatingPreview

        readonly property int mediaKind: ApplicationController.playback.mediaKind

        anchors.fill: parent
        bottomInset: centerWorkspace.bottomBarsHeight + (root.isCompact ? Theme.compactTabBarHeight : 0)
        z: 60
        shown: ApplicationController.floatingPreview
               && ApplicationController.displayManager.presentationWindowVisible
               && !ApplicationController.playback.standbyActive
               && (floatingPreview.mediaKind === PlaybackController.Video
                   || floatingPreview.mediaKind === PlaybackController.Image
                   || floatingPreview.mediaKind === PlaybackController.Pdf)
               && !root.previewPanelOpen
    }

    PresentationWindow {}

    TimerWindow {
        mainScreenName: root.Screen.name
    }

    // Live video frames are grabbed only while a preview is on screen.
    Binding {
        target: ApplicationController.playback
        property: "previewActive"
        value: root.previewPanelOpen || floatingPreview.shown
    }

    Binding {
        target: ApplicationController.playback
        property: "previewFrameWidth"
        value: floatingPreview.expanded ? 1280 : 480
    }
}
