import QtQuick
import QtQuick.Controls

Button {
    id: root
    property color startColor: "#9A5DE4"
    property color endColor: "#6E3CB5"

    contentItem: Text {
        text: root.text
        font.pixelSize: 13
        font.bold: true
        color: "white"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 10
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.down ? Qt.darker(root.startColor, 1.2) : root.startColor }
            GradientStop { position: 1.0; color: root.down ? Qt.darker(root.endColor, 1.2) : root.endColor }
        }
        border.color: "#C49CFF"
        border.width: 1
        layer.enabled: true
    }
}
