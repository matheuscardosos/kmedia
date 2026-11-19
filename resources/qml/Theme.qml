pragma Singleton
import QtQuick
import KMedia.UI

QtObject {
    readonly property bool isDark: ApplicationController.darkTheme

    // Brand accent: Broadcast Orange
    readonly property color accentPrimary: "#FF6B00"
    readonly property color accentHover: "#FF8533"
    readonly property color accentPressed: "#E05E00"
    readonly property color accentSoft: isDark ? "#26FF6B00" : "#1AFF6B00"
    readonly property color accentBorder: isDark ? "#70FF6B00" : "#66FF6B00"
    readonly property color textOnAccent: "#FFFFFF"

    // Studio Carbon (Dark) & Studio Daylight (Light) surfaces
    readonly property color windowBase: isDark ? "#0D0F12" : "#F1F3F6"
    readonly property color panelSurface: isDark ? "#13161C" : "#FFFFFF"
    readonly property color cardSurface: isDark ? "#181C23" : "#F8FAFC"
    readonly property color cardElevated: isDark ? "#1E232C" : "#FFFFFF"
    readonly property color cardHover: isDark ? "#242A35" : "#ECEFF4"
    readonly property color rowSelected: isDark ? "#291B12" : "#FFF3EB"
    readonly property color borderHairline: isDark ? "#262C36" : "#E2E6ED"
    readonly property color borderStrong: isDark ? "#353C4A" : "#CBD2DC"
    readonly property color glassSurface: isDark ? "#B8181C22" : "#D9FFFFFF"
    readonly property color glassBorder: isDark ? "#33FFFFFF" : "#26000000"
    readonly property color scrimOverlay: isDark ? "#B3090B0E" : "#80111827"

    // Typography colors
    readonly property color textPrimary: isDark ? "#F3F4F6" : "#111827"
    readonly property color textSecondary: isDark ? "#9CA3AF" : "#4B5563"
    readonly property color textMuted: isDark ? "#6B7280" : "#6B7280"

    // Semantic status colors
    readonly property color statusLive: "#DC2626"
    readonly property color statusReady: "#22C55E"
    readonly property color statusStandby: "#F59E0B"
    readonly property color statusOff: isDark ? "#6B7280" : "#9CA3AF"

    // Responsive layout breakpoints & geometry constants
    readonly property int compactBreakpoint: 680
    readonly property int wideBreakpoint: 1080
    readonly property int navRailWidth: 234
    readonly property int navRailCompactWidth: 64
    readonly property int inspectorPanelWidth: 344
    readonly property int transportBarHeight: 92
    readonly property int compactTabBarHeight: 54
    readonly property int headerHeight: 56

    readonly property int radiusSmall: 6
    readonly property int radiusMedium: 10
    readonly property int radiusLarge: 14
    readonly property int radiusPill: 999

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

    // Motion settings
    readonly property bool reducedMotion: false
    readonly property int animFast: reducedMotion ? 0 : 140
    readonly property int animNormal: reducedMotion ? 0 : 220
}
