pragma Singleton
import QtQuick

QtObject {
    // Brand accent: Broadcast Orange
    readonly property color accentPrimary: "#FF6B00"
    readonly property color accentHover: "#FF8533"
    readonly property color accentPressed: "#E05E00"
    readonly property color accentSoft: "#24FF6B00"

    // Studio Carbon neutral surfaces (no cold blue tint)
    readonly property color windowBase: "#0D0F12"
    readonly property color panelSurface: "#14171C"
    readonly property color cardSurface: "#1B1F26"
    readonly property color cardHover: "#232832"
    readonly property color borderHairline: "#282D37"

    // Typography colors
    readonly property color textPrimary: "#F4F5F7"
    readonly property color textSecondary: "#949BA8"
    readonly property color textMuted: "#68707E"

    // Semantic live broadcast status
    readonly property color statusLive: "#EF4444"
    readonly property color statusReady: "#10B981"
    readonly property color statusStandby: "#F59E0B"

    // Layout geometry constants
    readonly property int navRailWidth: 210
    readonly property int inspectorPanelWidth: 340
    readonly property int transportBarHeight: 68
    readonly property int headerHeight: 54
    readonly property int radiusSmall: 6
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 24

    // Typography scale
    readonly property string fontFamily: "Inter"
    readonly property int fontSizeXs: 11
    readonly property int fontSizeSm: 12
    readonly property int fontSizeMd: 13
    readonly property int fontSizeLg: 15
    readonly property int fontSizeTitle: 18
}
