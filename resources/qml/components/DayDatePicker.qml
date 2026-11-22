pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// Month calendar for the meeting media page: a colored dot marks each day's sync status, and only
// the days of the sync window can be picked.
Popup {
    id: root

    property string selectedIso: ""
    property string windowStart: ApplicationController.sync.windowStart
    property string windowEnd: ApplicationController.sync.windowEnd
    property var statuses: ApplicationController.sync.dayStatuses
    property int shownYear: 2026
    property int shownMonth: 1
    readonly property var uiLocale: Qt.locale(ApplicationController.uiLanguage)
    readonly property string todayIso: root.isoOf(new Date())
    readonly property string shownMonthKey: root.shownYear + "-" + root.pad(root.shownMonth)
    readonly property bool canGoBack: root.windowStart.length === 0
                                      || root.shownMonthKey > root.windowStart.substring(0, 7)
    readonly property bool canGoForward: root.windowEnd.length === 0
                                         || root.shownMonthKey < root.windowEnd.substring(0, 7)
    readonly property var cells: {
        const first = new Date(root.shownYear, root.shownMonth - 1, 1)
        const offset = (first.getDay() - root.uiLocale.firstDayOfWeek + 7) % 7
        const daysInMonth = new Date(root.shownYear, root.shownMonth, 0).getDate()
        const list = []
        for (let i = 0; i < 42; ++i) {
            const day = i - offset + 1
            if (day < 1 || day > daysInMonth) {
                list.push({ iso: "", day: 0 })
                continue
            }
            list.push({ iso: root.shownMonthKey + "-" + root.pad(day), day: day })
        }
        return list
    }

    signal dateChosen(string dateIso)

    function pad(value) {
        return value < 10 ? "0" + value : "" + value
    }

    function isoOf(date) {
        return date.getFullYear() + "-" + root.pad(date.getMonth() + 1) + "-" + root.pad(date.getDate())
    }

    function openAt(dateIso) {
        const parts = dateIso.split("-")
        const shown = parts.length === 3 ? new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]))
                                         : new Date()
        root.selectedIso = dateIso
        root.shownYear = shown.getFullYear()
        root.shownMonth = shown.getMonth() + 1
        root.open()
    }

    function shiftMonth(delta) {
        const shown = new Date(root.shownYear, root.shownMonth - 1 + delta, 1)
        root.shownYear = shown.getFullYear()
        root.shownMonth = shown.getMonth() + 1
    }

    function inWindow(dateIso) {
        return dateIso.length > 0
               && (root.windowStart.length === 0 || dateIso >= root.windowStart)
               && (root.windowEnd.length === 0 || dateIso <= root.windowEnd)
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

    padding: 12
    focus: true

    background: StudioCard {
        surfaceColor: Theme.cardElevated
        strokeColor: Theme.borderStrong
        cornerRadius: Theme.radiusLarge
    }

    contentItem: ColumnLayout {
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            StudioButton {
                variant: "ghost"
                iconName: "chevron-left"
                iconSize: 14
                enabled: root.canGoBack
                leftPadding: 0
                rightPadding: 0
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                onClicked: root.shiftMonth(-1)
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: {
                    const name = root.uiLocale.standaloneMonthName(root.shownMonth - 1, Locale.LongFormat)
                    return name.charAt(0).toUpperCase() + name.slice(1) + " " + root.shownYear
                }
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.DemiBold
            }

            StudioButton {
                variant: "ghost"
                iconName: "chevron-right"
                iconSize: 14
                enabled: root.canGoForward
                leftPadding: 0
                rightPadding: 0
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                onClicked: root.shiftMonth(1)
            }
        }

        GridLayout {
            columns: 7
            rowSpacing: 2
            columnSpacing: 2

            Repeater {
                model: 7

                delegate: Text {
                    id: weekdayLabel

                    required property int index

                    Layout.preferredWidth: 36
                    horizontalAlignment: Text.AlignHCenter
                    text: root.uiLocale.dayName((root.uiLocale.firstDayOfWeek + weekdayLabel.index) % 7,
                                                Locale.NarrowFormat).toUpperCase()
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: Font.DemiBold
                }
            }

            Repeater {
                model: root.cells

                delegate: Rectangle {
                    id: dayCell

                    required property var modelData

                    readonly property string iso: dayCell.modelData.iso
                    readonly property bool selectable: root.inWindow(dayCell.iso)
                    readonly property bool selected: dayCell.iso.length > 0 && dayCell.iso === root.selectedIso
                    readonly property string status: dayCell.iso.length > 0 ? (root.statuses[dayCell.iso] ?? "") : ""

                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: Theme.radiusSmall
                    color: dayCell.selected ? Theme.accentPrimary
                         : (dayMouse.containsMouse && dayCell.selectable ? Theme.cardHover : "transparent")
                    border.width: dayCell.iso === root.todayIso && !dayCell.selected ? 1 : 0
                    border.color: Theme.accentBorder

                    Text {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -2
                        visible: dayCell.modelData.day > 0
                        text: "" + dayCell.modelData.day
                        color: dayCell.selected ? Theme.textOnAccent
                             : (dayCell.selectable ? Theme.textPrimary : Theme.textMuted)
                        opacity: dayCell.selectable ? 1.0 : 0.45
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        font.weight: dayCell.selected ? Font.Bold : Font.Normal
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 4
                        width: 6
                        height: 6
                        radius: 3
                        visible: dayCell.status.length > 0
                        color: dayCell.selected ? Theme.textOnAccent : root.statusColor(dayCell.status)
                    }

                    MouseArea {
                        id: dayMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: dayCell.selectable
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.dateChosen(dayCell.iso)
                            root.close()
                        }
                    }
                }
            }
        }
    }
}
