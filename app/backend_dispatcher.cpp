#include "backend_dispatcher.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QStringList>
#include <QVariantList>

#include "voidcare/ui/app_controller.h"

namespace voidcare::app {

namespace {

QStringList toStringList(const QVariant& value) {
    if (value.canConvert<QStringList>()) {
        return value.toStringList();
    }

    QStringList list;
    for (const QVariant& item : value.toList()) {
        list.push_back(item.toString());
    }
    return list;
}

bool argBool(const QVariantMap& args, const QString& key, const bool defaultValue) {
    const auto it = args.find(key);
    if (it == args.end() || !it->isValid() || it->isNull()) {
        return defaultValue;
    }
    return it->toBool();
}

QVariantMap makeSimpleResult(const bool success, const QString& message, const int exitCode = 0) {
    QVariantMap map;
    map.insert(QStringLiteral("success"), success);
    map.insert(QStringLiteral("message"), message);
    map.insert(QStringLiteral("exitCode"), exitCode);
    map.insert(QStringLiteral("needsRestoreOverride"), false);
    map.insert(QStringLiteral("restoreDetail"), QString());
    return map;
}

}  // namespace

class BackendWorker final : public QObject {
    Q_OBJECT

public:
    explicit BackendWorker(QObject* parent = nullptr)
        : QObject(parent) {}

signals:
    void initialized(const QVariantMap& snapshot);
    void snapshotUpdated(const QString& scope, const QVariantMap& snapshot);
    void actionCompleted(quint64 requestId,
                         const QString& method,
                         const QVariantMap& result,
                         const QVariantMap& snapshot);
    void logLine(const QString& line, bool isError);
    void fatalError(const QString& message);

public slots:
    void initialize() {
        if (m_controller != nullptr) {
            return;
        }

        m_controller = std::make_unique<voidcare::ui::AppController>();
        connect(m_controller.get(),
                &voidcare::ui::AppController::logAppended,
                this,
                &BackendWorker::logLine,
                Qt::DirectConnection);
        connect(m_controller.get(),
                &voidcare::ui::AppController::stateChanged,
                this,
                [this](const QString& scope) {
                    if (m_controller == nullptr) {
                        return;
                    }

                    if (scope == QStringLiteral("logs")) {
                        emit snapshotUpdated(scope, {});
                        return;
                    }

                    emit snapshotUpdated(scope, m_controller->snapshotState());
                },
                Qt::DirectConnection);

        emit initialized(m_controller->snapshotState());
    }

