import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root
    required property var appController

    property var selectedSuspicious: []
    property var selectedQuarantine: []
    property string pendingAction: ""
    property string statusText: ""
    property color statusColor: "#EAD8FF"

    function containsPath(list, path) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i] === path) {
                return true
            }
        }
        return false
    }

    function togglePathSelection(listName, path, checked) {
        var list = listName === "s" ? selectedSuspicious : selectedQuarantine
        var next = []
        var found = false
        for (var i = 0; i < list.length; ++i) {
            if (list[i] === path) {
                found = true
                if (checked) {
                    next.push(path)
                }
            } else {
                next.push(list[i])
            }
        }
        if (checked && !found) {
            next.push(path)
        }
        if (listName === "s") {
            selectedSuspicious = next
        } else {
            selectedQuarantine = next
        }
    }

    function showActionResult(result) {
        statusText = result.message
        statusColor = result.success ? "#9AF3BF" : "#FFB7C4"
    }

    function executePending(overrideFlag) {
        var result = { success: false, message: "No action", needsRestoreOverride: false }
        if (pendingAction === "disablePersistence") {
            result = appController.disablePersistenceEntry(pendingTarget, true, overrideFlag)
        } else if (pendingAction === "quarantine") {
            result = appController.quarantineSelected(selectedSuspicious, true, overrideFlag)
            if (result.success) {
                selectedSuspicious = []
            }
        } else if (pendingAction === "deleteQuarantine") {
            result = appController.deleteQuarantined(selectedQuarantine, true, overrideFlag)
            if (result.success) {
                selectedQuarantine = []
            }
        }

        if (result.needsRestoreOverride) {
            overrideDialog.detailText = result.restoreDetail
            overrideDialog.open()
        } else {
            showActionResult(result)
            pendingAction = ""
            pendingTarget = ""
        }
    }

    property string pendingTarget: ""

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: 8
            color: "#6B283A"
            border.color: "#FF97B0"
            border.width: 1
            Label {
                anchors.centerIn: parent
                text: appController.warningBannerText
                color: "#FFEAF0"
                font.bold: true
                font.pixelSize: 12
            }
        }

        Label {
            text: statusText
            color: statusColor
            visible: statusText.length > 0
            font.pixelSize: 12
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 12

                GlassCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 210

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Label {
                            text: "Defender Scans"
                            font.pixelSize: 18
                            font.bold: true
                            color: "#F2E6FF"
                        }

                        RowLayout {
                            AccentButton {
                                text: "Quick"
                                enabled: appController.defenderScanAvailable
                                onClicked: showActionResult(appController.runDefenderQuickScan())
                            }
                            AccentButton {
                                text: "Full"
                                enabled: appController.defenderScanAvailable
                                onClicked: showActionResult(appController.runDefenderFullScan())
                            }
                            AccentButton {
                                text: "Auto-Remediate"
                                enabled: appController.defenderRemediationAvailable
                                startColor: "#C54D68"
                                endColor: "#9E314D"
                                onClicked: showActionResult(appController.runDefenderAutoRemediate())
                            }
                            AccentButton {
                                text: "Refresh AV"
                                onClicked: appController.refreshAntivirus()
                            }
                        }

                        RowLayout {
                            TextField {
                                id: customPathField
                                Layout.fillWidth: true
                                placeholderText: "Custom scan path (optional)"
                            }
                            AccentButton {
                                text: "Scan Path"
                                enabled: appController.defenderScanAvailable
                                onClicked: showActionResult(appController.runDefenderCustomScan(customPathField.text))
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#5E4386"
                            opacity: 0.6
                        }

                        RowLayout {
                            TextField {
                                id: externalExeField
                                Layout.preferredWidth: 240
                                placeholderText: "External scanner exe path"
                            }
                            TextField {
                                id: externalArgsField
                                Layout.fillWidth: true
                                placeholderText: "Arguments (session-only)"
                            }
                            AccentButton {
                                text: "Set External Cmd"
                                enabled: appController.externalScannerAvailable
                                onClicked: {
                                    if (appController.configureExternalScanner(externalExeField.text, externalArgsField.text)) {
                                        statusText = "External scanner command configured for this session."
                                        statusColor = "#9AF3BF"
                                    } else {
                                        statusText = "Failed to configure external scanner command."
                                        statusColor = "#FFB7C4"
                                    }
                                }
                            }
                            AccentButton {
                                text: "Run External Cmd"
                                enabled: appController.externalScannerAvailable
                                onClicked: showActionResult(appController.runExternalScannerCommand())
                            }
                        }
                    }
                }

                GlassCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 230

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        RowLayout {
                            Label {
                                text: "Persistence Audit"
                                font.pixelSize: 18
                                font.bold: true
                                color: "#F2E6FF"
                            }
                            Item { Layout.fillWidth: true }
                            AccentButton {
                                text: "Refresh"
                                onClicked: showActionResult(appController.refreshPersistenceAudit())
                            }
                        }

                        ListView {
                            id: persistenceView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 6
                            model: appController.persistenceEntries
                            delegate: Rectangle {
                                required property var modelData
                                width: persistenceView.width
                                height: 56
                                radius: 8
                                color: "#2D1D43"
                                border.width: 1
                                border.color: "#5D3C8A"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 8

                                    Label {
                                        Layout.preferredWidth: 120
                                        text: modelData.sourceType
                                        color: "#D6BAFF"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 1
                                        Label {
                                            width: parent.width
                                            text: modelData.name + " - " + modelData.path
                                            color: "#F3EBFF"
                                            font.pixelSize: 11
                                            elide: Text.ElideMiddle
                                        }
                                        Label {
                                            width: parent.width
                                            text: "Args: " + modelData.args + " | Publisher: " + modelData.publisher
                                            color: "#D8CCEE"
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Label {
                                        text: modelData.signatureStatus
                                        color: "#B6F0D4"
                                        font.pixelSize: 11
                                    }
                                    Button {
                                        text: "Disable"
                                        onClicked: {
                                            pendingAction = "disablePersistence"
                                            pendingTarget = modelData.id
                                            confirmDialog.titleText = "Disable persistence entry?"
                                            confirmDialog.bodyText = "VoidCare will attempt a restore point first."
                                            confirmDialog.open()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                GlassCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 300

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        RowLayout {
                            Label {
                                text: "Suspicious Files"
                                font.pixelSize: 18
                                font.bold: true
                                color: "#F2E6FF"
                            }
                            Item { Layout.fillWidth: true }
                            AccentButton {
                                text: "Quick Scan"
                                onClicked: showActionResult(appController.runQuickSuspiciousScan())
                            }
                            TextField {
                                id: fullRootsField
                                Layout.preferredWidth: 280
                                placeholderText: "Full scan roots; separated"
                            }
                            AccentButton {
                                text: "Full Scan"
                                onClicked: {
                                    var roots = fullRootsField.text.split(";")
                                    var cleaned = []
                                    for (var i = 0; i < roots.length; ++i) {
                                        var t = roots[i].trim()
                                        if (t.length > 0) {
                                            cleaned.push(t)
                                        }
                                    }
                                    showActionResult(appController.runFullSuspiciousScan(cleaned))
                                }
                            }
                        }

                        RowLayout {
                            AccentButton {
                                text: "Quarantine Selected"
                                startColor: "#D6724A"
                                endColor: "#A9492A"
                                onClicked: {
                                    pendingAction = "quarantine"
                                    confirmDialog.titleText = "Quarantine selected files?"
                                    confirmDialog.bodyText = "Files will be moved to secured quarantine."
                                    confirmDialog.open()
                                }
                            }
                            AccentButton {
                                text: "Restore Quarantined"
                                onClicked: showActionResult(appController.restoreQuarantined(selectedQuarantine, ""))
                            }
                            AccentButton {
                                text: "Delete Quarantined"
                                startColor: "#B93E53"
                                endColor: "#8F2136"
                                onClicked: {
                                    pendingAction = "deleteQuarantine"
                                    confirmDialog.titleText = "Delete quarantined files permanently?"
                                    confirmDialog.bodyText = "This cannot be undone. A restore point will be attempted first."
                                    confirmDialog.open()
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 8

                            ListView {
                                id: suspiciousView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 5
                                model: appController.suspiciousEntries
                                delegate: Rectangle {
                                    required property var modelData
                                    width: suspiciousView.width
                                    height: 66
                                    radius: 8
                                    color: "#2D1D43"
                                    border.color: "#674293"
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        spacing: 6
                                        CheckBox {
                                            checked: containsPath(selectedSuspicious, modelData.path)
                                            onToggled: togglePathSelection("s", modelData.path, checked)
                                        }
                                        Column {
                                            Layout.fillWidth: true
                                            spacing: 1
                                            Label {
                                                text: modelData.path + " | score " + modelData.score
                                                color: "#F5EEFF"
                                                font.pixelSize: 11
                                                elide: Text.ElideMiddle
                                                width: parent.width
                                            }
                                            Label {
                                                text: modelData.signatureStatus + " | " + modelData.sha256 + " | " + modelData.reasons.join(\"; \")
                                                color: "#D9CBEE"
                                                font.pixelSize: 10
                                                elide: Text.ElideMiddle
                                                width: parent.width
                                            }
                                        }
                                    }
                                }
                            }

                            ListView {
                                id: quarantineView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 5
                                model: appController.quarantineEntries
                                delegate: Rectangle {
                                    required property var modelData
                                    width: quarantineView.width
                                    height: 66
                                    radius: 8
                                    color: "#2A2036"
                                    border.color: "#5877A8"
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        spacing: 6
                                        CheckBox {
                                            checked: containsPath(selectedQuarantine, modelData.quarantinePath)
                                            onToggled: togglePathSelection("q", modelData.quarantinePath, checked)
                                        }
                                        Column {
                                            Layout.fillWidth: true
                                            spacing: 1
                                            Label {
                                                text: modelData.originalPath + " -> " + modelData.quarantinePath
                                                color: "#E4EEFF"
                                                font.pixelSize: 11
                                                elide: Text.ElideMiddle
                                                width: parent.width
                                            }
                                            Label {
                                                text: modelData.signatureStatus + " | " + modelData.sha256
                                                color: "#C6D7F7"
                                                font.pixelSize: 10
                                                elide: Text.ElideMiddle
                                                width: parent.width
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                GlassCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        RowLayout {
                            Label {
                                text: "Live Logs"
                                font.pixelSize: 18
                                font.bold: true
                                color: "#F2E6FF"
                            }
                            Item { Layout.fillWidth: true }
                            AccentButton {
                                text: "Clear"
                                onClicked: appController.clearLogs()
                            }
                        }

                        TextArea {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            readOnly: true
                            wrapMode: Text.WrapAnywhere
                            text: appController.logs.join("\n")
                            color: "#EEDFFF"
                            background: Rectangle {
                                color: "#160F23"
                                border.color: "#5B3E83"
                                border.width: 1
                                radius: 8
                            }
                        }
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
                wrapMode: Text.WordWrap
                text: confirmDialog.bodyText
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
                wrapMode: Text.WordWrap
                text: "Restore point could not be created. Continue anyway?"
                color: "#FFE8ED"
            }
            Label {
                width: 440
                wrapMode: Text.WordWrap
                text: overrideDialog.detailText
                color: "#FFC8D4"
                font.pixelSize: 11
            }
        }

        onAccepted: executePending(true)
    }
}
