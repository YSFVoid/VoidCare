import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root
    required property var appController

    property string statusText: ""
    property color statusColor: "#E8D8FF"
    property string pendingMode: ""

    function applyResult(result) {
        statusText = result.message
        statusColor = result.success ? "#A2F3C8" : "#FFB9C6"
        if (result.needsRestoreOverride) {
            overrideDialog.detailText = result.restoreDetail
            overrideDialog.open()
        }
    }

    function executePending(overrideFlag) {
        if (pendingMode === "enable") {
            applyResult(appController.enableGamingBoost(true, overrideFlag))
        } else {
            applyResult(appController.revertGamingBoost(true, overrideFlag))
        }
    }

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
            Layout.preferredHeight: 190

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Label {
                    text: "Gaming Boost"
                    font.pixelSize: 22
                    font.bold: true
                    color: "#F4E7FF"
                }
                Label {
                    text: "Best-effort tweaks: High Performance power plan and safe gaming toggles."
                    color: "#EAD9FF"
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    AccentButton {
                        text: "Enable Boost"
                        onClicked: {
                            pendingMode = "enable"
                            confirmDialog.titleText = "Enable gaming boost?"
                            confirmDialog.bodyText = "VoidCare will attempt a restore point before applying changes."
                            confirmDialog.open()
                        }
                    }

                    AccentButton {
                        text: "Revert"
                        startColor: "#D07084"
                        endColor: "#A43D56"
                        onClicked: {
                            pendingMode = "revert"
                            confirmDialog.titleText = "Revert gaming boost?"
                            confirmDialog.bodyText = "VoidCare will attempt a restore point first."
                            confirmDialog.open()
                        }
                    }
                }

                Label {
                    text: "Use restore point or Revert action to back out gaming changes."
                    color: "#FFD2D8"
                    font.pixelSize: 12
                }
            }
        }

        GlassCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                Label {
                    text: "Live Logs"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#F4E7FF"
                }
                TextArea {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    readOnly: true
                    text: appController.logs.join("\n")
                    wrapMode: Text.WrapAnywhere
                    color: "#F0E3FF"
                    background: Rectangle {
                        radius: 8
                        color: "#180F26"
                        border.color: "#604191"
                        border.width: 1
                    }
                }
            }
        }
    }

    Dialog {
        id: confirmDialog
        modal: true
        property string titleText: ""
        property string bodyText: ""
        title: titleText
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: Column {
            spacing: 8
            Label {
                width: 420
                text: confirmDialog.bodyText
                wrapMode: Text.WordWrap
                color: "#F6ECFF"
            }
        }

        onAccepted: executePending(false)
    }

    Dialog {
        id: overrideDialog
        modal: true
        property string detailText: ""
        title: "Restore point failed"
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: Column {
            spacing: 8
            Label {
                width: 420
                text: "Restore point could not be created. Continue anyway?"
                wrapMode: Text.WordWrap
                color: "#FFE8ED"
            }
            Label {
                width: 420
                text: overrideDialog.detailText
                wrapMode: Text.WordWrap
                color: "#FFC8D4"
                font.pixelSize: 11
            }
        }

        onAccepted: executePending(true)
    }
}
