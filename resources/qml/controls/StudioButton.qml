import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import ".."

T.Button {
    id: root

    // variant: "primary" | "secondary" | "ghost" | "danger"
    property string variant: "secondary"
    property string iconName: ""
    property string trailingIconName: ""
    property int iconSize: 15
    property int cornerRadius: Theme.radiusMedium

    readonly property color resolvedBg: {
        if (!root.enabled) {
            return Theme.cardSurface
        }
        if (root.variant === "primary") {
            return root.down ? Theme.accentPressed
                 : (root.hovered ? Theme.accentHover : Theme.accentPrimary)
        }
        if (root.variant === "danger") {
            return root.down ? "#991B1B" : (root.hovered ? "#DC2626" : "#B91C1C")
        }
        if (root.variant === "ghost") {
            return root.down ? Theme.cardHover : (root.hovered ? Theme.cardSurface : "transparent")
        }
        return root.down ? Theme.cardHover : (root.hovered ? Theme.cardElevated : Theme.cardSurface)
    }

    readonly property color resolvedFg: {
        if (!root.enabled) {
            return Theme.textMuted
        }
        if (root.variant === "primary" || root.variant === "danger") {
            return Theme.textOnAccent
        }
        return Theme.textPrimary
    }

    implicitWidth: contentRow.implicitWidth + leftPadding + rightPadding
    implicitHeight: 34
    leftPadding: 12
    rightPadding: 12
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        radius: root.cornerRadius
        color: root.resolvedBg
        border.width: root.variant === "secondary" ? 1 : 0
        border.color: root.hovered ? Theme.borderStrong : Theme.borderHairline

        Behavior on color {
            ColorAnimation {
                duration: Theme.animFast
            }
        }

        FocusRing {
            active: root.visualFocus
            targetRadius: root.cornerRadius
        }
    }

    contentItem: RowLayout {
        id: contentRow

        spacing: 6

        StudioIcon {
            visible: root.iconName.length > 0
            name: root.iconName
            size: root.iconSize
            color: root.resolvedFg
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            visible: root.text.length > 0
            text: root.text
            color: root.resolvedFg
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            font.weight: root.variant === "primary" ? Font.DemiBold : Font.Medium
            elide: Text.ElideRight
            Layout.alignment: Qt.AlignVCenter
        }

        StudioIcon {
            visible: root.trailingIconName.length > 0
            name: root.trailingIconName
            size: root.iconSize
            color: root.resolvedFg
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
