pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Popup {
    id: root

    signal openAddMediaRequested()
    signal navigateTabRequested(int tabIndex)

    property string queryText: ""

    readonly property var allCommands: [
        { title: qsTr("Play / Pause Active Media"), hint: "Space", icon: "play", actionId: "play_pause" },
        { title: qsTr("Toggle Standby Screen"), hint: "B", icon: "pause", actionId: "standby" },
        { title: qsTr("Next Playlist Item"), hint: "Alt+Right", icon: "skip-forward", actionId: "next" },
        { title: qsTr("Previous Playlist Item"), hint: "Alt+Left", icon: "skip-back", actionId: "prev" },
        { title: qsTr("Add Media or Package"), hint: "Ctrl+O", icon: "plus", actionId: "add_media" },
        { title: qsTr("Toggle Secondary Display"), hint: "Ctrl+P", icon: "monitor", actionId: "presentation" },
        { title: qsTr("Fade Background Music"), hint: "Ctrl+M", icon: "audio-lines", actionId: "fade_bgm" },
        { title: qsTr("Toggle Dark / Light Theme"), hint: "Ctrl+D", icon: Theme.isDark ? "sun" : "moon", actionId: "theme" },
        { title: qsTr("Go to Meeting"), hint: "1", icon: "calendar", actionId: "tab_0" },
        { title: qsTr("Go to Media Library"), hint: "2", icon: "film", actionId: "tab_1" },
        { title: qsTr("Go to Playlists"), hint: "3", icon: "layers", actionId: "tab_2" },
        { title: qsTr("Go to Downloads"), hint: "4", icon: "download", actionId: "tab_3" },
        { title: qsTr("Go to Settings"), hint: "5", icon: "settings", actionId: "tab_4" }
    ]

    function filteredCommands() {
        const q = queryText.trim().toLowerCase()
        if (q.length === 0) {
            return allCommands
        }
        const out = []
        for (let i = 0; i < allCommands.length; ++i) {
            const cmd = allCommands[i]
            if (cmd.title.toLowerCase().indexOf(q) >= 0) {
                out.push(cmd)
            }
        }
        return out
    }

    function executeAction(actionId) {
        if (actionId === "play_pause") {
            ApplicationController.playback.togglePause()
        } else if (actionId === "standby") {
            ApplicationController.playback.toggleStandby()
        } else if (actionId === "next") {
            ApplicationController.playlist.activateNext()
        } else if (actionId === "prev") {
            ApplicationController.playlist.activatePrevious()
        } else if (actionId === "add_media") {
            root.openAddMediaRequested()
        } else if (actionId === "presentation") {
            ApplicationController.displayManager.presentationWindowVisible = !ApplicationController.displayManager.presentationWindowVisible
        } else if (actionId === "fade_bgm") {
            if (ApplicationController.backgroundMusic.playing) {
                ApplicationController.backgroundMusic.fadeOutAndStop(4000)
            } else {
                ApplicationController.backgroundMusic.play()
            }
        } else if (actionId === "theme") {
            ApplicationController.toggleDarkTheme()
        } else if (actionId.startsWith("tab_")) {
            root.navigateTabRequested(parseInt(actionId.substring(4), 10))
        }
        close()
    }

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(480, parent ? parent.width - 32 : 480)
    height: Math.min(420, parent ? parent.height - 80 : 420)
    padding: 0

    onOpened: {
        queryText = ""
        searchField.forceActiveFocus()
    }

    Overlay.modal: Rectangle {
        color: Theme.scrimOverlay
    }

    background: StudioCard {
        surfaceColor: Theme.panelSurface
        strokeColor: Theme.borderStrong
        cornerRadius: Theme.radiusLarge
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            spacing: 10

            StudioIcon {
                name: "search"
                size: 16
                color: Theme.textSecondary
            }

            TextField {
                id: searchField

                Layout.fillWidth: true
                placeholderText: qsTr("Type a command or destination...")
                color: Theme.textPrimary
                placeholderTextColor: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                background: null
                onTextChanged: root.queryText = text
                onAccepted: {
                    const list = root.filteredCommands()
                    if (list.length > 0) {
                        root.executeAction(list[0].actionId)
                    }
                }
            }

            StudioBadge {
                text: "ESC"
                badgeColor: Theme.cardSurface
                textColor: Theme.textMuted
                hasBorder: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderHairline
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 6
            clip: true
            spacing: 2
            model: root.filteredCommands()

            delegate: Rectangle {
                id: cmdRow

                required property var modelData

                width: ListView.view.width
                height: 36
                radius: Theme.radiusSmall
                color: cmdHover.containsMouse ? Theme.cardHover : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10

                    StudioIcon {
                        name: cmdRow.modelData.icon
                        size: 15
                        color: Theme.accentPrimary
                    }

                    Text {
                        text: cmdRow.modelData.title
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: cmdRow.modelData.hint
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                    }
                }

                MouseArea {
                    id: cmdHover

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.executeAction(cmdRow.modelData.actionId)
                }
            }
        }
    }
}
