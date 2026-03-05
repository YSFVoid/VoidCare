import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root
    required property var appController

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        GlassCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 150

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Label {
                    text: "System Health"
                    font.pixelSize: 22
                    font.bold: true
                    color: "#F4E6FF"
                }

                Label {
                    text: appController.healthSummary
                    wrapMode: Text.WordWrap
                    color: "#E7D7FF"
                    font.pixelSize: 13
                }

                RowLayout {
                    AccentButton {
                        text: "Refresh Health"
                        onClicked: appController.refreshHealthReport()
                    }
                    AccentButton {
                        text: "Quick Suspicious Scan"
                        onClicked: appController.runQuickSuspiciousScan()
                    }
                    AccentButton {
                        text: "Refresh AV"
                        onClicked: appController.refreshAntivirus()
                    }
                    Item { Layout.fillWidth: true }
                    BusyIndicator {
                        running: true
                        implicitWidth: 26
                        implicitHeight: 26
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            GlassCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    Label {
                        text: "Antivirus"
                        font.pixelSize: 18
                        font.bold: true
                        color: "#F4E8FF"
                    }
                    Label {
                        text: appController.antivirusProviderName
                        color: "#DBC0FF"
                        font.pixelSize: 13
                    }
                    Label {
                        text: appController.antivirusStatus
                        wrapMode: Text.WordWrap
                        color: "#E9DAFF"
                        font.pixelSize: 12
                    }
                }
            }

            GlassCard {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    Label {
                        text: "Session Guardrails"
                        font.pixelSize: 18
                        font.bold: true
                        color: "#F4E8FF"
                    }
                    Label {
                        text: "Offline only, admin-only execution, no auto persistence, and quarantine-first suspicious file handling are active by design."
                        wrapMode: Text.WordWrap
                        color: "#E9DAFF"
                        font.pixelSize: 12
                    }
                    Label {
                        text: "Defender-only auto-remediation applies to Defender-detected threats only."
                        wrapMode: Text.WordWrap
                        color: "#FFBFCB"
                        font.bold: true
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
