#include "voidcare/core/antivirus_providers.h"

#include <Windows.h>

#include <QDir>
#include <QFileInfo>

#include <utility>

namespace voidcare::core {

namespace {

QString findMpCmdRunPath() {
    const QString defaultPath = QStringLiteral("C:/Program Files/Windows Defender/MpCmdRun.exe");
    if (QFileInfo::exists(defaultPath)) {
        return QDir::toNativeSeparators(defaultPath);
    }

    const QString platformRoot = qEnvironmentVariable("ProgramData") +
                                 QStringLiteral("/Microsoft/Windows Defender/Platform");
    QDir dir(platformRoot);
    if (!dir.exists()) {
        return {};
    }

    QFileInfoList versions = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (auto it = versions.crbegin(); it != versions.crend(); ++it) {
        const QString candidate = it->absoluteFilePath() + QStringLiteral("/MpCmdRun.exe");
        if (QFileInfo::exists(candidate)) {
            return QDir::toNativeSeparators(candidate);
        }
    }

    return {};
}

AntivirusActionResult fromRunResult(const ProcessRunResult& processResult,
                                    const QString& successMessage,
                                    const QString& failPrefix) {
    AntivirusActionResult result;
    result.exitCode = processResult.exitCode;
    if (processResult.success()) {
        result.success = true;
        result.message = successMessage;
        return result;
    }

    result.success = false;
    result.message = failPrefix;
    if (!processResult.errorMessage.trimmed().isEmpty()) {
        result.message += QStringLiteral(" ") + processResult.errorMessage.trimmed();
    } else if (!processResult.stdErr.trimmed().isEmpty()) {
        result.message += QStringLiteral(" ") + processResult.stdErr.trimmed();
    }
    return result;
}

}  // namespace

DefenderProvider::DefenderProvider(ProcessRunner* runner)
    : m_runner(runner) {}

QString DefenderProvider::name() const {
    return QStringLiteral("Microsoft Defender");
}

bool DefenderProvider::isAvailable() const {
    return !mpCmdRunPath().isEmpty();
}

bool DefenderProvider::canScan() const {
    return isAvailable() && m_runner != nullptr;
}

bool DefenderProvider::canRemediate() const {
    return isAvailable() && m_runner != nullptr;
}

QString DefenderProvider::status() const {
    return isAvailable() ? QStringLiteral("Defender is available.")
                         : QStringLiteral("Defender executable not found.");
}

AntivirusActionResult DefenderProvider::scanQuick(const LogCallback& logCallback) {
    return runScan({QStringLiteral("-Scan"), QStringLiteral("-ScanType"), QStringLiteral("1")},
                   logCallback);
}

AntivirusActionResult DefenderProvider::scanFull(const LogCallback& logCallback) {
    return runScan({QStringLiteral("-Scan"), QStringLiteral("-ScanType"), QStringLiteral("2")},
                   logCallback);
}

AntivirusActionResult DefenderProvider::scanCustom(const QString& targetPath,
                                                   const LogCallback& logCallback) {
    return runScan({QStringLiteral("-Scan"),
                    QStringLiteral("-ScanType"),
                    QStringLiteral("3"),
                    QStringLiteral("-File"),
                    targetPath},
                   logCallback);
}

AntivirusActionResult DefenderProvider::remediate(const LogCallback& logCallback) {
    if (!canRemediate()) {
        return {false, QStringLiteral("Defender remediation unavailable."), -1};
    }

    ProcessRunRequest request;
    request.executable = QStringLiteral("powershell.exe");
    request.arguments = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        QStringLiteral("$t=Get-MpThreatDetection; if($null -eq $t){Write-Output 'No Defender-detected threats.'; exit 3} else {$t | Remove-MpThreat; Write-Output 'Defender remediation finished.'; exit 0}"),
    };

    const ProcessRunResult runResult = m_runner->run(
        request,
        [logCallback](const QString& line, const bool isError) {
            if (logCallback) {
                logCallback(line, isError);
            }
        });

    AntivirusActionResult result;
    result.exitCode = runResult.exitCode;
    if (runResult.exitCode == 3) {
        result.success = false;
        result.message = QStringLiteral("No Defender-detected threats to remediate.");
        return result;
    }

    return fromRunResult(runResult,
                         QStringLiteral("Defender remediation command completed."),
                         QStringLiteral("Defender remediation failed."));
}

QString DefenderProvider::mpCmdRunPath() const {
    return findMpCmdRunPath();
}

AntivirusActionResult DefenderProvider::runScan(const QStringList& arguments,
                                                const LogCallback& logCallback) const {
    if (!canScan()) {
        return {false, QStringLiteral("Defender scan unavailable."), -1};
    }

    ProcessRunRequest request;
    request.executable = mpCmdRunPath();
    request.arguments = arguments;

    const ProcessRunResult runResult = m_runner->run(
        request,
        [logCallback](const QString& line, const bool isError) {
            if (logCallback) {
                logCallback(line, isError);
            }
        });

    return fromRunResult(runResult,
                         QStringLiteral("Defender scan completed."),
                         QStringLiteral("Defender scan failed."));
}

