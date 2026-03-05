#include "voidcare/core/optimization_service.h"

#include <Windows.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <shellapi.h>

#include <algorithm>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace voidcare::core {

namespace {

QDateTime cutoffDate(const int days) {
    if (days <= 0) {
        return QDateTime();
    }
    return QDateTime::currentDateTimeUtc().addDays(-days);
}

SafeCleanupSummary cleanTempPath(const QString& directoryPath,
                                 const SafeCleanupOptions& options,
                                 const bool cautiousMode,
                                 const LogCallback& logCallback) {
    SafeCleanupSummary summary;
    QDir root(directoryPath);
    if (!root.exists()) {
        return summary;
    }

    const QDateTime cutoff = cutoffDate(options.olderThanDays);
    QDirIterator iterator(directoryPath,
                          QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System,
                          QDirIterator::Subdirectories);
    QSet<QString> visitedDirs;
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info(path);
        if (!info.exists()) {
            continue;
        }

        if (info.isDir()) {
            visitedDirs.insert(info.absoluteFilePath());
            continue;
        }

        summary.filesScanned += 1;
        if (cutoff.isValid() && info.lastModified().toUTC() > cutoff) {
            continue;
        }

        if (cautiousMode) {
            const QFile::Permissions perms = info.permissions();
            if ((perms & QFile::WriteOwner) == 0) {
                continue;
            }
        }

        summary.filesDeleted += 1;
        summary.bytesFreed += static_cast<quint64>(std::max<qint64>(0, info.size()));
        if (options.dryRun) {
            continue;
        }

        if (!QFile::remove(path)) {
            summary.success = false;
            summary.warnings << QStringLiteral("Failed to remove: %1").arg(path);
        }
    }

    if (!options.dryRun) {
        QList<QString> dirs = visitedDirs.values();
        std::sort(dirs.begin(), dirs.end(), [](const QString& left, const QString& right) {
            return left.size() > right.size();
        });
        for (const QString& dirPath : dirs) {
            QDir(dirPath).rmdir(QStringLiteral("."));
        }
    }

    if (logCallback) {
        logCallback(QStringLiteral("Processed %1: scanned=%2, candidates=%3")
                        .arg(directoryPath)
                        .arg(summary.filesScanned)
                        .arg(summary.filesDeleted),
                    false);
    }
    return summary;
}

QStringList topMemoryProcesses() {
    struct ProcessMem {
        QString name;
        SIZE_T bytes;
    };

    QVector<ProcessMem> processes;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {};
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            HANDLE processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                                               FALSE,
                                               entry.th32ProcessID);
            if (processHandle == nullptr) {
                continue;
            }

            PROCESS_MEMORY_COUNTERS_EX counters = {};
            if (GetProcessMemoryInfo(processHandle,
                                     reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                                     sizeof(counters))) {
                processes.push_back({QString::fromWCharArray(entry.szExeFile), counters.WorkingSetSize});
            }
            CloseHandle(processHandle);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);

    std::sort(processes.begin(), processes.end(), [](const ProcessMem& left, const ProcessMem& right) {
        return left.bytes > right.bytes;
    });

    QStringList output;
    const int limit = std::min<int>(static_cast<int>(processes.size()), 5);
    for (int i = 0; i < limit; ++i) {
        const double mb = static_cast<double>(processes[i].bytes) / (1024.0 * 1024.0);
        output << QStringLiteral("%1 (%2 MB)").arg(processes[i].name).arg(QString::number(mb, 'f', 1));
    }
    return output;
}

}  // namespace

OptimizationService::OptimizationService(ProcessRunner* runner,
                                         PersistenceAuditService* persistenceAudit,
                                         QObject* parent)
    : QObject(parent)
    , m_runner(runner)
    , m_persistenceAudit(persistenceAudit) {}

SafeCleanupSummary OptimizationService::runSafeCleanupDetailed(const SafeCleanupOptions& options,
                                                               const LogCallback& logCallback) const {
    SafeCleanupSummary total;
    QSet<QString> seenRoots;
    auto merge = [&total](const SafeCleanupSummary& part) {
        total.success = total.success && part.success;
        total.filesScanned += part.filesScanned;
        total.filesDeleted += part.filesDeleted;
        total.bytesFreed += part.bytesFreed;
        total.warnings << part.warnings;
    };

    auto addRoot = [&seenRoots](const QString& root) -> QString {
        const QString normalized = QDir::cleanPath(root).toLower();
        if (normalized.isEmpty() || seenRoots.contains(normalized)) {
            return {};
        }
        seenRoots.insert(normalized);
        return QDir::cleanPath(root);
    };

    const QString tempRoot = addRoot(QDir::tempPath());
    if (!tempRoot.isEmpty()) {
        merge(cleanTempPath(tempRoot, options, false, logCallback));
    }

    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty()) {
        const QString localTempRoot = addRoot(localAppData + QStringLiteral("/Temp"));
        if (!localTempRoot.isEmpty()) {
            merge(cleanTempPath(localTempRoot, options, false, logCallback));
        }
    }

    if (options.includeWindowsTemp) {
        const QString windowsRoot = qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows"));
        const QString windowsTempRoot = addRoot(windowsRoot + QStringLiteral("/Temp"));
        if (!windowsTempRoot.isEmpty()) {
            merge(cleanTempPath(windowsTempRoot, options, true, logCallback));
        }
    }

    if (!options.dryRun) {
        const HRESULT recycleResult = SHEmptyRecycleBinW(nullptr,
                                                         nullptr,
                                                         SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        if (recycleResult != S_OK && recycleResult != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            total.success = false;
            total.warnings << QStringLiteral("Recycle Bin cleanup failed.");
        } else if (logCallback) {
            logCallback(QStringLiteral("Recycle Bin cleaned."), false);
        }
    }

    return total;
}

