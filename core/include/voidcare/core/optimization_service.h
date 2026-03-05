#pragma once

#include <QObject>

#include "voidcare/core/persistence_audit_service.h"
#include "voidcare/core/process_runner.h"
#include "voidcare/core/types.h"

namespace voidcare::core {

struct SafeCleanupOptions {
    int olderThanDays = 2;
    bool includeWindowsTemp = false;
    bool dryRun = false;
};

struct SafeCleanupSummary {
    bool success = true;
    quint64 filesScanned = 0;
    quint64 filesDeleted = 0;
    quint64 bytesFreed = 0;
    QStringList warnings;
};

class OptimizationService : public QObject {
public:
    explicit OptimizationService(ProcessRunner* runner,
                                 PersistenceAuditService* persistenceAudit,
                                 QObject* parent = nullptr);

    SafeCleanupSummary runSafeCleanupDetailed(const SafeCleanupOptions& options,
                                              const LogCallback& logCallback = {}) const;
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
