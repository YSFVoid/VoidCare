#include "voidcare/core/persistence_audit_service.h"

#include <Windows.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <vector>

#include "voidcare/core/parser_utils.h"
#include "voidcare/platform/windows/signature_verifier.h"
#include "voidcare/platform/windows/windows_paths.h"

namespace voidcare::core {

namespace {

struct RegistryLocation {
    HKEY root;
    QString rootName;
    QString subKey;
};

QString entryId(const QString& prefix, const QString& token) {
    return prefix + QStringLiteral(":") + token;
}

void appendSignature(PersistenceEntry* entry) {
    if (entry == nullptr || entry->path.isEmpty()) {
        return;
    }

    const platform::windows::SignatureInfo sig = platform::windows::verifyFileSignature(entry->path);
    entry->signatureStatus = sig.status;
    entry->publisher = sig.publisher;
}

}  // namespace

PersistenceAuditService::PersistenceAuditService(ProcessRunner* runner, QObject* parent)
    : QObject(parent)
    , m_runner(runner) {}

QVector<PersistenceEntry> PersistenceAuditService::enumerate(const LogCallback& logCallback) const {
    QVector<PersistenceEntry> all;

    auto appendEntries = [&all](const QVector<PersistenceEntry>& items) {
        for (const auto& item : items) {
            all.push_back(item);
        }
    };

    appendEntries(startupFolderEntries(logCallback));
    appendEntries(registryRunEntries(logCallback));
    appendEntries(scheduledTaskEntries(logCallback));
    appendEntries(autoStartServices(logCallback));

    return all;
}

AntivirusActionResult PersistenceAuditService::disableEntry(const PersistenceEntry& entry) const {
    if (entry.sourceType == QStringLiteral("StartupFolder")) {
        QFile file(entry.path);
        if (!file.exists()) {
            return {false, QStringLiteral("Startup file not found."), -1};
        }
        const QString disabledPath = entry.path + QStringLiteral(".voidcare.disabled");
        if (QFile::exists(disabledPath)) {
            return {false, QStringLiteral("Disabled marker file already exists."), -1};
        }
        if (!file.rename(disabledPath)) {
            return {false, QStringLiteral("Failed to disable startup file."), -1};
        }
        return {true, QStringLiteral("Startup file disabled."), 0};
    }

    if (entry.sourceType == QStringLiteral("RegistryRun")) {
        const QStringList tokens = entry.rawReference.split('|');
        if (tokens.size() != 3) {
            return {false, QStringLiteral("Invalid registry entry reference."), -1};
        }

        HKEY root = nullptr;
        if (tokens[0] == QStringLiteral("HKLM")) {
            root = HKEY_LOCAL_MACHINE;
        } else if (tokens[0] == QStringLiteral("HKCU")) {
            root = HKEY_CURRENT_USER;
        }
        if (root == nullptr) {
            return {false, QStringLiteral("Unsupported registry hive."), -1};
        }

        HKEY key = nullptr;
        LONG status = RegOpenKeyExW(root, reinterpret_cast<LPCWSTR>(tokens[1].utf16()), 0, KEY_SET_VALUE, &key);
        if (status != ERROR_SUCCESS) {
            return {false, QStringLiteral("Failed to open registry key."), static_cast<int>(status)};
        }

        status = RegDeleteValueW(key, reinterpret_cast<LPCWSTR>(tokens[2].utf16()));
        RegCloseKey(key);
        if (status != ERROR_SUCCESS) {
            return {false, QStringLiteral("Failed to disable registry startup entry."), static_cast<int>(status)};
        }

        return {true, QStringLiteral("Registry startup entry disabled."), 0};
    }

    if (entry.sourceType == QStringLiteral("ScheduledTask")) {
        if (m_runner == nullptr) {
            return {false, QStringLiteral("Process runner unavailable."), -1};
        }
        ProcessRunRequest request;
        request.executable = QStringLiteral("schtasks.exe");
        request.arguments = {QStringLiteral("/change"),
                             QStringLiteral("/tn"),
                             entry.rawReference,
                             QStringLiteral("/disable")};
        const ProcessRunResult run = m_runner->run(request);
        return {run.success(),
                run.success() ? QStringLiteral("Scheduled task disabled.")
                              : QStringLiteral("Failed to disable scheduled task."),
                run.exitCode};
    }

    if (entry.sourceType == QStringLiteral("Service")) {
        if (m_runner == nullptr) {
            return {false, QStringLiteral("Process runner unavailable."), -1};
        }
        ProcessRunRequest request;
        request.executable = QStringLiteral("sc.exe");
        request.arguments = {
            QStringLiteral("config"),
            entry.rawReference,
            QStringLiteral("start="),
            QStringLiteral("demand"),
        };
        const ProcessRunResult run = m_runner->run(request);
        return {run.success(),
                run.success() ? QStringLiteral("Service set to manual start.")
                              : QStringLiteral("Failed to change service start type."),
                run.exitCode};
    }

    return {false, QStringLiteral("Unsupported persistence entry type."), -1};
}

QVector<PersistenceEntry> PersistenceAuditService::startupFolderEntries(const LogCallback& logCallback) const {
    QVector<PersistenceEntry> entries;
    for (const QString& folderPath : platform::windows::startupFolders()) {
        QDir dir(folderPath);
        if (!dir.exists()) {
            continue;
        }

        const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fileInfo : files) {
            PersistenceEntry entry;
            entry.id = entryId(QStringLiteral("startup"), fileInfo.absoluteFilePath());
            entry.sourceType = QStringLiteral("StartupFolder");
            entry.name = fileInfo.fileName();
            entry.path = QDir::fromNativeSeparators(fileInfo.absoluteFilePath());
            entry.enabled = true;
            entry.rawReference = entry.path;
            appendSignature(&entry);
            entries.push_back(entry);
        }
    }

