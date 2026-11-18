import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import KMedia.UI
import "components"

ApplicationWindow {
    id: root

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 620
    visible: true
    title: qsTr("KMedia")
    color: Theme.windowBase

    readonly property list<string> sectionTitles: [
        qsTr("Meeting Workspace"),
        qsTr("Media Library"),
        qsTr("Playlists"),
        qsTr("Downloads"),
        qsTr("Settings")
    ]

    RowLayout {
        anchors.fill: parent
        spacing: 0

        LeftNavRail {
            id: navRail

            Layout.preferredWidth: Theme.navRailWidth
            Layout.fillHeight: true
        }

        CenterWorkspace {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sectionTitle: root.sectionTitles[navRail.activeTabIndex] ?? qsTr("Meeting Workspace")
        }

        InspectorPanel {
            Layout.preferredWidth: Theme.inspectorPanelWidth
            Layout.fillHeight: true
        }
    }

    PresentationWindow {
        id: presentationWindow
    }
}
