pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

Item {
    id: root

    property bool compactMode: false
    property bool showPreviewToggle: false
    property bool previewOpen: false
    signal addMediaRequested()
    signal previewToggled()

    function sectionColor(title) {
        const _watch = ApplicationController.playlist.sections
        return ApplicationController.playlist.colorForSection(title)
    }

    readonly property var sectionGlyphs: ({
        "tgw": "section-treasures",
        "ayfm": "section-ministry",
        "lac": "section-living",
        "pt": "section-public-talk",
        "wt": "section-watchtower",
        "circuit-overseer": "section-overseer"
    })

    function sectionGlyph(sectionId) {
        const glyph = root.sectionGlyphs[sectionId]
        if (ApplicationController.demoMode || glyph === undefined) {
            return { category: "lucide", name: "list-video" }
        }
        return { category: "custom", name: glyph }
    }

    CustomSectionPopup {
        id: sectionPopup
    }

    BoothChecklistPopup {
        id: checklistPopup
    }

    Popup {
        id: tagEditPopup

        property int targetIndex: -1
        property string selectedTagType: "para"

        function openFor(idx, currentType, currentVal) {
            tagEditPopup.targetIndex = idx
            tagEditPopup.selectedTagType = currentType.length > 0 ? currentType : "para"
            tagValueField.text = currentVal
            tagEditPopup.open()
        }

        width: 280
        padding: 12
        modal: true
        focus: true
        anchors.centerIn: Overlay.overlay

        background: StudioCard {
            surfaceColor: Theme.cardElevated
            strokeColor: Theme.borderStrong
        }

        contentItem: ColumnLayout {
            spacing: 8

            Text {
                text: qsTr("Edit Media Tag")
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.DemiBold
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Repeater {
                    model: [
                        { type: "song", label: qsTr("Song") },
                        { type: "para", label: qsTr("Para.") },
                        { type: "footnote", label: "*" },
                        { type: "custom", label: qsTr("Custom") }
                    ]

                    delegate: StudioButton {
                        id: tBtn

                        required property var modelData

                        variant: tagEditPopup.selectedTagType === tBtn.modelData.type ? "primary" : "secondary"
                        text: tBtn.modelData.label
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        onClicked: tagEditPopup.selectedTagType = tBtn.modelData.type
                    }
                }
            }

            TextField {
                id: tagValueField

                Layout.fillWidth: true
                placeholderText: qsTr("e.g. Song 126, Para. 4, *")
                color: Theme.textPrimary
                placeholderTextColor: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                background: StudioCard {
                    surfaceColor: Theme.windowBase
                    cornerRadius: Theme.radiusSmall
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                StudioButton {
                    variant: "ghost"
                    text: qsTr("Clear")
                    onClicked: {
                        ApplicationController.playlist.updateItemTag(tagEditPopup.targetIndex, "", "")
                        tagEditPopup.close()
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                StudioButton {
                    variant: "primary"
                    text: qsTr("Save")
                    onClicked: {
                        ApplicationController.playlist.updateItemTag(
                            tagEditPopup.targetIndex,
                            tagEditPopup.selectedTagType,
                            tagValueField.text
                        )
                        tagEditPopup.close()
                    }
                }
            }
        }
    }

    function formatMsToClock(ms) {
        if (ms <= 0) {
            return "--:--"
        }
        const totalSec = Math.floor(ms / 1000)
        const hrs = Math.floor(totalSec / 3600)
        const mins = Math.floor((totalSec % 3600) / 60)
        const secs = totalSec % 60
        const pad = (n) => (n < 10 ? "0" + n : "" + n)
        if (hrs > 0) {
            return hrs + ":" + pad(mins) + ":" + pad(secs)
        }
        return pad(mins) + ":" + pad(secs)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingMd

        // Top Header Card: Playlist identity + week/meeting switcher + summary CTA bar
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: headerCol.implicitHeight + 24
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: headerCol

                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                // Row 1: Title + Week/Playlist switcher
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: Theme.radiusSmall
                        color: Theme.cardSurface
                        border.width: 1
                        border.color: Theme.borderHairline

                        StudioIcon {
                            anchors.centerIn: parent
                            name: "calendar"
                            size: 17
                            color: Theme.accentPrimary
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            text: ApplicationController.playlist.playlistName
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTitle
                            font.weight: Font.Bold
                            font.letterSpacing: -0.2
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: ApplicationController.playlist.playlistSubtitle
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    // Date-keyed Congregation Workspace Strip
                    RowLayout {
                        visible: !root.compactMode
                        spacing: 4

                        StudioButton {
                            id: prevDayBtn

                            variant: "secondary"
                            iconName: "chevron-left"
                            iconSize: 15
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            leftPadding: 0
                            rightPadding: 0
                            onClicked: {
                                ApplicationController.calendar.shiftSelectedDate(-1)
                                ApplicationController.openDateWorkspace(ApplicationController.calendar.selectedDateIso, false)
                            }
                            ToolTip.visible: prevDayBtn.hovered
                            ToolTip.text: qsTr("Previous day workspace")
                        }

                        StudioButton {
                            id: playlistSwitchBtn

                            variant: "secondary"
                            text: ApplicationController.calendar.selectedDateIso + " (" + ApplicationController.calendar.selectedMeetingKind.toUpperCase() + ")"
                            trailingIconName: "chevron-down"
                            iconSize: 13
                            Layout.preferredHeight: 32
                            onClicked: playlistMenuPopup.open()

                            Popup {
                                id: playlistMenuPopup

                                x: 0
                                y: playlistSwitchBtn.height + 4
                                width: Math.max(240, playlistSwitchBtn.width)
                                padding: 6

                                background: StudioCard {
                                    surfaceColor: Theme.cardElevated
                                    strokeColor: Theme.borderStrong
                                }

                                contentItem: ColumnLayout {
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        StudioButton {
                                            variant: ApplicationController.calendar.selectedMeetingKind === "midweek" ? "primary" : "secondary"
                                            text: qsTr("Midweek")
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 28
                                            onClicked: {
                                                ApplicationController.openDateWorkspace(
                                                    ApplicationController.calendar.meetingDateInWeek(
                                                        ApplicationController.calendar.selectedDateIso, "midweek"), false)
                                                playlistMenuPopup.close()
                                            }
                                        }

                                        StudioButton {
                                            variant: ApplicationController.calendar.selectedMeetingKind === "weekend" ? "primary" : "secondary"
                                            text: qsTr("Weekend")
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 28
                                            onClicked: {
                                                ApplicationController.openDateWorkspace(
                                                    ApplicationController.calendar.meetingDateInWeek(
                                                        ApplicationController.calendar.selectedDateIso, "weekend"), false)
                                                playlistMenuPopup.close()
                                            }
                                        }

                                        StudioButton {
                                            variant: "ghost"
                                            text: qsTr("Today")
                                            Layout.preferredHeight: 28
                                            onClicked: {
                                                ApplicationController.calendar.goToToday()
                                                playlistMenuPopup.close()
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 1
                                        color: Theme.borderHairline
                                    }

                                    Repeater {
                                        model: ApplicationController.playlist.availablePlaylistNames

                                        delegate: Rectangle {
                                            id: plOption

                                            required property string modelData
                                            required property int index

                                            readonly property bool isCurrent: index === ApplicationController.playlist.activePlaylistIndex

                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 32
                                            radius: Theme.radiusSmall
                                            color: plHover.containsMouse ? Theme.cardHover
                                                 : (plOption.isCurrent ? Theme.accentSoft : "transparent")

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8

                                                Text {
                                                    text: plOption.modelData
                                                    color: plOption.isCurrent ? Theme.accentPrimary : Theme.textPrimary
                                                    font.family: Theme.fontFamily
                                                    font.pixelSize: Theme.fontSizeSm
                                                    font.weight: plOption.isCurrent ? Font.DemiBold : Font.Normal
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }

                                                StudioIcon {
                                                    visible: plOption.isCurrent
                                                    name: "check"
                                                    size: 14
                                                    color: Theme.accentPrimary
                                                }
                                            }

                                            MouseArea {
                                                id: plHover

                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    ApplicationController.playlist.switchPlaylistByIndex(plOption.index)
                                                    playlistMenuPopup.close()
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        StudioButton {
                            id: nextDayBtn

                            variant: "secondary"
                            iconName: "chevron-right"
                            iconSize: 15
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            leftPadding: 0
                            rightPadding: 0
                            onClicked: {
                                ApplicationController.calendar.shiftSelectedDate(1)
                                ApplicationController.openDateWorkspace(ApplicationController.calendar.selectedDateIso, false)
                            }
                            ToolTip.visible: nextDayBtn.hovered
                            ToolTip.text: qsTr("Next day workspace")
                        }
                    }

                    StudioButton {
                        visible: root.showPreviewToggle
                        variant: root.previewOpen ? "primary" : "secondary"
                        iconName: "monitor"
                        text: root.compactMode ? "" : (root.previewOpen ? qsTr("Hide Preview") : qsTr("Preview"))
                        Layout.preferredHeight: 32
                        leftPadding: root.compactMode ? 8 : 10
                        rightPadding: root.compactMode ? 8 : 10
                        onClicked: root.previewToggled()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Toggle live preview and inspector")
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.borderHairline
                }

                // Row 2: Media count + duration + Search + Hidden toggle + Organization CTAs
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.compactMode ? 6 : 8

                    RowLayout {
                        spacing: root.compactMode ? 4 : 6

                        StudioIcon {
                            name: "layers"
                            size: 14
                            color: Theme.textSecondary
                        }

                        Text {
                            text: root.compactMode
                                  ? ("" + ApplicationController.playlist.count)
                                  : qsTr("%1 items").arg(ApplicationController.playlist.count)
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Font.Medium
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.preferredHeight: 14
                        color: Theme.borderHairline
                    }

                    RowLayout {
                        spacing: root.compactMode ? 4 : 6

                        StudioIcon {
                            name: "clock"
                            size: 14
                            color: Theme.textSecondary
                        }

                        Text {
                            text: root.compactMode
                                  ? ApplicationController.playlist.formattedTotalDuration
                                  : qsTr("%1 duration").arg(ApplicationController.playlist.formattedTotalDuration)
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Font.Medium
                        }
                    }

                    StudioBadge {
                        visible: !root.compactMode && ApplicationController.catalog.weekEventLabel.length > 0
                        text: ApplicationController.catalog.weekEventLabel
                        showDot: true
                        dotColor: ApplicationController.catalog.weekEventType === "co_visit"
                                  ? "#3F7F5F" : "#6B4E9B"
                        badgeColor: Theme.accentSoft
                        textColor: Theme.accentPrimary
                        hasBorder: true
                    }

                    StudioCard {
                        visible: !root.compactMode
                        Layout.fillWidth: true
                        Layout.minimumWidth: 120
                        Layout.maximumWidth: 220
                        Layout.preferredHeight: 32
                        surfaceColor: Theme.windowBase
                        cornerRadius: Theme.radiusSmall

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            StudioIcon {
                                name: "search"
                                size: 13
                                color: Theme.textMuted
                            }

                            TextField {
                                id: playlistSearchField

                                Layout.fillWidth: true
                                placeholderText: qsTr("Filter items...")
                                text: ApplicationController.playlist.searchQuery
                                onTextChanged: ApplicationController.playlist.searchQuery = playlistSearchField.text
                                color: Theme.textPrimary
                                placeholderTextColor: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                leftPadding: 0
                                rightPadding: 0
                                background: Item {}

                                Shortcut {
                                    sequence: "Ctrl+F"
                                    onActivated: playlistSearchField.forceActiveFocus()
                                }
                            }
                        }
                    }

                    Item {
                        visible: root.compactMode
                        Layout.fillWidth: true
                    }

                    StudioButton {
                        id: showHiddenBtn

                        visible: !root.compactMode && (ApplicationController.playlist.hiddenCount > 0 || ApplicationController.playlist.showHidden)
                        variant: ApplicationController.playlist.showHidden ? "primary" : "secondary"
                        iconName: ApplicationController.playlist.showHidden ? "eye" : "eye-off"
                        text: qsTr("Hidden (%1)").arg(ApplicationController.playlist.hiddenCount)
                        Layout.preferredHeight: 32
                        onClicked: ApplicationController.playlist.showHidden = !ApplicationController.playlist.showHidden
                        ToolTip.visible: showHiddenBtn.hovered
                        ToolTip.text: qsTr("Show or conceal hidden meeting items")
                    }

                    StudioButton {
                        id: dividerBtn

                        visible: !root.compactMode
                        variant: "secondary"
                        iconName: "minus"
                        text: qsTr("Divider")
                        Layout.preferredHeight: 32
                        onClicked: ApplicationController.playlist.addDivider(
                            qsTr("Part Transition"),
                            ApplicationController.playlist.sections.length > 0
                                ? ApplicationController.playlist.sections[0].id
                                : qsTr("Meeting"),
                            "#D47B2A",
                            ApplicationController.playlist.activeIndex)
                        ToolTip.visible: dividerBtn.hovered
                        ToolTip.text: qsTr("Insert colored visual separator")
                    }

                    StudioButton {
                        id: resetOrderBtn

                        visible: !root.compactMode && ApplicationController.playlist.count > 1
                        variant: "ghost"
                        iconName: "rotate-ccw"
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        leftPadding: 0
                        rightPadding: 0
                        onClicked: ApplicationController.playlist.resetSortOrder()
                        ToolTip.visible: resetOrderBtn.hovered
                        ToolTip.text: qsTr("Reset original synced order")
                    }

                    StudioButton {
                        id: checklistBtn

                        variant: "secondary"
                        iconName: "clipboard-check"
                        text: root.compactMode ? "" : qsTr("Checklist")
                        leftPadding: root.compactMode ? 0 : 10
                        rightPadding: root.compactMode ? 0 : 10
                        Layout.preferredWidth: root.compactMode ? 34 : checklistBtn.implicitWidth
                        Layout.preferredHeight: 34
                        onClicked: checklistPopup.open()
                        ToolTip.visible: checklistBtn.hovered
                        ToolTip.text: qsTr("Pre and post-meeting booth checklists")
                    }

                    StudioButton {
                        id: addSectionBtn

                        variant: "secondary"
                        iconName: "folder-plus"
                        text: root.compactMode ? "" : qsTr("Section")
                        leftPadding: root.compactMode ? 0 : 10
                        rightPadding: root.compactMode ? 0 : 10
                        Layout.preferredWidth: root.compactMode ? 34 : addSectionBtn.implicitWidth
                        Layout.preferredHeight: 34
                        onClicked: sectionPopup.openCreate()
                        ToolTip.visible: addSectionBtn.hovered
                        ToolTip.text: qsTr("Create custom meeting section")
                    }

                    StudioButton {
                        id: syncWeekBtn

                        variant: "secondary"
                        iconName: "refresh-cw"
                        text: root.compactMode ? "" : qsTr("Sync Week")
                        leftPadding: root.compactMode ? 0 : 10
                        rightPadding: root.compactMode ? 0 : 10
                        Layout.preferredWidth: root.compactMode ? 34 : syncWeekBtn.implicitWidth
                        Layout.preferredHeight: 34
                        enabled: !ApplicationController.catalog.busy
                        onClicked: ApplicationController.catalog.syncDateToPlaylist(
                            ApplicationController.calendar.selectedDateIso.length > 0
                                ? new Date(ApplicationController.calendar.selectedDateIso + "T12:00:00")
                                : new Date(),
                            ApplicationController.calendar.selectedMeetingKind)
                        ToolTip.visible: syncWeekBtn.hovered
                        ToolTip.text: qsTr("Fetch weekly workbook and Watchtower media automatically")
                    }

                    StudioButton {
                        id: addMediaBtn

                        variant: "primary"
                        iconName: "plus"
                        text: root.compactMode ? qsTr("Add") : qsTr("Add Media")
                        leftPadding: root.compactMode ? 8 : 12
                        rightPadding: root.compactMode ? 8 : 12
                        Layout.preferredHeight: 34
                        onClicked: root.addMediaRequested()
                    }
                }

                // Custom section chips strip for quick color, reorder, and section creation
                Flow {
                    visible: !root.compactMode && ApplicationController.playlist.sections.length > 0
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: ApplicationController.playlist.sections

                        delegate: Rectangle {
                            id: secChip

                            required property var modelData

                            width: secChipRow.implicitWidth + 16
                            height: 24
                            radius: 12
                            color: secChipHover.containsMouse ? Theme.cardHover : Theme.cardSurface
                            border.width: 1
                            border.color: Theme.borderHairline

                            RowLayout {
                                id: secChipRow

                                anchors.centerIn: parent
                                spacing: 6

                                Rectangle {
                                    Layout.preferredWidth: 8
                                    Layout.preferredHeight: 8
                                    radius: 4
                                    color: secChip.modelData.color
                                }

                                Text {
                                    text: secChip.modelData.name + " (" + secChip.modelData.itemCount + ")"
                                    color: Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    font.weight: Font.Medium
                                }
                            }

                            MouseArea {
                                id: secChipHover

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: sectionPopup.openEdit(
                                    secChip.modelData.id,
                                    secChip.modelData.color)
                            }
                        }
                    }
                }
            }
        }

        // Structured Playlist Table Card
        StudioCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 4

                // Table Column Header Row
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    Layout.leftMargin: 8
                    Layout.rightMargin: 18
                    spacing: 10

                    Item {
                        visible: !root.compactMode
                        Layout.preferredWidth: 16
                    }

                    Text {
                        Layout.preferredWidth: 22
                        text: "#"
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Media")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.preferredWidth: 56
                        text: qsTr("Duration")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignRight
                    }

                    Text {
                        Layout.preferredWidth: 46
                        text: qsTr("Status")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.borderHairline
                }

                // Playlist Items ListView
                ListView {
                    id: playlistView

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    topMargin: 4
                    bottomMargin: 4
                    model: ApplicationController.playlist
                    currentIndex: ApplicationController.playlist.activeIndex

                    ScrollBar.vertical: StudioScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    // Meeting section dividers (Treasures, Apply Yourself, Living as Christians,
                    // Public Talk, Watchtower Study, Circuit Overseer, custom). Each section gets
                    // its own color avatar with a generic list glyph, like the M3 demo layout.
                    section.property: "section"
                    section.criteria: ViewSection.FullString
                    section.delegate: Item {
                        id: sectionHeader

                        required property string section

                        readonly property var secMeta: {
                            const list = ApplicationController.playlist.sections
                            for (let i = 0; i < list.length; ++i) {
                                if (list[i].id === sectionHeader.section) {
                                    return list[i]
                                }
                            }
                            return { id: sectionHeader.section, name: sectionHeader.section, color: "#945EB5", repeatable: false, collapsed: false, itemCount: 0 }
                        }
                        readonly property string resolvedColorHex: {
                            const _watch = ApplicationController.playlist.sections
                            return ApplicationController.playlist.colorForSection(sectionHeader.section)
                        }
                        readonly property var resolvedGlyph: root.sectionGlyph(sectionHeader.section)

                        width: playlistView.width - 10
                        height: 40

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            anchors.topMargin: 6
                            anchors.bottomMargin: 4
                            spacing: 8

                            StudioButton {
                                id: secCollapseBtn

                                variant: "ghost"
                                iconName: sectionHeader.secMeta.collapsed ? "chevron-right" : "chevron-down"
                                iconSize: 13
                                leftPadding: 0
                                rightPadding: 0
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                                onClicked: ApplicationController.playlist.toggleSectionCollapsed(sectionHeader.section)
                            }

                            Rectangle {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                Layout.alignment: Qt.AlignVCenter
                                radius: 12
                                color: sectionHeader.resolvedColorHex

                                StudioIcon {
                                    anchors.centerIn: parent
                                    name: sectionHeader.resolvedGlyph.name
                                    category: sectionHeader.resolvedGlyph.category
                                    size: 13
                                    color: "#FFFFFF"
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: {
                                    const _watch = ApplicationController.playlist.sections
                                    return ApplicationController.playlist.sectionLabel(sectionHeader.section)
                                }
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Font.Bold
                                font.letterSpacing: 0.6
                                font.capitalization: Font.AllUppercase
                                elide: Text.ElideRight
                            }

                            StudioButton {
                                id: repeatSecBtn

                                variant: sectionHeader.secMeta.repeatable ? "primary" : "ghost"
                                iconName: "repeat"
                                iconSize: 12
                                leftPadding: 0
                                rightPadding: 0
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                onClicked: ApplicationController.playlist.setSectionRepeatable(
                                    sectionHeader.section, !sectionHeader.secMeta.repeatable)
                                ToolTip.visible: repeatSecBtn.hovered
                                ToolTip.text: qsTr("Toggle section repeat / slideshow")
                            }

                            StudioButton {
                                id: editSecBtn

                                variant: "ghost"
                                iconName: "sliders-horizontal"
                                iconSize: 12
                                leftPadding: 0
                                rightPadding: 0
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                onClicked: sectionPopup.openEdit(sectionHeader.section, sectionHeader.resolvedColorHex)
                                ToolTip.visible: editSecBtn.hovered
                                ToolTip.text: qsTr("Customize or reorder section")
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            height: 1
                            color: sectionHeader.resolvedColorHex
                            opacity: 0.45
                        }
                    }

                    // Empty state placeholder when playlist has zero items
                    Item {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 32, 360)
                        height: emptyCol.implicitHeight
                        visible: playlistView.count === 0

                        ColumnLayout {
                            id: emptyCol

                            anchors.fill: parent
                            spacing: 10

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                                radius: 24
                                color: Theme.cardSurface
                                border.width: 1
                                border.color: Theme.borderHairline

                                StudioIcon {
                                    anchors.centerIn: parent
                                    name: "folder-plus"
                                    size: 22
                                    color: Theme.accentPrimary
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("No media queued for this meeting")
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLg
                                font.weight: Font.DemiBold
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("Drop video, audio, images, PDFs, .jwpub, or .jwlplaylist files anywhere, or press Ctrl+O.")
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    delegate: Rectangle {
                        id: rowDelegate

                        required property int index
                        required property string title
                        required property string subtitle
                        required property string section
                        required property string thumbUrl
                        required property string kindLabel
                        required property int mediaKind
                        required property string formattedDuration
                        required property int trimStart
                        required property int trimEnd
                        required property bool fileExists
                        required property bool isActive
                        required property string groupKey
                        required property bool groupExpanded
                        required property bool isGroupStart
                        required property int groupItemCount
                        required property string itemKind
                        required property bool hidden
                        required property string tagType
                        required property string tagValue
                        required property string streamUrl
                        required property real playbackRate
                        required property bool isCbs
                        required property string dividerColor
                        required property int slideIntervalMs
                        required property string pairedAudioPath
                        required property bool rowVisible

                        readonly property bool isDivider: itemKind === "divider"
                        readonly property bool isVideoVisual: kindLabel === "VID" || subtitle.indexOf("Visual") >= 0
                        readonly property bool hasGroupHeader: isGroupStart && groupItemCount > 1
                        readonly property string typeIconName: {
                            if (isDivider) return "minus"
                            if (kindLabel === "AUD") return "audio-lines"
                            if (kindLabel === "PDF") return "file-text"
                            if (isVideoVisual) return "film"
                            if (kindLabel === "STRM") return "globe"
                            return "image"
                        }

                        visible: rowDelegate.rowVisible
                        width: playlistView.width - 10
                        height: !rowDelegate.rowVisible ? 0
                              : (rowDelegate.isDivider ? 36 : (rowDelegate.hasGroupHeader ? (root.compactMode ? 78 : 84) : (root.compactMode ? 54 : 60)))
                        radius: Theme.radiusMedium
                        opacity: rowDelegate.hidden ? 0.52 : 1.0
                        color: rowDelegate.isDivider ? "transparent"
                             : (rowDelegate.isActive ? Theme.rowSelected
                                : (rowHover.containsMouse ? Theme.cardHover : "transparent"))
                        border.width: (!rowDelegate.isDivider && rowDelegate.isActive) ? 1 : 0
                        border.color: (!rowDelegate.isDivider && rowDelegate.isActive) ? Theme.accentPrimary : "transparent"

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.animFast
                            }
                        }

                        // Colored divider bar variant
                        RowLayout {
                            visible: rowDelegate.isDivider
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 3
                                radius: 2
                                color: rowDelegate.dividerColor.length > 0 ? rowDelegate.dividerColor : Theme.accentPrimary
                            }

                            Text {
                                text: rowDelegate.title
                                color: rowDelegate.dividerColor.length > 0 ? rowDelegate.dividerColor : Theme.accentPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Font.Bold
                                font.capitalization: Font.AllUppercase
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 2
                                radius: 1
                                opacity: 0.45
                                color: rowDelegate.dividerColor.length > 0 ? rowDelegate.dividerColor : Theme.accentPrimary
                            }

                            StudioButton {
                                variant: "ghost"
                                iconName: "trash-2"
                                iconSize: 12
                                leftPadding: 0
                                rightPadding: 0
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                onClicked: ApplicationController.playlist.removeAt(rowDelegate.index)
                            }
                        }

                        ColumnLayout {
                            visible: !rowDelegate.isDivider
                            anchors.fill: parent
                            spacing: 2

                            // Optional collapsible extract MediaGroup banner above first group member
                            Rectangle {
                                visible: rowDelegate.hasGroupHeader
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                Layout.leftMargin: 8
                                Layout.rightMargin: 8
                                radius: Theme.radiusSmall
                                color: Theme.cardSurface

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 6

                                    StudioIcon {
                                        name: rowDelegate.groupExpanded ? "chevron-down" : "chevron-right"
                                        size: 12
                                        color: Theme.accentPrimary
                                    }

                                    Text {
                                        text: rowDelegate.groupKey
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeXs
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: qsTr("%1 items").arg(rowDelegate.groupItemCount)
                                        color: Theme.textMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 10
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: ApplicationController.playlist.toggleGroupExpanded(rowDelegate.groupKey)
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.leftMargin: 8
                                Layout.rightMargin: 8
                                spacing: 10

                                // Column 1: Drag handle
                                Item {
                                    visible: !root.compactMode
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 32

                                    StudioIcon {
                                        anchors.centerIn: parent
                                        name: "grip-vertical"
                                        size: 14
                                        color: dragMouse.pressed ? Theme.accentPrimary : Theme.textMuted
                                    }

                                    MouseArea {
                                        id: dragMouse

                                        property int startRow: -1
                                        property real pressSceneY: 0

                                        anchors.fill: parent
                                        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                        onPressed: (mouse) => {
                                            startRow = rowDelegate.index
                                            pressSceneY = mapToItem(playlistView, 0, mouse.y).y
                                            mouse.accepted = true
                                        }
                                        onPositionChanged: (mouse) => {
                                            if (!pressed || startRow < 0) {
                                                return
                                            }
                                            const curY = mapToItem(playlistView, 0, mouse.y).y
                                            const rowStep = rowDelegate.height + playlistView.spacing
                                            const delta = Math.round((curY - pressSceneY) / rowStep)
                                            const target = Math.max(0, Math.min(playlistView.count - 1, startRow + delta))
                                            if (target !== rowDelegate.index) {
                                                ApplicationController.playlist.moveItem(rowDelegate.index, target)
                                                startRow = target
                                                pressSceneY = curY
                                            }
                                        }
                                    }
                                }

                                // Column 2: 1-based item index
                                Text {
                                    Layout.preferredWidth: 22
                                    text: "" + (rowDelegate.index + 1)
                                    color: rowDelegate.isActive ? Theme.accentPrimary : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    font.weight: rowDelegate.isActive ? Font.Bold : Font.Medium
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                // Column 3: 16:9 Thumbnail + Title & Tags/Subtitle
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Rectangle {
                                        Layout.preferredWidth: root.compactMode ? 54 : 72
                                        Layout.preferredHeight: root.compactMode ? 32 : 42
                                        radius: Theme.radiusSmall
                                        color: Theme.cardSurface
                                        border.width: 1
                                        border.color: rowDelegate.isActive ? Theme.accentBorder : Theme.borderHairline
                                        clip: true

                                        Image {
                                            anchors.fill: parent
                                            source: rowDelegate.thumbUrl
                                            visible: rowDelegate.thumbUrl.length > 0
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                            smooth: true
                                            mipmap: true
                                        }

                                        StudioIcon {
                                            anchors.centerIn: parent
                                            visible: rowDelegate.thumbUrl.length === 0
                                            name: rowDelegate.typeIconName
                                            size: 16
                                            color: Theme.textSecondary
                                        }

                                        Rectangle {
                                            visible: rowDelegate.thumbUrl.length > 0 && rowDelegate.isVideoVisual
                                            anchors.centerIn: parent
                                            width: 20
                                            height: 20
                                            radius: 10
                                            color: "#8A0D0F12"

                                            StudioIcon {
                                                anchors.centerIn: parent
                                                anchors.horizontalCenterOffset: 1
                                                name: "play"
                                                size: 9
                                                color: "#FFFFFF"
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 6

                                            Text {
                                                text: rowDelegate.title
                                                color: Theme.textPrimary
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeMd
                                                font.weight: rowDelegate.isActive ? Font.DemiBold : Font.Medium
                                                font.strikeout: rowDelegate.hidden
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            Rectangle {
                                                visible: !root.compactMode && rowDelegate.tagValue.length > 0
                                                Layout.preferredHeight: 18
                                                Layout.preferredWidth: tagText.implicitWidth + 12
                                                radius: 9
                                                color: rowDelegate.tagType === "song" ? "#2E6F5E"
                                                     : (rowDelegate.tagType === "footnote" ? Theme.cardSurface : Theme.accentSoft)
                                                border.width: 1
                                                border.color: rowDelegate.tagType === "song" ? "#3B8D78" : Theme.accentBorder

                                                Text {
                                                    id: tagText

                                                    anchors.centerIn: parent
                                                    text: rowDelegate.tagValue
                                                    color: rowDelegate.tagType === "song" ? "#FFFFFF" : Theme.accentPrimary
                                                    font.family: Theme.fontFamily
                                                    font.pixelSize: 10
                                                    font.weight: Font.DemiBold
                                                }

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: tagEditPopup.openFor(rowDelegate.index, rowDelegate.tagType, rowDelegate.tagValue)
                                                }
                                            }

                                            StudioBadge {
                                                visible: rowDelegate.isCbs && !root.compactMode
                                                Layout.preferredHeight: 18
                                                text: qsTr("CBS")
                                                badgeColor: Theme.panelSurface
                                                textColor: Theme.textSecondary
                                                hasBorder: true
                                            }

                                            StudioBadge {
                                                visible: rowDelegate.kindLabel === "STRM" && !root.compactMode
                                                Layout.preferredHeight: 18
                                                text: qsTr("STREAM")
                                                badgeColor: Theme.accentSoft
                                                textColor: Theme.accentPrimary
                                                hasBorder: true
                                            }

                                            StudioBadge {
                                                visible: rowDelegate.trimEnd > rowDelegate.trimStart && !root.compactMode
                                                Layout.preferredHeight: 18
                                                text: qsTr("TRIM")
                                                badgeColor: Theme.accentSoft
                                                textColor: Theme.accentPrimary
                                                hasBorder: true
                                                strokeColor: Theme.accentBorder
                                            }

                                            StudioBadge {
                                                visible: rowDelegate.slideIntervalMs > 0 && !root.compactMode
                                                Layout.preferredHeight: 18
                                                text: (rowDelegate.slideIntervalMs / 1000) + "s"
                                                badgeColor: Theme.cardSurface
                                                textColor: Theme.textSecondary
                                                hasBorder: true
                                            }
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 5

                                            StudioIcon {
                                                name: rowDelegate.typeIconName
                                                size: 11
                                                color: Theme.textMuted
                                            }

                                            Text {
                                                text: rowDelegate.subtitle.length > 0 ? rowDelegate.subtitle : rowDelegate.section
                                                color: Theme.textSecondary
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeXs
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }
                                    }
                                }

                                // Column 4: Tabular Duration
                                Text {
                                    Layout.preferredWidth: 56
                                    text: rowDelegate.formattedDuration
                                    color: rowDelegate.isActive ? Theme.textPrimary : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    font.weight: rowDelegate.isActive ? Font.DemiBold : Font.Normal
                                    horizontalAlignment: Text.AlignRight
                                }

                                // Column 5: Hide toggle + Readiness Status Icon
                                RowLayout {
                                    Layout.preferredWidth: 54
                                    Layout.preferredHeight: 28
                                    spacing: 4

                                    StudioButton {
                                        id: hideRowBtn

                                        visible: !root.compactMode && (rowHover.containsMouse || rowDelegate.hidden)
                                        variant: "ghost"
                                        iconName: rowDelegate.hidden ? "eye" : "eye-off"
                                        iconSize: 13
                                        leftPadding: 0
                                        rightPadding: 0
                                        Layout.preferredWidth: 24
                                        Layout.preferredHeight: 24
                                        onClicked: ApplicationController.playlist.toggleItemHidden(rowDelegate.index)
                                        ToolTip.visible: hideRowBtn.hovered
                                        ToolTip.text: rowDelegate.hidden ? qsTr("Unhide item") : qsTr("Hide item")
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }

                                    StudioIcon {
                                        name: rowDelegate.fileExists ? "circle-check" : "triangle-exclamation-point"
                                        size: 16
                                        color: rowDelegate.fileExists ? Theme.statusReady : Theme.statusStandby
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: rowHover

                            anchors.fill: parent
                            anchors.leftMargin: root.compactMode ? 0 : 24
                            anchors.rightMargin: 54
                            anchors.topMargin: rowDelegate.hasGroupHeader ? 24 : 0
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            cursorShape: Qt.PointingHandCursor
                            onClicked: (mouse) => {
                                if (rowDelegate.isDivider) {
                                    if (mouse.button === Qt.RightButton) {
                                        rowContextMenu.popup()
                                    }
                                    return
                                }
                                if (mouse.button === Qt.RightButton) {
                                    ApplicationController.playlist.activateIndex(rowDelegate.index)
                                    rowContextMenu.popup()
                                } else {
                                    ApplicationController.playlist.activateIndex(rowDelegate.index)
                                }
                            }
                            onDoubleClicked: {
                                if (!rowDelegate.isDivider) {
                                    ApplicationController.playlist.activateIndex(rowDelegate.index)
                                    ApplicationController.playback.play()
                                }
                            }
                        }

                        Menu {
                            id: rowContextMenu

                            MenuItem {
                                text: qsTr("Play Now")
                                visible: !rowDelegate.isDivider
                                onTriggered: {
                                    ApplicationController.playlist.activateIndex(rowDelegate.index)
                                    ApplicationController.playback.play()
                                }
                            }
                            MenuItem {
                                text: rowDelegate.hidden ? qsTr("Show Item") : qsTr("Hide Item")
                                onTriggered: ApplicationController.playlist.toggleItemHidden(rowDelegate.index)
                            }
                            MenuItem {
                                text: qsTr("Edit Tag...")
                                visible: !rowDelegate.isDivider
                                onTriggered: tagEditPopup.openFor(rowDelegate.index, rowDelegate.tagType, rowDelegate.tagValue)
                            }
                            Menu {
                                title: qsTr("Slide Interval")
                                enabled: !rowDelegate.isDivider

                                MenuItem {
                                    text: qsTr("Manual (Off)")
                                    onTriggered: ApplicationController.playlist.setItemSlideInterval(rowDelegate.index, 0)
                                }
                                MenuItem {
                                    text: qsTr("10 seconds")
                                    onTriggered: ApplicationController.playlist.setItemSlideInterval(rowDelegate.index, 10000)
                                }
                                MenuItem {
                                    text: qsTr("30 seconds")
                                    onTriggered: ApplicationController.playlist.setItemSlideInterval(rowDelegate.index, 30000)
                                }
                                MenuItem {
                                    text: qsTr("1 minute")
                                    onTriggered: ApplicationController.playlist.setItemSlideInterval(rowDelegate.index, 60000)
                                }
                                MenuItem {
                                    text: qsTr("5 minutes")
                                    onTriggered: ApplicationController.playlist.setItemSlideInterval(rowDelegate.index, 300000)
                                }
                                MenuItem {
                                    text: qsTr("10 minutes")
                                    onTriggered: ApplicationController.playlist.setItemSlideInterval(rowDelegate.index, 600000)
                                }
                            }
                            MenuItem {
                                text: qsTr("Insert Divider Below")
                                onTriggered: ApplicationController.playlist.addDivider(
                                    qsTr("Section Break"), rowDelegate.section, "#D47B2A", rowDelegate.index)
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: qsTr("Move Up")
                                enabled: rowDelegate.index > 0
                                onTriggered: ApplicationController.playlist.moveItem(rowDelegate.index, rowDelegate.index - 1)
                            }
                            MenuItem {
                                text: qsTr("Move Down")
                                enabled: rowDelegate.index < playlistView.count - 1
                                onTriggered: ApplicationController.playlist.moveItem(rowDelegate.index, rowDelegate.index + 1)
                            }
                            MenuItem {
                                text: qsTr("Move to Section...")
                                onTriggered: sectionPopup.openAssign(rowDelegate.index, rowDelegate.section)
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: qsTr("Remove from Playlist")
                                onTriggered: ApplicationController.playlist.removeAt(rowDelegate.index)
                            }
                        }
                    }
                }
            }
        }
    }
}
