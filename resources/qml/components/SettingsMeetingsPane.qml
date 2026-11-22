pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// Settings > Meetings: the congregation, its meeting schedule, and how meeting media is fetched.
ColumnLayout {
    id: root

    property int selectedMidweekDay: ApplicationController.calendar.midweekDay
    property int selectedWeekendDay: ApplicationController.calendar.weekendDay
    readonly property var dayOptions: [
        { day: 1, label: qsTr("Mon") },
        { day: 2, label: qsTr("Tue") },
        { day: 3, label: qsTr("Wed") },
        { day: 4, label: qsTr("Thu") },
        { day: 5, label: qsTr("Fri") },
        { day: 6, label: qsTr("Sat") },
        { day: 7, label: qsTr("Sun") }
    ]

    function languageLabel(code) {
        const languages = ApplicationController.catalog.languages
        for (let i = 0; i < languages.length; ++i) {
            if (languages[i].code === code) {
                return languages[i].label
            }
        }
        return code
    }

    spacing: Theme.spacingMd

    Connections {
        target: ApplicationController.calendar

        // A congregation picked from the lookup brings its own schedule.
        function onScheduleChanged() {
            root.selectedMidweekDay = ApplicationController.calendar.midweekDay
            root.selectedWeekendDay = ApplicationController.calendar.weekendDay
            saveButton.midweekTime = ApplicationController.calendar.midweekTime
            saveButton.weekendTime = ApplicationController.calendar.weekendTime
        }
    }

    component SectionTitle: RowLayout {
        id: sectionTitle

        property string iconName: ""
        property string text: ""

        Layout.fillWidth: true
        spacing: 8

        StudioIcon {
            name: sectionTitle.iconName
            size: 17
            color: Theme.accentPrimary
        }

        Text {
            Layout.fillWidth: true
            text: sectionTitle.text
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLg
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }

    component FilterRow: RowLayout {
        id: filterRow

        property string text: ""
        property bool checked: false
        signal toggled(bool checked)

        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: filterRow.text
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            wrapMode: Text.WordWrap
        }

        StudioSwitch {
            checked: filterRow.checked
            onToggled: filterRow.toggled(checked)
        }
    }

    component LanguagePopup: Popup {
        id: languagePopup

        property string currentCode: ""
        signal languageChosen(string code)

        implicitWidth: 300
        padding: 8
        focus: true
        margins: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onAboutToShow: languageFilter.text = ""

        background: StudioCard {
            surfaceColor: Theme.cardElevated
            strokeColor: Theme.borderStrong
        }

        contentItem: ColumnLayout {
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                TextField {
                    id: languageFilter

                    Layout.fillWidth: true
                    placeholderText: qsTr("Filter languages...")
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
                    iconName: "refresh-cw"
                    leftPadding: 0
                    rightPadding: 0
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    onClicked: ApplicationController.catalog.refreshLanguages()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Fetch full JW language list")
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 240
                clip: true
                spacing: 2
                model: {
                    const query = languageFilter.text.trim().toLowerCase()
                    const languages = ApplicationController.catalog.languages
                    if (query.length === 0) {
                        return languages
                    }
                    return languages.filter(language => language.label.toLowerCase().indexOf(query) >= 0
                                                        || language.code.toLowerCase() === query)
                }

                ScrollBar.vertical: StudioScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Rectangle {
                    id: languageRow

                    required property var modelData
                    readonly property bool selected: languageRow.modelData.code === languagePopup.currentCode

                    width: ListView.view ? ListView.view.width : 280
                    height: 32
                    radius: Theme.radiusSmall
                    color: languageMouse.containsMouse ? Theme.cardHover
                                                       : (languageRow.selected ? Theme.accentSoft : "transparent")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: languageRow.modelData.label
                            color: languageRow.selected ? Theme.accentPrimary : Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: languageRow.selected ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }

                        StudioIcon {
                            visible: languageRow.selected
                            name: "check"
                            size: 14
                            color: Theme.accentPrimary
                        }
                    }

                    MouseArea {
                        id: languageMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            languagePopup.languageChosen(languageRow.modelData.code)
                            languagePopup.close()
                        }
                    }
                }
            }
        }
    }

    StudioCard {
        Layout.fillWidth: true
        implicitHeight: congregationColumn.implicitHeight + 28
        surfaceColor: Theme.panelSurface

        ColumnLayout {
            id: congregationColumn

            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            SectionTitle {
                iconName: "users"
                text: qsTr("Congregation Lookup (hub.jw.org)")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                TextField {
                    id: congregationQuery

                    Layout.fillWidth: true
                    placeholderText: qsTr("City, congregation name...")
                    color: Theme.textPrimary
                    placeholderTextColor: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    onAccepted: ApplicationController.catalog.searchCongregations(congregationQuery.text)
                    background: StudioCard {
                        surfaceColor: Theme.windowBase
                        cornerRadius: Theme.radiusSmall
                    }
                }

                StudioButton {
                    variant: "primary"
                    iconName: "search"
                    leftPadding: 0
                    rightPadding: 0
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    enabled: congregationQuery.text.trim().length > 0 && !ApplicationController.catalog.busy
                    onClicked: ApplicationController.catalog.searchCongregations(congregationQuery.text)
                }
            }

            Repeater {
                model: ApplicationController.catalog.congregationResults

                delegate: Rectangle {
                    id: congregationRow

                    required property var modelData

                    Layout.fillWidth: true
                    implicitHeight: 48
                    radius: Theme.radiusSmall
                    color: congregationMouse.containsMouse ? Theme.cardHover : Theme.cardSurface
                    border.width: 1
                    border.color: Theme.borderHairline

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 1

                        Text {
                            Layout.fillWidth: true
                            text: congregationRow.modelData.orgName + " (" + congregationRow.modelData.languageCode + ")"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: congregationRow.modelData.address.length > 0
                                  ? congregationRow.modelData.address
                                  : (congregationRow.modelData.midweekTime + " / " + congregationRow.modelData.weekendTime)
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: congregationMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: ApplicationController.catalog.applyCongregationResult(congregationRow.modelData.index)
                    }
                }
            }
        }
    }

    StudioCard {
        Layout.fillWidth: true
        implicitHeight: scheduleColumn.implicitHeight + 28
        surfaceColor: Theme.panelSurface

        ColumnLayout {
            id: scheduleColumn

            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            SectionTitle {
                iconName: "calendar-days"
                text: qsTr("Congregation Meeting Schedule")
            }

            Repeater {
                model: [
                    { kind: "midweek", label: qsTr("Midweek Meeting"), first: 0, count: 5 },
                    { kind: "weekend", label: qsTr("Weekend Meeting"), first: 5, count: 2 }
                ]

                delegate: ColumnLayout {
                    id: meetingSchedule

                    required property var modelData
                    readonly property bool midweek: meetingSchedule.modelData.kind === "midweek"

                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: meetingSchedule.modelData.label
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.DemiBold
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Repeater {
                            model: root.dayOptions.slice(meetingSchedule.modelData.first,
                                                         meetingSchedule.modelData.first + meetingSchedule.modelData.count)

                            delegate: StudioButton {
                                id: dayButton

                                required property var modelData

                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                variant: (meetingSchedule.midweek ? root.selectedMidweekDay : root.selectedWeekendDay)
                                         === dayButton.modelData.day ? "primary" : "secondary"
                                text: dayButton.modelData.label
                                onClicked: {
                                    if (meetingSchedule.midweek) {
                                        root.selectedMidweekDay = dayButton.modelData.day
                                    } else {
                                        root.selectedWeekendDay = dayButton.modelData.day
                                    }
                                }
                            }
                        }

                        TextField {
                            id: timeField

                            Layout.preferredWidth: 84
                            horizontalAlignment: Text.AlignHCenter
                            text: meetingSchedule.midweek ? ApplicationController.calendar.midweekTime
                                                          : ApplicationController.calendar.weekendTime
                            placeholderText: meetingSchedule.midweek ? "19:30" : "09:30"
                            color: Theme.textPrimary
                            placeholderTextColor: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            onTextEdited: {
                                if (meetingSchedule.midweek) {
                                    saveButton.midweekTime = timeField.text
                                } else {
                                    saveButton.weekendTime = timeField.text
                                }
                            }
                            background: StudioCard {
                                surfaceColor: Theme.windowBase
                                cornerRadius: Theme.radiusSmall
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                }

                StudioButton {
                    id: saveButton

                    property string midweekTime: ApplicationController.calendar.midweekTime
                    property string weekendTime: ApplicationController.calendar.weekendTime

                    variant: "primary"
                    text: qsTr("Save Times")
                    onClicked: {
                        ApplicationController.calendar.setScheduleDays(root.selectedMidweekDay, saveButton.midweekTime,
                                                                       root.selectedWeekendDay, saveButton.weekendTime)
                        ApplicationController.showToast(qsTr("Meeting schedule updated"), "success")
                    }
                }
            }
        }
    }

    StudioCard {
        Layout.fillWidth: true
        implicitHeight: mediaColumn.implicitHeight + 28
        surfaceColor: Theme.panelSurface

        ColumnLayout {
            id: mediaColumn

            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            SectionTitle {
                iconName: "globe"
                text: qsTr("Meeting Media")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Media language")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                }

                StudioButton {
                    id: mediaLanguageButton

                    variant: "secondary"
                    trailingIconName: "chevron-down"
                    text: ApplicationController.catalog.selectedLanguageLabel
                    Layout.preferredHeight: 32
                    onClicked: mediaLanguagePopup.open()

                    LanguagePopup {
                        id: mediaLanguagePopup

                        y: mediaLanguageButton.height + 4
                        currentCode: ApplicationController.catalog.selectedLanguageCode
                        onLanguageChosen: (code) => ApplicationController.catalog.selectedLanguageCode = code
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Fallback language")
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                }

                StudioButton {
                    id: fallbackLanguageButton

                    variant: "secondary"
                    trailingIconName: "chevron-down"
                    text: root.languageLabel(ApplicationController.catalog.fallbackLanguageCode)
                    Layout.preferredHeight: 32
                    onClicked: fallbackLanguagePopup.open()

                    LanguagePopup {
                        id: fallbackLanguagePopup

                        y: fallbackLanguageButton.height + 4
                        currentCode: ApplicationController.catalog.fallbackLanguageCode
                        onLanguageChosen: (code) => ApplicationController.catalog.fallbackLanguageCode = code
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.borderHairline
            }

            FilterRow {
                text: qsTr("Hide media from footnotes")
                checked: ApplicationController.catalog.excludeFootnotes
                onToggled: (checked) => ApplicationController.catalog.excludeFootnotes = checked
            }

            FilterRow {
                text: qsTr("Include pictures meant only for the printed edition")
                checked: ApplicationController.catalog.includePrinted
                onToggled: (checked) => ApplicationController.catalog.includePrinted = checked
            }

            FilterRow {
                text: qsTr("Skip videos inside Watchtower paragraphs")
                checked: ApplicationController.catalog.excludeWtParagraphVideos
                onToggled: (checked) => ApplicationController.catalog.excludeWtParagraphVideos = checked
            }

            FilterRow {
                text: qsTr("Skip videos from the Teaching brochure")
                checked: ApplicationController.catalog.excludeTh
                onToggled: (checked) => ApplicationController.catalog.excludeTh = checked
            }

            FilterRow {
                text: qsTr("Skip Congregation Bible Study videos")
                checked: ApplicationController.catalog.excludeCbsPubs
                onToggled: (checked) => ApplicationController.catalog.excludeCbsPubs = checked
            }
        }
    }

    MediaExportCard {
        Layout.fillWidth: true
    }
}
