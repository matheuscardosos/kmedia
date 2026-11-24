pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// The meeting timer, part by part (inspired by Meeting Media Manager).
ColumnLayout {
    id: root

    readonly property var timer: ApplicationController.meetingTimer
    readonly property bool meetingDay: root.timer.meetingKind === "midweek" || root.timer.meetingKind === "weekend"
    readonly property var currentPart: root.timer.currentIndex >= 0 && root.timer.currentIndex < root.timer.parts.length
                                       ? root.timer.parts[root.timer.currentIndex] : null
    readonly property var nextPart: root.timer.nextIndex >= 0 ? root.timer.parts[root.timer.nextIndex] : null
    property bool showOptions: false

    // The timing report is saved by the pill bar, which outlives this popover.
    signal saveReportRequested(var titles, var headers, string heading)

    function requestReport() {
        const titles = {}
        for (const part of root.timer.parts)
            titles[part.id] = root.partTitle(part)
        const headers = [qsTr("Part"), qsTr("Planned start"), qsTr("Started"),
                         qsTr("Planned"), qsTr("Took"), qsTr("Difference")]
        const day = Date.fromLocaleDateString(Qt.locale(), root.timer.dateIso, "yyyy-MM-dd")
        root.saveReportRequested(titles, headers,
                                 qsTr("Meeting timing report, %1").arg(day.toLocaleDateString(Qt.locale(), Locale.LongFormat)))
    }

    function partTitle(part) {
        if (!part)
            return ""
        if (part.custom)
            return part.title.length > 0 ? part.title : qsTr("Untitled part")
        const id = part.id
        if (id.startsWith("ayfm-"))
            return qsTr("Apply Yourself %1").arg(id.substring(5))
        if (id.startsWith("lac-"))
            return qsTr("Living as Christians %1").arg(id.substring(4))
        switch (id) {
        case "opening-song":
        case "closing-song":
            return qsTr("Song and prayer")
        case "middle-song":
            return qsTr("Song")
        case "introduction":
            return qsTr("Opening comments")
        case "treasures":
            return qsTr("Treasures From God's Word")
        case "gems":
            return qsTr("Spiritual gems")
        case "bible-reading":
            return qsTr("Bible reading")
        case "cbs":
            return qsTr("Congregation Bible Study")
        case "concluding-comments":
            return qsTr("Concluding comments")
        case "co-service-talk":
            return qsTr("Service talk")
        case "public-talk":
            return qsTr("Public talk")
        case "wt":
        case "abbreviated-wt":
            return qsTr("Watchtower Study")
        case "co-final-talk":
            return qsTr("Circuit overseer's talk")
        }
        return id
    }

    function formatDuration(ms) {
        const seconds = Math.floor(Math.max(0, ms) / 1000)
        const minutes = Math.floor(seconds / 60)
        const rest = seconds % 60
        return (minutes < 10 ? "0" : "") + minutes + ":" + (rest < 10 ? "0" : "") + rest
    }

    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        StudioIcon {
            name: "clock"
            size: 17
            color: root.timer.overtime
                   ? Theme.statusLive
                   : (root.timer.running ? Theme.statusReady : Theme.accentPrimary)
        }

        Text {
            text: qsTr("Meeting Timer")
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
            Layout.fillWidth: true
        }

        // How far ahead or behind the meeting is, judged by when the current part started.
        StudioBadge {
            visible: root.timer.scheduleOffsetKnown
            text: root.timer.scheduleOffsetMinutes > 0
                  ? qsTr("%n min behind", "", root.timer.scheduleOffsetMinutes)
                  : (root.timer.scheduleOffsetMinutes < 0
                     ? qsTr("%n min ahead", "", -root.timer.scheduleOffsetMinutes)
                     : qsTr("On time"))
            showDot: true
            dotColor: root.timer.scheduleOffsetMinutes > 0 ? Theme.statusStandby : Theme.statusReady
            hasBorder: true
        }
    }

    // The current part, or the next one while nothing runs.
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 70
        radius: Theme.radiusMedium
        color: Theme.cardSurface
        border.width: 1
        border.color: root.timer.overtime ? Theme.statusLive : Theme.borderHairline

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: root.timer.running
                          ? (root.timer.paused ? qsTr("Paused") : qsTr("Now"))
                          : (root.nextPart ? qsTr("Next") : qsTr("No parts left"))
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }

                Text {
                    Layout.fillWidth: true
                    text: root.partTitle(root.timer.running ? root.currentPart : root.nextPart)
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    readonly property var part: root.timer.running ? root.currentPart : root.nextPart
                    visible: part !== null
                    text: part ? qsTr("%n min", "", part.minutes)
                                 + (part.plannedStart.length > 0 ? "  ·  " + qsTr("planned %1").arg(part.plannedStart) : "")
                               : ""
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    elide: Text.ElideRight
                }
            }

            Text {
                text: root.timer.formattedTime
                color: root.timer.overtime ? Theme.statusLive : Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: 26
                font.weight: Font.Bold
                font.features: { "tnum": 1 }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        StudioButton {
            variant: "primary"
            iconName: root.timer.running ? "skip-forward" : "play"
            text: root.timer.running ? qsTr("Next part") : qsTr("Start")
            Layout.fillWidth: true
            enabled: root.timer.running || root.nextPart !== null
            onClicked: {
                if (root.timer.running && root.nextPart === null)
                    root.timer.stop()
                else
                    root.timer.startNext()
            }
        }

        StudioButton {
            visible: root.timer.running
            variant: "secondary"
            iconName: root.timer.paused ? "play" : "pause"
            text: root.timer.paused ? qsTr("Resume") : qsTr("Pause")
            onClicked: root.timer.togglePause()
        }

        StudioButton {
            visible: root.timer.running
            variant: "secondary"
            iconName: "square"
            leftPadding: 0
            rightPadding: 0
            Layout.preferredWidth: 34
            onClicked: root.timer.stop()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Stop")
        }
    }

    // The parts of the day: click one to time it, and change its minutes.
    ListView {
        id: partList

        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(contentHeight, 232)
        visible: count > 0
        clip: true
        spacing: 2
        model: root.timer.parts
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: StudioScrollBar {}

        delegate: Rectangle {
            id: partRow

            required property var modelData
            required property int index
            readonly property bool isCurrent: partRow.modelData.state === "current"

            width: ListView.view.width
            height: 30
            radius: Theme.radiusSmall
            color: partRow.isCurrent
                   ? Theme.accentSoft
                   : (rowMouse.containsMouse ? Theme.cardHover : "transparent")

            MouseArea {
                id: rowMouse

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.timer.startPart(partRow.index)
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 4
                spacing: 6

                Rectangle {
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    radius: 3.5
                    color: partRow.isCurrent
                           ? Theme.statusReady
                           : (partRow.modelData.state === "done"
                              ? (partRow.modelData.over ? Theme.statusLive : Theme.textMuted)
                              : Theme.statusOff)
                }

                Text {
                    Layout.fillWidth: true
                    text: root.partTitle(partRow.modelData)
                    color: partRow.modelData.state === "done" ? Theme.textSecondary : Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: partRow.isCurrent ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }

                // What a finished part really took.
                Text {
                    visible: partRow.modelData.state === "done"
                    text: root.formatDuration(partRow.modelData.durationMs)
                    color: partRow.modelData.over ? Theme.statusLive : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    font.features: { "tnum": 1 }
                }

                StudioButton {
                    variant: "ghost"
                    iconName: "minus"
                    leftPadding: 0
                    rightPadding: 0
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    onClicked: root.timer.adjustPartMinutes(partRow.index, -1)
                }

                Text {
                    Layout.preferredWidth: 22
                    horizontalAlignment: Text.AlignHCenter
                    text: partRow.modelData.minutes
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    font.features: { "tnum": 1 }
                }

                StudioButton {
                    variant: "ghost"
                    iconName: "plus"
                    leftPadding: 0
                    rightPadding: 0
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    onClicked: root.timer.adjustPartMinutes(partRow.index, 1)
                }

                StudioButton {
                    visible: partRow.modelData.custom
                    variant: "ghost"
                    iconName: "trash"
                    leftPadding: 0
                    rightPadding: 0
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    onClicked: root.timer.removeCustomPart(partRow.index)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Remove part")
                }
            }
        }
    }

    // Days without a meeting time parts of their own.
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        visible: !root.meetingDay

        TextField {
            id: customTitleField

            Layout.fillWidth: true
            placeholderText: qsTr("Part title")
            color: Theme.textPrimary
            placeholderTextColor: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            background: StudioCard {
                surfaceColor: Theme.windowBase
                cornerRadius: Theme.radiusSmall
            }
            onAccepted: addPartButton.clicked()
        }

        StudioButton {
            id: addPartButton

            variant: "secondary"
            iconName: "plus"
            text: qsTr("Add part")
            onClicked: {
                root.timer.addCustomPart(customTitleField.text, 10)
                customTitleField.clear()
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        StudioButton {
            variant: "ghost"
            iconName: root.showOptions ? "chevron-up" : "chevron-down"
            text: qsTr("Options")
            onClicked: root.showOptions = !root.showOptions
        }

        Item {
            Layout.fillWidth: true
        }

        StudioButton {
            variant: "ghost"
            iconName: "download"
            leftPadding: 0
            rightPadding: 0
            Layout.preferredWidth: 30
            enabled: root.timer.hasTimings
            onClicked: root.requestReport()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Save timing report")
        }

        StudioButton {
            variant: "ghost"
            iconName: "rotate-ccw"
            text: qsTr("Clear times")
            enabled: root.timer.hasTimings
            onClicked: root.timer.resetTimings()
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8
        visible: root.showOptions

        component CountRow: RowLayout {
            id: countRow

            property string label
            property int value
            signal changed(int value)

            Layout.fillWidth: true
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: countRow.label
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                elide: Text.ElideRight
            }

            StudioButton {
                variant: "ghost"
                iconName: "minus"
                leftPadding: 0
                rightPadding: 0
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                onClicked: countRow.changed(countRow.value - 1)
            }

            Text {
                text: countRow.value
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
            }

            StudioButton {
                variant: "ghost"
                iconName: "plus"
                leftPadding: 0
                rightPadding: 0
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                onClicked: countRow.changed(countRow.value + 1)
            }
        }

        CountRow {
            visible: root.timer.meetingKind === "midweek"
            label: qsTr("Apply Yourself parts")
            value: root.timer.ayfmCount
            onChanged: (value) => root.timer.ayfmCount = value
        }

        CountRow {
            visible: root.timer.meetingKind === "midweek"
            label: qsTr("Living as Christians parts")
            value: root.timer.lacCount
            onChanged: (value) => root.timer.lacCount = value
        }

        // Counting down, the study can be made to end at a set time, so it absorbs earlier delays.
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.timer.hasStudyPart

            Text {
                Layout.fillWidth: true
                text: qsTr("End the study at")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                elide: Text.ElideRight
            }

            TextField {
                Layout.preferredWidth: 70
                text: root.timer.studyEndTime
                placeholderText: "hh:mm"
                validator: RegularExpressionValidator { regularExpression: /^$|^([01]?\d|2[0-3]):[0-5]\d$/ }
                horizontalAlignment: TextInput.AlignHCenter
                color: Theme.textPrimary
                placeholderTextColor: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                background: StudioCard {
                    surfaceColor: Theme.windowBase
                    cornerRadius: Theme.radiusSmall
                }
                onEditingFinished: root.timer.studyEndTime = text
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: qsTr("Timer window")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                elide: Text.ElideRight
            }

            StudioSwitch {
                checked: ApplicationController.timerWindowVisible
                onToggled: ApplicationController.timerWindowVisible = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: qsTr("Count down")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
            }

            StudioSwitch {
                checked: root.timer.countDown
                onToggled: root.timer.countDown = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: !root.timer.countDown

            Text {
                Layout.fillWidth: true
                text: qsTr("Show only the extra time")
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                elide: Text.ElideRight
            }

            StudioSwitch {
                checked: root.timer.overtimeAmountOnly
                onToggled: root.timer.overtimeAmountOnly = checked
            }
        }
    }
}
