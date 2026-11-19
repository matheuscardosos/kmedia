pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import ".."
import "../controls"

Rectangle {
    id: root

    property int activeTabIndex: 0
    signal tabSelected(int tabIndex)

    readonly property var tabs: [
        { label: qsTr("Meeting"), icon: "calendar" },
        { label: qsTr("Media"), icon: "film" },
        { label: qsTr("Lists"), icon: "layers" },
        { label: qsTr("Downloads"), icon: "download" },
        { label: qsTr("Settings"), icon: "settings" }
    ]

    implicitHeight: Theme.compactTabBarHeight
    color: Theme.panelSurface

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Theme.borderHairline
    }

    RowLayout {
        anchors.fill: parent
        spacing: 2

        Repeater {
            model: root.tabs

            delegate: Rectangle {
                id: compactTab

                required property var modelData
                required property int index

                readonly property bool isCurrent: root.activeTabIndex === compactTab.index

                Layout.fillWidth: true
                Layout.fillHeight: true
                color: compactTab.isCurrent ? Theme.accentSoft : "transparent"

                Rectangle {
                    visible: compactTab.isCurrent
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 28
                    height: 2
                    radius: 1
                    color: Theme.accentPrimary
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 3

                    StudioIcon {
                        Layout.alignment: Qt.AlignHCenter
                        name: compactTab.modelData.icon
                        size: 16
                        color: compactTab.isCurrent ? Theme.accentPrimary : Theme.textSecondary
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: compactTab.modelData.label
                        color: compactTab.isCurrent ? Theme.accentPrimary : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        font.weight: compactTab.isCurrent ? Font.DemiBold : Font.Medium
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.tabSelected(compactTab.index)
                }
            }
        }
    }
}
