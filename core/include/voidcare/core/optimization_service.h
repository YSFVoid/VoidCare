#pragma once

#include <QObject>

#include "voidcare/core/persistence_audit_service.h"
#include "voidcare/core/process_runner.h"
#include "voidcare/core/types.h"

namespace voidcare::core {

class OptimizationService : public QObject {
public:
    explicit OptimizationService(ProcessRunner* runner,
                                 PersistenceAuditService* persistenceAudit,
                                 QObject* parent = nullptr);

    AntivirusActionResult runSafeCleanup(const LogCallback& logCallback = {}) const;
    HealthReport collectHealthReport() const;

    AntivirusActionResult runAggressiveActions(bool removeBloat,
                                               bool disableCopilot,
                                               const LogCallback& logCallback = {}) const;

private:
    ProcessRunner* m_runner = nullptr;
    PersistenceAuditService* m_persistenceAudit = nullptr;
};

}  // namespace voidcare::core
