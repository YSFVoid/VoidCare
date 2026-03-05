#pragma once

#include <QObject>
#include <QVector>

#include <atomic>

#include "voidcare/core/process_runner.h"
#include "voidcare/core/types.h"

namespace voidcare::core {

class SuspiciousFileScannerService : public QObject {
public:
    explicit SuspiciousFileScannerService(ProcessRunner* runner, QObject* parent = nullptr);

    QVector<SuspiciousFileRecord> scanQuick(const QVector<PersistenceEntry>& persistenceEntries,
                                            const LogCallback& logCallback = {},
                                            std::atomic_bool* cancelToken = nullptr) const;

    QVector<SuspiciousFileRecord> scanFull(const QStringList& roots,
                                           const QVector<PersistenceEntry>& persistenceEntries,
                                           const LogCallback& logCallback = {},
                                           std::atomic_bool* cancelToken = nullptr) const;

    AntivirusActionResult quarantineSelected(const QVector<SuspiciousFileRecord>& records,
                                             QVector<QuarantineManifestEntry>* manifestOut,
                                             QString* quarantineFolderOut) const;

    AntivirusActionResult restoreFromManifest(const QVector<QuarantineManifestEntry>& entries,
                                              const QString& destinationOverride = QString()) const;

    AntivirusActionResult deletePermanentlyFromQuarantine(const QVector<QuarantineManifestEntry>& entries,
                                                          QString* summaryOut = nullptr) const;

    static int calculateHeuristicScore(const SuspiciousFileRecord& baseRecord,
                                       bool userWritable,
                                       bool hasDoubleExtension,
                                       bool entropyFlag,
                                       bool persistenceLinked,
                                       QStringList* reasonsOut);

    static bool hasDoubleExtensionPattern(const QString& fileName);
    static bool hasHighEntropyName(const QString& fileName);

private:
    QVector<SuspiciousFileRecord> scanInternal(const QStringList& roots,
                                               const QVector<PersistenceEntry>& persistenceEntries,
                                               const LogCallback& logCallback,
                                               std::atomic_bool* cancelToken) const;

    ProcessRunner* m_runner = nullptr;
};

QByteArray quarantineManifestToJson(const QVector<QuarantineManifestEntry>& entries);
QVector<QuarantineManifestEntry> quarantineManifestFromJson(const QByteArray& json);

}  // namespace voidcare::core
