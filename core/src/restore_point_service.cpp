#include "voidcare/core/restore_point_service.h"

namespace voidcare::core {

RestorePointService::RestorePointService(ProcessRunner* runner, QObject* parent)
    : QObject(parent)
    , m_runner(runner) {}

RestorePointResult RestorePointService::createRestorePoint(const QString& actionLabel) const {
    RestorePointResult restoreResult;
    if (m_runner == nullptr) {
        restoreResult.status = RestorePointStatus::Failed;
        restoreResult.message = QStringLiteral("Process runner is not available.");
        return restoreResult;
    }

    const QString description = QStringLiteral("VoidCare - %1").arg(actionLabel.left(60));
    QString escapedDescription = description;
    escapedDescription.replace('\'', QStringLiteral("''"));

    ProcessRunRequest request;
    request.executable = QStringLiteral("powershell.exe");
    request.arguments = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        QStringLiteral("Checkpoint-Computer -Description '%1' -RestorePointType 'MODIFY_SETTINGS'")
            .arg(escapedDescription),
    };

    const ProcessRunResult processResult = m_runner->run(request);
    if (processResult.success()) {
        restoreResult.status = RestorePointStatus::Success;
        restoreResult.message = QStringLiteral("Restore point created.");
        return restoreResult;
    }

    restoreResult.status = RestorePointStatus::Failed;
    restoreResult.message = QStringLiteral("Failed to create restore point.");
    restoreResult.detail = processResult.errorMessage;
    if (!processResult.stdErr.trimmed().isEmpty()) {
        restoreResult.detail += (restoreResult.detail.isEmpty() ? QString() : QStringLiteral("\n")) +
                                processResult.stdErr.trimmed();
    }
    if (restoreResult.detail.isEmpty() && !processResult.stdOut.trimmed().isEmpty()) {
        restoreResult.detail = processResult.stdOut.trimmed();
    }
    return restoreResult;
}

}  // namespace voidcare::core
