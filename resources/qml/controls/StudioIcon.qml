import QtQuick
import QtQuick.Effects
import ".."

Item {
    id: root

    property string name: ""
    property string category: "lucide"
    property int size: 16
    property color color: Theme.textPrimary

    implicitWidth: root.size
    implicitHeight: root.size

    Image {
        id: glyph

        anchors.fill: parent
        visible: false
        source: root.name.length > 0
                ? ("qrc:/qt/qml/KMedia/UI/icons/" + root.category + "/" + root.name + ".svg")
                : ""
        sourceSize.width: Math.max(24, root.size * 2)
        sourceSize.height: Math.max(24, root.size * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }

    MultiEffect {
        anchors.fill: glyph
        source: glyph
        visible: root.name.length > 0 && glyph.status === Image.Ready
        colorization: 1.0
        colorizationColor: root.color
    }
}