    void execute(const quint64 requestId, const QString& method, const QVariantMap& args) {
        if (m_controller == nullptr) {
            emit fatalError(QStringLiteral("Backend worker is not initialized."));
            return;
        }

        const QVariantMap result = dispatch(method, args);
        emit actionCompleted(requestId, method, result, m_controller->snapshotState());
    }

private:
    QVariantMap dispatch(const QString& method, const QVariantMap& args) {
        if (method == QStringLiteral("get_initial_snapshot")) {
            return m_controller->getInitialSnapshot();
        }
        if (method == QStringLiteral("get_state")) {
            QVariantMap map;
            map.insert(QStringLiteral("success"), true);
            map.insert(QStringLiteral("message"), QStringLiteral("State snapshot ready."));
            map.insert(QStringLiteral("state"), m_controller->snapshotState());
            return map;
        }
        if (method == QStringLiteral("navigate")) {
            m_controller->navigateTo(args.value(QStringLiteral("page")).toString());
            return makeSimpleResult(true, QStringLiteral("Navigated."));
        }
        if (method == QStringLiteral("set_discord_enabled")) {
            m_controller->setDiscordEnabled(argBool(args, QStringLiteral("enabled"), true));
            return makeSimpleResult(true, QStringLiteral("Discord RPC state updated."));
        }
        if (method == QStringLiteral("refresh_antivirus")) {
            m_controller->refreshAntivirus();
            return makeSimpleResult(true, QStringLiteral("Antivirus refreshed."));
        }
        if (method == QStringLiteral("configure_external_scanner")) {
            const bool success =
                m_controller->configureExternalScanner(args.value(QStringLiteral("executable")).toString(),
                                                       args.value(QStringLiteral("argsLine")).toString());
            return makeSimpleResult(success,
                                    success ? QStringLiteral("External scanner configured.")
                                            : QStringLiteral("Failed to configure external scanner."),
                                    success ? 0 : -1);
        }
        if (method == QStringLiteral("run_external_scanner")) {
            return m_controller->runExternalScannerCommand();
        }
        if (method == QStringLiteral("run_defender_quick_scan")) {
            return m_controller->runDefenderQuickScan();
        }
        if (method == QStringLiteral("run_defender_full_scan")) {
            return m_controller->runDefenderFullScan();
        }
        if (method == QStringLiteral("run_defender_custom_scan")) {
            return m_controller->runDefenderCustomScan(args.value(QStringLiteral("customPath")).toString());
        }
        if (method == QStringLiteral("run_defender_auto_remediate")) {
            return m_controller->runDefenderAutoRemediate();
        }
        if (method == QStringLiteral("refresh_persistence_audit")) {
            return m_controller->refreshPersistenceAudit();
        }
        if (method == QStringLiteral("disable_persistence_entry")) {
            return m_controller->disablePersistenceEntry(args.value(QStringLiteral("entryId")).toString(),
                                                         argBool(args, QStringLiteral("initialConfirmed"), false),
                                                         argBool(args,
                                                                 QStringLiteral("proceedWithoutRestorePoint"),
                                                                 false));
        }
        if (method == QStringLiteral("run_quick_suspicious_scan")) {
            return m_controller->runQuickSuspiciousScan();
        }
        if (method == QStringLiteral("run_full_suspicious_scan")) {
            return m_controller->runFullSuspiciousScan(toStringList(args.value(QStringLiteral("roots"))));
        }
        if (method == QStringLiteral("quarantine_selected")) {
            return m_controller->quarantineSelected(
                toStringList(args.value(QStringLiteral("filePaths"))),
                argBool(args, QStringLiteral("initialConfirmed"), false),
                argBool(args, QStringLiteral("proceedWithoutRestorePoint"), false));
        }
        if (method == QStringLiteral("restore_quarantined")) {
            return m_controller->restoreQuarantined(
                toStringList(args.value(QStringLiteral("quarantinePaths"))),
                args.value(QStringLiteral("destinationOverride")).toString());
        }
        if (method == QStringLiteral("delete_quarantined")) {
            return m_controller->deleteQuarantined(
                toStringList(args.value(QStringLiteral("quarantinePaths"))),
                argBool(args, QStringLiteral("initialConfirmed"), false),
                argBool(args, QStringLiteral("proceedWithoutRestorePoint"), false));
        }
        if (method == QStringLiteral("run_safe_optimization")) {
            return m_controller->runSafeOptimization(
                argBool(args, QStringLiteral("initialConfirmed"), false),
                argBool(args, QStringLiteral("proceedWithoutRestorePoint"), false));
        }
        if (method == QStringLiteral("run_aggressive_optimization")) {
            return m_controller->runAggressiveOptimization(
                argBool(args, QStringLiteral("removeBloat"), false),
                argBool(args, QStringLiteral("disableCopilot"), false),
                argBool(args, QStringLiteral("initialConfirmed"), false),
                argBool(args, QStringLiteral("proceedWithoutRestorePoint"), false));
        }
        if (method == QStringLiteral("enable_gaming_boost")) {
            return m_controller->enableGamingBoost(
                argBool(args, QStringLiteral("initialConfirmed"), false),
                argBool(args, QStringLiteral("proceedWithoutRestorePoint"), false));
        }
        if (method == QStringLiteral("revert_gaming_boost")) {
            return m_controller->revertGamingBoost(
                argBool(args, QStringLiteral("initialConfirmed"), false),
                argBool(args, QStringLiteral("proceedWithoutRestorePoint"), false));
        }
        if (method == QStringLiteral("refresh_installed_apps")) {
            return m_controller->refreshInstalledApps();
        }
        if (method == QStringLiteral("open_apps_settings")) {
            const bool success = m_controller->openAppsSettings();
            return makeSimpleResult(success,
                                    success ? QStringLiteral("Apps settings opened.")
                                            : QStringLiteral("Failed to open Apps settings."),
                                    success ? 0 : -1);
        }
        if (method == QStringLiteral("open_programs_features")) {
            const bool success = m_controller->openProgramsAndFeatures();
            return makeSimpleResult(success,
                                    success ? QStringLiteral("Programs and Features opened.")
                                            : QStringLiteral("Failed to open Programs and Features."),
                                    success ? 0 : -1);
        }
        if (method == QStringLiteral("refresh_health_report")) {
            return m_controller->refreshHealthReport();
        }
        if (method == QStringLiteral("clear_logs")) {
            m_controller->clearLogs();
            return makeSimpleResult(true, QStringLiteral("Logs cleared."));
        }

        return makeSimpleResult(false,
                                QStringLiteral("Unsupported method: %1").arg(method),
                                -1);
    }

    std::unique_ptr<voidcare::ui::AppController> m_controller;
};

struct BackendDispatcher::Impl {
    QThread workerThread;
    BackendWorker* worker = nullptr;
    std::atomic<quint64> nextRequestId = 1;
    std::atomic_bool running = false;