ExternalProvider::ExternalProvider(ProcessRunner* runner, QString managedByName)
    : m_runner(runner)
    , m_managedByName(std::move(managedByName)) {}

void ExternalProvider::setCommandForSession(const QString& executable, const QStringList& arguments) {
    m_executable = executable;
    m_arguments = arguments;
}

QString ExternalProvider::commandExecutable() const {
    return m_executable;
}

QStringList ExternalProvider::commandArguments() const {
    return m_arguments;
}

QString ExternalProvider::name() const {
    return QStringLiteral("External AV Runner");
}

bool ExternalProvider::isAvailable() const {
    return !m_managedByName.isEmpty();
}

bool ExternalProvider::canScan() const {
    return isAvailable() && m_runner != nullptr && !m_executable.trimmed().isEmpty();
}

bool ExternalProvider::canRemediate() const {
    return false;
}

QString ExternalProvider::status() const {
    if (!isAvailable()) {
        return QStringLiteral("No third-party antivirus detected.");
    }

    if (m_executable.isEmpty()) {
        return QStringLiteral("Managed by %1. Configure an external scanner command for this session.")
            .arg(m_managedByName);
    }

    return QStringLiteral("Managed by %1. External command ready.").arg(m_managedByName);
}

AntivirusActionResult ExternalProvider::scanQuick(const LogCallback& logCallback) {
    return runExternal(QString(), logCallback);
}

AntivirusActionResult ExternalProvider::scanFull(const LogCallback& logCallback) {
    return runExternal(QString(), logCallback);
}

AntivirusActionResult ExternalProvider::scanCustom(const QString& targetPath,
                                                   const LogCallback& logCallback) {
    return runExternal(targetPath, logCallback);
}

AntivirusActionResult ExternalProvider::remediate(const LogCallback& logCallback) {
    Q_UNUSED(logCallback)
    return {false,
            QStringLiteral("External remediation is intentionally disabled. Use your AV UI."),
            -1};
}

AntivirusActionResult ExternalProvider::runExternal(const QString& extraPath,
                                                    const LogCallback& logCallback) const {
    if (!canScan()) {
        return {false,
                QStringLiteral("External scanner command is not configured for this session."),
                -1};
    }

    ProcessRunRequest request;
    request.executable = m_executable;
    request.arguments = m_arguments;
    if (!extraPath.trimmed().isEmpty()) {
        request.arguments << extraPath;
    }

    const ProcessRunResult runResult = m_runner->run(
        request,
        [logCallback](const QString& line, const bool isError) {
            if (logCallback) {
                logCallback(line, isError);
            }
        });

    return fromRunResult(runResult,
                         QStringLiteral("External scanner command completed."),
                         QStringLiteral("External scanner command failed."));
}

QString NoneProvider::name() const {
    return QStringLiteral("No Antivirus Provider");
}

bool NoneProvider::isAvailable() const {
    return true;
}

bool NoneProvider::canScan() const {
    return false;
}

bool NoneProvider::canRemediate() const {
    return false;
}

QString NoneProvider::status() const {
    return QStringLiteral("No antivirus product detected. Audits remain available.");
}

AntivirusActionResult NoneProvider::scanQuick(const LogCallback& logCallback) {
    Q_UNUSED(logCallback)
    return {false, QStringLiteral("No antivirus provider available."), -1};
}

AntivirusActionResult NoneProvider::scanFull(const LogCallback& logCallback) {
    Q_UNUSED(logCallback)
    return {false, QStringLiteral("No antivirus provider available."), -1};
}

AntivirusActionResult NoneProvider::scanCustom(const QString& targetPath,
                                               const LogCallback& logCallback) {
    Q_UNUSED(targetPath)
    Q_UNUSED(logCallback)
    return {false, QStringLiteral("No antivirus provider available."), -1};
}

AntivirusActionResult NoneProvider::remediate(const LogCallback& logCallback) {
    Q_UNUSED(logCallback)
    return {false, QStringLiteral("Remediation disabled without Defender detection."), -1};
}

std::unique_ptr<IAntivirusProvider> makePreferredProvider(
    const QVector<AntivirusProduct>& discoveredProducts,
    ProcessRunner* runner,
    ExternalProvider** externalProviderOut) {
    auto defender = std::make_unique<DefenderProvider>(runner);
    if (defender->isAvailable()) {
        if (externalProviderOut != nullptr) {
            *externalProviderOut = nullptr;
        }
        return defender;
    }

    for (const AntivirusProduct& product : discoveredProducts) {
        if (!product.name.contains(QStringLiteral("Defender"), Qt::CaseInsensitive)) {
            auto external = std::make_unique<ExternalProvider>(runner, product.name);
            if (externalProviderOut != nullptr) {
                *externalProviderOut = external.get();
            }
            return external;
        }
    }

    if (externalProviderOut != nullptr) {
        *externalProviderOut = nullptr;
    }
    return std::make_unique<NoneProvider>();
}

}  // namespace voidcare::core
