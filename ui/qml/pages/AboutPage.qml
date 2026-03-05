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
            Layout.preferredHeight: 220

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    text: "VoidCare"
                    font.pixelSize: 36
                    font.bold: true
                    color: "#FAEFFF"
                }
                Label {
                    text: "by VoidTools"
                    font.pixelSize: 16
                    color: "#D9BCFF"
                }
                Label {
                    text: appController.creditsText
                    font.pixelSize: 14
                    color: "#FFFFFF"
                    font.bold: true
                }

                Label {
                    text: "Offline-only security and optimization workspace for Windows x64."
                    font.pixelSize: 13
                    color: "#E8D9FF"
                    wrapMode: Text.WordWrap
                }
            }
        }

        GlassCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 250

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                Label {
                    text: "Platform Safety Notes"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#F2E6FF"
                }

                Label {
                    text: "- Administrator elevation is required at launch."
                    color: "#E7D8FF"
                }
                Label {
                    text: "- Suspicious file flags are heuristic and can produce false positives."
                    color: "#E7D8FF"
                    wrapMode: Text.WordWrap
                }
                Label {
                    text: "- Default action for suspicious files is quarantine, not delete."
                    color: "#E7D8FF"
                }
                Label {
                    text: "- Auto-remediation only applies to Defender-detected threats."
                    color: "#E7D8FF"
                }
                Label {
                    text: "- Discord RPC uses local IPC named pipes only."
                    color: "#E7D8FF"
                }
                Label {
                    text: appController.discordAboutStatus
                    color: "#FFD7E6"
                    font.bold: true
                }
            }
        }

        GlassCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Column {
                anchors.fill: parent
                spacing: 6
                Label {
                    text: "Build Info"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#F2E6FF"
                }
                Label {
                    text: "Target: Windows 10/11 x64"
                    color: "#E7D8FF"
                }
                Label {
                    text: "Framework: Qt 6 + QML"
                    color: "#E7D8FF"
                }
                Label {
                    text: "Session-only settings by default"
                    color: "#E7D8FF"
                }
            }
        }
    }
}
