pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

StudioCard {
    id: root

    property bool compactMode: false

    implicitHeight: assetsCol.implicitHeight + 28
    surfaceColor: Theme.panelSurface

    ColumnLayout {
        id: assetsCol

        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioIcon {
                name: "jwpub"
                category: "custom"
                size: 18
                color: Theme.accentPrimary
            }

            Text {
                text: qsTr("Meeting & Publication Media (%1)").arg(ApplicationController.catalog.assets.length)
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            StudioBadge {
                text: "MWB " + ApplicationController.catalog.midweekIssueTag + " | W " + ApplicationController.catalog.watchtowerIssueTag
                badgeColor: Theme.cardSurface
                textColor: Theme.textSecondary
                hasBorder: true
                visible: !root.compactMode
            }
        }

        Text {
            visible: ApplicationController.catalog.assets.length === 0
            Layout.fillWidth: true
            text: ApplicationController.catalog.busy
                  ? qsTr("Fetching publication catalog...")
                  : qsTr("Select a week and click Fetch & Auto-Fill to load meeting media.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: ApplicationController.catalog.assets

            delegate: Rectangle {
                id: assetRow

                required property var modelData

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                radius: Theme.radiusSmall
                color: Theme.cardSurface
                border.width: 1
                border.color: assetRow.modelData.downloaded ? Theme.accentBorder : Theme.borderHairline

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10

                    StudioBadge {
                        text: assetRow.modelData.fileFormat
                        badgeColor: assetRow.modelData.fileFormat === "JWPUB"
                                    ? Theme.accentSoft : Theme.panelSurface
                        textColor: assetRow.modelData.fileFormat === "JWPUB"
                                   ? Theme.accentPrimary : Theme.textSecondary
                        hasBorder: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            text: assetRow.modelData.title
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: assetRow.modelData.sectionTitle
                                  + (assetRow.modelData.durationText.length > 0 ? (" | " + assetRow.modelData.durationText) : "")
                                  + " | " + assetRow.modelData.sizeText
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    StudioButton {
                        variant: assetRow.modelData.downloaded ? "secondary" : "primary"
                        iconName: assetRow.modelData.downloaded ? "plus" : "download"
                        text: root.compactMode
                              ? ""
                              : (assetRow.modelData.downloaded ? qsTr("Import") : qsTr("Download"))
                        implicitHeight: 30
                        leftPadding: root.compactMode ? 8 : 10
                        rightPadding: root.compactMode ? 8 : 10
                        onClicked: {
                            const id = ApplicationController.catalog.downloadAssetAt(assetRow.modelData.index)
                            if (id.length === 0) {
                                ApplicationController.showToast(qsTr("Imported cached package to playlist"), "success")
                            }
                        }
                    }
                }
            }
        }
    }
}
