import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "pages"

ApplicationWindow {
    id: window
    width: 1480
    height: 920
    minimumWidth: 1080
    minimumHeight: 720
    visible: true
    title: "VoidCare"
    color: "#130E1E"

    property var pageNames: ["Dashboard", "Security", "Optimize", "Gaming", "Apps", "About"]

    function indexForPage(name) {
        var idx = pageNames.indexOf(name)
        return idx >= 0 ? idx : 0
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#201330" }
            GradientStop { position: 0.4; color: "#140E22" }
            GradientStop { position: 1.0; color: "#0E0A16" }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        width: parent.width * 0.45
        height: parent.height * 0.45
        radius: width / 2
        color: "#4F2A8E"
        opacity: 0.22
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 16

        Rectangle {
            id: navPanel
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            radius: 20
            color: "#241735"
            border.width: 1
            border.color: "#6A42A1"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Label {
                    text: "VoidCare"
                    font.pixelSize: 28
                    font.bold: true
                    color: "#F5E7FF"
                }

                Label {
                    text: "by VoidTools"
                    color: "#B99DE0"
                    font.pixelSize: 13
                }

                Item { Layout.preferredHeight: 8 }

                Repeater {
                    model: window.pageNames
                    delegate: Button {
                        required property string modelData
                        Layout.fillWidth: true
                        text: modelData
                        font.bold: true
                        onClicked: appController.navigateTo(modelData)
                        background: Rectangle {
                            radius: 10
                            color: appController.currentPage === modelData ? "#7741C0" : "#2A1B3C"
                            border.color: appController.currentPage === modelData ? "#E1C8FF" : "#6D4C99"
                            border.width: 1
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 14
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    height: 34
                    radius: 17
                    color: appController.discordEnabled ? "#2A7C52" : "#6D2538"
                    border.color: "#B7E7CF"
                    border.width: 1
                    Label {
                        anchors.centerIn: parent
                        text: appController.discordChipText
                        color: "white"
                        font.bold: true
                        font.pixelSize: 12
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                radius: 16
                color: "#26183A"
                border.color: "#6B3FA9"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16

                    Label {
                        text: appController.currentPage
                        font.pixelSize: 30
                        font.bold: true
                        color: "#FAEEFF"
                    }

                    Item { Layout.fillWidth: true }

                    Switch {
                        text: "Enable Discord Rich Presence"
                        checked: appController.discordEnabled
                        onToggled: appController.discordEnabled = checked
                        contentItem: Text {
                            text: parent.text
                            color: "#F6E9FF"
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: parent.indicator.width + parent.spacing
                        }
                    }
                }
            }

            StackLayout {
                id: pageStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.indexForPage(appController.currentPage)
                opacity: 1.0

                onCurrentIndexChanged: {
                    opacity = 0.0
                    pageFade.restart()
                }

                NumberAnimation {
                    id: pageFade
                    target: pageStack
                    property: "opacity"
                    from: 0.0
                    to: 1.0
                    duration: 220
                }

                DashboardPage {
                    appController: appController
                }

                SecurityPage {
                    appController: appController
                }

                OptimizePage {
                    appController: appController
                }

                GamingPage {
                    appController: appController
                }

                AppsPage {
                    appController: appController
                }

                AboutPage {
                    appController: appController
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: 10
                color: "#231533"
                border.color: "#5F3E8E"
                border.width: 1
                Label {
                    anchors.centerIn: parent
                    text: appController.footerText
                    color: "#E8D8FF"
                    font.pixelSize: 12
                }
            }
        }
    }

    Rectangle {
        id: splash
        anchors.fill: parent
        color: "#B0000000"
        visible: true
        opacity: visible ? 1 : 0
        z: 100

        Behavior on opacity { NumberAnimation { duration: 450 } }

        Column {
            anchors.centerIn: parent
            spacing: 8

            Label {
                text: "VoidCare"
                font.pixelSize: 54
                font.bold: true
                color: "#FFFFFF"
                horizontalAlignment: Text.AlignHCenter
            }
            Label {
                text: "VoidTools"
                font.pixelSize: 20
                color: "#D8BFFF"
                horizontalAlignment: Text.AlignHCenter
            }
            Label {
                text: appController.creditsText
                font.pixelSize: 14
                color: "#F0E6FF"
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Timer {
            interval: 1800
            running: true
            repeat: false
            onTriggered: {
                splash.opacity = 0
                splash.visible = false
            }
        }
    }
}
