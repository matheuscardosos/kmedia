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

    readonly property var filteredLanguages: {
        const q = langFilterField.text.trim().toLowerCase()
        const all = ApplicationController.catalog.languages
        if (!all) {
            return []
        }
        if (q.length === 0) {
            return all.slice(0, 60)
        }
        const out = []
        for (let i = 0; i < all.length && out.length < 60; ++i) {
            const item = all[i]
            if ((item.label || "").toLowerCase().indexOf(q) >= 0 ||
                (item.name || "").toLowerCase().indexOf(q) >= 0 ||
                (item.code || "").toLowerCase().indexOf(q) >= 0) {
                out.push(item)
            }
        }
        return out
    }

    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical: StudioScrollBar {
        policy: ScrollBar.AsNeeded
    }

    ColumnLayout {
        width: parent.width
        spacing: Theme.spacingMd

        // Week Sync & Language Header Card
        StudioCard {
            Layout.fillWidth: true
            implicitHeight: syncCol.implicitHeight + 28
            surfaceColor: Theme.panelSurface

            ColumnLayout {
                id: syncCol

                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                // Row 1: Week Navigation + Language Selector
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StudioButton {
                        variant: "secondary"
                        iconName: "chevron-left"
                        implicitWidth: 32
                        implicitHeight: 32
                        leftPadding: 0
                        rightPadding: 0
                        onClicked: ApplicationController.catalog.previousWeek()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Previous week")
                    }

                    StudioButton {
                        variant: "secondary"
                        iconName: "calendar"
                        text: ApplicationController.catalog.weekLabel
                        implicitHeight: 32
                        onClicked: ApplicationController.catalog.goToCurrentWeek()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Jump to current week")
                    }

                    StudioButton {
                        variant: "secondary"
                        iconName: "chevron-right"
                        implicitWidth: 32
                        implicitHeight: 32
                        leftPadding: 0
                        rightPadding: 0
                        onClicked: ApplicationController.catalog.nextWeek()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Next week")
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StudioButton {
                        id: congBtn

                        visible: !root.compactMode
                        variant: "secondary"
                        iconName: "users"
                        text: qsTr("Find Congregation")
                        Layout.preferredHeight: 32
                        onClicked: congPopup.open()

                        Popup {
                            id: congPopup

                            x: Math.min(0, congBtn.width - width)
                            y: congBtn.height + 4
                            width: 340
                            padding: 10
                            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                            background: StudioCard {
                                surfaceColor: Theme.cardElevated
                                strokeColor: Theme.borderStrong
                            }

                            contentItem: ColumnLayout {
                                spacing: 8

                                Text {
                                    text: qsTr("Congregation Lookup (hub.jw.org)")
                                    color: Theme.textPrimary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    font.weight: Font.DemiBold
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    TextField {
                                        id: congQueryField

                                        Layout.fillWidth: true
                                        placeholderText: qsTr("City, congregation name...")
                                        color: Theme.textPrimary
                                        placeholderTextColor: Theme.textMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onAccepted: ApplicationController.catalog.searchCongregations(congQueryField.text)
                                        background: StudioCard {
                                            surfaceColor: Theme.windowBase
                                            cornerRadius: Theme.radiusSmall
                                        }
                                    }

                                    StudioButton {
                                        variant: "primary"
                                        iconName: "search"
                                        Layout.preferredWidth: 32
                                        Layout.preferredHeight: 32
                                        leftPadding: 0
                                        rightPadding: 0
                                        enabled: congQueryField.text.trim().length > 0 && !ApplicationController.catalog.busy
                                        onClicked: ApplicationController.catalog.searchCongregations(congQueryField.text)
                                    }
                                }

                                ListView {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 180
                                    clip: true
                                    spacing: 4
                                    model: ApplicationController.catalog.congregationResults
                                    ScrollBar.vertical: StudioScrollBar {
                                        policy: ScrollBar.AsNeeded
                                    }

                                    delegate: Rectangle {
                                        id: congRow

                                        required property var modelData

                                        width: ListView.view ? ListView.view.width : 310
                                        height: 46
                                        radius: Theme.radiusSmall
                                        color: congHover.containsMouse ? Theme.cardHover : Theme.cardSurface
                                        border.width: 1
                                        border.color: Theme.borderHairline

                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 8
                                            spacing: 1

                                            Text {
                                                text: congRow.modelData.orgName + " (" + congRow.modelData.languageCode + ")"
                                                color: Theme.textPrimary
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeXs
                                                font.weight: Font.DemiBold
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            Text {
                                                text: congRow.modelData.address.length > 0
                                                      ? congRow.modelData.address
                                                      : (congRow.modelData.midweekTime + " / " + congRow.modelData.weekendTime)
                                                color: Theme.textMuted
                                                font.family: Theme.fontFamily
                                                font.pixelSize: 10
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }

                                        MouseArea {
                                            id: congHover

                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                ApplicationController.catalog.applyCongregationResult(congRow.modelData.index)
                                                congPopup.close()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    StudioButton {
                        id: langBtn

                        variant: "secondary"
                        iconName: "globe"
                        trailingIconName: "chevron-down"
                        text: root.compactMode
                              ? ApplicationController.catalog.selectedLanguageCode
                              : ApplicationController.catalog.selectedLanguageLabel
                        implicitHeight: 32
                        onClicked: langPopup.open()

                        Popup {
                            id: langPopup

                            x: Math.min(0, langBtn.width - width)
                            y: langBtn.height + 4
                            width: 280
                            padding: 8
                            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

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
                                        id: langFilterField

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
                                        implicitWidth: 32
                                        implicitHeight: 32
                                        leftPadding: 0
                                        rightPadding: 0
                                        onClicked: ApplicationController.catalog.refreshLanguages()
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Fetch full JW language list")
                                    }
                                }

                                ListView {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 220
                                    clip: true
                                    spacing: 2
                                    model: root.filteredLanguages
                                    ScrollBar.vertical: StudioScrollBar {
                                        policy: ScrollBar.AsNeeded
                                    }

                                    delegate: Rectangle {
                                        id: langRow

                                        required property var modelData
                                        readonly property bool selected: langRow.modelData.code === ApplicationController.catalog.selectedLanguageCode

                                        width: ListView.view ? ListView.view.width : 260
                                        height: 32
                                        radius: Theme.radiusSmall
                                        color: langMouse.containsMouse
                                               ? Theme.cardHover
                                               : (langRow.selected ? Theme.accentSoft : "transparent")

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            spacing: 6

                                            Text {
                                                text: langRow.modelData.label
                                                color: langRow.selected ? Theme.accentPrimary : Theme.textPrimary
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeSm
                                                font.weight: langRow.selected ? Font.DemiBold : Font.Normal
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            StudioIcon {
                                                visible: langRow.selected
                                                name: "check"
                                                size: 14
                                                color: Theme.accentPrimary
                                            }
                                        }

                                        MouseArea {
                                            id: langMouse

                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                ApplicationController.catalog.selectedLanguageCode = langRow.modelData.code
                                                langPopup.close()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Row 2: Auto-Sync Week & Custom Publication Lookup
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StudioButton {
                        variant: "primary"
                        iconName: "refresh-cw"
                        text: root.compactMode ? qsTr("Sync Week") : qsTr("Fetch & Auto-Fill Week")
                        enabled: !ApplicationController.catalog.busy
                        onClicked: ApplicationController.sync.resyncDay(
                            ApplicationController.calendar.selectedDateIso)
                    }

                    StudioButton {
                        variant: "secondary"
                        iconName: "search"
                        text: root.compactMode ? qsTr("Browse") : qsTr("Browse Week")
                        enabled: !ApplicationController.catalog.busy
                        onClicked: ApplicationController.catalog.fetchWeekCatalog()
                    }

                    TextField {
                        id: pubField

                        visible: !root.compactMode
                        Layout.fillWidth: true
                        placeholderText: qsTr("Publication (e.g. sjjm, lff, mwb)")
                        color: Theme.textPrimary
                        placeholderTextColor: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        onAccepted: {
                            if (pubField.text.trim().length > 0) {
                                ApplicationController.catalog.fetchPublication(pubField.text.trim(), 0, "")
                            }
                        }
                        background: StudioCard {
                            surfaceColor: Theme.windowBase
                            cornerRadius: Theme.radiusSmall
                        }
                    }

                    StudioButton {
                        visible: !root.compactMode
                        variant: "secondary"
                        iconName: "search"
                        text: qsTr("Lookup")
                        enabled: !ApplicationController.catalog.busy && pubField.text.trim().length > 0
                        onClicked: ApplicationController.catalog.fetchPublication(pubField.text.trim(), 0, "")
                    }
                }

                // Row 3: Extraction filters & fallback language
                Flow {
                    visible: !root.compactMode
                    Layout.fillWidth: true
                    spacing: 6

                    StudioButton {
                        variant: ApplicationController.catalog.autoLoadWeekMedia ? "primary" : "secondary"
                        text: qsTr("Auto-Load Week")
                        height: 28
                        onClicked: ApplicationController.catalog.autoLoadWeekMedia = !ApplicationController.catalog.autoLoadWeekMedia
                    }

                    StudioButton {
                        variant: ApplicationController.catalog.excludeFootnotes ? "primary" : "secondary"
                        text: qsTr("Hide Footnotes (*)")
                        height: 28
                        onClicked: ApplicationController.catalog.excludeFootnotes = !ApplicationController.catalog.excludeFootnotes
                    }

                    StudioButton {
                        variant: ApplicationController.catalog.includePrinted ? "primary" : "secondary"
                        text: qsTr("Include Printed")
                        height: 28
                        onClicked: ApplicationController.catalog.includePrinted = !ApplicationController.catalog.includePrinted
                    }

                    StudioButton {
                        variant: ApplicationController.catalog.excludeWtParagraphVideos ? "primary" : "secondary"
                        text: qsTr("Skip WT Inline Videos")
                        height: 28
                        onClicked: ApplicationController.catalog.excludeWtParagraphVideos = !ApplicationController.catalog.excludeWtParagraphVideos
                    }

                    StudioButton {
                        variant: ApplicationController.catalog.excludeTh ? "primary" : "secondary"
                        text: qsTr("Skip TH Videos")
                        height: 28
                        onClicked: ApplicationController.catalog.excludeTh = !ApplicationController.catalog.excludeTh
                    }

                    StudioButton {
                        variant: ApplicationController.catalog.excludeCbsPubs ? "primary" : "secondary"
                        text: qsTr("Skip CBS Extracts")
                        height: 28
                        onClicked: ApplicationController.catalog.excludeCbsPubs = !ApplicationController.catalog.excludeCbsPubs
                    }

                    StudioButton {
                        id: fallbackLangBtn

                        variant: "secondary"
                        text: qsTr("Fallback: %1").arg(ApplicationController.catalog.fallbackLanguageCode)
                        height: 28
                        onClicked: fallbackLangPopup.open()

                        Popup {
                            id: fallbackLangPopup

                            x: 0
                            y: fallbackLangBtn.height + 4
                            width: 220
                            padding: 6

                            background: StudioCard {
                                surfaceColor: Theme.cardElevated
                                strokeColor: Theme.borderStrong
                            }

                            contentItem: ListView {
                                width: 208
                                height: 160
                                clip: true
                                spacing: 2
                                model: ApplicationController.catalog.languages

                                delegate: Rectangle {
                                    id: fbRow

                                    required property var modelData

                                    width: 208
                                    height: 28
                                    radius: Theme.radiusSmall
                                    color: fbHover.containsMouse ? Theme.cardHover : "transparent"

                                    Text {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        verticalAlignment: Text.AlignVCenter
                                        text: fbRow.modelData.label
                                        color: Theme.textPrimary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeXs
                                        elide: Text.ElideRight
                                    }

                                    MouseArea {
                                        id: fbHover

                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            ApplicationController.catalog.fallbackLanguageCode = fbRow.modelData.code
                                            fallbackLangPopup.close()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: ApplicationController.catalog.statusMessage.length > 0
                    Layout.fillWidth: true
                    text: ApplicationController.catalog.statusMessage
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    elide: Text.ElideRight
                }
            }
        }

        CatalogBrowserCard {
            Layout.fillWidth: true
            compactMode: root.compactMode
        }

        DownloadQueueCard {
            Layout.fillWidth: true
            compactMode: root.compactMode
        }
    }
}
