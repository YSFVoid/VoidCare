#include "voidcare/core/optimization_service.h"

#include <Windows.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <shellapi.h>

#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace voidcare::core {

namespace {

bool clearDirectory(const QString& directoryPath) {
    QDir dir(directoryPath);
    if (!dir.exists()) {
        return true;
    }

    bool ok = true;
    const QFileInfoList entries =
        dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System);
    for (const QFileInfo& info : entries) {
        if (info.isDir()) {
            QDir subDir(info.absoluteFilePath());
            ok = ok && subDir.removeRecursively();
        } else {
            ok = ok && QFile::remove(info.absoluteFilePath());
        }
    }
    return ok;
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

AntivirusActionResult OptimizationService::runSafeCleanup(const LogCallback& logCallback) const {
    bool ok = true;

    const QString tempPath = QDir::tempPath();
    ok = clearDirectory(tempPath) && ok;
    if (logCallback) {
        logCallback(QStringLiteral("Temp cleanup %1.").arg(ok ? QStringLiteral("completed")
                                                               : QStringLiteral("had errors")),
                    !ok);
    }

    const HRESULT recycleResult = SHEmptyRecycleBinW(nullptr, nullptr,
                                                     SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI |
                                                         SHERB_NOSOUND);
    if (recycleResult != S_OK && recycleResult != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
        ok = false;
        if (logCallback) {
            logCallback(QStringLiteral("Recycle Bin cleanup failed."), true);
        }
    } else if (logCallback) {
        logCallback(QStringLiteral("Recycle Bin cleaned."), false);
    }

    return {ok,
            ok ? QStringLiteral("Safe cleanup completed.")
               : QStringLiteral("Safe cleanup completed with warnings."),
            ok ? 0 : 1};
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
