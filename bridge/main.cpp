#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <QVariantMap>

#include <atomic>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "voidcare/ui/app_controller.h"

namespace {

QStringList jsonArrayToStringList(const QJsonValue& value) {
    QStringList out;
    const QJsonArray array = value.toArray();
    out.reserve(array.size());
    for (const QJsonValue& item : array) {
        out.push_back(item.toString());
    }
    return out;
}

class BridgeServer final : public QObject {
public:
    explicit BridgeServer(QObject* parent = nullptr)
        : QObject(parent) {
        connect(&m_controller, &voidcare::ui::AppController::logAppended, this, [this](const QString& line, const bool isError) {
            QJsonObject payload;
            payload.insert(QStringLiteral("line"), line);
            payload.insert(QStringLiteral("isError"), isError);
            writeEvent(QStringLiteral("log"), payload);
        });
        connect(&m_controller, &voidcare::ui::AppController::stateChanged, this, [this](const QString& scope) {
            QJsonObject payload;
            payload.insert(QStringLiteral("scope"), scope);
            if (scope != QStringLiteral("logs")) {
                payload.insert(QStringLiteral("state"), QJsonValue::fromVariant(m_controller.snapshotState()));
            } else {
                payload.insert(QStringLiteral("logCount"), m_controller.logs().size());
            }
            writeEvent(QStringLiteral("state"), payload);
        });
    }

    ~BridgeServer() override {
        stopReader();
    }

    void start() {
        writeEvent(QStringLiteral("state"),
                   QJsonObject{
                       {QStringLiteral("scope"), QStringLiteral("bootstrap")},
                       {QStringLiteral("state"), QJsonValue::fromVariant(m_controller.snapshotState())},
                   });

        m_readerThread = std::thread([this]() {
            std::string line;
            while (!m_stopRequested.load() && std::getline(std::cin, line)) {
                const QString requestLine = QString::fromUtf8(line.c_str(), static_cast<int>(line.size()));
                QMetaObject::invokeMethod(
                    this,
                    [this, requestLine]() {
                        handleIncomingLine(requestLine);
                    },
                    Qt::QueuedConnection);
            }
            QMetaObject::invokeMethod(qApp, []() {
                QCoreApplication::quit();
            });
        });
    }

private:
    void stopReader() {
        m_stopRequested.store(true);
        if (m_readerThread.joinable()) {
            m_readerThread.join();
        }
    }

    void writeJsonLine(const QJsonObject& object) {
        const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
        std::lock_guard<std::mutex> lock(m_writeMutex);
        std::cout.write(payload.constData(), payload.size());
        std::cout.flush();
    }

    void writeEvent(const QString& eventName, const QJsonObject& payload) {
        QJsonObject event;
        event.insert(QStringLiteral("type"), QStringLiteral("event"));
        event.insert(QStringLiteral("event"), eventName);
        event.insert(QStringLiteral("payload"), payload);
        writeJsonLine(event);
    }

    QJsonObject makeResponse(const QString& id,
                             const bool ok,
                             const QJsonValue& result,
                             const QString& errorCode,
                             const QString& errorMessage) const {
        QJsonObject response;
        response.insert(QStringLiteral("type"), QStringLiteral("response"));
        response.insert(QStringLiteral("id"), id);
        response.insert(QStringLiteral("ok"), ok);
        response.insert(QStringLiteral("result"), result);
        if (ok) {
            response.insert(QStringLiteral("error"), QJsonValue());
        } else {
            QJsonObject error;
            error.insert(QStringLiteral("code"), errorCode);
            error.insert(QStringLiteral("message"), errorMessage);
            response.insert(QStringLiteral("error"), error);
        }
        return response;
    }