    if (logCallback) {
        logCallback(QStringLiteral("Startup folder entries: %1").arg(entries.size()), false);
    }
    return entries;
}

QVector<PersistenceEntry> PersistenceAuditService::registryRunEntries(const LogCallback& logCallback) const {
    QVector<PersistenceEntry> entries;

    const std::array<RegistryLocation, 4> locations = {
        RegistryLocation{HKEY_CURRENT_USER, QStringLiteral("HKCU"),
                         QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Run")},
        RegistryLocation{HKEY_CURRENT_USER, QStringLiteral("HKCU"),
                         QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce")},
        RegistryLocation{HKEY_LOCAL_MACHINE, QStringLiteral("HKLM"),
                         QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Run")},
        RegistryLocation{HKEY_LOCAL_MACHINE, QStringLiteral("HKLM"),
                         QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce")},
    };

    for (const RegistryLocation& location : locations) {
        HKEY key = nullptr;
        if (RegOpenKeyExW(location.root,
                          reinterpret_cast<LPCWSTR>(location.subKey.utf16()),
                          0,
                          KEY_QUERY_VALUE,
                          &key) != ERROR_SUCCESS) {
            continue;
        }

        DWORD valueCount = 0;
        DWORD maxNameLen = 0;
        DWORD maxDataLen = 0;
        RegQueryInfoKeyW(key,
                         nullptr,
                         nullptr,
                         nullptr,
                         nullptr,
                         nullptr,
                         nullptr,
                         &valueCount,
                         &maxNameLen,
                         &maxDataLen,
                         nullptr,
                         nullptr);

        std::vector<wchar_t> valueName(maxNameLen + 2);
        std::vector<BYTE> data(maxDataLen + 2);

        for (DWORD index = 0; index < valueCount; ++index) {
            DWORD valueNameSize = maxNameLen + 1;
            DWORD dataSize = maxDataLen;
            DWORD type = 0;
            if (RegEnumValueW(key,
                              index,
                              valueName.data(),
                              &valueNameSize,
                              nullptr,
                              &type,
                              data.data(),
                              &dataSize) != ERROR_SUCCESS) {
                continue;
            }

            if (type != REG_SZ && type != REG_EXPAND_SZ) {
                continue;
            }

            const QString regName = QString::fromWCharArray(valueName.data(), static_cast<int>(valueNameSize));
            QString command = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(data.data()),
                                                      static_cast<int>(dataSize / sizeof(wchar_t)));
            command = command.trimmed();
            if (type == REG_EXPAND_SZ) {
                command = expandEnvironmentStrings(command);
            }
            if (command.isEmpty()) {
                continue;
            }

            auto [path, args] = splitExecutableAndArgs(command);

            PersistenceEntry entry;
            entry.id = entryId(QStringLiteral("reg"), location.rootName + location.subKey + regName);
            entry.sourceType = QStringLiteral("RegistryRun");
            entry.name = regName;
            entry.path = path;
            entry.args = args;
            entry.enabled = true;
            entry.rawReference = location.rootName + QStringLiteral("|") + location.subKey +
                                 QStringLiteral("|") + regName;
            appendSignature(&entry);
            entries.push_back(entry);
        }

        RegCloseKey(key);
    }

    if (logCallback) {
        logCallback(QStringLiteral("Registry startup entries: %1").arg(entries.size()), false);
    }
    return entries;
}

QVector<PersistenceEntry> PersistenceAuditService::scheduledTaskEntries(const LogCallback& logCallback) const {
    QVector<PersistenceEntry> entries;
    if (m_runner == nullptr) {
        return entries;
    }

    ProcessRunRequest request;
    request.executable = QStringLiteral("schtasks.exe");
    request.arguments = {QStringLiteral("/query"), QStringLiteral("/fo"), QStringLiteral("csv"),
                         QStringLiteral("/v")};

    ProcessRunResult result = m_runner->run(request);
    if (!result.success()) {
        return entries;
    }

    const QStringList lines = result.stdOut.split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 2) {
        return entries;
    }

    const QStringList headers = parseCsvLine(lines[0]);
    const int taskNameIndex = headers.indexOf(QStringLiteral("TaskName"));
    const int taskRunIndex = headers.indexOf(QStringLiteral("Task To Run"));
    const int statusIndex = headers.indexOf(QStringLiteral("Status"));

    if (taskNameIndex < 0 || taskRunIndex < 0) {
        return entries;
    }

    for (int i = 1; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList cells = parseCsvLine(line);
        if (cells.size() <= std::max(taskNameIndex, taskRunIndex)) {
            continue;
        }

        const QString taskName = cells[taskNameIndex].trimmed();
        const QString taskCommand = cells[taskRunIndex].trimmed();
        if (taskName.isEmpty() || taskCommand.isEmpty()) {
            continue;
        }

        auto [path, args] = splitExecutableAndArgs(taskCommand);

        PersistenceEntry entry;
        entry.id = entryId(QStringLiteral("task"), taskName);
        entry.sourceType = QStringLiteral("ScheduledTask");
        entry.name = taskName;
        entry.path = path;
        entry.args = args;
        entry.rawReference = taskName;
        entry.enabled = true;
        if (statusIndex >= 0 && cells.size() > statusIndex) {
            entry.enabled = !cells[statusIndex].contains(QStringLiteral("Disabled"), Qt::CaseInsensitive);
        }

        appendSignature(&entry);
        entries.push_back(entry);
    }

    if (logCallback) {
        logCallback(QStringLiteral("Scheduled task entries: %1").arg(entries.size()), false);
    }
    return entries;
}

QVector<PersistenceEntry> PersistenceAuditService::autoStartServices(const LogCallback& logCallback) const {
    QVector<PersistenceEntry> entries;

    HKEY servicesKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Services",
                      0,
                      KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE,
                      &servicesKey) != ERROR_SUCCESS) {
        return entries;
    }

    DWORD subKeyCount = 0;
    DWORD maxSubKeyLen = 0;
    RegQueryInfoKeyW(servicesKey,
                     nullptr,
                     nullptr,
                     nullptr,
                     &subKeyCount,
                     &maxSubKeyLen,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr);

    std::vector<wchar_t> subKeyBuffer(maxSubKeyLen + 2);

    for (DWORD i = 0; i < subKeyCount; ++i) {
        DWORD subKeyLen = maxSubKeyLen + 1;
        FILETIME ft = {};
        if (RegEnumKeyExW(servicesKey, i, subKeyBuffer.data(), &subKeyLen, nullptr, nullptr, nullptr, &ft) !=
            ERROR_SUCCESS) {
            continue;
        }

        const QString serviceName = QString::fromWCharArray(subKeyBuffer.data(), static_cast<int>(subKeyLen));
        HKEY serviceKey = nullptr;
        if (RegOpenKeyExW(servicesKey,
                          reinterpret_cast<LPCWSTR>(serviceName.utf16()),
                          0,
                          KEY_QUERY_VALUE,
                          &serviceKey) != ERROR_SUCCESS) {
            continue;
        }

        DWORD startType = 0;
        DWORD type = 0;
        DWORD startSize = sizeof(DWORD);
        if (RegQueryValueExW(serviceKey,
                             L"Start",
                             nullptr,
                             &type,
                             reinterpret_cast<LPBYTE>(&startType),
                             &startSize) != ERROR_SUCCESS ||
            type != REG_DWORD || startType != 2) {
            RegCloseKey(serviceKey);
            continue;
        }

        wchar_t imagePath[4096] = {};
        DWORD imageSize = sizeof(imagePath);
        QString image;
        if (RegQueryValueExW(serviceKey,
                             L"ImagePath",
                             nullptr,
                             &type,
                             reinterpret_cast<LPBYTE>(imagePath),
                             &imageSize) == ERROR_SUCCESS &&
            (type == REG_SZ || type == REG_EXPAND_SZ)) {
            image = QString::fromWCharArray(imagePath).trimmed();
            if (type == REG_EXPAND_SZ) {
                image = expandEnvironmentStrings(image);
            }
        }

        RegCloseKey(serviceKey);

        auto [path, args] = splitExecutableAndArgs(image);

        PersistenceEntry entry;
        entry.id = entryId(QStringLiteral("service"), serviceName);
        entry.sourceType = QStringLiteral("Service");
        entry.name = serviceName;
        entry.path = path;
        entry.args = args;
        entry.enabled = true;
        entry.rawReference = serviceName;
        appendSignature(&entry);
        entries.push_back(entry);
    }

    RegCloseKey(servicesKey);

    if (logCallback) {
        logCallback(QStringLiteral("Auto-start services: %1").arg(entries.size()), false);
    }
    return entries;
}

}  // namespace voidcare::core
