#pragma once

#include <QObject>
#include <QVector>

#include "voidcare/core/process_runner.h"
#include "voidcare/core/types.h"

namespace voidcare::core {

class PersistenceAuditService : public QObject {
public:
    explicit PersistenceAuditService(ProcessRunner* runner, QObject* parent = nullptr);

    QVector<PersistenceEntry> enumerate(const LogCallback& logCallback = {}) const;
    AntivirusActionResult disableEntry(const PersistenceEntry& entry) const;

private:
    QVector<PersistenceEntry> startupFolderEntries(const LogCallback& logCallback) const;
    QVector<PersistenceEntry> registryRunEntries(const LogCallback& logCallback) const;
    QVector<PersistenceEntry> scheduledTaskEntries(const LogCallback& logCallback) const;
    QVector<PersistenceEntry> autoStartServices(const LogCallback& logCallback) const;

    ProcessRunner* m_runner = nullptr;
};

}  // namespace voidcare::core