    QJsonObject dispatchRequest(const QString& requestId, const QString& method, const QJsonObject& args) {
        if (method == QStringLiteral("ping")) {
            return makeResponse(requestId, true, QJsonObject{{QStringLiteral("message"), QStringLiteral("pong")}}, {}, {});
        }
        if (method == QStringLiteral("get_initial_snapshot")) {
            return makeResponse(requestId, true, QJsonValue::fromVariant(m_controller.getInitialSnapshot()), {}, {});
        }
        if (method == QStringLiteral("get_state")) {
            return makeResponse(requestId, true, QJsonValue::fromVariant(m_controller.snapshotState()), {}, {});
        }
        if (method == QStringLiteral("navigate")) {
            m_controller.navigateTo(args.value(QStringLiteral("page")).toString());
            return makeResponse(requestId, true, QJsonValue::fromVariant(m_controller.snapshotState()), {}, {});
        }
        if (method == QStringLiteral("set_discord_enabled")) {
            m_controller.setDiscordEnabled(args.value(QStringLiteral("enabled")).toBool());
            return makeResponse(requestId, true, QJsonObject{{QStringLiteral("success"), true}}, {}, {});
        }
        if (method == QStringLiteral("refresh_antivirus")) {
            m_controller.refreshAntivirus();
            return makeResponse(requestId, true, QJsonObject{{QStringLiteral("success"), true}}, {}, {});
        }
        if (method == QStringLiteral("configure_external_scanner")) {
            const bool success =
                m_controller.configureExternalScanner(args.value(QStringLiteral("executable")).toString(),
                                                     args.value(QStringLiteral("argsLine")).toString());
            return makeResponse(
                requestId,
                true,
                QJsonObject{
                    {QStringLiteral("success"), success},
                    {QStringLiteral("message"), success ? QStringLiteral("External scanner configured.")
                                                       : QStringLiteral("Failed to configure external scanner.")},
                },
                {},
                {});
        }
        if (method == QStringLiteral("run_external_scanner")) {
            return makeResponse(requestId, true, QJsonValue::fromVariant(m_controller.runExternalScannerCommand()), {}, {});
        }
        if (method == QStringLiteral("run_defender_quick_scan")) {
            return makeResponse(requestId, true, QJsonValue::fromVariant(m_controller.runDefenderQuickScan()), {}, {});
        }
        if (method == QStringLiteral("run_defender_full_scan")) {
            return makeResponse(requestId, true, QJsonValue::fromVariant(m_controller.runDefenderFullScan()), {}, {});
        }
        if (method == QStringLiteral("run_defender_custom_scan")) {
            return makeResponse(
                requestId,
                true,
                QJsonValue::fromVariant(m_controller.runDefenderCustomScan(args.value(QStringLiteral("customPath")).toString())),
                {},
                {});
        }
        if (method == QStringLiteral("run_defender_auto_remediate")) {
            return makeResponse(requestId, true, QJsonValue::fromVariant(m_controller.runDefenderAutoRemediate()), {}, {});
        }
        if (method == QStringLiteral("refresh_persistence_audit")) {
            return makeResponse(requestId, true, QJsonValue::fromVariant(m_controller.refreshPersistenceAudit()), {}, {});
        }
        if (method == QStringLiteral("disable_persistence_entry")) {
            return makeResponse(
                requestId,
                true,
                QJsonValue::fromVariant(
                    m_controller.disablePersistenceEntry(args.value(QStringLiteral("entryId")).toString(),
                                                         args.value(QStringLiteral("initialConfirmed")).toBool(true),
                                                         args.value(QStringLiteral("proceedWithoutRestorePoint")).toBool(false))),
                {},
                {});
        }
        if (method == QStringLiteral("run_quick_suspicious_scan")) {
            return makeResponse(requestId, true, QJsonValue::fromVariant(m_controller.runQuickSuspiciousScan()), {}, {});
        }
        if (method == QStringLiteral("run_full_suspicious_scan")) {
            return makeResponse(
                requestId,
                true,
                QJsonValue::fromVariant(m_controller.runFullSuspiciousScan(
                    jsonArrayToStringList(args.value(QStringLiteral("roots"))))),
                {},
                {});
        }
        if (method == QStringLiteral("quarantine_selected")) {
            return makeResponse(
                requestId,
                true,
                QJsonValue::fromVariant(
                    m_controller.quarantineSelected(jsonArrayToStringList(args.value(QStringLiteral("filePaths"))),
                                                   args.value(QStringLiteral("initialConfirmed")).toBool(true),
                                                   args.value(QStringLiteral("proceedWithoutRestorePoint")).toBool(false))),
                {},
                {});
        }
        if (method == QStringLiteral("restore_quarantined")) {
            return makeResponse(
                requestId,
                true,
                QJsonValue::fromVariant(
                    m_controller.restoreQuarantined(jsonArrayToStringList(args.value(QStringLiteral("quarantinePaths"))),
                                                    args.value(QStringLiteral("destinationOverride")).toString())),
                {},
                {});
        }
        if (method == QStringLiteral("delete_quarantined")) {
            return makeResponse(
                requestId,
                true,
                QJsonValue::fromVariant(
                    m_controller.deleteQuarantined(jsonArrayToStringList(args.value(QStringLiteral("quarantinePaths"))),
                                                   args.value(QStringLiteral("initialConfirmed")).toBool(true),
                                                   args.value(QStringLiteral("proceedWithoutRestorePoint")).toBool(false))),
                {},
                {});
        }
        if (method == QStringLiteral("run_safe_optimization")) {
            return makeResponse(
                requestId,
                true,
                QJsonValue::fromVariant(m_controller.runSafeOptimization(
                    args.value(QStringLiteral("initialConfirmed")).toBool(true),
                    args.value(QStringLiteral("proceedWithoutRestorePoint")).toBool(false))),
                {},
                {});
        }
        if (method == QStringLiteral("run_aggressive_optimization")) {
            return makeResponse(
                requestId,
                true,
                QJsonValue::fromVariant(
                    m_controller.runAggressiveOptimization(args.value(QStringLiteral("removeBloat")).toBool(false),
                                                           args.value(QStringLiteral("disableCopilot")).toBool(false),
                                                           args.value(QStringLiteral("initialConfirmed")).toBool(true),
                                                           args.value(QStringLiteral("proceedWithoutRestorePoint")).toBool(false))),
                {},
                {});
        }
        if (method == QStringLiteral("enable_gaming_boost")) {
            return makeResponse(
                requestId,
                true,
                QJsonValue::fromVariant(m_controller.enableGamingBoost(
                    args.value(QStringLiteral("initialConfirmed")).toBool(true),
                    args.value(QStringLiteral("proceedWithoutRestorePoint")).toBool(false))),
                {},
                {});
        }
        if (method == QStringLiteral("revert_gaming_boost")) {
            return makeResponse(
                requestId,
                true,
                QJsonValue::fromVariant(m_controller.revertGamingBoost(
                    args.value(QStringLiteral("initialConfirmed")).toBool(true),
                    args.value(QStringLiteral("proceedWithoutRestorePoint")).toBool(false))),
                {},
                {});
        }
        if (method == QStringLiteral("refresh_installed_apps")) {
            return makeResponse(requestId, true, QJsonValue::fromVariant(m_controller.refreshInstalledApps()), {}, {});
        }
        if (method == QStringLiteral("open_apps_settings")) {
            return makeResponse(requestId,
                                true,
                                QJsonObject{{QStringLiteral("success"), m_controller.openAppsSettings()}},
                                {},
                                {});
        }
        if (method == QStringLiteral("open_programs_features")) {
            return makeResponse(requestId,
                                true,
                                QJsonObject{{QStringLiteral("success"), m_controller.openProgramsAndFeatures()}},
                                {},
                                {});
        }
        if (method == QStringLiteral("refresh_health_report")) {
            return makeResponse(requestId, true, QJsonValue::fromVariant(m_controller.refreshHealthReport()), {}, {});
        }
        if (method == QStringLiteral("clear_logs")) {
            m_controller.clearLogs();
            return makeResponse(requestId, true, QJsonObject{{QStringLiteral("success"), true}}, {}, {});
        }

        return makeResponse(requestId, false, QJsonValue(), QStringLiteral("unknown_method"),
                            QStringLiteral("Unsupported method: %1").arg(method));
    }

