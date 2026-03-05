import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root
    required property var appController

    property string statusText: ""
    property color statusColor: "#E8D8FF"

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            text: statusText
            color: statusColor
            visible: statusText.length > 0
            font.pixelSize: 12
        }

        GlassCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 130

            RowLayout {
                anchors.fill: parent
                spacing: 10

                Label {
                    text: "Apps Manager (Inventory + Launchers)"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#F2E7FF"
                    Layout.fillWidth: true
                }

                AccentButton {
                    text: "Refresh Apps"
                    onClicked: {
                        var result = appController.refreshInstalledApps()
                        statusText = result.message
                        statusColor = result.success ? "#A2F3C8" : "#FFB9C6"
                    }
                }
                AccentButton {
                    text: "Apps Settings"
                    onClicked: {
                        statusText = appController.openAppsSettings() ? "Opened Apps settings." : "Failed to open Apps settings."
                        statusColor = "#E8D8FF"
                    }
                }
                AccentButton {
                    text: "Programs & Features"
                    onClicked: {
                        statusText = appController.openProgramsAndFeatures() ? "Opened Programs and Features." : "Failed to open Programs and Features."
                        statusColor = "#E8D8FF"
                    }
                }
            }
        }

        GlassCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                Label {
                    text: "Installed Apps"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#F3EAFF"
                }

                ListView {
                    id: appsList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: appController.installedApps
                    clip: true
                    spacing: 6

                    delegate: Rectangle {
                        required property var modelData
                        width: appsList.width
                        height: 44
                        radius: 8
                        color: "#2C1D42"
                        border.color: "#634092"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            Label {
                                Layout.preferredWidth: 320
                                text: modelData.name
                                color: "#F5EEFF"
                                elide: Text.ElideRight
                                font.pixelSize: 12
                            }
                            Label {
                                Layout.preferredWidth: 120
                                text: modelData.version
                                color: "#DCC6FF"
                                font.pixelSize: 11
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.publisher
                                color: "#C7F0E3"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}
