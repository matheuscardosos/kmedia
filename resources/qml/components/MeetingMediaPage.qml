pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.qmlmodels
import KMedia.UI
import ".."
import "../controls"

// The media of the open day, laid out like the Meeting Media Manager media calendar: a header with
// the date picker and the add menu, then one card per section with its media rows.
Item {
    id: root

    property bool compactMode: false
    property bool showPreviewToggle: false
    property bool previewOpen: false
    signal previewToggled()

    readonly property string dateIso: ApplicationController.playlist.currentDateIso
    readonly property string meetingKind: ApplicationController.playlist.currentMeetingKind
    readonly property bool isMeetingDay: ApplicationController.meetingDay.isMeetingDay
    readonly property string dayStatus: ApplicationController.sync.dayStatuses[root.dateIso] ?? ""
    readonly property bool daySyncing: root.isMeetingDay
                                       && (root.dayStatus === "pending" || root.dayStatus === "syncing")
    readonly property var uiLocale: Qt.locale(ApplicationController.uiLanguage)
    readonly property string longDate: root.formatDay("dddd, d MMMM")
    readonly property string shortDate: root.formatDay("ddd, d MMM")
    readonly property string titleText: {
        if (root.meetingKind === "midweek") {
            return qsTr("Midweek Meeting")
        }
        if (root.meetingKind === "weekend") {
            return qsTr("Weekend Meeting")
        }
        if (root.meetingKind === "memorial") {
            return qsTr("Memorial")
        }
        return root.longDate
    }
    readonly property string previousDay: {
        const _watch = ApplicationController.sync.dayStatuses
        return ApplicationController.sync.dayWithContent(root.dateIso, -1)
    }
    readonly property string nextDay: {
        const _watch = ApplicationController.sync.dayStatuses
        return ApplicationController.sync.dayWithContent(root.dateIso, 1)
    }
    readonly property int thumbWidth: root.compactMode ? 100 : 150
    readonly property int thumbHeight: root.compactMode ? 56 : 84

    readonly property var sectionGlyphs: ({
        "tgw": "section-treasures",
        "ayfm": "section-ministry",
        "lac": "section-living",
        "pt": "section-public-talk",
        "wt": "section-watchtower",
        "circuit-overseer": "section-overseer"
    })

    function dateFromIso(dateIso) {
        const parts = dateIso.split("-")
        return parts.length === 3 ? new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]))
                                  : new Date()
    }

    function formatDay(format) {
        const text = root.dateFromIso(root.dateIso).toLocaleDateString(root.uiLocale, format)
        return text.length > 0 ? text.charAt(0).toUpperCase() + text.slice(1) : text
    }

    function statusColor(status) {
        if (status === "complete") {
            return Theme.statusReady
        }
        if (status === "error") {
            return Theme.statusLive
        }
        if (status === "pending" || status === "syncing") {
            return Theme.statusStandby
        }
        if (status === "media") {
            return "#945EB5"
        }
        return "transparent"
    }

    function goToDay(dateIso) {
        if (dateIso.length > 0) {
            ApplicationController.openDateWorkspace(dateIso, false)
        }
    }

    // Where added media goes when no section was picked: the public talk on a regular weekend,
    // the only section of a day without a meeting; otherwise the operator picks one.
    function defaultSection() {
        if (root.meetingKind === "weekend" && !ApplicationController.playlist.currentIsCoWeek) {
            return "pt"
        }
        if (!root.isMeetingDay && ApplicationController.meetingDay.sectionCount <= 1) {
            const sections = ApplicationController.playlist.sections
            return sections.length > 0 ? sections[0].id : "imported-media"
        }
        return ""
    }

    function addFrom(sourceKind, sectionId) {
        const target = sectionId.length > 0 ? sectionId : root.defaultSection()
        if (target.length === 0) {
            sectionChooser.openFor(sourceKind)
            return
        }
        if (sourceKind === "song") {
            songPicker.openFor(target)
        } else {
            mediaFileDialog.targetSection = target
            mediaFileDialog.open()
        }
    }

    function sectionChoices() {
        const _watch = ApplicationController.meetingDay.sectionCount
        const choices = []
        const sections = ApplicationController.playlist.sections
        for (let i = 0; i < sections.length; ++i) {
            const row = ApplicationController.meetingDay.headerRowOfSection(sections[i].id)
            if (row >= 0) {
                choices.push({ id: sections[i].id, name: sections[i].name, color: sections[i].color, row: row })
            }
        }
        if (!choices.some(choice => choice.id === "imported-media")) {
            choices.push({
                id: "imported-media",
                name: ApplicationController.playlist.sectionLabel("imported-media"),
                color: ApplicationController.playlist.colorForSection("imported-media"),
                row: 100000
            })
        }
        return choices.sort((a, b) => a.row - b.row)
    }

    function sectionRepeatable(sectionId) {
        const sections = ApplicationController.playlist.sections
        for (let i = 0; i < sections.length; ++i) {
            if (sections[i].id === sectionId) {
                return sections[i].repeatable
            }
        }
        return false
    }

    function noticeText(sectionId, total, hidden) {
        if (hidden > 0 && hidden === total) {
            return qsTr("All media items for this section are hidden.")
        }
        if (hidden > 0) {
            return qsTr("Some media items have been hidden.")
        }
        if (root.meetingKind === "weekend" && root.isMeetingDay) {
            return ApplicationController.meetingDay.mediaCount > 0
                   ? qsTr("Don't forget to add any missing songs or media.")
                   : qsTr("There are no media items for the selected date.")
        }
        if (sectionId === "tgw" || sectionId === "ayfm" || sectionId === "lac" || sectionId === "wt"
                || sectionId === "pt" || sectionId === "circuit-overseer") {
            return qsTr("There are no media items for this meeting section.")
        }
        return qsTr("There are no media items in this section.")
    }

    // One slice of a section card: rows of the same section stack into a single rounded card.
    component CardSlice: Item {
        id: slice

        property bool roundTop: false
        property bool roundBottom: false

        clip: true

        Rectangle {
            x: 0
            y: slice.roundTop ? 0 : -2 * Theme.radiusLarge
            width: slice.width
            height: slice.height + (slice.roundTop ? 0 : 2 * Theme.radiusLarge)
                    + (slice.roundBottom ? 0 : 2 * Theme.radiusLarge)
            radius: Theme.radiusLarge
            color: Theme.panelSurface
            border.width: 1
            border.color: Theme.borderHairline
        }
    }

    Connections {
        target: ApplicationController.sync

        function onSongAdded(dateIso, number, title) {
            ApplicationController.showToast(qsTr("Song %1 added").arg(number), "success")
        }

        function onSongFailed(dateIso, number) {
            ApplicationController.showToast(qsTr("Song %1 could not be found").arg(number), "warning")
        }
    }

    FileDialog {
        id: mediaFileDialog

        property string targetSection: ""

        title: qsTr("Add media")
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            qsTr("Supported Media (*.mp4 *.mkv *.webm *.mov *.mp3 *.aac *.flac *.ogg *.png *.jpg *.jpeg *.webp *.pdf *.jwpub *.jwlplaylist)"),
            qsTr("All Files (*)")
        ]
        onAccepted: {
            const added = ApplicationController.playlist.importUrls(selectedFiles, mediaFileDialog.targetSection)
            if (added > 0) {
                ApplicationController.showToast(qsTr("Added %1 media item(s)").arg(added), "success")
            }
        }
    }

    CustomSectionPopup {
        id: sectionPopup
    }

    BoothChecklistPopup {
        id: checklistPopup
    }

    TagEditPopup {
        id: tagEditPopup
    }

    RenamePopup {
        id: renamePopup
    }

    SongPickerPopup {
        id: songPicker

        onSongChosen: (number, sectionId) => ApplicationController.sync.addSong(root.dateIso, number, sectionId)
    }

    DayDatePicker {
        id: datePicker

        parent: dateButton
        y: dateButton.height + 6
        margins: 8
        onDateChosen: (dateIso) => root.goToDay(dateIso)
    }

    Popup {
        id: sectionChooser

        property string sourceKind: ""

        function openFor(sourceKind) {
            sectionChooser.sourceKind = sourceKind
            sectionChooser.open()
        }

        parent: addButton
        y: addButton.height + 6
        x: addButton.width - sectionChooser.width
        margins: 8
        padding: 6
        focus: true

        background: Rectangle {
            radius: Theme.radiusMedium
            color: Theme.cardElevated
            border.width: 1
            border.color: Theme.borderStrong
        }

        contentItem: ColumnLayout {
            spacing: 0

            Text {
                Layout.leftMargin: 10
                Layout.topMargin: 6
                Layout.bottomMargin: 4
                text: qsTr("Add to section")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
            }

            Repeater {
                model: sectionChooser.visible ? root.sectionChoices() : []

                delegate: Rectangle {
                    id: choiceRow

                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredWidth: 240
                    implicitHeight: 34
                    radius: Theme.radiusSmall
                    color: choiceMouse.containsMouse ? Theme.cardHover : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 10
                            Layout.preferredHeight: 10
                            radius: 5
                            color: choiceRow.modelData.color
                        }

                        Text {
                            Layout.fillWidth: true
                            text: choiceRow.modelData.name
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMd
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: choiceMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            sectionChooser.close()
                            root.addFrom(sectionChooser.sourceKind, choiceRow.modelData.id)
                        }
                    }
                }
            }
        }
    }

    StudioMenu {
        id: addMenu

        property string targetSection: ""

        StudioMenuItem {
            caption: true
            text: qsTr("From JW.org")
        }
        StudioMenuItem {
            iconName: "music"
            text: qsTr("Song")
            onTriggered: root.addFrom("song", addMenu.targetSection)
        }
        StudioMenuSeparator {}
        StudioMenuItem {
            caption: true
            text: qsTr("From this computer")
        }
        StudioMenuItem {
            iconName: "folder-open"
            text: qsTr("Files")
            onTriggered: root.addFrom("files", addMenu.targetSection)
        }
    }

    StudioMenu {
        id: pageMenu

        StudioMenuItem {
            visible: root.isMeetingDay
            iconName: "refresh-cw"
            text: qsTr("Sync this day again")
            onTriggered: ApplicationController.sync.resyncDay(root.dateIso)
        }
        StudioMenuItem {
            visible: ApplicationController.playlist.hiddenCount > 0 || ApplicationController.playlist.showHidden
            iconName: ApplicationController.playlist.showHidden ? "eye-off" : "eye"
            text: ApplicationController.playlist.showHidden ? qsTr("Conceal hidden media")
                                                            : qsTr("Show hidden media (%1)").arg(ApplicationController.playlist.hiddenCount)
            onTriggered: ApplicationController.playlist.showHidden = !ApplicationController.playlist.showHidden
        }
        StudioMenuItem {
            visible: root.isMeetingDay && ApplicationController.meetingDay.mediaCount > 1
            iconName: "rotate-ccw"
            text: qsTr("Reset order")
            onTriggered: ApplicationController.playlist.resetSortOrder()
        }
        StudioMenuItem {
            visible: !root.isMeetingDay
            iconName: "folder-plus"
            text: qsTr("New section")
            onTriggered: sectionPopup.openCreate()
        }
        StudioMenuItem {
            iconName: "minus"
            text: qsTr("Add divider")
            onTriggered: {
                const sections = ApplicationController.playlist.sections
                ApplicationController.playlist.addDivider(qsTr("Part Transition"),
                                                          sections.length > 0 ? sections[0].id : "imported-media",
                                                          "#D47B2A", ApplicationController.playlist.activeIndex)
            }
        }
        StudioMenuSeparator {}
        StudioMenuItem {
            iconName: "clipboard-check"
            text: qsTr("Checklist")
            onTriggered: checklistPopup.open()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingSm

        StudioCard {
            Layout.fillWidth: true
            implicitHeight: 60
            surfaceColor: Theme.panelSurface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 10
                spacing: 8

                Rectangle {
                    visible: !root.compactMode
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: Theme.radiusSmall
                    color: Theme.accentSoft

                    StudioIcon {
                        anchors.centerIn: parent
                        name: "calendar-days"
                        size: 18
                        color: Theme.accentPrimary
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Text {
                        Layout.fillWidth: true
                        text: root.titleText
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
                    variant: "ghost"
                    iconName: "chevron-left"
                    iconSize: 15
                    enabled: root.previousDay.length > 0
                    leftPadding: 0
                    rightPadding: 0
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    onClicked: root.goToDay(root.previousDay)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Previous day with media")
                }

                StudioButton {
                    id: dateButton

                    variant: "secondary"
                    iconName: "calendar"
                    text: root.compactMode ? "" : root.shortDate
                    Layout.preferredHeight: 34
                    onClicked: datePicker.openAt(root.dateIso)
                    ToolTip.visible: hovered && root.compactMode
                    ToolTip.text: root.longDate

                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.rightMargin: 5
                        anchors.topMargin: 5
                        width: 8
                        height: 8
                        radius: 4
                        visible: root.dayStatus.length > 0
                        color: root.statusColor(root.dayStatus)
                        border.width: 1
                        border.color: Theme.panelSurface
                    }
                }

                StudioButton {
                    variant: "ghost"
                    iconName: "chevron-right"
                    iconSize: 15
                    enabled: root.nextDay.length > 0
                    leftPadding: 0
                    rightPadding: 0
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    onClicked: root.goToDay(root.nextDay)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Next day with media")
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
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Preview")
                }

                StudioButton {
                    id: addButton

                    variant: "primary"
                    iconName: "plus"
                    text: root.compactMode ? "" : qsTr("Add media")
                    Layout.preferredHeight: 34
                    onClicked: {
                        addMenu.targetSection = ""
                        addMenu.popup(addButton, 0, addButton.height + 6)
                    }
                }

                StudioButton {
                    id: moreButton

                    variant: "ghost"
                    iconName: "ellipsis-vertical"
                    leftPadding: 0
                    rightPadding: 0
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 34
                    onClicked: pageMenu.popup(moreButton, moreButton.width - pageMenu.width, moreButton.height + 6)
                }
            }
        }

        Rectangle {
            visible: root.dayStatus === "error"
            Layout.fillWidth: true
            implicitHeight: 40
            radius: Theme.radiusMedium
            color: Theme.isDark ? "#2A1414" : "#FDECEC"
            border.width: 1
            border.color: Theme.statusLive

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 6
                spacing: 8

                StudioIcon {
                    name: "circle-exclamation-point"
                    size: 15
                    color: Theme.statusLive
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Some media for this day could not be downloaded.")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    elide: Text.ElideRight
                }

                StudioButton {
                    variant: "secondary"
                    text: qsTr("Try again")
                    Layout.preferredHeight: 28
                    onClicked: ApplicationController.sync.resyncDay(root.dateIso)
                }
            }
        }

        ListView {
            id: dayList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 0
            boundsBehavior: Flickable.StopAtBounds
            model: ApplicationController.meetingDay
            reuseItems: true
            bottomMargin: Theme.spacingSm

            ScrollBar.vertical: StudioScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: DelegateChooser {
                role: "rowType"

                DelegateChoice {
                    roleValue: MeetingDayModel.SectionHeaderRow

                    delegate: Item {
                        id: header

                        required property string rowSectionId
                        required property string rowSectionLabel
                        required property string rowSectionColor
                        required property string rowSectionSubtitle
                        required property bool rowSectionCollapsed
                        required property bool rowSectionIsCustom
                        required property bool rowSectionHasSong
                        required property int rowSectionIndex
                        required property int rowSectionTotalCount
                        required property bool isLastInSection

                        readonly property int gap: header.rowSectionIndex > 0 ? Theme.spacingMd : 0
                        readonly property string glyph: root.sectionGlyphs[header.rowSectionId] ?? ""
                        readonly property bool songButton: root.isMeetingDay && !header.rowSectionHasSong
                                                           && (header.rowSectionId === "pt"
                                                               || header.rowSectionId === "circuit-overseer")
                        readonly property bool editable: !root.isMeetingDay || header.rowSectionIsCustom

                        width: dayList.width - 12
                        height: header.gap + (header.rowSectionSubtitle.length > 0 ? 64 : 54)

                        CardSlice {
                            anchors.fill: parent
                            anchors.topMargin: header.gap
                            roundTop: true
                            roundBottom: header.isLastInSection

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 8
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    radius: 16
                                    color: header.rowSectionColor.length > 0 ? header.rowSectionColor : "#945EB5"

                                    StudioIcon {
                                        anchors.centerIn: parent
                                        name: header.glyph.length > 0 ? header.glyph : "list-video"
                                        category: header.glyph.length > 0 ? "custom" : "lucide"
                                        size: 16
                                        color: "#FFFFFF"
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Text {
                                        Layout.fillWidth: true
                                        text: header.rowSectionLabel
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeLg
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        visible: header.rowSectionSubtitle.length > 0
                                        text: header.rowSectionSubtitle
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                    }
                                }

                                StudioButton {
                                    visible: header.songButton
                                    variant: "secondary"
                                    iconName: "music"
                                    text: root.compactMode ? "" : qsTr("Song")
                                    Layout.preferredHeight: 30
                                    onClicked: songPicker.openFor(header.rowSectionId)
                                }

                                StudioButton {
                                    id: sectionAddButton

                                    variant: "ghost"
                                    iconName: "plus"
                                    leftPadding: 0
                                    rightPadding: 0
                                    Layout.preferredWidth: 30
                                    Layout.preferredHeight: 30
                                    onClicked: {
                                        addMenu.targetSection = header.rowSectionId
                                        addMenu.popup(sectionAddButton, 0, sectionAddButton.height + 4)
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Add media to this section")
                                }

                                StudioButton {
                                    id: sectionMoreButton

                                    variant: "ghost"
                                    iconName: "ellipsis-vertical"
                                    leftPadding: 0
                                    rightPadding: 0
                                    Layout.preferredWidth: 30
                                    Layout.preferredHeight: 30
                                    onClicked: sectionMenu.popup(sectionMoreButton,
                                                                 sectionMoreButton.width - sectionMenu.width,
                                                                 sectionMoreButton.height + 4)
                                }

                                StudioButton {
                                    variant: "ghost"
                                    iconName: header.rowSectionCollapsed ? "chevron-down" : "chevron-up"
                                    leftPadding: 0
                                    rightPadding: 0
                                    Layout.preferredWidth: 30
                                    Layout.preferredHeight: 30
                                    onClicked: ApplicationController.playlist.toggleSectionCollapsed(header.rowSectionId)
                                    ToolTip.visible: hovered
                                    ToolTip.text: header.rowSectionCollapsed ? qsTr("Expand") : qsTr("Collapse")
                                }
                            }
                        }

                        StudioMenu {
                            id: sectionMenu

                            StudioMenuItem {
                                iconName: "repeat"
                                checkable: true
                                checked: root.sectionRepeatable(header.rowSectionId)
                                text: qsTr("Repeat section")
                                onTriggered: ApplicationController.playlist.setSectionRepeatable(
                                    header.rowSectionId, !root.sectionRepeatable(header.rowSectionId))
                            }
                            StudioMenuItem {
                                visible: header.editable
                                iconName: "sliders-horizontal"
                                text: qsTr("Edit section")
                                onTriggered: sectionPopup.openEdit(header.rowSectionId, header.rowSectionColor)
                            }
                            StudioMenuItem {
                                visible: header.editable && header.rowSectionIndex > 0
                                iconName: "chevron-up"
                                text: qsTr("Move up")
                                onTriggered: ApplicationController.playlist.moveSection(header.rowSectionId, -1)
                            }
                            StudioMenuItem {
                                visible: header.editable
                                iconName: "chevron-down"
                                text: qsTr("Move down")
                                onTriggered: ApplicationController.playlist.moveSection(header.rowSectionId, 1)
                            }
                            StudioMenuSeparator {
                                visible: header.rowSectionId.startsWith("custom-")
                            }
                            StudioMenuItem {
                                visible: header.rowSectionId.startsWith("custom-")
                                iconName: "trash"
                                destructive: true
                                text: qsTr("Delete section")
                                onTriggered: ApplicationController.playlist.removeSection(header.rowSectionId)
                            }
                        }
                    }
                }

                DelegateChoice {
                    roleValue: MeetingDayModel.SectionNoticeRow

                    delegate: Item {
                        id: notice

                        required property string rowSectionId
                        required property int rowSectionHiddenCount
                        required property int rowSectionTotalCount
                        required property bool isLastInSection

                        readonly property bool skeleton: root.daySyncing && notice.rowSectionTotalCount === 0

                        width: dayList.width - 12
                        height: notice.skeleton ? 2 * (root.thumbHeight + 12) + 8 : 44

                        CardSlice {
                            anchors.fill: parent
                            roundBottom: notice.isLastInSection

                            Column {
                                visible: notice.skeleton
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 12

                                Repeater {
                                    model: notice.skeleton ? 2 : 0

                                    delegate: Row {
                                        spacing: 14

                                        Rectangle {
                                            width: root.thumbWidth
                                            height: root.thumbHeight
                                            radius: Theme.radiusSmall
                                            color: Theme.cardHover

                                            SequentialAnimation on opacity {
                                                loops: Animation.Infinite
                                                running: notice.skeleton
                                                NumberAnimation { from: 1.0; to: 0.45; duration: 700 }
                                                NumberAnimation { from: 0.45; to: 1.0; duration: 700 }
                                            }
                                        }

                                        Column {
                                            anchors.verticalCenter: parent.verticalCenter
                                            spacing: 8

                                            Rectangle {
                                                width: 220
                                                height: 12
                                                radius: 6
                                                color: Theme.cardHover
                                            }

                                            Rectangle {
                                                width: 140
                                                height: 10
                                                radius: 5
                                                color: Theme.cardHover
                                            }
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                visible: !notice.skeleton
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 10
                                spacing: 8

                                StudioIcon {
                                    name: notice.rowSectionHiddenCount > 0 ? "eye-off" : "info"
                                    size: 14
                                    color: Theme.textMuted
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.noticeText(notice.rowSectionId, notice.rowSectionTotalCount,
                                                          notice.rowSectionHiddenCount)
                                    color: Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    elide: Text.ElideRight
                                }

                                StudioButton {
                                    visible: notice.rowSectionHiddenCount > 0
                                    variant: "ghost"
                                    text: qsTr("Show")
                                    Layout.preferredHeight: 28
                                    onClicked: ApplicationController.playlist.showHidden = true
                                }
                            }
                        }
                    }
                }

                DelegateChoice {
                    roleValue: MeetingDayModel.MediaRow

                    delegate: Item {
                        id: mediaRow

                        required property int sourceRow
                        required property string rowSectionId
                        required property bool isLastInSection
                        required property string title
                        required property string thumbUrl
                        required property string kindLabel
                        required property int mediaKind
                        required property real durationMs
                        required property string formattedDuration
                        required property real trimStart
                        required property real trimEnd
                        required property bool fileExists
                        required property bool isActive
                        required property string itemKind
                        required property bool hidden
                        required property string tagType
                        required property string tagValue
                        required property string streamUrl
                        required property string dividerColor
                        required property int slideIntervalMs
                        required property string source
                        required property string groupKey
                        required property bool isGroupStart
                        required property int groupItemCount
                        required property bool groupExpanded

                        readonly property bool isDivider: mediaRow.itemKind === "divider"
                        readonly property bool isPicture: mediaRow.mediaKind === PlaybackController.Image
                        readonly property bool missingFile: !mediaRow.fileExists && mediaRow.streamUrl.length === 0
                        readonly property bool groupHeader: mediaRow.isGroupStart && mediaRow.groupItemCount > 1
                        readonly property string typeIcon: {
                            if (mediaRow.kindLabel === "AUD") {
                                return "audio-lines"
                            }
                            if (mediaRow.kindLabel === "PDF") {
                                return "file-text"
                            }
                            if (mediaRow.kindLabel === "STRM") {
                                return "globe"
                            }
                            return mediaRow.isPicture ? "image" : "film"
                        }

                        width: dayList.width - 12
                        height: mediaRow.isDivider ? 40
                              : root.thumbHeight + 14 + (mediaRow.groupHeader ? 22 : 0)

                        CardSlice {
                            anchors.fill: parent
                            roundBottom: mediaRow.isLastInSection
                        }

                        // Active and hovered rows get a soft inner highlight.
                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            anchors.topMargin: 2
                            anchors.bottomMargin: mediaRow.isLastInSection ? 6 : 2
                            radius: Theme.radiusMedium
                            visible: !mediaRow.isDivider
                            color: mediaRow.isActive ? Theme.rowSelected
                                 : (rowMouse.containsMouse ? Theme.cardHover : "transparent")
                            border.width: mediaRow.isActive ? 1 : 0
                            border.color: Theme.accentBorder
                        }

                        RowLayout {
                            visible: mediaRow.isDivider
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 10
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 3
                                radius: 2
                                color: mediaRow.dividerColor.length > 0 ? mediaRow.dividerColor : Theme.accentPrimary
                            }

                            Text {
                                text: mediaRow.title
                                color: mediaRow.dividerColor.length > 0 ? mediaRow.dividerColor : Theme.accentPrimary
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
                                opacity: 0.4
                                color: mediaRow.dividerColor.length > 0 ? mediaRow.dividerColor : Theme.accentPrimary
                            }

                            StudioButton {
                                variant: "ghost"
                                iconName: "trash"
                                iconSize: 13
                                leftPadding: 0
                                rightPadding: 0
                                Layout.preferredWidth: 26
                                Layout.preferredHeight: 26
                                onClicked: ApplicationController.playlist.removeAt(mediaRow.sourceRow)
                            }
                        }

                        MouseArea {
                            id: rowMouse

                            anchors.fill: parent
                            visible: !mediaRow.isDivider
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            cursorShape: Qt.PointingHandCursor
                            onClicked: (mouse) => {
                                ApplicationController.playlist.activateIndex(mediaRow.sourceRow)
                                if (mouse.button === Qt.RightButton) {
                                    rowMenu.popup()
                                }
                            }
                            onDoubleClicked: {
                                ApplicationController.playlist.activateIndex(mediaRow.sourceRow)
                                ApplicationController.playback.play()
                            }
                        }

                        ColumnLayout {
                            visible: !mediaRow.isDivider
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 10
                            anchors.topMargin: 7
                            anchors.bottomMargin: 7
                            spacing: 2

                            Item {
                                visible: mediaRow.groupHeader
                                Layout.fillWidth: true
                                Layout.preferredHeight: 20

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 26
                                    spacing: 6

                                    StudioIcon {
                                        name: mediaRow.groupExpanded ? "chevron-down" : "chevron-right"
                                        size: 12
                                        color: Theme.textSecondary
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: mediaRow.groupKey + "  ·  " + mediaRow.groupItemCount
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeXs
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: ApplicationController.playlist.toggleGroupExpanded(mediaRow.groupKey)
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 10

                                // Drag handle: drop on another row to move there, across sections too.
                                Item {
                                    Layout.preferredWidth: 16
                                    Layout.fillHeight: true

                                    StudioIcon {
                                        anchors.centerIn: parent
                                        visible: rowMouse.containsMouse || dragMouse.pressed
                                        name: "grip-vertical"
                                        size: 14
                                        color: dragMouse.pressed ? Theme.accentPrimary : Theme.textMuted
                                    }

                                    MouseArea {
                                        id: dragMouse

                                        anchors.fill: parent
                                        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                        onPositionChanged: (mouse) => {
                                            if (!pressed) {
                                                return
                                            }
                                            const point = dragMouse.mapToItem(dayList.contentItem, mouse.x, mouse.y)
                                            const targetRow = dayList.indexAt(point.x, point.y)
                                            const targetSource = ApplicationController.meetingDay.sourceRowAt(targetRow)
                                            if (targetSource >= 0 && targetSource !== mediaRow.sourceRow) {
                                                ApplicationController.playlist.moveItem(mediaRow.sourceRow, targetSource)
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.preferredWidth: root.thumbWidth
                                    Layout.preferredHeight: root.thumbHeight
                                    radius: Theme.radiusSmall
                                    color: Theme.cardSurface
                                    border.width: 1
                                    border.color: mediaRow.isActive ? Theme.accentBorder : Theme.borderHairline
                                    clip: true
                                    opacity: mediaRow.hidden ? 0.45 : 1.0

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 1
                                        visible: mediaRow.thumbUrl.length > 0
                                        source: mediaRow.thumbUrl
                                        sourceSize.width: 320
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        smooth: true
                                    }

                                    StudioIcon {
                                        anchors.centerIn: parent
                                        visible: mediaRow.thumbUrl.length === 0
                                        name: mediaRow.missingFile ? "download" : mediaRow.typeIcon
                                        size: 20
                                        color: Theme.textMuted
                                    }

                                    Rectangle {
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        anchors.margins: 4
                                        visible: mediaRow.durationMs > 0
                                        width: durationText.implicitWidth + 10
                                        height: 18
                                        radius: 9
                                        color: "#B3000000"

                                        Text {
                                            id: durationText

                                            anchors.centerIn: parent
                                            text: mediaRow.formattedDuration
                                            color: "#FFFFFF"
                                            font.family: Theme.fontFamily
                                            font.pixelSize: 10
                                            font.weight: Font.DemiBold
                                        }
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.bottom: parent.bottom
                                        anchors.margins: 4
                                        visible: mediaRow.missingFile
                                        width: 18
                                        height: 18
                                        radius: 9
                                        color: "#B3000000"

                                        StudioIcon {
                                            anchors.centerIn: parent
                                            name: "download"
                                            size: 11
                                            color: "#FFFFFF"
                                        }
                                    }
                                }

                                // Song or paragraph tag, as in the publication.
                                Item {
                                    visible: mediaRow.tagValue.length > 0
                                    Layout.alignment: Qt.AlignVCenter
                                    implicitWidth: tagChip.implicitWidth
                                    implicitHeight: 24

                                    Row {
                                        id: tagChip

                                        spacing: 0

                                        Rectangle {
                                            width: 24
                                            height: 24
                                            radius: Theme.radiusSmall
                                            color: mediaRow.tagType === "song" ? "#2E6F5E" : Theme.accentPrimary

                                            StudioIcon {
                                                anchors.centerIn: parent
                                                name: mediaRow.tagType === "song" ? "music" : "pilcrow"
                                                size: 13
                                                color: "#FFFFFF"
                                            }
                                        }

                                        Rectangle {
                                            width: tagValueText.implicitWidth + 12
                                            height: 24
                                            color: Theme.cardSurface
                                            border.width: 1
                                            border.color: Theme.borderHairline

                                            Text {
                                                id: tagValueText

                                                anchors.centerIn: parent
                                                text: mediaRow.tagValue
                                                color: Theme.textPrimary
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeSm
                                                font.weight: Font.DemiBold
                                            }
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: tagEditPopup.openFor(mediaRow.sourceRow, mediaRow.tagType, mediaRow.tagValue)
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 4

                                    Text {
                                        Layout.fillWidth: true
                                        text: mediaRow.title
                                        color: mediaRow.missingFile ? Theme.textMuted : Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeMd
                                        font.weight: mediaRow.isActive ? Font.DemiBold : Font.Medium
                                        font.strikeout: mediaRow.hidden
                                        wrapMode: Text.Wrap
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                    }

                                    Row {
                                        spacing: 6
                                        visible: mediaRow.trimEnd > mediaRow.trimStart || mediaRow.slideIntervalMs > 0
                                                 || mediaRow.hidden

                                        StudioIcon {
                                            visible: mediaRow.hidden
                                            name: "eye-off"
                                            size: 13
                                            color: Theme.textMuted
                                        }

                                        StudioIcon {
                                            visible: mediaRow.trimEnd > mediaRow.trimStart
                                            name: "scissors"
                                            size: 13
                                            color: Theme.accentPrimary
                                        }

                                        Text {
                                            visible: mediaRow.slideIntervalMs > 0
                                            text: Math.round(mediaRow.slideIntervalMs / 1000) + "s"
                                            color: Theme.textSecondary
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeXs
                                        }
                                    }
                                }

                                StudioButton {
                                    variant: mediaRow.isActive ? "primary" : "secondary"
                                    iconName: "play"
                                    iconSize: 14
                                    enabled: !mediaRow.missingFile
                                    cornerRadius: 17
                                    leftPadding: 0
                                    rightPadding: 0
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 34
                                    onClicked: {
                                        ApplicationController.playlist.activateIndex(mediaRow.sourceRow)
                                        ApplicationController.playback.play()
                                    }
                                }

                                StudioButton {
                                    id: rowMoreButton

                                    variant: "ghost"
                                    iconName: "ellipsis-vertical"
                                    leftPadding: 0
                                    rightPadding: 0
                                    Layout.preferredWidth: 28
                                    Layout.preferredHeight: 34
                                    onClicked: rowMenu.popup(rowMoreButton, rowMoreButton.width - rowMenu.width,
                                                             rowMoreButton.height + 4)
                                }
                            }
                        }

                        StudioMenu {
                            id: rowMenu

                            StudioMenuItem {
                                iconName: mediaRow.hidden ? "eye" : "eye-off"
                                text: mediaRow.hidden ? qsTr("Show in media list") : qsTr("Hide from media list")
                                onTriggered: ApplicationController.playlist.toggleItemHidden(mediaRow.sourceRow)
                            }
                            StudioMenuItem {
                                iconName: "pilcrow"
                                text: qsTr("Edit tag")
                                onTriggered: tagEditPopup.openFor(mediaRow.sourceRow, mediaRow.tagType, mediaRow.tagValue)
                            }
                            StudioMenuItem {
                                iconName: "sliders-horizontal"
                                text: qsTr("Rename")
                                onTriggered: renamePopup.openFor(mediaRow.sourceRow, mediaRow.title)
                            }
                            StudioMenu {
                                title: qsTr("Slide interval")
                                enabled: mediaRow.isPicture

                                StudioMenuItem {
                                    text: qsTr("Manual")
                                    checkable: true
                                    checked: mediaRow.slideIntervalMs === 0
                                    onTriggered: ApplicationController.playlist.setItemSlideInterval(mediaRow.sourceRow, 0)
                                }
                                StudioMenuItem {
                                    text: qsTr("10 seconds")
                                    checkable: true
                                    checked: mediaRow.slideIntervalMs === 10000
                                    onTriggered: ApplicationController.playlist.setItemSlideInterval(mediaRow.sourceRow, 10000)
                                }
                                StudioMenuItem {
                                    text: qsTr("30 seconds")
                                    checkable: true
                                    checked: mediaRow.slideIntervalMs === 30000
                                    onTriggered: ApplicationController.playlist.setItemSlideInterval(mediaRow.sourceRow, 30000)
                                }
                                StudioMenuItem {
                                    text: qsTr("1 minute")
                                    checkable: true
                                    checked: mediaRow.slideIntervalMs === 60000
                                    onTriggered: ApplicationController.playlist.setItemSlideInterval(mediaRow.sourceRow, 60000)
                                }
                            }
                            StudioMenuSeparator {}
                            StudioMenuItem {
                                iconName: "layers"
                                text: qsTr("Move to section")
                                onTriggered: sectionPopup.openAssign(mediaRow.sourceRow, mediaRow.rowSectionId)
                            }
                            StudioMenuItem {
                                iconName: "minus"
                                text: qsTr("Insert divider below")
                                onTriggered: ApplicationController.playlist.addDivider(
                                    qsTr("Section Break"), mediaRow.rowSectionId, "#D47B2A", mediaRow.sourceRow)
                            }
                            StudioMenuSeparator {
                                visible: mediaRow.source !== "dynamic"
                            }
                            StudioMenuItem {
                                // Synced media can only be hidden; the next sync would bring it back.
                                visible: mediaRow.source !== "dynamic"
                                iconName: "trash"
                                destructive: true
                                text: qsTr("Delete")
                                onTriggered: ApplicationController.playlist.removeAt(mediaRow.sourceRow)
                            }
                        }
                    }
                }
            }

            // A day without a meeting and without media.
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 32, 380)
                visible: dayList.count === 0
                spacing: 12

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                    radius: 26
                    color: Theme.cardSurface
                    border.width: 1
                    border.color: Theme.borderHairline

                    StudioIcon {
                        anchors.centerIn: parent
                        name: "calendar-days"
                        size: 22
                        color: Theme.accentPrimary
                    }
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("There are no media items for the selected date.")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLg
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8

                    StudioButton {
                        variant: "secondary"
                        iconName: "plus"
                        text: qsTr("Add media")
                        onClicked: addMenu.popup()
                    }

                    StudioButton {
                        visible: root.nextDay.length > 0
                        variant: "primary"
                        trailingIconName: "chevron-right"
                        text: qsTr("Next day with media")
                        onClicked: root.goToDay(root.nextDay)
                    }
                }
            }
        }
    }
}
