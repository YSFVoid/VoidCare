#include "voidcare/core/process_runner.h"

#include <Windows.h>

#include <QByteArray>
#include <QDir>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "voidcare/platform/windows/admin_utils.h"

namespace voidcare::core {

namespace {

QString quoteArg(const QString& arg) {
    if (arg.isEmpty()) {
        return QStringLiteral("\"\"");
    }

    if (!arg.contains(' ') && !arg.contains('\t') && !arg.contains('"')) {
        return arg;
    }

    QString escaped = arg;
    escaped.replace('"', QStringLiteral("\\\""));
    return QStringLiteral("\"") + escaped + QStringLiteral("\"");
}

std::wstring buildCommandLine(const ProcessRunRequest& request) {
    QStringList parts;
    parts << quoteArg(request.executable);
    for (const QString& argument : request.arguments) {
        parts << quoteArg(argument);
    }
    return parts.join(' ').toStdWString();
}

void readPipeLoop(HANDLE handle,
                  ProcessOutputStream stream,
                  QString* aggregate,
                  std::mutex* aggregateMutex,
                  const LogCallback& callback) {
    constexpr DWORD bufferSize = 4096;
    char buffer[bufferSize];
    DWORD read = 0;

    while (ReadFile(handle, buffer, bufferSize - 1, &read, nullptr) && read > 0) {
        buffer[read] = '\0';
        const QString chunk = QString::fromLocal8Bit(buffer, static_cast<int>(read));

        {
            std::lock_guard<std::mutex> lock(*aggregateMutex);
            aggregate->append(chunk);
        }

        if (callback) {
            QStringList lines = chunk.split('\n', Qt::SkipEmptyParts);
            for (QString line : lines) {
                line = line.trimmed();
                if (!line.isEmpty()) {
                    callback(line, stream == ProcessOutputStream::StdErr);
                }
            }
        }
    }
}

}  // namespace

ProcessRunner::ProcessRunner(QObject* parent)
    : QObject(parent) {}

ProcessRunResult ProcessRunner::run(const ProcessRunRequest& request,
                                    const LogCallback& logCallback,
                                    std::atomic_bool* cancelToken) const {
    ProcessRunResult result;
    if (request.executable.trimmed().isEmpty()) {
        result.errorMessage = QStringLiteral("Executable path is empty.");
        return result;
    }

    SECURITY_ATTRIBUTES securityAttributes = {};
    securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE stdoutRead = nullptr;
    HANDLE stdoutWrite = nullptr;
    HANDLE stderrRead = nullptr;
    HANDLE stderrWrite = nullptr;

    if (!CreatePipe(&stdoutRead, &stdoutWrite, &securityAttributes, 0)) {
        result.errorMessage = QStringLiteral("Failed to create stdout pipe: %1")
                                  .arg(platform::windows::formatWin32Error(GetLastError()));
        return result;
    }
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);

    if (!request.mergeStdErr) {
        if (!CreatePipe(&stderrRead, &stderrWrite, &securityAttributes, 0)) {
            CloseHandle(stdoutRead);
            CloseHandle(stdoutWrite);
            result.errorMessage = QStringLiteral("Failed to create stderr pipe: %1")
                                      .arg(platform::windows::formatWin32Error(GetLastError()));
            return result;
        }
        SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);
    } else {
        stderrWrite = stdoutWrite;
        stderrRead = nullptr;
    }

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(STARTUPINFOW);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = stdoutWrite;
    startupInfo.hStdError = stderrWrite;

    PROCESS_INFORMATION processInfo = {};
    std::wstring commandLine = buildCommandLine(request);
    const std::wstring workingDir = request.workingDirectory.isEmpty()
                                        ? QDir::currentPath().toStdWString()
                                        : request.workingDirectory.toStdWString();

    std::vector<wchar_t> commandLineBuffer(commandLine.begin(), commandLine.end());
    commandLineBuffer.push_back(L'\0');

    bool hasEnvironment = !request.environment.isEmpty();
    std::vector<wchar_t> environmentBlock;
    if (hasEnvironment) {
        for (auto it = request.environment.cbegin(); it != request.environment.cend(); ++it) {
            const QString entry = it.key() + QStringLiteral("=") + it.value();
            const std::wstring wideEntry = entry.toStdWString();
            environmentBlock.insert(environmentBlock.end(), wideEntry.begin(), wideEntry.end());
            environmentBlock.push_back(L'\0');
        }
        environmentBlock.push_back(L'\0');
    }

    const DWORD creationFlags = CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT;
    const BOOL createOk = CreateProcessW(nullptr,
                                         commandLineBuffer.data(),
                                         nullptr,
                                         nullptr,
                                         TRUE,
                                         creationFlags,
                                         hasEnvironment ? environmentBlock.data() : nullptr,
                                         workingDir.c_str(),
                                         &startupInfo,
                                         &processInfo);

    CloseHandle(stdoutWrite);
    if (!request.mergeStdErr && stderrWrite != nullptr) {
        CloseHandle(stderrWrite);
    }

    if (!createOk) {
        CloseHandle(stdoutRead);
        if (stderrRead != nullptr) {
            CloseHandle(stderrRead);
        }
        result.errorMessage = QStringLiteral("CreateProcess failed: %1")
                                  .arg(platform::windows::formatWin32Error(GetLastError()));
        return result;
    }

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job != nullptr) {
        AssignProcessToJobObject(job, processInfo.hProcess);
    }

    std::mutex aggregateMutex;
    std::thread stdoutThread(readPipeLoop,
                             stdoutRead,
                             ProcessOutputStream::StdOut,
                             &result.stdOut,
                             &aggregateMutex,
                             logCallback);

    std::thread stderrThread;
    if (stderrRead != nullptr) {
        stderrThread = std::thread(readPipeLoop,
                                   stderrRead,
                                   ProcessOutputStream::StdErr,
                                   &result.stdErr,
                                   &aggregateMutex,
                                   logCallback);
    }

    while (true) {
        const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 100);
        if (cancelToken != nullptr && cancelToken->load()) {
            result.canceled = true;
            if (job != nullptr) {
                TerminateJobObject(job, 1);
            } else {
                TerminateProcess(processInfo.hProcess, 1);
            }
            break;
        }

        if (waitResult == WAIT_OBJECT_0) {
            break;
        }

        if (waitResult == WAIT_FAILED) {
            result.errorMessage = QStringLiteral("WaitForSingleObject failed: %1")
                                      .arg(platform::windows::formatWin32Error(GetLastError()));
            break;
        }
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);

    DWORD exitCode = 0;
    if (GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
        result.exitCode = static_cast<int>(exitCode);
    }

    CloseHandle(stdoutRead);
    if (stderrRead != nullptr) {
        CloseHandle(stderrRead);
    }

    if (stdoutThread.joinable()) {
        stdoutThread.join();
    }
    if (stderrThread.joinable()) {
        stderrThread.join();
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    if (job != nullptr) {
        CloseHandle(job);
    }

    return result;
}

}  // namespace voidcare::core
