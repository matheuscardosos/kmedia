pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

ColumnLayout {
    id: root

    property bool compactMode: false
    property string selectedCategory: "general"

    readonly property var categories: [
        { id: "general", label: qsTr("General"), icon: "globe", keywords: "general theme dark light language congregation profile" },
        { id: "schedule", label: qsTr("Meetings"), icon: "calendar-days", keywords: "meetings congregation schedule midweek weekend day time language fallback footnote printed watchtower teaching study filter" },
        { id: "display", label: qsTr("Display"), icon: "monitor", keywords: "display screen projector presentation standby blackout" },
        { id: "audio", label: qsTr("Audio"), icon: "audio-lines", keywords: "audio background music fade volume duration" },
        { id: "integrations", label: qsTr("Integrations"), icon: "radio", keywords: "obs websocket zoom meeting cloud folder watch dropbox" },
        { id: "security", label: qsTr("Security"), icon: "lock", keywords: "pin lock security shortcuts diagnostics version memory" }
    ]

    function isCategoryVisible(catId) {
        const q = searchField.text.trim().toLowerCase()
        if (q.length === 0) {
            return root.selectedCategory === catId
        }
        for (let i = 0; i < root.categories.length; ++i) {
            if (root.categories[i].id === catId) {
                return root.categories[i].keywords.indexOf(q) >= 0 || catId.indexOf(q) >= 0
            }
        }
        return false
    }

    spacing: Theme.spacingMd

    // Search & PIN lock status bar
    StudioCard {
        Layout.fillWidth: true
        implicitHeight: topBarRow.implicitHeight + 20
        surfaceColor: Theme.panelSurface

        RowLayout {
            id: topBarRow

            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            StudioCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                surfaceColor: Theme.windowBase
                cornerRadius: Theme.radiusSmall

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    StudioIcon {
                        name: "search"
                        size: 14
                        color: Theme.textMuted
                    }

                    TextField {
                        id: searchField

                        Layout.fillWidth: true
                        enabled: ApplicationController.boothUnlocked
                        placeholderText: qsTr("Search settings...")
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        leftPadding: 0
                        rightPadding: 0
                        background: Item {}
                    }
                }
            }

            StudioButton {
                id: lockBtn

                visible: ApplicationController.boothPinEnabled && ApplicationController.boothUnlocked
                variant: "secondary"
                iconName: "lock"
                text: root.compactMode ? "" : qsTr("Lock")
                leftPadding: root.compactMode ? 0 : 10
                rightPadding: root.compactMode ? 0 : 10
                Layout.preferredWidth: root.compactMode ? 34 : lockBtn.implicitWidth
                Layout.preferredHeight: 34
                onClicked: ApplicationController.lockBoothSettings()
            }
        }
    }

    // PIN Gate card when booth settings are locked
    SettingsPinGate {
        visible: !ApplicationController.boothUnlocked
        Layout.fillWidth: true
    }

    Item {
        visible: !ApplicationController.boothUnlocked
        Layout.fillHeight: true
    }

    // Unlocked two-pane layout (Desktop/Medium) or stacked layout (Compact)
    GridLayout {
        visible: ApplicationController.boothUnlocked
        Layout.fillWidth: true
        Layout.fillHeight: true
        columns: root.compactMode ? 1 : 2
        columnSpacing: Theme.spacingMd
        rowSpacing: Theme.spacingMd

        // Category rail / horizontal compact chips
        StudioCard {
            id: catRailCard

            Layout.fillWidth: root.compactMode
            Layout.preferredWidth: root.compactMode ? catRailCard.implicitWidth : 196
            Layout.fillHeight: !root.compactMode
            implicitHeight: catContainer.implicitHeight + 20
            surfaceColor: Theme.panelSurface

            Flow {
                id: catContainer

                anchors.fill: parent
                anchors.margins: 10
                spacing: 6
                flow: root.compactMode ? Flow.LeftToRight : Flow.TopToBottom

                Repeater {
                    model: root.categories

                    delegate: Rectangle {
                        id: catBtn

                        required property var modelData
                        readonly property bool selected: root.selectedCategory === catBtn.modelData.id && searchField.text.trim().length === 0

                        width: root.compactMode ? (catRow.implicitWidth + 18) : catContainer.width
                        height: 36
                        radius: Theme.radiusSmall
                        color: catBtn.selected
                               ? Theme.accentSoft
                               : (catMouse.containsMouse ? Theme.cardHover : Theme.cardSurface)
                        border.width: catBtn.selected ? 1 : 0
                        border.color: catBtn.selected ? Theme.accentBorder : "transparent"

                        RowLayout {
                            id: catRow

                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.right: root.compactMode ? undefined : parent.right
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            StudioIcon {
                                name: catBtn.modelData.icon
                                size: 15
                                color: catBtn.selected ? Theme.accentPrimary : Theme.textSecondary
                            }

                            Text {
                                text: catBtn.modelData.label
                                color: catBtn.selected ? Theme.accentPrimary : Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                font.weight: catBtn.selected ? Font.DemiBold : Font.Medium
                                elide: Text.ElideRight
                                Layout.fillWidth: !root.compactMode
                            }
                        }

                        MouseArea {
                            id: catMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                searchField.text = ""
                                root.selectedCategory = catBtn.modelData.id
                            }
                        }
                    }
                }
            }
        }

        // Right category settings content pane
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.vertical: StudioScrollBar {
                policy: ScrollBar.AsNeeded
            }

            ColumnLayout {
                width: parent.width
                spacing: Theme.spacingMd

                // Category 0: General & Appearance
                StudioCard {
                    visible: root.isCategoryVisible("general")
                    Layout.fillWidth: true
                    implicitHeight: genCol.implicitHeight + 28
                    surfaceColor: Theme.panelSurface

                    ColumnLayout {
                        id: genCol

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
                                Layout.preferredHeight: 30
                                onClicked: ApplicationController.setUiLanguage("en")
                            }

                            StudioButton {
                                variant: ApplicationController.uiLanguage === "pt" ? "primary" : "secondary"
                                text: qsTr("Português")
                                Layout.preferredHeight: 30
                                onClicked: ApplicationController.setUiLanguage("pt")
                            }
                        }
                    }
                }

                // Category 1: Congregation, meeting schedule, and meeting media
                SettingsMeetingsPane {
                    visible: root.isCategoryVisible("schedule")
                    Layout.fillWidth: true
                }

                // Category 2: Display & Standby
                StudioCard {
                    visible: root.isCategoryVisible("display")
                    Layout.fillWidth: true
                    implicitHeight: dispCol.implicitHeight + 28
                    surfaceColor: Theme.panelSurface

                    ColumnLayout {
                        id: dispCol

                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            StudioIcon {
                                name: "monitor"
                                size: 17
                                color: Theme.accentPrimary
                            }

                            Text {
                                text: qsTr("Display Routing & Standby")
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLg
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                            }

                            StudioButton {
                                variant: "ghost"
                                text: qsTr("Auto")
                                onClicked: ApplicationController.displayManager.resetAutoSelection()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: qsTr("Presentation Output Active")
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                Layout.fillWidth: true
                            }

                            StudioSwitch {
                                checked: ApplicationController.displayManager.presentationWindowVisible
                                onToggled: ApplicationController.displayManager.presentationWindowVisible = checked
                            }
                        }

                        Repeater {
                            model: ApplicationController.displayManager.screens

                            delegate: Rectangle {
                                id: dispRow

                                required property var modelData
                                readonly property bool selectedScreen: dispRow.modelData.index === ApplicationController.displayManager.targetScreenIndex

                                Layout.fillWidth: true
                                Layout.preferredHeight: 38
                                radius: Theme.radiusSmall
                                color: dispRow.selectedScreen ? Theme.rowSelected : Theme.cardSurface
                                border.width: 1
                                border.color: dispRow.selectedScreen ? Theme.accentPrimary : Theme.borderHairline

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 8

                                    Text {
                                        text: dispRow.modelData.name
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: dispRow.modelData.width + "x" + dispRow.modelData.height
                                        color: Theme.textMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeXs
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: ApplicationController.displayManager.manualScreenIndex = dispRow.modelData.index
                                }
                            }
                        }
                    }
                }

                // Category 3: Audio & Background Music
                StudioCard {
                    visible: root.isCategoryVisible("audio")
                    Layout.fillWidth: true
                    implicitHeight: audioCol.implicitHeight + 28
                    surfaceColor: Theme.panelSurface

                    ColumnLayout {
                        id: audioCol

                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            StudioIcon {
                                name: "audio-lines"
                                size: 17
                                color: Theme.accentPrimary
                            }

                            Text {
                                text: qsTr("Background Music & Fade Timing")
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLg
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                            }

                            StudioBadge {
                                text: ApplicationController.backgroundMusic.targetVolume + "%"
                                badgeColor: Theme.cardSurface
                                textColor: Theme.textSecondary
                                hasBorder: true
                            }
                        }

                        StudioSlider {
                            id: bgmVolSlider

                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            stepSize: 1
                            value: ApplicationController.backgroundMusic.targetVolume
                            onMoved: ApplicationController.backgroundMusic.targetVolume = Math.round(bgmVolSlider.value)
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: qsTr("Fade-Out Duration")
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                Layout.fillWidth: true
                            }

                            Repeater {
                                model: [3000, 5000, 8000]

                                delegate: StudioButton {
                                    id: fadeBtn

                                    required property int modelData

                                    variant: ApplicationController.backgroundMusic.fadeDurationMs === fadeBtn.modelData
                                             ? "primary" : "secondary"
                                    text: (fadeBtn.modelData / 1000) + "s"
                                    Layout.preferredHeight: 30
                                    onClicked: {
                                        ApplicationController.saveFadeDurationMs(fadeBtn.modelData)
                                        ApplicationController.showToast(qsTr("Updated fade duration"), "info")
                                    }
                                }
                            }
                        }
                    }
                }

                // Category 4: OBS Studio, Zoom & Watched Cloud Folder
                ColumnLayout {
                    visible: root.isCategoryVisible("integrations")
                    Layout.fillWidth: true
                    spacing: Theme.spacingMd

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

                                    Layout.preferredWidth: 82
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
                                        ApplicationController.obsClient.host = obsHostField.text.trim()
                                        ApplicationController.obsClient.port = parseInt(obsPortField.text, 10) || 4455
                                        ApplicationController.obsClient.enabled = true
                                        ApplicationController.obsClient.connectToObs()
                                        ApplicationController.zoomBridge.meetingId = zoomIdField.text.trim()
                                        ApplicationController.saveObsAndZoomSettings()
                                        ApplicationController.showToast(qsTr("Booth integration settings saved"), "success")
                                    }
                                }
                            }

                            StudioButton {
                                Layout.fillWidth: true
                                variant: "secondary"
                                iconName: "external-link"
                                text: qsTr("Launch Zoom Meeting")
                                enabled: ApplicationController.zoomBridge.meetingId.length > 0
                                onClicked: {
                                    if (!ApplicationController.zoomBridge.joinMeeting()) {
                                        ApplicationController.showToast(qsTr("Set a valid Zoom Meeting ID first"), "warning")
                                    }
                                }
                            }
                        }
                    }

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
                }

                // Category 5: Booth PIN Security, Shortcuts & Runtime Diagnostics
                SettingsSecurityCard {
                    visible: root.isCategoryVisible("security")
                    Layout.fillWidth: true
                }
            }
        }
    }
}