AntivirusActionResult OptimizationService::runSafeCleanup(const LogCallback& logCallback) const {
    SafeCleanupOptions options;
    options.olderThanDays = 2;
    options.includeWindowsTemp = false;
    options.dryRun = false;
    const SafeCleanupSummary summary = runSafeCleanupDetailed(options, logCallback);
    return {summary.success,
            summary.success ? QStringLiteral("Safe cleanup completed.")
                            : QStringLiteral("Safe cleanup completed with warnings."),
            summary.success ? 0 : 1};
}

HealthReport OptimizationService::collectHealthReport() const {
    HealthReport report;

    ULARGE_INTEGER freeBytes = {};
    ULARGE_INTEGER totalBytes = {};
    ULARGE_INTEGER totalFree = {};
    if (GetDiskFreeSpaceExW(L"C:\\", &freeBytes, &totalBytes, &totalFree)) {
        report.freeDiskBytes = freeBytes.QuadPart;
        report.totalDiskBytes = totalBytes.QuadPart;
    }

    if (m_persistenceAudit != nullptr) {
        report.startupItemCount = m_persistenceAudit->enumerate().size();
    }

    report.heavyProcesses = topMemoryProcesses();
    return report;
}

AntivirusActionResult OptimizationService::runAggressiveActions(const bool removeBloat,
                                                                const bool disableCopilot,
                                                                const LogCallback& logCallback) const {
    if (m_runner == nullptr) {
        return {false, QStringLiteral("Process runner unavailable."), -1};
    }

    bool overallSuccess = true;
    QStringList messages;

    if (removeBloat) {
        ProcessRunRequest bloatRequest;
        bloatRequest.executable = QStringLiteral("powershell.exe");
        bloatRequest.arguments = {
            QStringLiteral("-NoProfile"),
            QStringLiteral("-ExecutionPolicy"),
            QStringLiteral("Bypass"),
            QStringLiteral("-Command"),
            QStringLiteral("$targets=@('*Xbox*','*Solitaire*','*BingNews*'); foreach($t in $targets){ Get-AppxPackage $t -ErrorAction SilentlyContinue | Remove-AppxPackage -ErrorAction SilentlyContinue }; Write-Output 'Optional bloat removal attempted.'"),
        };

        const ProcessRunResult run = m_runner->run(
            bloatRequest,
            [logCallback](const QString& line, const bool isError) {
                if (logCallback) {
                    logCallback(line, isError);
                }
            });
        overallSuccess = overallSuccess && run.success();
        messages << (run.success() ? QStringLiteral("Bloat removal attempted.")
                                   : QStringLiteral("Bloat removal completed with warnings."));
    }

    if (disableCopilot) {
        ProcessRunRequest copilotRequest;
        copilotRequest.executable = QStringLiteral("powershell.exe");
        copilotRequest.arguments = {
            QStringLiteral("-NoProfile"),
            QStringLiteral("-ExecutionPolicy"),
            QStringLiteral("Bypass"),
            QStringLiteral("-Command"),
            QStringLiteral("$paths=@('HKCU:\\Software\\Policies\\Microsoft\\Windows\\WindowsCopilot','HKLM:\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsCopilot'); foreach($p in $paths){ New-Item -Path $p -Force | Out-Null; Set-ItemProperty -Path $p -Name TurnOffWindowsCopilot -Type DWord -Value 1 }; Write-Output 'Copilot policy toggled (best effort).'"),
        };

        const ProcessRunResult run = m_runner->run(
            copilotRequest,
            [logCallback](const QString& line, const bool isError) {
                if (logCallback) {
                    logCallback(line, isError);
                }
            });
        overallSuccess = overallSuccess && run.success();
        messages << (run.success() ? QStringLiteral("Copilot disable policy applied.")
                                   : QStringLiteral("Copilot policy operation reported warnings."));
    }

    if (!removeBloat && !disableCopilot) {
        return {true, QStringLiteral("No aggressive options selected."), 0};
    }

    messages << QStringLiteral("To undo, use a restore point or reset related policies/packages manually.");
    return {overallSuccess, messages.join(QStringLiteral(" ")), overallSuccess ? 0 : 1};
}

}  // namespace voidcare::core