    std::mutex queueMutex;
    std::deque<BackendEvent> queue;

    void enqueue(BackendEvent&& event) {
        std::lock_guard<std::mutex> lock(queueMutex);
        queue.push_back(std::move(event));
    }
};

BackendDispatcher::BackendDispatcher()
    : m_impl(std::make_unique<Impl>()) {}

BackendDispatcher::~BackendDispatcher() {
    stop();
}

bool BackendDispatcher::start() {
    if (m_impl->running.load()) {
        return true;
    }

    m_impl->worker = new BackendWorker();
    m_impl->worker->moveToThread(&m_impl->workerThread);

    QObject::connect(m_impl->worker, &BackendWorker::initialized, [this](const QVariantMap& snapshotMap) {
        BackendEvent event;
        event.type = BackendEventType::Initialized;
        event.snapshot = parseSnapshot(snapshotMap);
        m_impl->enqueue(std::move(event));
    });
    QObject::connect(m_impl->worker,
                     &BackendWorker::snapshotUpdated,
                     [this](const QString& scope, const QVariantMap& snapshotMap) {
                         BackendEvent event;
                         event.type = BackendEventType::Snapshot;
                         event.scope = scope;
                         if (!snapshotMap.isEmpty()) {
                             event.snapshot = parseSnapshot(snapshotMap);
                         }
                         m_impl->enqueue(std::move(event));
                     });
    QObject::connect(
        m_impl->worker,
        &BackendWorker::actionCompleted,
        [this](const quint64 requestId,
               const QString& method,
               const QVariantMap& resultMap,
               const QVariantMap& snapshotMap) {
            BackendEvent event;
            event.type = BackendEventType::ActionResult;
            event.requestId = requestId;
            event.method = method;
            event.actionResult = parseActionResult(resultMap);
            event.snapshot = parseSnapshot(snapshotMap);
            m_impl->enqueue(std::move(event));
        });
    QObject::connect(m_impl->worker, &BackendWorker::logLine, [this](const QString& line, const bool isError) {
        BackendEvent event;
        event.type = BackendEventType::LogLine;
        event.logLine = line;
        event.logIsError = isError;
        m_impl->enqueue(std::move(event));
    });
    QObject::connect(m_impl->worker, &BackendWorker::fatalError, [this](const QString& message) {
        BackendEvent event;
        event.type = BackendEventType::FatalError;
        event.message = message;
        m_impl->enqueue(std::move(event));
    });

    m_impl->workerThread.start();
    if (!QMetaObject::invokeMethod(m_impl->worker, "initialize", Qt::QueuedConnection)) {
        BackendEvent event;
        event.type = BackendEventType::FatalError;
        event.message = QStringLiteral("Failed to initialize backend worker.");
        m_impl->enqueue(std::move(event));
        return false;
    }

    m_impl->running.store(true);
    return true;
}

void BackendDispatcher::stop() {
    if (!m_impl->running.load()) {
        return;
    }

    m_impl->running.store(false);
    if (m_impl->worker != nullptr) {
        QMetaObject::invokeMethod(m_impl->worker, "deleteLater", Qt::QueuedConnection);
    }
    m_impl->workerThread.quit();
    m_impl->workerThread.wait();
    m_impl->worker = nullptr;
}

quint64 BackendDispatcher::submit(const QString& method, const QVariantMap& args) {
    if (!m_impl->running.load() || m_impl->worker == nullptr) {
        BackendEvent event;
        event.type = BackendEventType::FatalError;
        event.message = QStringLiteral("Backend is not running.");
        m_impl->enqueue(std::move(event));
        return 0;
    }

    const quint64 requestId = m_impl->nextRequestId.fetch_add(1);
    const bool invoked = QMetaObject::invokeMethod(m_impl->worker,
                                                   "execute",
                                                   Qt::QueuedConnection,
                                                   Q_ARG(quint64, requestId),
                                                   Q_ARG(QString, method),
                                                   Q_ARG(QVariantMap, args));
    if (!invoked) {
        BackendEvent event;
        event.type = BackendEventType::FatalError;
        event.message = QStringLiteral("Failed to dispatch command: %1").arg(method);
        m_impl->enqueue(std::move(event));
        return 0;
    }

    return requestId;
}

bool BackendDispatcher::pollEvent(BackendEvent* outEvent) {
    if (outEvent == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_impl->queueMutex);
    if (m_impl->queue.empty()) {
        return false;
    }

    *outEvent = std::move(m_impl->queue.front());
    m_impl->queue.pop_front();
    return true;
}

bool BackendDispatcher::isRunning() const {
    return m_impl->running.load();
}

}  // namespace voidcare::app

#include "backend_dispatcher.moc"
