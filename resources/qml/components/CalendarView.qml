pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

ScrollView {
    id: root

    property bool compactMode: false
    signal openMeetingRequested()

    readonly property var weekdayHeaders: [
        qsTr("Mon"),
        qsTr("Tue"),
        qsTr("Wed"),
        qsTr("Thu"),
        qsTr("Fri"),
        qsTr("Sat"),
        qsTr("Sun")
    ]

    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical: StudioScrollBar {
        policy: ScrollBar.AsNeeded
    }

    CalendarSchedulePopup {
        id: schedulePopup
    }

    CalendarEventPopup {
        id: eventPopup
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacingMd

        // Month navigation and congregation schedule bar
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: navBarLayout.implicitHeight + 24
            surfaceColor: Theme.panelSurface

            RowLayout {
                id: navBarLayout

                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                StudioButton {
                    id: prevMonthBtn

                    variant: "secondary"
                    iconName: "chevron-left"
                    leftPadding: 0
                    rightPadding: 0
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 32
                    ToolTip.visible: prevMonthBtn.hovered
                    ToolTip.text: qsTr("Previous month")
                    onClicked: ApplicationController.calendar.previousMonth()
                }

                Text {
                    text: ApplicationController.calendar.monthTitle
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLg
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                StudioButton {
                    id: nextMonthBtn

                    variant: "secondary"
                    iconName: "chevron-right"
                    leftPadding: 0
                    rightPadding: 0
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 32
                    ToolTip.visible: nextMonthBtn.hovered
                    ToolTip.text: qsTr("Next month")
                    onClicked: ApplicationController.calendar.nextMonth()
                }

                StudioButton {
                    variant: "ghost"
                    text: qsTr("Today")
                    Layout.preferredHeight: 32
                    onClicked: ApplicationController.calendar.goToToday()
                }

                StudioBadge {
                    visible: !root.compactMode
                    text: ApplicationController.calendar.scheduleSummary
                    showDot: true
                    dotColor: Theme.accentPrimary
                    badgeColor: Theme.cardSurface
                    textColor: Theme.textSecondary
                    hasBorder: true
                }

                StudioButton {
                    id: scheduleBtn

                    variant: "secondary"
                    iconName: "sliders-horizontal"
                    text: root.compactMode ? "" : qsTr("Schedule")
                    leftPadding: root.compactMode ? 0 : 12
                    rightPadding: root.compactMode ? 0 : 12
                    Layout.preferredWidth: root.compactMode ? 34 : scheduleBtn.implicitWidth
                    Layout.preferredHeight: 32
                    ToolTip.visible: scheduleBtn.hovered
                    ToolTip.text: qsTr("Configure meeting days and times")
                    onClicked: schedulePopup.open()
                }
            }
        }

        // 7-column Mon-Sun calendar grid
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: calGridCol.implicitHeight + 24
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: calGridCol

                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                GridLayout {
                    Layout.fillWidth: true
                    columns: 7
                    columnSpacing: 6

                    Repeater {
                        model: root.weekdayHeaders

                        delegate: Text {
                            id: dayHeader

                            required property string modelData

                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: dayHeader.modelData
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            font.weight: Font.DemiBold
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 7
                    columnSpacing: 6
                    rowSpacing: 6

                    Repeater {
                        model: ApplicationController.calendar.calendarDays

                        delegate: Rectangle {
                            id: dayCell

                            required property var modelData

                            readonly property bool hasMeeting: dayCell.modelData.meetingKind.length > 0

                            Layout.fillWidth: true
                            Layout.preferredHeight: root.compactMode ? 48 : 60
                            radius: Theme.radiusSmall
                            color: dayCell.modelData.isSelectedWeek
                                   ? Theme.rowSelected
                                   : (dayMouse.containsMouse ? Theme.cardHover : Theme.cardSurface)
                            border.width: dayCell.modelData.isSelected ? 2 : 1
                            border.color: dayCell.modelData.isSelected
                                          ? Theme.accentPrimary
                                          : (dayCell.modelData.isSpecialEvent
                                             ? Theme.accentBorder
                                             : (dayCell.modelData.isToday ? Theme.borderStrong : Theme.borderHairline))

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: "" + dayCell.modelData.dayNumber
                                        color: dayCell.modelData.inCurrentMonth ? Theme.textPrimary : Theme.textMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        font.weight: (dayCell.modelData.isToday || dayCell.modelData.isSelected)
                                                     ? Font.Bold : Font.Medium
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }

                                    Rectangle {
                                        visible: dayCell.hasMeeting || dayCell.modelData.isSpecialEvent
                                        Layout.preferredWidth: 7
                                        Layout.preferredHeight: 7
                                        radius: 4
                                        color: dayCell.modelData.mediaReady
                                               ? Theme.statusReady
                                               : (dayCell.modelData.isSpecialEvent
                                                  ? "#6B4E9B"
                                                  : (dayCell.modelData.meetingKind === "midweek"
                                                     ? Theme.accentPrimary : Theme.statusStandby))
                                    }
                                }

                                RowLayout {
                                    visible: dayCell.hasMeeting && !root.compactMode
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: dayCell.modelData.meetingKind === "midweek"
                                              ? qsTr("MWB")
                                              : (dayCell.modelData.meetingKind === "memorial" ? qsTr("MEM") : qsTr("W"))
                                        color: dayCell.modelData.mediaReady
                                               ? Theme.statusReady : Theme.accentPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: dayCell.modelData.meetingTime
                                        color: Theme.textMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 10
                                    }
                                }

                                Item {
                                    Layout.fillHeight: true
                                }
                            }

                            MouseArea {
                                id: dayMouse

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: ApplicationController.openDateWorkspace(dayCell.modelData.dateIso, false)
                                onDoubleClicked: {
                                    ApplicationController.openDateWorkspace(dayCell.modelData.dateIso, false)
                                    root.openMeetingRequested()
                                }
                            }
                        }
                    }
                }
            }
        }

        // Selected week banner and upcoming 4-week readiness list
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: upcomingCol.implicitHeight + 24
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: upcomingCol

                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StudioIcon {
                        name: "calendar-days"
                        size: 17
                        color: Theme.accentPrimary
                    }

                    Text {
                        text: ApplicationController.calendar.selectedWeekLabel
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeMd
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    StudioBadge {
                        visible: ApplicationController.calendar.selectedWeekEventLabel.length > 0
                        text: ApplicationController.calendar.selectedWeekEventLabel
                        showDot: true
                        dotColor: ApplicationController.calendar.selectedWeekEventType === "co_visit"
                                  ? "#3F7F5F" : "#6B4E9B"
                        badgeColor: Theme.accentSoft
                        textColor: Theme.accentPrimary
                        hasBorder: true
                    }

                    StudioButton {
                        id: eventBtn

                        variant: "secondary"
                        iconName: "users"
                        text: root.compactMode ? "" : qsTr("Event")
                        leftPadding: root.compactMode ? 0 : 10
                        rightPadding: root.compactMode ? 0 : 10
                        Layout.preferredWidth: root.compactMode ? 32 : eventBtn.implicitWidth
                        Layout.preferredHeight: 30
                        ToolTip.visible: eventBtn.hovered
                        ToolTip.text: qsTr("Set Circuit Overseer visit or special week event")
                        onClicked: eventPopup.open()
                    }

                    StudioButton {
                        variant: "secondary"
                        iconName: "play"
                        text: qsTr("Open Week")
                        Layout.preferredHeight: 30
                        onClicked: {
                            ApplicationController.openDateWorkspace(
                                ApplicationController.calendar.selectedDateIso, false)
                            root.openMeetingRequested()
                        }
                    }

                    StudioButton {
                        id: syncWeekBtn

                        variant: "primary"
                        iconName: "refresh-cw"
                        text: root.compactMode ? "" : qsTr("Sync Week")
                        leftPadding: root.compactMode ? 0 : 12
                        rightPadding: root.compactMode ? 0 : 12
                        Layout.preferredWidth: root.compactMode ? 32 : syncWeekBtn.implicitWidth
                        Layout.preferredHeight: 30
                        ToolTip.visible: syncWeekBtn.hovered
                        ToolTip.text: qsTr("Open selected week and fetch missing media")
                        onClicked: {
                            ApplicationController.calendar.openWeekForDate(
                                ApplicationController.calendar.selectedDateIso, true)
                            root.openMeetingRequested()
                        }
                    }
                }

                Repeater {
                    model: ApplicationController.calendar.upcomingMeetings

                    delegate: Rectangle {
                        id: upRow

                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        radius: Theme.radiusSmall
                        color: Theme.cardSurface
                        border.width: 1
                        border.color: upRow.modelData.mediaReady ? Theme.accentBorder : Theme.borderHairline

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            StudioBadge {
                                text: upRow.modelData.dayBadge
                                badgeColor: upRow.modelData.meetingKind === "midweek"
                                            ? Theme.accentSoft : Theme.panelSurface
                                textColor: upRow.modelData.meetingKind === "midweek"
                                           ? Theme.accentPrimary : Theme.textSecondary
                                hasBorder: true
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    text: upRow.modelData.issueLabel
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: upRow.modelData.dateIso + "  " + upRow.modelData.meetingTime
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                }
                            }

                            StudioBadge {
                                visible: !root.compactMode
                                text: upRow.modelData.mediaReady
                                      ? qsTr("%1 files").arg(upRow.modelData.cachedCount)
                                      : qsTr("Not cached")
                                showDot: true
                                dotColor: upRow.modelData.mediaReady ? Theme.statusReady : Theme.statusOff
                                badgeColor: Theme.panelSurface
                                textColor: Theme.textSecondary
                                hasBorder: true
                            }

                            StudioButton {
                                id: openRowBtn

                                variant: "ghost"
                                iconName: "play"
                                leftPadding: 0
                                rightPadding: 0
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 30
                                ToolTip.visible: openRowBtn.hovered
                                ToolTip.text: qsTr("Load meeting week")
                                onClicked: {
                                    ApplicationController.openDateWorkspace(upRow.modelData.dateIso, false)
                                    root.openMeetingRequested()
                                }
                            }

                            StudioButton {
                                id: syncRowBtn

                                variant: "secondary"
                                iconName: "download"
                                leftPadding: 0
                                rightPadding: 0
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 30
                                ToolTip.visible: syncRowBtn.hovered
                                ToolTip.text: qsTr("Sync week media")
                                onClicked: {
                                    ApplicationController.calendar.openWeekForDate(upRow.modelData.dateIso, true)
                                    root.openMeetingRequested()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
