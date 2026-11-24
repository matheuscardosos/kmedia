pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

ColumnLayout {
    id: root

    property bool compactMode: false
    property string selectedCategory: "general"

    // The keywords are translated, so the search finds a category in the interface language.
    readonly property var categories: [
        { id: "general", label: qsTr("General"), icon: "globe", keywords: qsTr("general appearance theme dark light interface language congregation profile") },
        { id: "schedule", label: qsTr("Meetings"), icon: "calendar-days", keywords: qsTr("meetings congregation lookup schedule midweek weekend day time media language fallback footnote printed watchtower teaching study filter export ffmpeg mp4") },
        { id: "display", label: qsTr("Display"), icon: "monitor", keywords: qsTr("display screen projector presentation standby blackout picture") },
        { id: "audio", label: qsTr("Audio"), icon: "audio-lines", keywords: qsTr("audio background music fade volume output device speakers") },
        { id: "integrations", label: qsTr("Integrations"), icon: "radio", keywords: qsTr("obs websocket password scene zoom meeting passcode shared folder cloud dropbox onedrive") },
        { id: "security", label: qsTr("Security"), icon: "lock", keywords: qsTr("pin lock password security shortcuts diagnostics version memory") }
    ]

    // Lower case without accents, so "audio" also finds "Áudio".
    function fold(text) {
        const lower = text.toLowerCase()
        return lower.normalize ? lower.normalize("NFD").replace(/[\u0300-\u036f]/g, "") : lower
    }

    function isCategoryVisible(catId) {
        const q = root.fold(searchField.text.trim())
        if (q.length === 0) {
            return root.selectedCategory === catId
        }
        for (let i = 0; i < root.categories.length; ++i) {
            const category = root.categories[i]
            if (category.id === catId) {
                return root.fold(category.label + " " + category.keywords).indexOf(q) >= 0
            }
        }
        return false
    }

    function saveIntegrations() {
        ApplicationController.saveObsAndZoomSettings()
        ApplicationController.showToast(qsTr("Booth integration settings saved"), "success")
    }

    // A labeled value that opens a list of choices ({ value, label }) to pick from.
    component ChoiceRow: RowLayout {
        id: choiceRow

        property string text: ""
        property string value: ""
        property string valueLabel: ""
        property var choices: []
        signal chosen(string value)

        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: choiceRow.text
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            elide: Text.ElideRight
        }

        StudioButton {
            id: choiceButton

            variant: "secondary"
            text: choiceRow.valueLabel
            iconName: "chevron-down"
            Layout.preferredHeight: 30
            Layout.maximumWidth: 260
            onClicked: choicePopup.open()

            Popup {
                id: choicePopup

                y: choiceButton.height + 4
                implicitWidth: Math.max(choiceButton.width, 240)
                padding: 6
                margins: 8
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                background: StudioCard {
                    surfaceColor: Theme.cardElevated
                    strokeColor: Theme.borderStrong
                }

                contentItem: ListView {
                    implicitHeight: Math.min(contentHeight, 260)
                    clip: true
                    spacing: 2
                    model: choiceRow.choices

                    ScrollBar.vertical: StudioScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        id: choiceItem

                        required property var modelData
                        readonly property bool selected: choiceItem.modelData.value === choiceRow.value

                        width: ListView.view ? ListView.view.width : 240
                        height: 32
                        radius: Theme.radiusSmall
                        color: choiceMouse.containsMouse ? Theme.cardHover
                                                         : (choiceItem.selected ? Theme.accentSoft : "transparent")

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                text: choiceItem.modelData.label
                                color: choiceItem.selected ? Theme.accentPrimary : Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                font.weight: choiceItem.selected ? Font.DemiBold : Font.Normal
                                elide: Text.ElideRight
                            }

                            StudioIcon {
                                visible: choiceItem.selected
                                name: "check"
                                size: 14
                                color: Theme.accentPrimary
                            }
                        }

                        MouseArea {
                            id: choiceMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                choiceRow.chosen(choiceItem.modelData.value)
                                choicePopup.close()
                            }
                        }
                    }
                }
            }
        }
    }

    component SettingsField: TextField {
        Layout.fillWidth: true
        color: Theme.textPrimary
        placeholderTextColor: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
        background: StudioCard {
            surfaceColor: Theme.windowBase
            cornerRadius: Theme.radiusSmall
        }
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

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: qsTr("Floating preview")
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("While the preview panel is closed, show a small live preview of the presentation screen while a picture, PDF page, or video is on it.")
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    wrapMode: Text.WordWrap
                                }
                            }

                            StudioSwitch {
                                checked: ApplicationController.floatingPreview
                                onToggled: ApplicationController.floatingPreview = checked
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

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.borderHairline
                        }

                        RowLayout {
                            id: standbyRow

                            readonly property string imagePath: ApplicationController.playback.standbyImageUrl.toString()

                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: qsTr("Standby picture")
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                            }

                            Text {
                                Layout.fillWidth: true
                                text: decodeURIComponent(standbyRow.imagePath.substring(standbyRow.imagePath.lastIndexOf("/") + 1))
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideMiddle
                            }

                            StudioButton {
                                variant: "secondary"
                                iconName: "image"
                                text: qsTr("Choose")
                                Layout.preferredHeight: 30
                                onClicked: standbyDialog.open()
                            }

                            StudioButton {
                                visible: standbyRow.imagePath.length > 0
                                variant: "ghost"
                                iconName: "x"
                                leftPadding: 0
                                rightPadding: 0
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                onClicked: ApplicationController.saveStandbyImage("")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Remove picture")
                            }

                            FileDialog {
                                id: standbyDialog

                                title: qsTr("Standby picture")
                                nameFilters: [qsTr("Pictures (*.png *.jpg *.jpeg *.webp *.bmp)")]
                                onAccepted: ApplicationController.saveStandbyImage(standbyDialog.selectedFile)
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

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: qsTr("Start before meetings")
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                Layout.fillWidth: true
                            }

                            StudioSwitch {
                                checked: ApplicationController.music.autoStart
                                onToggled: ApplicationController.saveMusicAutoStart(checked)
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.borderHairline
                        }

                        ChoiceRow {
                            text: qsTr("Audio output")
                            value: ApplicationController.playback.audioDevice
                            valueLabel: ApplicationController.playback.audioDeviceDescription.length > 0
                                        ? ApplicationController.playback.audioDeviceDescription
                                        : qsTr("System default")
                            choices: {
                                const list = [{ value: "auto", label: qsTr("System default") }]
                                const devices = ApplicationController.playback.audioDevices
                                for (let i = 0; i < devices.length; ++i) {
                                    if (devices[i].name !== "auto") {
                                        list.push({ value: devices[i].name, label: devices[i].description })
                                    }
                                }
                                return list
                            }
                            onChosen: value => ApplicationController.saveAudioOutputDevice(value)
                        }
                    }
                }

                // Category 4: OBS Studio, Zoom, and the shared folder
                ColumnLayout {
                    visible: root.isCategoryVisible("integrations")
                    Layout.fillWidth: true
                    spacing: Theme.spacingMd

                    // OBS Studio: connection, then the scenes KMedia switches to on its own.
                    StudioCard {
                        Layout.fillWidth: true
                        implicitHeight: obsCol.implicitHeight + 28
                        surfaceColor: Theme.panelSurface

                        ColumnLayout {
                            id: obsCol

                            readonly property var obs: ApplicationController.obsClient
                            readonly property var sceneChoices: {
                                const list = [{ value: "", label: qsTr("Not set") }]
                                for (let i = 0; i < obsCol.obs.availableScenes.length; ++i) {
                                    list.push({ value: obsCol.obs.availableScenes[i], label: obsCol.obs.availableScenes[i] })
                                }
                                return list
                            }

                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                StudioIcon {
                                    name: "obs-studio"
                                    category: "custom"
                                    size: 17
                                    color: Theme.accentPrimary
                                }

                                Text {
                                    text: "OBS Studio"
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeLg
                                    font.weight: Font.DemiBold
                                    Layout.fillWidth: true
                                }

                                StudioBadge {
                                    visible: obsCol.obs.enabled
                                    text: obsCol.obs.connected ? qsTr("Connected") : qsTr("Not connected")
                                    badgeColor: Theme.cardSurface
                                    textColor: obsCol.obs.connected ? Theme.statusReady : Theme.textSecondary
                                    hasBorder: true
                                }

                                StudioSwitch {
                                    checked: obsCol.obs.enabled
                                    onToggled: {
                                        obsCol.obs.enabled = checked
                                        if (checked) {
                                            obsCol.obs.connectToObs()
                                        }
                                        ApplicationController.saveObsAndZoomSettings()
                                    }
                                }
                            }

                            RowLayout {
                                visible: obsCol.obs.enabled
                                Layout.fillWidth: true
                                spacing: 8

                                SettingsField {
                                    id: obsHostField

                                    placeholderText: qsTr("OBS Host (127.0.0.1)")
                                    text: obsCol.obs.host
                                }

                                SettingsField {
                                    id: obsPortField

                                    Layout.fillWidth: false
                                    Layout.preferredWidth: 82
                                    placeholderText: "4455"
                                    text: String(obsCol.obs.port)
                                    inputMethodHints: Qt.ImhDigitsOnly
                                }
                            }

                            RowLayout {
                                visible: obsCol.obs.enabled
                                Layout.fillWidth: true
                                spacing: 8

                                SettingsField {
                                    id: obsPasswordField

                                    placeholderText: qsTr("WebSocket password")
                                    text: obsCol.obs.password
                                    echoMode: TextInput.Password
                                }

                                StudioButton {
                                    variant: "primary"
                                    text: qsTr("Save & Connect")
                                    Layout.preferredHeight: 34
                                    onClicked: {
                                        obsCol.obs.host = obsHostField.text.trim()
                                        obsCol.obs.port = parseInt(obsPortField.text, 10) || 4455
                                        obsCol.obs.password = obsPasswordField.text
                                        obsCol.obs.connectToObs()
                                        root.saveIntegrations()
                                    }
                                }
                            }

                            ChoiceRow {
                                visible: obsCol.obs.enabled && obsCol.obs.availableScenes.length > 0
                                text: qsTr("Stage scene")
                                value: obsCol.obs.stageScene
                                valueLabel: obsCol.obs.stageScene.length > 0 ? obsCol.obs.stageScene : qsTr("Not set")
                                choices: obsCol.sceneChoices
                                onChosen: value => {
                                    obsCol.obs.stageScene = value
                                    ApplicationController.saveObsAndZoomSettings()
                                }
                            }

                            ChoiceRow {
                                visible: obsCol.obs.enabled && obsCol.obs.availableScenes.length > 0
                                text: qsTr("Media scene")
                                value: obsCol.obs.mediaScene
                                valueLabel: obsCol.obs.mediaScene.length > 0 ? obsCol.obs.mediaScene : qsTr("Not set")
                                choices: obsCol.sceneChoices
                                onChosen: value => {
                                    obsCol.obs.mediaScene = value
                                    ApplicationController.saveObsAndZoomSettings()
                                }
                            }

                            ChoiceRow {
                                visible: obsCol.obs.enabled && obsCol.obs.availableScenes.length > 0
                                text: qsTr("Image scene")
                                value: obsCol.obs.imageScene
                                valueLabel: obsCol.obs.imageScene.length > 0 ? obsCol.obs.imageScene : qsTr("Same as media scene")
                                choices: {
                                    const list = obsCol.sceneChoices.slice()
                                    list[0] = { value: "", label: qsTr("Same as media scene") }
                                    return list
                                }
                                onChosen: value => {
                                    obsCol.obs.imageScene = value
                                    ApplicationController.saveObsAndZoomSettings()
                                }
                            }
                        }
                    }

                    // Zoom: the congregation's meeting, joined from here or from the pill bar.
                    StudioCard {
                        Layout.fillWidth: true
                        implicitHeight: zoomCol.implicitHeight + 28
                        surfaceColor: Theme.panelSurface

                        ColumnLayout {
                            id: zoomCol

                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                StudioIcon {
                                    name: "zoom"
                                    category: "custom"
                                    size: 17
                                    color: Theme.accentPrimary
                                }

                                Text {
                                    text: "Zoom"
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

                                SettingsField {
                                    id: zoomIdField

                                    placeholderText: qsTr("Zoom Meeting ID")
                                    text: ApplicationController.zoomBridge.meetingId
                                }

                                SettingsField {
                                    id: zoomPassField

                                    placeholderText: qsTr("Passcode")
                                    text: ApplicationController.zoomBridge.meetingPassword
                                    echoMode: TextInput.Password
                                }
                            }

                            SettingsField {
                                id: zoomNameField

                                placeholderText: qsTr("Name shown in Zoom")
                                text: ApplicationController.zoomBridge.displayName
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                StudioButton {
                                    variant: "primary"
                                    text: qsTr("Save")
                                    Layout.preferredHeight: 34
                                    onClicked: {
                                        ApplicationController.zoomBridge.meetingId = zoomIdField.text.trim()
                                        ApplicationController.zoomBridge.meetingPassword = zoomPassField.text
                                        ApplicationController.zoomBridge.displayName = zoomNameField.text.trim()
                                        root.saveIntegrations()
                                    }
                                }

                                StudioButton {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
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
                    }

                    SharedFolderCard {
                        Layout.fillWidth: true
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
