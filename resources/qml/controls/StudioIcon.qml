import QtQuick
import ".."

// A bundled icon drawn in any color: the icon provider recolors the SVG once and Qt caches the
// image, so no shader layer is needed per icon.
Item {
    id: root

    property string name: ""
    property string category: "lucide"
    property int size: 16
    property color color: Theme.textPrimary

    implicitWidth: root.size
    implicitHeight: root.size

    Image {
        anchors.fill: parent
        visible: root.name.length > 0
        source: root.name.length > 0
                ? ("image://icon/" + root.category + "/" + root.name + "/" + root.color.toString().slice(-6))
                : ""
        sourceSize.width: Math.max(24, root.size * 2)
        sourceSize.height: Math.max(24, root.size * 2)
        opacity: root.color.a
        fillMode: Image.PreserveAspectFit
        smooth: true
    }
}
