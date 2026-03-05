#include "voidcare/ui/app_controller.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QProcess>
#include <QSet>

#ifndef VOIDCARE_DISCORD_CLIENT_ID
#define VOIDCARE_DISCORD_CLIENT_ID ""
#endif

namespace voidcare::ui {

namespace {

QVariantMap quarantineToVariant(const core::QuarantineManifestEntry& entry) {
    QVariantMap map;
    map.insert(QStringLiteral("originalPath"), entry.originalPath);
    map.insert(QStringLiteral("quarantinePath"), entry.quarantinePath);
    map.insert(QStringLiteral("sha256"), entry.sha256);
    map.insert(QStringLiteral("timestamp"), entry.timestamp);
    map.insert(QStringLiteral("reasons"), entry.reasons);
    map.insert(QStringLiteral("signatureStatus"), entry.signatureStatus);
    return map;
}

}  // namespace

AppController::AppController(QObject* parent)
    : QObject(parent)
    , m_runner(std::make_unique<core::ProcessRunner>())
    , m_restorePoints(std::make_unique<core::RestorePointService>(m_runner.get()))
    , m_destructiveGuard(std::make_unique<core::DestructiveActionGuard>(m_restorePoints.get()))
    , m_avDiscovery(std::make_unique<core::AvDiscoveryService>())
    , m_persistenceAudit(std::make_unique<core::PersistenceAuditService>(m_runner.get()))
    , m_suspiciousScanner(std::make_unique<core::SuspiciousFileScannerService>(m_runner.get()))
    , m_optimization(std::make_unique<core::OptimizationService>(m_runner.get(), m_persistenceAudit.get()))
    , m_gaming(std::make_unique<core::GamingBoostService>(m_runner.get()))
    , m_windowsApps(std::make_unique<core::WindowsAppsService>()) {
    const QString discordClientId = QString::fromLatin1(VOIDCARE_DISCORD_CLIENT_ID).trimmed();
    m_discordConfigured = !discordClientId.isEmpty();

    m_discord = std::make_unique<core::DiscordRpcService>(discordClientId);
    connect(m_discord.get(), &core::DiscordRpcService::enabledChanged, this, &AppController::discordStateChanged);
    connect(m_discord.get(),
            &core::DiscordRpcService::discordDetectedChanged,
            this,
            &AppController::discordStateChanged);

    m_discordHeartbeat = new QTimer(this);
    m_discordHeartbeat->setInterval(15000);
    connect(m_discordHeartbeat, &QTimer::timeout, this, [this]() {
        updateDiscordPresenceForCurrentPage();
    });
    m_discordHeartbeat->start();

    refreshAntivirus();
    refreshPersistenceAudit();
    refreshInstalledApps();
    refreshHealthReport();
    updateDiscordPresenceForCurrentPage();
}

QString AppController::currentPage() const {
    return m_currentPage;
}

QString AppController::creditsText() const {
    return QStringLiteral("Developed by Ysf (Lone Wolf Developer)");
}

QString AppController::footerText() const {
    return QStringLiteral("VoidCare by VoidTools  |  Developed by Ysf (Lone Wolf Developer)");
}

QString AppController::warningBannerText() const {
    return QStringLiteral("Suspicious ≠ confirmed malware. Review before deleting.");
}

bool AppController::discordEnabled() const {
    return m_discord != nullptr && m_discord->isEnabled();
}

QString AppController::discordChipText() const {
    if (!m_discordConfigured) {
        return QStringLiteral("Discord RPC: Off");
    }
    return discordEnabled() ? QStringLiteral("Discord RPC: On") : QStringLiteral("Discord RPC: Off");
}

QString AppController::discordAboutStatus() const {
    if (!m_discordConfigured) {
        return QStringLiteral("Discord client ID is not configured at build time.");
    }

    return (m_discord != nullptr && m_discord->discordDetected()) ? QStringLiteral("Discord detected")
                                                                   : QStringLiteral("Discord not detected");
}

QString AppController::antivirusProviderName() const {
    return m_antivirusProviderName;
}

QString AppController::antivirusStatus() const {
    return m_antivirusStatus;
}

bool AppController::defenderScanAvailable() const {
    return m_defenderScanAvailable;
}

bool AppController::defenderRemediationAvailable() const {
    return m_defenderRemediationAvailable;
}

bool AppController::externalScannerAvailable() const {
    return m_externalProvider != nullptr;
}

QVariantList AppController::persistenceEntries() const {
    return m_persistenceVariant;
}

QVariantList AppController::suspiciousEntries() const {
    return m_suspiciousVariant;
}

QVariantList AppController::quarantineEntries() const {
    return m_quarantineVariant;
}

QVariantList AppController::installedApps() const {
    return m_appsVariant;
}

QStringList AppController::logs() const {
    return m_logs;
}

QString AppController::healthSummary() const {
    return m_healthSummary;
}

QVariantMap AppController::snapshotState() const {
    QVariantMap map;
    map.insert(QStringLiteral("currentPage"), m_currentPage);
    map.insert(QStringLiteral("creditsText"), creditsText());
    map.insert(QStringLiteral("footerText"), footerText());
    map.insert(QStringLiteral("warningBannerText"), warningBannerText());
    map.insert(QStringLiteral("discordEnabled"), discordEnabled());
    map.insert(QStringLiteral("discordChipText"), discordChipText());
    map.insert(QStringLiteral("discordAboutStatus"), discordAboutStatus());
    map.insert(QStringLiteral("antivirusProviderName"), m_antivirusProviderName);
    map.insert(QStringLiteral("antivirusStatus"), m_antivirusStatus);
    map.insert(QStringLiteral("defenderScanAvailable"), m_defenderScanAvailable);
    map.insert(QStringLiteral("defenderRemediationAvailable"), m_defenderRemediationAvailable);
    map.insert(QStringLiteral("externalScannerAvailable"), externalScannerAvailable());
    map.insert(QStringLiteral("persistenceEntries"), m_persistenceVariant);
    map.insert(QStringLiteral("suspiciousEntries"), m_suspiciousVariant);
    map.insert(QStringLiteral("quarantineEntries"), m_quarantineVariant);
    map.insert(QStringLiteral("installedApps"), m_appsVariant);
    map.insert(QStringLiteral("logs"), m_logs);
    map.insert(QStringLiteral("healthSummary"), m_healthSummary);
    return map;
}

void AppController::navigateTo(const QString& pageName) {
    if (m_currentPage == pageName) {
        return;
    }

    m_currentPage = pageName;
    emit currentPageChanged();
    updateDiscordPresenceForCurrentPage();
    emit stateChanged(QStringLiteral("navigation"));
}

void AppController::refreshAntivirus() {
    QString preservedExternalExe;
    QStringList preservedExternalArgs;
    if (auto* existingExternal = dynamic_cast<core::ExternalProvider*>(m_antivirusProvider.get())) {
        preservedExternalExe = existingExternal->commandExecutable();
        preservedExternalArgs = existingExternal->commandArguments();
    }

    m_discoveredProducts = m_avDiscovery->discover();

    m_antivirusProvider = core::makePreferredProvider(m_discoveredProducts, m_runner.get(), &m_externalProvider);
    if (m_externalProvider != nullptr && !preservedExternalExe.isEmpty()) {
        m_externalProvider->setCommandForSession(preservedExternalExe, preservedExternalArgs);
    }
    m_antivirusProviderName = m_antivirusProvider->name();

    QStringList productNames;
    for (const auto& product : m_discoveredProducts) {
        productNames << QStringLiteral("%1 (%2)").arg(product.name, product.statusText);
    }

    if (productNames.isEmpty()) {
        m_antivirusStatus = QStringLiteral("No AV registered in SecurityCenter2. %1")
                                .arg(m_antivirusProvider->status());
    } else {
        m_antivirusStatus = QStringLiteral("Detected: %1. %2")
                                .arg(productNames.join(QStringLiteral(", ")),
                                     m_antivirusProvider->status());
    }

    m_defenderScanAvailable = dynamic_cast<core::DefenderProvider*>(m_antivirusProvider.get()) != nullptr &&
                              m_antivirusProvider->canScan();
    m_defenderRemediationAvailable =
        dynamic_cast<core::DefenderProvider*>(m_antivirusProvider.get()) != nullptr &&
        m_antivirusProvider->canRemediate();

    emit antivirusChanged();
    emit stateChanged(QStringLiteral("antivirus"));
}

bool AppController::configureExternalScanner(const QString& executable, const QString& argsLine) {
    if (m_externalProvider == nullptr) {
        appendLog(QStringLiteral("External provider is not active in this session."), true);
        return false;
    }

    m_externalProvider->setCommandForSession(executable, QProcess::splitCommand(argsLine));
    appendLog(QStringLiteral("External scanner command configured for this session."), false);
    m_antivirusStatus = m_antivirusProvider->status();
    emit antivirusChanged();
    return true;
}

QVariantMap AppController::runExternalScannerCommand() {
    if (m_externalProvider == nullptr) {
        return makeActionResult(false,
                                QStringLiteral("No third-party AV is active for external command mode."),
                                -1);
    }
    const auto result = m_externalProvider->scanQuick([this](const QString& line, const bool isError) {
        appendLog(line, isError);
    });
    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::runDefenderQuickScan() {
    auto* defender = dynamic_cast<core::DefenderProvider*>(m_antivirusProvider.get());
    if (defender == nullptr || !defender->canScan()) {
        return makeActionResult(false, QStringLiteral("Defender quick scan unavailable."), -1);
    }

    const auto result = defender->scanQuick([this](const QString& line, const bool isError) {
        appendLog(line, isError);
    });
    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::runDefenderFullScan() {
    auto* defender = dynamic_cast<core::DefenderProvider*>(m_antivirusProvider.get());
    if (defender == nullptr || !defender->canScan()) {
        return makeActionResult(false, QStringLiteral("Defender full scan unavailable."), -1);
    }

    const auto result = defender->scanFull([this](const QString& line, const bool isError) {
        appendLog(line, isError);
    });
    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::runDefenderCustomScan(const QString& customPath) {
    auto* defender = dynamic_cast<core::DefenderProvider*>(m_antivirusProvider.get());
    if (defender == nullptr || !defender->canScan()) {
        return makeActionResult(false, QStringLiteral("Defender custom scan unavailable."), -1);
    }

    const auto result = defender->scanCustom(customPath, [this](const QString& line, const bool isError) {
        appendLog(line, isError);
    });
    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::runDefenderAutoRemediate() {
    auto* defender = dynamic_cast<core::DefenderProvider*>(m_antivirusProvider.get());
    if (defender == nullptr || !defender->canRemediate()) {
        return makeActionResult(false, QStringLiteral("Defender auto-remediation unavailable."), -1);
    }

    const auto result = defender->remediate([this](const QString& line, const bool isError) {
        appendLog(line, isError);
    });
    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::refreshPersistenceAudit() {
    m_persistenceRaw = m_persistenceAudit->enumerate([this](const QString& line, const bool isError) {
        appendLog(line, isError);
    });
    rebuildPersistenceVariant();
    emit persistenceEntriesChanged();
    emit stateChanged(QStringLiteral("persistence"));
    return makeActionResult(true,
                            QStringLiteral("Persistence audit refreshed: %1 entries.")
                                .arg(m_persistenceRaw.size()),
                            0);
}

QVariantMap AppController::disablePersistenceEntry(const QString& entryId,
                                                   const bool initialConfirmed,
                                                   const bool proceedWithoutRestorePoint) {
    const QVariantMap guardBlock =
        guardOrBlock(QStringLiteral("Disable persistence entry"), initialConfirmed,
                     proceedWithoutRestorePoint);
    if (!guardBlock.isEmpty()) {
        return guardBlock;
    }

    const auto it = std::find_if(m_persistenceRaw.begin(), m_persistenceRaw.end(),
                                 [&entryId](const core::PersistenceEntry& entry) {
                                     return entry.id == entryId;
                                 });
    if (it == m_persistenceRaw.end()) {
        return makeActionResult(false, QStringLiteral("Selected persistence entry not found."), -1);
    }

    const auto result = m_persistenceAudit->disableEntry(*it);
    refreshPersistenceAudit();
    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::runQuickSuspiciousScan() {
    if (m_discord != nullptr) {
        m_discord->updatePresence(core::DiscordPresenceState::SuspiciousFiles);
    }
    m_suspiciousRaw = m_suspiciousScanner->scanQuick(
        m_persistenceRaw,
        [this](const QString& line, const bool isError) {
            appendLog(line, isError);
        });
    rebuildSuspiciousVariant();
    emit suspiciousEntriesChanged();
    emit stateChanged(QStringLiteral("suspicious"));
    return makeActionResult(true,
                            QStringLiteral("Quick suspicious scan completed: %1 items.")
                                .arg(m_suspiciousRaw.size()),
                            0);
}

QVariantMap AppController::runFullSuspiciousScan(const QStringList& roots) {
    if (roots.isEmpty()) {
        return makeActionResult(false, QStringLiteral("Select at least one folder for full scan."), -1);
    }

    if (m_discord != nullptr) {
        m_discord->updatePresence(core::DiscordPresenceState::SuspiciousFiles);
    }
    m_suspiciousRaw = m_suspiciousScanner->scanFull(
        roots,
        m_persistenceRaw,
        [this](const QString& line, const bool isError) {
            appendLog(line, isError);
        });
    rebuildSuspiciousVariant();
    emit suspiciousEntriesChanged();
    emit stateChanged(QStringLiteral("suspicious"));
    return makeActionResult(true,
                            QStringLiteral("Full suspicious scan completed: %1 items.")
                                .arg(m_suspiciousRaw.size()),
                            0);
}

QVariantMap AppController::quarantineSelected(const QStringList& filePaths,
                                              const bool initialConfirmed,
                                              const bool proceedWithoutRestorePoint) {
    if (m_discord != nullptr) {
        m_discord->updatePresence(core::DiscordPresenceState::SuspiciousFiles);
    }
    const QVariantMap guardBlock =
        guardOrBlock(QStringLiteral("Quarantine suspicious files"), initialConfirmed,
                     proceedWithoutRestorePoint);
    if (!guardBlock.isEmpty()) {
        return guardBlock;
    }

    const QVector<core::SuspiciousFileRecord> selected = resolveSuspiciousSelection(filePaths);
    if (selected.isEmpty()) {
        return makeActionResult(false, QStringLiteral("No suspicious files selected."), -1);
    }

    QVector<core::QuarantineManifestEntry> manifest;
    QString folder;
    const auto result = m_suspiciousScanner->quarantineSelected(selected, &manifest, &folder);
    if (!manifest.isEmpty()) {
        m_quarantineRaw += manifest;
        rebuildQuarantineVariant();
        emit quarantineEntriesChanged();

        QSet<QString> selectedNormalized;
        for (const QString& path : filePaths) {
            selectedNormalized.insert(path);
        }
        m_suspiciousRaw.erase(
            std::remove_if(m_suspiciousRaw.begin(), m_suspiciousRaw.end(), [&](const auto& record) {
                return selectedNormalized.contains(record.path);
            }),
            m_suspiciousRaw.end());
        rebuildSuspiciousVariant();
        emit suspiciousEntriesChanged();
        emit stateChanged(QStringLiteral("quarantine"));
        emit stateChanged(QStringLiteral("suspicious"));
    }

    const QString message = result.success
                                ? QStringLiteral("Files quarantined in %1").arg(folder)
                                : result.message;
    return makeActionResult(result.success, message, result.exitCode);
}

QVariantMap AppController::restoreQuarantined(const QStringList& quarantinePaths,
                                              const QString& destinationOverride) {
    if (m_discord != nullptr) {
        m_discord->updatePresence(core::DiscordPresenceState::SuspiciousFiles);
    }
    const QVector<core::QuarantineManifestEntry> selected = resolveQuarantineSelection(quarantinePaths);
    if (selected.isEmpty()) {
        return makeActionResult(false, QStringLiteral("No quarantined files selected."), -1);
    }

    const auto result = m_suspiciousScanner->restoreFromManifest(selected, destinationOverride);
    if (result.success) {
        QSet<QString> selectedSet;
        for (const QString& path : quarantinePaths) {
            selectedSet.insert(path);
        }
        m_quarantineRaw.erase(
            std::remove_if(m_quarantineRaw.begin(), m_quarantineRaw.end(), [&](const auto& entry) {
                return selectedSet.contains(entry.quarantinePath);
            }),
            m_quarantineRaw.end());
        rebuildQuarantineVariant();
        emit quarantineEntriesChanged();
        emit stateChanged(QStringLiteral("quarantine"));
    }

    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::deleteQuarantined(const QStringList& quarantinePaths,
                                             const bool initialConfirmed,
                                             const bool proceedWithoutRestorePoint) {
    if (m_discord != nullptr) {
        m_discord->updatePresence(core::DiscordPresenceState::SuspiciousFiles);
    }
    const QVariantMap guardBlock =
        guardOrBlock(QStringLiteral("Delete quarantined files permanently"), initialConfirmed,
                     proceedWithoutRestorePoint);
    if (!guardBlock.isEmpty()) {
        return guardBlock;
    }

    const QVector<core::QuarantineManifestEntry> selected = resolveQuarantineSelection(quarantinePaths);
    if (selected.isEmpty()) {
        return makeActionResult(false, QStringLiteral("No quarantined files selected."), -1);
    }

    QString summary;
    const auto result = m_suspiciousScanner->deletePermanentlyFromQuarantine(selected, &summary);
    if (result.success) {
        QSet<QString> selectedSet;
        for (const QString& path : quarantinePaths) {
            selectedSet.insert(path);
        }
        m_quarantineRaw.erase(
            std::remove_if(m_quarantineRaw.begin(), m_quarantineRaw.end(), [&](const auto& entry) {
                return selectedSet.contains(entry.quarantinePath);
            }),
            m_quarantineRaw.end());
        rebuildQuarantineVariant();
        emit quarantineEntriesChanged();
        emit stateChanged(QStringLiteral("quarantine"));
    }

    appendLog(QStringLiteral("Delete summary:\n%1").arg(summary), !result.success);
    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::runSafeOptimization(const bool initialConfirmed,
                                               const bool proceedWithoutRestorePoint) {
    if (m_discord != nullptr) {
        m_discord->updatePresence(core::DiscordPresenceState::Optimize);
    }
    const QVariantMap guardBlock =
        guardOrBlock(QStringLiteral("Run safe optimization cleanup"), initialConfirmed,
                     proceedWithoutRestorePoint);
    if (!guardBlock.isEmpty()) {
        return guardBlock;
    }

    const auto result = m_optimization->runSafeCleanup([this](const QString& line, const bool isError) {
        appendLog(line, isError);
    });
    refreshHealthReport();
    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::runAggressiveOptimization(const bool removeBloat,
                                                     const bool disableCopilot,
                                                     const bool initialConfirmed,
                                                     const bool proceedWithoutRestorePoint) {
    if (m_discord != nullptr) {
        m_discord->updatePresence(core::DiscordPresenceState::Optimize);
    }
    const QVariantMap guardBlock =
        guardOrBlock(QStringLiteral("Run aggressive optimization actions"), initialConfirmed,
                     proceedWithoutRestorePoint);
    if (!guardBlock.isEmpty()) {
        return guardBlock;
    }

    const auto result = m_optimization->runAggressiveActions(
        removeBloat,
        disableCopilot,
        [this](const QString& line, const bool isError) {
            appendLog(line, isError);
        });
    refreshHealthReport();
    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::enableGamingBoost(const bool initialConfirmed,
                                             const bool proceedWithoutRestorePoint) {
    if (m_discord != nullptr) {
        m_discord->updatePresence(core::DiscordPresenceState::Gaming);
    }
    const QVariantMap guardBlock =
        guardOrBlock(QStringLiteral("Enable gaming boost"), initialConfirmed,
                     proceedWithoutRestorePoint);
    if (!guardBlock.isEmpty()) {
        return guardBlock;
    }

    const auto result = m_gaming->enableBoost([this](const QString& line, const bool isError) {
        appendLog(line, isError);
    });
    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::revertGamingBoost(const bool initialConfirmed,
                                             const bool proceedWithoutRestorePoint) {
    if (m_discord != nullptr) {
        m_discord->updatePresence(core::DiscordPresenceState::Gaming);
    }
    const QVariantMap guardBlock =
        guardOrBlock(QStringLiteral("Revert gaming boost"), initialConfirmed,
                     proceedWithoutRestorePoint);
    if (!guardBlock.isEmpty()) {
        return guardBlock;
    }

    const auto result = m_gaming->revertBoost([this](const QString& line, const bool isError) {
        appendLog(line, isError);
    });
    return makeActionResult(result.success, result.message, result.exitCode);
}

QVariantMap AppController::refreshInstalledApps() {
    if (m_discord != nullptr) {
        m_discord->updatePresence(core::DiscordPresenceState::Apps);
    }
    m_appsRaw = m_windowsApps->enumerateInstalledApps();
    rebuildAppsVariant();
    emit installedAppsChanged();
    emit stateChanged(QStringLiteral("apps"));

    return makeActionResult(true, QStringLiteral("Installed apps refreshed: %1 items.").arg(m_appsRaw.size()), 0);
}

bool AppController::openAppsSettings() {
    return m_windowsApps->openAppsSettings();
}

bool AppController::openProgramsAndFeatures() {
    return m_windowsApps->openProgramsAndFeatures();
}

QVariantMap AppController::refreshHealthReport() {
    const core::HealthReport report = m_optimization->collectHealthReport();
    const double freeGb = static_cast<double>(report.freeDiskBytes) / (1024.0 * 1024.0 * 1024.0);
    const double totalGb = static_cast<double>(report.totalDiskBytes) / (1024.0 * 1024.0 * 1024.0);

    m_healthSummary = QStringLiteral("Disk Free: %1 GB / %2 GB | Startup Items: %3 | Heavy Processes: %4")
                          .arg(QString::number(freeGb, 'f', 1),
                               QString::number(totalGb, 'f', 1),
                               QString::number(report.startupItemCount),
                               report.heavyProcesses.join(QStringLiteral(", ")));
    emit healthSummaryChanged();
    emit stateChanged(QStringLiteral("health"));

    return makeActionResult(true, QStringLiteral("Health report updated."), 0);
}

void AppController::clearLogs() {
    m_logs.clear();
    emit logsChanged();
    emit stateChanged(QStringLiteral("logs"));
}

QVariantMap AppController::getInitialSnapshot() {
    QVariantMap map;
    map.insert(QStringLiteral("success"), true);
    map.insert(QStringLiteral("message"), QStringLiteral("Snapshot ready."));
    map.insert(QStringLiteral("state"), snapshotState());
    return map;
}

void AppController::setDiscordEnabled(const bool enabled) {
    if (m_discord == nullptr) {
        return;
    }

    m_discord->setEnabled(enabled);
    updateDiscordPresenceForCurrentPage();
    emit discordStateChanged();
    emit stateChanged(QStringLiteral("discord"));
}

QVariantMap AppController::guardOrBlock(const QString& actionName,
                                        const bool initialConfirmed,
                                        const bool proceedWithoutRestorePoint) const {
    const core::GuardOutcome guard =
        m_destructiveGuard->evaluate(actionName, initialConfirmed, proceedWithoutRestorePoint);
    if (guard.proceed) {
        return {};
    }

    return makeActionResult(false,
                            guard.restorePoint.detail.isEmpty() ? guard.message : guard.restorePoint.detail,
                            -1,
                            guard.needsRestoreOverride,
                            guard.restorePoint.detail);
}

QVariantMap AppController::makeActionResult(const bool success,
                                            const QString& message,
                                            const int exitCode,
                                            const bool needsRestoreOverride,
                                            const QString& restoreDetail) const {
    QVariantMap map;
    map.insert(QStringLiteral("success"), success);
    map.insert(QStringLiteral("message"), message);
    map.insert(QStringLiteral("exitCode"), exitCode);
    map.insert(QStringLiteral("needsRestoreOverride"), needsRestoreOverride);
    map.insert(QStringLiteral("restoreDetail"), restoreDetail);
    return map;
}

void AppController::appendLog(const QString& line, const bool isError) {
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const QString tagged = QStringLiteral("[%1] %2%3")
                               .arg(timestamp, isError ? QStringLiteral("[ERROR] ") : QString(), line);
    m_logs.push_back(tagged);
    emit logAppended(tagged, isError);
    constexpr int maxLogs = 800;
    while (m_logs.size() > maxLogs) {
        m_logs.removeFirst();
    }
    emit logsChanged();
    emit stateChanged(QStringLiteral("logs"));
}

void AppController::updateDiscordPresenceForCurrentPage() {
    if (m_discord == nullptr) {
        return;
    }

    core::DiscordPresenceState state = core::DiscordPresenceState::Dashboard;
    if (m_currentPage == QStringLiteral("Security")) {
        state = core::DiscordPresenceState::Security;
    } else if (m_currentPage == QStringLiteral("Suspicious Files")) {
        state = core::DiscordPresenceState::SuspiciousFiles;
    } else if (m_currentPage == QStringLiteral("Optimize")) {
        state = core::DiscordPresenceState::Optimize;
    } else if (m_currentPage == QStringLiteral("Gaming")) {
        state = core::DiscordPresenceState::Gaming;
    } else if (m_currentPage == QStringLiteral("Apps")) {
        state = core::DiscordPresenceState::Apps;
    }

    m_discord->updatePresence(state);
    emit discordStateChanged();
}

void AppController::rebuildPersistenceVariant() {
    m_persistenceVariant.clear();
    m_persistenceVariant.reserve(m_persistenceRaw.size());
    for (const auto& entry : m_persistenceRaw) {
        m_persistenceVariant.push_back(core::persistenceEntryToVariant(entry));
    }
}

void AppController::rebuildSuspiciousVariant() {
    m_suspiciousVariant.clear();
    m_suspiciousVariant.reserve(m_suspiciousRaw.size());
    for (const auto& entry : m_suspiciousRaw) {
        m_suspiciousVariant.push_back(core::suspiciousRecordToVariant(entry));
    }
}

void AppController::rebuildQuarantineVariant() {
    m_quarantineVariant.clear();
    m_quarantineVariant.reserve(m_quarantineRaw.size());
    for (const auto& entry : m_quarantineRaw) {
        m_quarantineVariant.push_back(quarantineToVariant(entry));
    }
}

void AppController::rebuildAppsVariant() {
    m_appsVariant.clear();
    m_appsVariant.reserve(m_appsRaw.size());
    for (const auto& entry : m_appsRaw) {
        m_appsVariant.push_back(core::installedAppToVariant(entry));
    }
}

QVector<core::SuspiciousFileRecord> AppController::resolveSuspiciousSelection(
    const QStringList& filePaths) const {
    QVector<core::SuspiciousFileRecord> selected;
    QSet<QString> pathSet;
    for (const QString& path : filePaths) {
        pathSet.insert(path);
    }

    for (const auto& record : m_suspiciousRaw) {
        if (pathSet.contains(record.path)) {
            selected.push_back(record);
        }
    }
    return selected;
}

QVector<core::QuarantineManifestEntry> AppController::resolveQuarantineSelection(
    const QStringList& quarantinePaths) const {
    QVector<core::QuarantineManifestEntry> selected;
    QSet<QString> pathSet;
    for (const QString& path : quarantinePaths) {
        pathSet.insert(path);
    }

    for (const auto& record : m_quarantineRaw) {
        if (pathSet.contains(record.quarantinePath)) {
            selected.push_back(record);
        }
    }
    return selected;
}

}  // namespace voidcare::ui