    void handleIncomingLine(const QString& line) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            writeJsonLine(makeResponse(QString(), false, QJsonValue(), QStringLiteral("bad_json"),
                                       QStringLiteral("Malformed JSON request.")));
            return;
        }

        const QJsonObject request = doc.object();
        const QString type = request.value(QStringLiteral("type")).toString();
        const QString requestId = request.value(QStringLiteral("id")).toString();
        if (type != QStringLiteral("request")) {
            writeJsonLine(makeResponse(requestId, false, QJsonValue(), QStringLiteral("bad_type"),
                                       QStringLiteral("Request type must be 'request'.")));
            return;
        }

        const QString method = request.value(QStringLiteral("method")).toString();
        if (method.isEmpty()) {
            writeJsonLine(makeResponse(requestId, false, QJsonValue(), QStringLiteral("bad_method"),
                                       QStringLiteral("Method is required.")));
            return;
        }

        const QJsonObject args = request.value(QStringLiteral("args")).toObject();
        writeJsonLine(dispatchRequest(requestId, method, args));
    }

    voidcare::ui::AppController m_controller;
    std::thread m_readerThread;
    std::atomic_bool m_stopRequested = false;
    std::mutex m_writeMutex;
};

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    BridgeServer server;
    QMetaObject::invokeMethod(&server, [&server]() {
        server.start();
    });
    return app.exec();
}
