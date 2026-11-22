import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import ".."

T.MenuItem {
    id: root

    property string iconName: ""
    property bool destructive: false
    // A group caption such as "From JW.org": not clickable, drawn small and muted.
    property bool caption: false

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    height: visible ? implicitHeight : 0
    enabled: !root.caption
    topPadding: root.caption ? 8 : 7
    bottomPadding: root.caption ? 4 : 7
    leftPadding: 10
    rightPadding: 10
    spacing: 10

    contentItem: RowLayout {
        spacing: root.spacing

        StudioIcon {
            visible: root.iconName.length > 0
            name: root.iconName
            size: 15
            color: root.destructive ? Theme.statusLive : (root.enabled ? Theme.textSecondary : Theme.textMuted)
        }

        Text {
            Layout.fillWidth: true
            text: root.text
            color: root.caption ? Theme.textMuted
                 : (root.destructive ? Theme.statusLive
                    : (root.enabled ? Theme.textPrimary : Theme.textMuted))
            font.family: Theme.fontFamily
            font.pixelSize: root.caption ? Theme.fontSizeXs : Theme.fontSizeMd
            font.weight: root.caption ? Font.DemiBold : Font.Normal
            font.capitalization: root.caption ? Font.AllUppercase : Font.MixedCase
            font.letterSpacing: root.caption ? 0.5 : 0
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        StudioIcon {
            visible: root.checkable && root.checked
            name: "check"
            size: 14
            color: Theme.accentPrimary
        }

        StudioIcon {
            visible: root.subMenu !== null
            name: "chevron-right"
            size: 13
            color: Theme.textSecondary
        }
    }

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: root.caption ? 26 : 34
        radius: Theme.radiusSmall
        color: root.highlighted && !root.caption ? Theme.cardHover : "transparent"
    }
}
