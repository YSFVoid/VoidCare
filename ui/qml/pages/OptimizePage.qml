import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root
    required property var appController

    property bool removeBloat: false
    property bool disableCopilot: false
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
        var result = { success: false, message: "No action" }
        if (pendingMode === "safe") {
            result = appController.runSafeOptimization(true, overrideFlag)
        } else if (pendingMode === "aggressive") {
            result = appController.runAggressiveOptimization(removeBloat, disableCopilot, true, overrideFlag)
        }
        applyResult(result)
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
            Layout.preferredHeight: 170

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    text: "Safe Optimization"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#F4E7FF"
                }

                Label {
                    text: "Cleans temp files and recycle bin, then refreshes health metrics."
                    color: "#EAD9FF"
                    wrapMode: Text.WordWrap
                }

                AccentButton {
                    text: "Run Safe Cleanup"
                    onClicked: {
                        pendingMode = "safe"
                        confirmDialog.titleText = "Run safe optimization?"
                        confirmDialog.bodyText = "VoidCare will attempt a restore point first."
                        confirmDialog.open()
                    }
                }
            }
        }

        GlassCard {
            Layout.fillWidth: true
            Layout.preferredHeight: 220

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    text: "Aggressive Optimization"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#F4E7FF"
                }

                CheckBox {
                    text: "Optional bloat removal checklist (unchecked by default)"
                    checked: removeBloat
                    onToggled: removeBloat = checked
                }

                CheckBox {
                    text: "Disable Copilot via policy (best effort; reversible guidance)"
                    checked: disableCopilot
                    onToggled: disableCopilot = checked
                }

                Label {
                    text: "Undo path: use system restore point or manually re-enable policies/packages."
                    wrapMode: Text.WordWrap
                    color: "#FFD2D8"
                    font.pixelSize: 12
                }

                AccentButton {
                    text: "Run Aggressive Actions"
                    startColor: "#D66582"
                    endColor: "#A33A5D"
                    onClicked: {
                        pendingMode = "aggressive"
                        confirmDialog.titleText = "Run aggressive optimization?"
                        confirmDialog.bodyText = "This may remove optional apps or modify policies. Restore point is attempted first."
                        confirmDialog.open()
                    }
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
                    text: "Live Optimization Logs"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#F4E7FF"
                }
                TextArea {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    readOnly: true
                    text: appController.logs.join("\n")
                    color: "#EFE2FF"
                    wrapMode: Text.WrapAnywhere
                    background: Rectangle {
                        radius: 8
                        color: "#190F27"
                        border.color: "#5F3A94"
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
                width: 440
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
                width: 440
                text: "Restore point creation failed. Continue anyway?"
                wrapMode: Text.WordWrap
                color: "#FFE8ED"
            }
            Label {
                width: 440
                text: overrideDialog.detailText
                wrapMode: Text.WordWrap
                color: "#FFC8D4"
                font.pixelSize: 11
            }
        }

        onAccepted: executePending(true)
    }
}
