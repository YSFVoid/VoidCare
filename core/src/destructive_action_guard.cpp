#include "voidcare/core/destructive_action_guard.h"

namespace voidcare::core {

DestructiveActionGuard::DestructiveActionGuard(RestorePointService* restorePointService, QObject* parent)
    : QObject(parent)
    , m_restorePointService(restorePointService) {}

GuardOutcome DestructiveActionGuard::evaluate(const QString& actionName,
                                              const bool userConfirmed,
                                              const bool proceedWhenRestoreFails) const {
    GuardOutcome outcome;
    if (!userConfirmed) {
        outcome.proceed = false;
        outcome.restorePoint.status = RestorePointStatus::SkippedByUser;
        outcome.restorePoint.message = QStringLiteral("User canceled action.");
        outcome.message = outcome.restorePoint.message;
        return outcome;
    }

    if (m_restorePointService == nullptr) {
        outcome.proceed = proceedWhenRestoreFails;
        outcome.needsRestoreOverride = !proceedWhenRestoreFails;
        outcome.restorePoint.status = RestorePointStatus::Failed;
        outcome.restorePoint.message = QStringLiteral("Restore point service unavailable.");
        outcome.message = outcome.restorePoint.message;
        return outcome;
    }

    outcome.restorePoint = m_restorePointService->createRestorePoint(actionName);
    if (outcome.restorePoint.status == RestorePointStatus::Success) {
        outcome.proceed = true;
        outcome.restorePointCreated = true;
        outcome.message = QStringLiteral("Restore point created; action allowed.");
        return outcome;
    }

    if (!proceedWhenRestoreFails) {
        outcome.proceed = false;
        outcome.needsRestoreOverride = true;
        outcome.message = QStringLiteral("Restore point failed. User override required.");
        return outcome;
    }

    outcome.proceed = true;
    outcome.needsRestoreOverride = false;
    outcome.message = QStringLiteral("Proceeding without restore point after explicit user override.");
    return outcome;
}

}  // namespace voidcare::core
