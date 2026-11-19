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
                        onClicked: ApplicationController.catalog.syncCurrentWeekToPlaylist()
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
