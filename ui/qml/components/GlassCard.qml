import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    default property alias contentData: contentItem.data
    color: "#22142F"
    radius: 16
    border.width: 1
    border.color: "#6D40A7"
    opacity: 0.96
    layer.enabled: true
    layer.smooth: true

    gradient: Gradient {
        GradientStop { position: 0.0; color: "#2B1841" }
        GradientStop { position: 1.0; color: "#1B102B" }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: parent.radius - 1
        color: "transparent"
        border.color: "#442B63"
        border.width: 1
    }

    Item {
        id: contentItem
        anchors.fill: parent
        anchors.margins: 14
    }
}
