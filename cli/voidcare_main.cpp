#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QOperatingSystemVersion>
#include <QTextStream>

#include <algorithm>
#include <iostream>
#include <memory>

#include "cli_common.h"
#include "voidcare/core/antivirus_providers.h"
#include "voidcare/core/av_discovery_service.h"
#include "voidcare/core/destructive_action_guard.h"
#include "voidcare/core/gaming_boost_service.h"
#include "voidcare/core/optimization_service.h"
#include "voidcare/core/persistence_audit_service.h"
#include "voidcare/core/process_runner.h"
#include "voidcare/core/restore_point_service.h"
#include "voidcare/core/suspicious_file_scanner_service.h"
#include "voidcare/core/types.h"
#include "voidcare/platform/windows/admin_utils.h"
#include "voidcare/platform/windows/signature_verifier.h"
#include "voidcare/platform/windows/windows_paths.h"

namespace {

using voidcare::cli::ChipKind;
using voidcare::cli::CliResult;
using voidcare::cli::GlobalOptions;

constexpr const char* kCredits = "Developed by Ysf (Lone Wolf Developer)";
constexpr const char* kOfflineNote = "Offline-only: no HTTP/web calls, downloads, or telemetry.";

struct CliContext {
    GlobalOptions options;
    bool ansi = false;

    std::unique_ptr<voidcare::core::ProcessRunner> runner;
    std::unique_ptr<voidcare::core::RestorePointService> restorePoints;
    std::unique_ptr<voidcare::core::DestructiveActionGuard> guard;
    std::unique_ptr<voidcare::core::AvDiscoveryService> avDiscovery;
    std::unique_ptr<voidcare::core::PersistenceAuditService> persistence;
    std::unique_ptr<voidcare::core::SuspiciousFileScannerService> suspicious;
    std::unique_ptr<voidcare::core::OptimizationService> optimization;
    std::unique_ptr<voidcare::core::GamingBoostService> gaming;

    QVector<voidcare::core::AntivirusProduct> avProducts;
    std::unique_ptr<voidcare::core::IAntivirusProvider> preferredProvider;
    voidcare::core::ExternalProvider* externalProvider = nullptr;
    QString sessionSummary = QStringLiteral("No operation executed in this session.");
};

CliResult makeResult(const QString& command, const bool success, const int exitCode, const QString& message) {
    CliResult result;
    result.command = command;
    result.success = success;
    result.exitCode = exitCode;
    result.message = message;
    return result;
}

void refreshProvider(CliContext* ctx) {
    if (ctx == nullptr) {
        return;
    }
    ctx->avProducts = ctx->avDiscovery->discover();
    ctx->preferredProvider = voidcare::core::makePreferredProvider(ctx->avProducts, ctx->runner.get(), &ctx->externalProvider);
}

QStringList splitRoots(const QString& raw) {
    QString normalized = raw;
    normalized.replace('\n', ';');
    normalized.replace(',', ';');
    const QStringList parts = normalized.split(';', Qt::SkipEmptyParts);
    QStringList roots;
    for (QString part : parts) {
        part = part.trimmed();
        if (!part.isEmpty()) {
            roots.push_back(part);
        }
    }
    return roots;
}

QString buildType() {
#ifdef NDEBUG
    return QStringLiteral("Release");
#else
    return QStringLiteral("Debug");
#endif
}

bool confirmPrompt(const QString& prompt, const bool autoYes) {
    if (autoYes) {
        return true;
    }

    QTextStream out(stdout);
    QTextStream in(stdin);
    out << prompt << " [y/N]: ";
    out.flush();
    const QString answer = in.readLine().trimmed().toLower();
    return answer == QStringLiteral("y") || answer == QStringLiteral("yes");
}

void printStreamLine(const bool ansi, const QString& line, const bool isError) {
    QTextStream stream(isError ? stderr : stdout);
    stream << voidcare::cli::chip(isError ? QStringLiteral("[ERR]") : QStringLiteral("[INFO]"),
                                  isError ? ChipKind::Error : ChipKind::Info,
                                  ansi)
           << " " << line << "\n";
    stream.flush();
}

void applyGuardOrFail(CliContext* ctx,
                      CliResult* result,
                      const QString& actionLabel,
                      const bool doubleConfirm = false) {
    if (ctx == nullptr || result == nullptr) {
        return;
    }

    if (!confirmPrompt(actionLabel, ctx->options.yes)) {
        *result = makeResult(result->command, false, 1, QStringLiteral("Action canceled by user."));
        return;
    }
    if (doubleConfirm && !confirmPrompt(QStringLiteral("Type confirmation again to proceed."), ctx->options.yes)) {
        *result = makeResult(result->command, false, 1, QStringLiteral("Second confirmation declined."));
        return;
    }

    const voidcare::core::GuardOutcome first = ctx->guard->evaluate(actionLabel, true, false);
    if (first.proceed) {
        return;
    }

    if (!first.needsRestoreOverride) {
        *result = makeResult(result->command, false, 1, first.message);
        return;
    }

    const QString restoreDetail = first.restorePoint.detail.isEmpty() ? first.message : first.restorePoint.detail;
    if (!confirmPrompt(QStringLiteral("Restore point failed: %1. Continue without restore point?").arg(restoreDetail),
                       ctx->options.yes)) {
        *result = makeResult(result->command, false, 1, QStringLiteral("Action canceled after restore-point failure."));
        return;
    }

    const voidcare::core::GuardOutcome second = ctx->guard->evaluate(actionLabel, true, true);
    if (!second.proceed) {
        *result = makeResult(result->command, false, 1, second.message);
    }
}

QJsonArray toJsonArray(const QStringList& values) {
    QJsonArray out;
    for (const QString& value : values) {
        out.push_back(value);
    }
    return out;
}

QVector<voidcare::core::SuspiciousFileRecord> sortedSuspicious(QVector<voidcare::core::SuspiciousFileRecord> records) {
    std::sort(records.begin(), records.end(), [](const auto& left, const auto& right) {
        if (left.score == right.score) {
            return left.path.toLower() < right.path.toLower();
        }
        return left.score > right.score;
    });
    return records;
}

CliResult commandHelp() {
    CliResult result = makeResult(QStringLiteral("help"), true, 0, QStringLiteral("VoidCare CLI command help"));
    result.humanLines = {
        QStringLiteral("VoidCare CLI (Windows x64)"),
        QStringLiteral("Build: %1 x64").arg(buildType()),
        QString::fromLatin1(kOfflineNote),
        QStringLiteral(""),
        QStringLiteral("Usage:"),
        QStringLiteral("  voidcare [--json] [--yes] [--dry-run] <command>"),
        QStringLiteral(""),
        QStringLiteral("Commands:"),
        QStringLiteral("  voidcare --help | --version | about"),
        QStringLiteral("  voidcare status"),
        QStringLiteral("  voidcare security av list"),
        QStringLiteral("  voidcare security defender status"),
        QStringLiteral("  voidcare security scan --quick | --full | --path <PATH>"),
        QStringLiteral("  voidcare security remediate"),
        QStringLiteral("  voidcare security persistence list"),
        QStringLiteral("  voidcare security persistence disable --id <ID>"),
        QStringLiteral("  voidcare security suspicious scan --quick | --full --roots \"C:\\;D:\\\""),
        QStringLiteral("  voidcare security suspicious quarantine list"),
        QStringLiteral("  voidcare security suspicious quarantine --ids 1,2,3 [--quick | --full --roots <ROOTS>]"),
        QStringLiteral("  voidcare security suspicious restore --id <ID> [--to <PATH>]"),
        QStringLiteral("  voidcare security suspicious delete --id <ID>"),
        QStringLiteral("  voidcare optimize safe [--days N] [--include-windows-temp]"),
        QStringLiteral("  voidcare optimize power --high"),
        QStringLiteral("  voidcare optimize startup report"),
        QStringLiteral(""),
        QString::fromLatin1(kCredits),
    };
    return result;
}

CliResult commandVersion() {
    CliResult result = makeResult(QStringLiteral("version"), true, 0, QStringLiteral("VoidCare CLI version"));
    result.humanLines = {
        QStringLiteral("voidcare 1.0.0"),
        QStringLiteral("Build: %1 x64").arg(buildType()),
        QString::fromLatin1(kOfflineNote),
        QString::fromLatin1(kCredits),
    };
    return result;
}

CliResult commandAbout() {
    CliResult result = makeResult(QStringLiteral("about"), true, 0, QStringLiteral("About VoidCare CLI"));
    result.humanLines = {
        QStringLiteral("VoidCare CLI by VoidTools"),
        QStringLiteral("Windows x64 offline optimization and security toolkit."),
        QString::fromLatin1(kOfflineNote),
        QString::fromLatin1(kCredits),
    };
    return result;
}

CliResult commandStatus(CliContext* ctx) {
    refreshProvider(ctx);
    const bool isAdmin = voidcare::platform::windows::isRunningAsAdmin();
    const QOperatingSystemVersion os = QOperatingSystemVersion::current();

    QJsonArray avNames;
    QVector<QStringList> avRows;
    for (const auto& product : ctx->avProducts) {
        avRows.push_back({
            product.name,
            QStringLiteral("0x%1").arg(QString::number(product.rawState, 16).toUpper()),
            product.active ? QStringLiteral("Yes") : QStringLiteral("No"),
            product.upToDate ? QStringLiteral("Yes") : QStringLiteral("No"),
            product.statusText,
        });

        QJsonObject obj;
        obj.insert(QStringLiteral("name"), product.name);
        obj.insert(QStringLiteral("productState"), static_cast<int>(product.rawState));
        obj.insert(QStringLiteral("active"), product.active);
        obj.insert(QStringLiteral("upToDate"), product.upToDate);
        obj.insert(QStringLiteral("status"), product.statusText);
        avNames.push_back(obj);
    }

    const auto* defender = dynamic_cast<voidcare::core::DefenderProvider*>(ctx->preferredProvider.get());
    CliResult result = makeResult(QStringLiteral("status"), true, 0, QStringLiteral("Status collected."));
    result.humanLines = {
        QStringLiteral("Admin: %1").arg(isAdmin ? QStringLiteral("yes") : QStringLiteral("no")),
        QStringLiteral("OS: %1.%2.%3")
            .arg(os.majorVersion())
            .arg(os.minorVersion())
            .arg(os.microVersion()),
        QStringLiteral("Defender available: %1").arg(defender != nullptr && defender->isAvailable() ? QStringLiteral("yes")
                                                                                                     : QStringLiteral("no")),
        QStringLiteral("Preferred provider: %1").arg(ctx->preferredProvider->name()),
        QStringLiteral("Last operation summary: %1").arg(ctx->sessionSummary),
        QStringLiteral(""),
        voidcare::cli::renderTable(
            {QStringLiteral("Name"), QStringLiteral("State"), QStringLiteral("Active"), QStringLiteral("UpToDate"), QStringLiteral("Status")},
            avRows),
    };
    if (avRows.isEmpty()) {
        result.warnings.push_back(QStringLiteral("No AV registered."));
    }

    QJsonObject data;
    data.insert(QStringLiteral("admin"), isAdmin);
    data.insert(QStringLiteral("osVersion"), QStringLiteral("%1.%2.%3").arg(os.majorVersion()).arg(os.minorVersion()).arg(os.microVersion()));
    data.insert(QStringLiteral("defenderAvailable"), defender != nullptr && defender->isAvailable());
    data.insert(QStringLiteral("preferredProvider"), ctx->preferredProvider->name());
    data.insert(QStringLiteral("lastOperationSummary"), ctx->sessionSummary);
    data.insert(QStringLiteral("registeredAntivirus"), avNames);
    result.data = data;
    return result;
}

CliResult commandSecurityAvList(CliContext* ctx) {
    refreshProvider(ctx);
    CliResult result = makeResult(QStringLiteral("security av list"), true, 0, QStringLiteral("AV products enumerated."));
    QVector<QStringList> rows;
    QJsonArray items;
    for (const auto& product : ctx->avProducts) {
        rows.push_back({
            product.name,
            QStringLiteral("0x%1").arg(QString::number(product.rawState, 16).toUpper()),
            product.active ? QStringLiteral("Active") : QStringLiteral("Passive"),
            product.upToDate ? QStringLiteral("UpToDate") : QStringLiteral("Stale"),
            product.statusText,
        });
        QJsonObject item;
        item.insert(QStringLiteral("name"), product.name);
        item.insert(QStringLiteral("productState"), static_cast<int>(product.rawState));
        item.insert(QStringLiteral("active"), product.active);
        item.insert(QStringLiteral("upToDate"), product.upToDate);
        item.insert(QStringLiteral("status"), product.statusText);
        items.push_back(item);
    }

    if (rows.isEmpty()) {
        result.warnings.push_back(QStringLiteral("No AV registered."));
        result.humanLines = {QStringLiteral("No AV registered.")};
    } else {
        result.humanLines = {voidcare::cli::renderTable(
            {QStringLiteral("Name"), QStringLiteral("ProductState"), QStringLiteral("Mode"), QStringLiteral("Signatures"), QStringLiteral("Decoded")},
            rows)};
    }

    QJsonObject data;
    data.insert(QStringLiteral("products"), items);
    result.data = data;
    return result;
}

CliResult commandSecurityDefenderStatus(CliContext* ctx) {
    voidcare::core::DefenderProvider defender(ctx->runner.get());
    const bool available = defender.isAvailable();

    QJsonObject data;
    data.insert(QStringLiteral("defenderCliAvailable"), available);
    data.insert(QStringLiteral("mpCmdRunPath"), defender.mpCmdRunPath());

    CliResult result = makeResult(QStringLiteral("security defender status"),
                                  true,
                                  0,
                                  available ? QStringLiteral("Defender CLI available.") : QStringLiteral("Defender CLI unavailable."));

    if (available) {
        result.humanLines.push_back(QStringLiteral("Defender CLI available: yes"));
        result.humanLines.push_back(QStringLiteral("MpCmdRun: %1").arg(defender.mpCmdRunPath()));
    } else {
        result.humanLines.push_back(QStringLiteral("Defender CLI available: no"));
    }

    voidcare::core::ProcessRunRequest request;
    request.executable = QStringLiteral("powershell.exe");
    request.arguments = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        QStringLiteral("if(Get-Command Get-MpComputerStatus -ErrorAction SilentlyContinue){Get-MpComputerStatus | Select-Object AMServiceEnabled,AntispywareEnabled,AntivirusEnabled,RealTimeProtectionEnabled | ConvertTo-Json -Compress}else{Write-Output ''}"),
    };
    const auto psResult = ctx->runner->run(request);
    const QString trimmed = psResult.stdOut.trimmed();
    data.insert(QStringLiteral("powershellAvailable"), !trimmed.isEmpty());
    data.insert(QStringLiteral("powershellRaw"), trimmed);
    if (!trimmed.isEmpty()) {
        result.humanLines.push_back(QStringLiteral("Get-MpComputerStatus available: yes"));
        result.humanLines.push_back(trimmed);
    } else {
        result.humanLines.push_back(QStringLiteral("Get-MpComputerStatus available: no"));
    }

    result.data = data;
    return result;
}

CliResult requireAdmin(const QString& commandName) {
    if (voidcare::platform::windows::isRunningAsAdmin()) {
        return makeResult(commandName, true, 0, QString());
    }
    CliResult result = makeResult(commandName, false, 2, QStringLiteral("Administrator privileges are required for this command."));
    result.warnings.push_back(QStringLiteral("Run terminal as Administrator."));
    return result;
}

CliResult commandSecurityScan(CliContext* ctx, const QStringList& args) {
    voidcare::core::DefenderProvider defender(ctx->runner.get());
    if (!defender.isAvailable()) {
        return makeResult(QStringLiteral("security scan"), false, 3, QStringLiteral("Scan unavailable (Defender not present)."));
    }

    QString mode;
    QString customPath;
    if (args.contains(QStringLiteral("--quick"))) {
        mode = QStringLiteral("quick");
    } else if (args.contains(QStringLiteral("--full"))) {
        mode = QStringLiteral("full");
    } else {
        const int pathIndex = args.indexOf(QStringLiteral("--path"));
        if (pathIndex >= 0 && pathIndex + 1 < args.size()) {
            mode = QStringLiteral("path");
            customPath = args[pathIndex + 1];
        }
    }
    if (mode.isEmpty()) {
        return makeResult(QStringLiteral("security scan"), false, 2, QStringLiteral("Specify --quick, --full, or --path <path>."));
    }

    auto callback = [ansi = ctx->ansi](const QString& line, const bool isError) {
        printStreamLine(ansi, line, isError);
    };

    voidcare::core::AntivirusActionResult scanResult;
    if (mode == QStringLiteral("quick")) {
        scanResult = defender.scanQuick(callback);
    } else if (mode == QStringLiteral("full")) {
        scanResult = defender.scanFull(callback);
    } else {
        scanResult = defender.scanCustom(customPath, callback);
    }

    CliResult result = makeResult(QStringLiteral("security scan"), scanResult.success, scanResult.exitCode, scanResult.message);
    QJsonObject data;
    data.insert(QStringLiteral("mode"), mode);
    data.insert(QStringLiteral("path"), customPath);
    data.insert(QStringLiteral("exitCode"), scanResult.exitCode);
    result.data = data;
    return result;
}

CliResult commandSecurityRemediate(CliContext* ctx) {
    CliResult admin = requireAdmin(QStringLiteral("security remediate"));
    if (!admin.success) {
        return admin;
    }

    voidcare::core::DefenderProvider defender(ctx->runner.get());
    if (!defender.isAvailable()) {
        return makeResult(QStringLiteral("security remediate"),
                          false,
                          3,
                          QStringLiteral("Remediation unavailable (Defender not present)."));
    }

    if (!confirmPrompt(QStringLiteral("Run Defender remediation for Defender-detected threats?"), ctx->options.yes)) {
        return makeResult(QStringLiteral("security remediate"), false, 1, QStringLiteral("Action canceled by user."));
    }

    auto callback = [ansi = ctx->ansi](const QString& line, const bool isError) {
        printStreamLine(ansi, line, isError);
    };
    const auto remediateResult = defender.remediate(callback);
    return makeResult(QStringLiteral("security remediate"),
                      remediateResult.success,
                      remediateResult.exitCode,
                      remediateResult.message);
}

CliResult commandPersistenceList(CliContext* ctx) {
    const QVector<voidcare::core::PersistenceEntry> entries = ctx->persistence->enumerate();
    QVector<QStringList> rows;
    rows.reserve(entries.size());
    QJsonArray items;
    for (const auto& entry : entries) {
        rows.push_back({
            entry.id,
            entry.sourceType,
            entry.name,
            entry.path,
            entry.args,
            entry.publisher,
            voidcare::platform::windows::signatureStatusToString(entry.signatureStatus),
            entry.rawReference,
        });
        items.push_back(QJsonValue::fromVariant(voidcare::core::persistenceEntryToVariant(entry)));
    }

    CliResult result = makeResult(QStringLiteral("security persistence list"), true, 0, QStringLiteral("Persistence entries listed."));
    result.humanLines = {voidcare::cli::renderTable(
        {QStringLiteral("ID"), QStringLiteral("Type"), QStringLiteral("Name"), QStringLiteral("Path"), QStringLiteral("Args"), QStringLiteral("Publisher"), QStringLiteral("Sig"), QStringLiteral("Location")},
        rows)};
    QJsonObject data;
    data.insert(QStringLiteral("entries"), items);
    result.data = data;
    return result;
}

CliResult commandPersistenceDisable(CliContext* ctx, const QStringList& args) {
    CliResult admin = requireAdmin(QStringLiteral("security persistence disable"));
    if (!admin.success) {
        return admin;
    }

    const int idIndex = args.indexOf(QStringLiteral("--id"));
    if (idIndex < 0 || idIndex + 1 >= args.size()) {
        return makeResult(QStringLiteral("security persistence disable"), false, 2, QStringLiteral("Missing required --id <ID>."));
    }
    const QString requestedId = args[idIndex + 1];

    const QVector<voidcare::core::PersistenceEntry> entries = ctx->persistence->enumerate();
    auto it = std::find_if(entries.begin(), entries.end(), [&requestedId](const auto& entry) {
        return entry.id == requestedId;
    });
    if (it == entries.end()) {
        return makeResult(QStringLiteral("security persistence disable"), false, 2, QStringLiteral("Persistence entry ID not found."));
    }

    CliResult result = makeResult(QStringLiteral("security persistence disable"), true, 0, QStringLiteral("Ready."));
    if (ctx->options.dryRun) {
        result.message = QStringLiteral("Dry-run: persistence entry would be disabled.");
        result.plannedActions.push_back(QStringLiteral("Disable persistence entry: %1 (%2)").arg(it->id, it->name));
        return result;
    }

    applyGuardOrFail(ctx, &result, QStringLiteral("Disable persistence entry %1").arg(it->name));
    if (!result.success) {
        return result;
    }

    const auto disableResult = ctx->persistence->disableEntry(*it);
    result.success = disableResult.success;
    result.exitCode = disableResult.success ? 0 : 1;
    result.message = disableResult.message;
    return result;
}

CliResult commandSuspiciousScan(CliContext* ctx, const QStringList& args) {
    const bool quick = args.contains(QStringLiteral("--quick"));
    const bool full = args.contains(QStringLiteral("--full"));
    if (!quick && !full) {
        return makeResult(QStringLiteral("security suspicious scan"),
                          false,
                          2,
                          QStringLiteral("Specify --quick or --full --roots \"C:\\;D:\\\"."));
    }

    QVector<voidcare::core::PersistenceEntry> persistenceEntries = ctx->persistence->enumerate();
    QVector<voidcare::core::SuspiciousFileRecord> records;
    auto callback = [ansi = ctx->ansi](const QString& line, const bool isError) {
        printStreamLine(ansi, line, isError);
    };

    if (quick) {
        records = ctx->suspicious->scanQuick(persistenceEntries, callback);
    } else {
        const int rootsIndex = args.indexOf(QStringLiteral("--roots"));
        if (rootsIndex < 0 || rootsIndex + 1 >= args.size()) {
            return makeResult(QStringLiteral("security suspicious scan"), false, 2, QStringLiteral("Missing --roots for full scan."));
        }
        records = ctx->suspicious->scanFull(splitRoots(args[rootsIndex + 1]), persistenceEntries, callback);
    }

    records = sortedSuspicious(records);
    QVector<QStringList> rows;
    rows.reserve(records.size());
    QJsonArray items;
    for (int i = 0; i < records.size(); ++i) {
        const auto& record = records[i];
        rows.push_back({
            QString::number(i + 1),
            QString::number(record.score),
            voidcare::platform::windows::signatureStatusToString(record.signatureStatus),
            voidcare::cli::formatBytes(static_cast<quint64>(std::max<qint64>(record.size, 0))),
            record.modified.toString(Qt::ISODate),
            record.path,
            record.reasons.join(QStringLiteral("; ")),
        });
        QVariantMap map = voidcare::core::suspiciousRecordToVariant(record);
        map.insert(QStringLiteral("id"), i + 1);
        items.push_back(QJsonValue::fromVariant(map));
    }

    CliResult result = makeResult(QStringLiteral("security suspicious scan"), true, 0, QStringLiteral("Suspicious scan completed."));
    result.humanLines = {voidcare::cli::renderTable(
        {QStringLiteral("ID"), QStringLiteral("Score"), QStringLiteral("Sig"), QStringLiteral("Size"), QStringLiteral("Modified"), QStringLiteral("Path"), QStringLiteral("Reasons")},
        rows)};
    if (records.isEmpty()) {
        result.warnings.push_back(QStringLiteral("No suspicious candidates found."));
    }
    QJsonObject data;
    data.insert(QStringLiteral("items"), items);
    data.insert(QStringLiteral("count"), records.size());
    result.data = data;
    return result;
}

QVector<voidcare::core::SuspiciousFileRecord> selectSuspiciousByIds(const QVector<voidcare::core::SuspiciousFileRecord>& records,
                                                                    const QVector<int>& ids) {
    QVector<voidcare::core::SuspiciousFileRecord> selected;
    for (int id : ids) {
        const int index = id - 1;
        if (index >= 0 && index < records.size()) {
            selected.push_back(records[index]);
        }
    }
    return selected;
}

CliResult commandSuspiciousQuarantineList() {
    QString error;
    const QVector<voidcare::cli::QuarantineListItem> items =
        voidcare::cli::loadQuarantineIndex(voidcare::platform::windows::quarantineRoot(), &error);

    QVector<QStringList> rows;
    QJsonArray jsonItems;
    for (const auto& item : items) {
        rows.push_back({
            QString::number(item.id),
            item.entry.timestamp,
            item.entry.signatureStatus,
            item.entry.originalPath,
            item.entry.quarantinePath,
            item.entry.sha256,
        });
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), item.id);
        obj.insert(QStringLiteral("originalPath"), item.entry.originalPath);
        obj.insert(QStringLiteral("quarantinePath"), item.entry.quarantinePath);
        obj.insert(QStringLiteral("sha256"), item.entry.sha256);
        obj.insert(QStringLiteral("timestamp"), item.entry.timestamp);
        obj.insert(QStringLiteral("signatureStatus"), item.entry.signatureStatus);
        obj.insert(QStringLiteral("reasons"), toJsonArray(item.entry.reasons));
        obj.insert(QStringLiteral("manifestPath"), item.manifestPath);
        jsonItems.push_back(obj);
    }

    CliResult result = makeResult(QStringLiteral("security suspicious quarantine list"), true, 0, QStringLiteral("Quarantine index listed."));
    if (!error.isEmpty() && items.isEmpty()) {
        result.warnings.push_back(error);
    }
    result.humanLines = {voidcare::cli::renderTable(
        {QStringLiteral("ID"), QStringLiteral("Timestamp"), QStringLiteral("Sig"), QStringLiteral("Original"), QStringLiteral("Quarantine"), QStringLiteral("SHA256")},
        rows)};
    QJsonObject data;
    data.insert(QStringLiteral("items"), jsonItems);
    data.insert(QStringLiteral("count"), items.size());
    result.data = data;
    return result;
}

CliResult commandSuspiciousQuarantine(CliContext* ctx, const QStringList& args) {
    CliResult admin = requireAdmin(QStringLiteral("security suspicious quarantine"));
    if (!admin.success) {
        return admin;
    }

    const int idsIndex = args.indexOf(QStringLiteral("--ids"));
    if (idsIndex < 0 || idsIndex + 1 >= args.size()) {
        return makeResult(QStringLiteral("security suspicious quarantine"), false, 2, QStringLiteral("Missing --ids 1,2,3."));
    }

    bool idsOk = false;
    const QVector<int> ids = voidcare::cli::parseIdCsv(args[idsIndex + 1], &idsOk);
    if (!idsOk || ids.isEmpty()) {
        return makeResult(QStringLiteral("security suspicious quarantine"), false, 2, QStringLiteral("Invalid --ids list."));
    }

    const bool full = args.contains(QStringLiteral("--full"));
    const bool quick = !full;

    QVector<voidcare::core::PersistenceEntry> persistenceEntries = ctx->persistence->enumerate();
    QVector<voidcare::core::SuspiciousFileRecord> records;
    auto callback = [ansi = ctx->ansi](const QString& line, const bool isError) {
        printStreamLine(ansi, line, isError);
    };
    if (quick) {
        records = ctx->suspicious->scanQuick(persistenceEntries, callback);
    } else {
        const int rootsIndex = args.indexOf(QStringLiteral("--roots"));
        if (rootsIndex < 0 || rootsIndex + 1 >= args.size()) {
            return makeResult(QStringLiteral("security suspicious quarantine"), false, 2, QStringLiteral("Missing --roots for full mode."));
        }
        records = ctx->suspicious->scanFull(splitRoots(args[rootsIndex + 1]), persistenceEntries, callback);
    }
    records = sortedSuspicious(records);

    const QVector<voidcare::core::SuspiciousFileRecord> selected = selectSuspiciousByIds(records, ids);
    if (selected.isEmpty()) {
        return makeResult(QStringLiteral("security suspicious quarantine"), false, 2, QStringLiteral("No suspicious items matched the requested IDs."));
    }

    CliResult result = makeResult(QStringLiteral("security suspicious quarantine"), true, 0, QStringLiteral("Ready."));
    if (ctx->options.dryRun) {
        result.message = QStringLiteral("Dry-run: files would be moved to quarantine.");
        for (const auto& record : selected) {
            result.plannedActions.push_back(QStringLiteral("Quarantine %1 (sha256: %2)").arg(record.path, record.sha256));
        }
        return result;
    }

    applyGuardOrFail(ctx, &result, QStringLiteral("Quarantine suspicious files"));
    if (!result.success) {
        return result;
    }

    QVector<voidcare::core::QuarantineManifestEntry> manifest;
    QString folder;
    const auto quarantineResult = ctx->suspicious->quarantineSelected(selected, &manifest, &folder);
    result.success = quarantineResult.success;
    result.exitCode = quarantineResult.success ? 0 : 1;
    result.message = quarantineResult.success ? QStringLiteral("Quarantined %1 files into %2").arg(manifest.size()).arg(folder)
                                              : quarantineResult.message;
    return result;
}

CliResult commandSuspiciousRestore(CliContext* ctx, const QStringList& args) {
    CliResult admin = requireAdmin(QStringLiteral("security suspicious restore"));
    if (!admin.success) {
        return admin;
    }

    const int idIndex = args.indexOf(QStringLiteral("--id"));
    if (idIndex < 0 || idIndex + 1 >= args.size()) {
        return makeResult(QStringLiteral("security suspicious restore"), false, 2, QStringLiteral("Missing --id <ID>."));
    }
    bool idOk = false;
    const int id = args[idIndex + 1].toInt(&idOk);
    if (!idOk || id <= 0) {
        return makeResult(QStringLiteral("security suspicious restore"), false, 2, QStringLiteral("Invalid --id value."));
    }

    const int toIndex = args.indexOf(QStringLiteral("--to"));
    const QString destinationOverride = (toIndex >= 0 && toIndex + 1 < args.size()) ? args[toIndex + 1] : QString();

    const QVector<voidcare::cli::QuarantineListItem> items =
        voidcare::cli::loadQuarantineIndex(voidcare::platform::windows::quarantineRoot());
    auto it = std::find_if(items.begin(), items.end(), [id](const auto& item) { return item.id == id; });
    if (it == items.end()) {
        return makeResult(QStringLiteral("security suspicious restore"), false, 2, QStringLiteral("Quarantine ID not found."));
    }

    CliResult result = makeResult(QStringLiteral("security suspicious restore"), true, 0, QStringLiteral("Ready."));
    if (ctx->options.dryRun) {
        result.message = QStringLiteral("Dry-run: quarantined file would be restored.");
        result.plannedActions.push_back(QStringLiteral("Restore %1 -> %2")
                                            .arg(it->entry.quarantinePath,
                                                 destinationOverride.isEmpty() ? it->entry.originalPath : destinationOverride));
        return result;
    }

    if (!confirmPrompt(QStringLiteral("Restore selected quarantined item?"), ctx->options.yes)) {
        return makeResult(QStringLiteral("security suspicious restore"), false, 1, QStringLiteral("Action canceled by user."));
    }

    const auto restoreResult = ctx->suspicious->restoreFromManifest({it->entry}, destinationOverride);
    result.success = restoreResult.success;
    result.exitCode = restoreResult.success ? 0 : 1;
    result.message = restoreResult.message;
    return result;
}

CliResult commandSuspiciousDelete(CliContext* ctx, const QStringList& args) {
    CliResult admin = requireAdmin(QStringLiteral("security suspicious delete"));
    if (!admin.success) {
        return admin;
    }

    const int idIndex = args.indexOf(QStringLiteral("--id"));
    if (idIndex < 0 || idIndex + 1 >= args.size()) {
        return makeResult(QStringLiteral("security suspicious delete"), false, 2, QStringLiteral("Missing --id <ID>."));
    }
    bool idOk = false;
    const int id = args[idIndex + 1].toInt(&idOk);
    if (!idOk || id <= 0) {
        return makeResult(QStringLiteral("security suspicious delete"), false, 2, QStringLiteral("Invalid --id value."));
    }

    const QVector<voidcare::cli::QuarantineListItem> items =
        voidcare::cli::loadQuarantineIndex(voidcare::platform::windows::quarantineRoot());
    auto it = std::find_if(items.begin(), items.end(), [id](const auto& item) { return item.id == id; });
    if (it == items.end()) {
        return makeResult(QStringLiteral("security suspicious delete"), false, 2, QStringLiteral("Quarantine ID not found."));
    }

    CliResult result = makeResult(QStringLiteral("security suspicious delete"), true, 0, QStringLiteral("Ready."));
    if (ctx->options.dryRun) {
        result.message = QStringLiteral("Dry-run: quarantined file would be permanently deleted.");
        result.plannedActions.push_back(QStringLiteral("Delete %1 (sha256: %2)").arg(it->entry.quarantinePath, it->entry.sha256));
        return result;
    }

    applyGuardOrFail(ctx, &result, QStringLiteral("Permanently delete quarantined item"), true);
    if (!result.success) {
        return result;
    }

    QString summary;
    const auto deleteResult = ctx->suspicious->deletePermanentlyFromQuarantine({it->entry}, &summary);
    result.success = deleteResult.success;
    result.exitCode = deleteResult.success ? 0 : 1;
    result.message = deleteResult.message;
    if (!summary.isEmpty()) {
        result.humanLines.push_back(summary);
    }
    return result;
}

CliResult commandOptimizeSafe(CliContext* ctx, const QStringList& args) {
    int days = 2;
    const int daysIndex = args.indexOf(QStringLiteral("--days"));
    if (daysIndex >= 0 && daysIndex + 1 < args.size()) {
        bool ok = false;
        days = args[daysIndex + 1].toInt(&ok);
        if (!ok || days < 0) {
            return makeResult(QStringLiteral("optimize safe"), false, 2, QStringLiteral("Invalid --days value."));
        }
    }

    const bool includeWindowsTemp = args.contains(QStringLiteral("--include-windows-temp"));
    voidcare::core::SafeCleanupOptions options;
    options.olderThanDays = days;
    options.includeWindowsTemp = includeWindowsTemp;
    options.dryRun = ctx->options.dryRun;

    const auto summary = ctx->optimization->runSafeCleanupDetailed(options, [ansi = ctx->ansi](const QString& line, const bool isError) {
        printStreamLine(ansi, line, isError);
    });

    CliResult result = makeResult(QStringLiteral("optimize safe"), summary.success, summary.success ? 0 : 1, QStringLiteral("Safe cleanup completed."));
    if (ctx->options.dryRun) {
        result.message = QStringLiteral("Dry-run: safe cleanup analysis completed.");
    }
    result.humanLines = {
        QStringLiteral("Files scanned: %1").arg(summary.filesScanned),
        QStringLiteral("Files %1: %2").arg(ctx->options.dryRun ? QStringLiteral("to delete") : QStringLiteral("deleted")).arg(summary.filesDeleted),
        QStringLiteral("Bytes %1: %2")
            .arg(ctx->options.dryRun ? QStringLiteral("to free") : QStringLiteral("freed"))
            .arg(voidcare::cli::formatBytes(summary.bytesFreed)),
    };
    result.warnings << summary.warnings;

    QJsonObject data;
    data.insert(QStringLiteral("filesScanned"), static_cast<qint64>(summary.filesScanned));
    data.insert(QStringLiteral("filesDeleted"), static_cast<qint64>(summary.filesDeleted));
    data.insert(QStringLiteral("bytesFreed"), static_cast<qint64>(summary.bytesFreed));
    data.insert(QStringLiteral("warnings"), toJsonArray(summary.warnings));
    result.data = data;
    return result;
}

CliResult commandOptimizePower(CliContext* ctx, const QStringList& args) {
    if (!args.contains(QStringLiteral("--high"))) {
        return makeResult(QStringLiteral("optimize power"), false, 2, QStringLiteral("Only --high is supported in phase 1."));
    }

    CliResult result = makeResult(QStringLiteral("optimize power"), true, 0, QStringLiteral("High performance power plan applied."));
    if (ctx->options.dryRun) {
        result.message = QStringLiteral("Dry-run: would switch to high performance power plan.");
        result.plannedActions.push_back(QStringLiteral("powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c"));
        return result;
    }

    voidcare::core::ProcessRunRequest request;
    request.executable = QStringLiteral("powercfg.exe");
    request.arguments = {QStringLiteral("/setactive"), QStringLiteral("8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c")};
    const auto run = ctx->runner->run(request, [ansi = ctx->ansi](const QString& line, const bool isError) {
        printStreamLine(ansi, line, isError);
    });
    if (!run.success()) {
        result.success = false;
        result.exitCode = run.exitCode == 0 ? 1 : run.exitCode;
        result.message = QStringLiteral("Failed to switch power plan. Try running elevated.");
    }
    return result;
}

CliResult commandOptimizeStartupReport(CliContext* ctx) {
    const QVector<voidcare::core::PersistenceEntry> entries = ctx->persistence->enumerate();
    QMap<QString, int> byType;
    for (const auto& entry : entries) {
        byType[entry.sourceType] += 1;
    }

    QVector<QStringList> rows;
    for (auto it = byType.cbegin(); it != byType.cend(); ++it) {
        rows.push_back({it.key(), QString::number(it.value()), it.value() > 15 ? QStringLiteral("Heavy") : QStringLiteral("Normal")});
    }

    QVector<QStringList> topRows;
    const int maxRows = std::min(static_cast<int>(entries.size()), 15);
    for (int i = 0; i < maxRows; ++i) {
        const auto& e = entries[i];
        topRows.push_back({e.name, e.sourceType, e.path, QStringLiteral("Review manually")});
    }

    CliResult result = makeResult(QStringLiteral("optimize startup report"), true, 0, QStringLiteral("Startup report generated."));
    result.humanLines = {
        voidcare::cli::renderTable({QStringLiteral("Type"), QStringLiteral("Count"), QStringLiteral("Tag")}, rows),
        QStringLiteral(""),
        voidcare::cli::renderTable({QStringLiteral("Name"), QStringLiteral("Type"), QStringLiteral("Path"), QStringLiteral("Suggestion")}, topRows),
    };
    return result;
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();
    args.removeFirst();

    const auto parse = voidcare::cli::parseGlobalFlags(args);
    args = parse.remainingArgs;

    CliContext ctx;
    ctx.options = parse.options;
    ctx.ansi = voidcare::cli::enableAnsiOutputIfSupported();
    ctx.runner = std::make_unique<voidcare::core::ProcessRunner>();
    ctx.restorePoints = std::make_unique<voidcare::core::RestorePointService>(ctx.runner.get());
    ctx.guard = std::make_unique<voidcare::core::DestructiveActionGuard>(ctx.restorePoints.get());
    ctx.avDiscovery = std::make_unique<voidcare::core::AvDiscoveryService>();
    ctx.persistence = std::make_unique<voidcare::core::PersistenceAuditService>(ctx.runner.get());
    ctx.suspicious = std::make_unique<voidcare::core::SuspiciousFileScannerService>(ctx.runner.get());
    ctx.optimization = std::make_unique<voidcare::core::OptimizationService>(ctx.runner.get(), ctx.persistence.get());
    ctx.gaming = std::make_unique<voidcare::core::GamingBoostService>(ctx.runner.get());
    refreshProvider(&ctx);

    CliResult result;
    if (args.isEmpty() || args[0] == QStringLiteral("--help")) {
        result = commandHelp();
    } else if (args[0] == QStringLiteral("--version")) {
        result = commandVersion();
    } else if (args[0] == QStringLiteral("about")) {
        result = commandAbout();
    } else if (args[0] == QStringLiteral("status")) {
        result = commandStatus(&ctx);
    } else if (args[0] == QStringLiteral("security") && args.size() >= 3 && args[1] == QStringLiteral("av") &&
               args[2] == QStringLiteral("list")) {
        result = commandSecurityAvList(&ctx);
    } else if (args[0] == QStringLiteral("security") && args.size() >= 3 && args[1] == QStringLiteral("defender") &&
               args[2] == QStringLiteral("status")) {
        result = commandSecurityDefenderStatus(&ctx);
    } else if (args[0] == QStringLiteral("security") && args.size() >= 2 && args[1] == QStringLiteral("scan")) {
        result = commandSecurityScan(&ctx, args);
    } else if (args[0] == QStringLiteral("security") && args.size() >= 2 && args[1] == QStringLiteral("remediate")) {
        result = commandSecurityRemediate(&ctx);
    } else if (args[0] == QStringLiteral("security") && args.size() >= 3 && args[1] == QStringLiteral("persistence") &&
               args[2] == QStringLiteral("list")) {
        result = commandPersistenceList(&ctx);
    } else if (args[0] == QStringLiteral("security") && args.size() >= 3 && args[1] == QStringLiteral("persistence") &&
               args[2] == QStringLiteral("disable")) {
        result = commandPersistenceDisable(&ctx, args);
    } else if (args[0] == QStringLiteral("security") && args.size() >= 3 && args[1] == QStringLiteral("suspicious") &&
               args[2] == QStringLiteral("scan")) {
        result = commandSuspiciousScan(&ctx, args);
    } else if (args[0] == QStringLiteral("security") && args.size() >= 4 && args[1] == QStringLiteral("suspicious") &&
               args[2] == QStringLiteral("quarantine") && args[3] == QStringLiteral("list")) {
        result = commandSuspiciousQuarantineList();
    } else if (args[0] == QStringLiteral("security") && args.size() >= 3 && args[1] == QStringLiteral("suspicious") &&
               args[2] == QStringLiteral("quarantine")) {
        result = commandSuspiciousQuarantine(&ctx, args);
    } else if (args[0] == QStringLiteral("security") && args.size() >= 3 && args[1] == QStringLiteral("suspicious") &&
               args[2] == QStringLiteral("restore")) {
        result = commandSuspiciousRestore(&ctx, args);
    } else if (args[0] == QStringLiteral("security") && args.size() >= 3 && args[1] == QStringLiteral("suspicious") &&
               args[2] == QStringLiteral("delete")) {
        result = commandSuspiciousDelete(&ctx, args);
    } else if (args[0] == QStringLiteral("optimize") && args.size() >= 2 && args[1] == QStringLiteral("safe")) {
        result = commandOptimizeSafe(&ctx, args);
    } else if (args[0] == QStringLiteral("optimize") && args.size() >= 2 && args[1] == QStringLiteral("power")) {
        result = commandOptimizePower(&ctx, args);
    } else if (args[0] == QStringLiteral("optimize") && args.size() >= 3 && args[1] == QStringLiteral("startup") &&
               args[2] == QStringLiteral("report")) {
        result = commandOptimizeStartupReport(&ctx);
    } else {
        result = makeResult(QStringLiteral("unknown"), false, 2, QStringLiteral("Unknown or not-yet-implemented command."));
        result.humanLines = {QStringLiteral("Unknown command. Run `voidcare --help` for usage.")};
    }

    if (!args.isEmpty() && args[0] != QStringLiteral("status")) {
        ctx.sessionSummary = result.message;
    }

    if (ctx.options.json) {
        QTextStream out(stdout);
        out << voidcare::cli::renderJsonEnvelope(result);
        out.flush();
    } else {
        QTextStream out(stdout);
        for (const QString& line : result.humanLines) {
            out << line << "\n";
        }
        if (!result.message.isEmpty()) {
            out << voidcare::cli::chip(result.success ? QStringLiteral("[OK]") : QStringLiteral("[ERR]"),
                                       result.success ? ChipKind::Ok : ChipKind::Error,
                                       ctx.ansi)
                << " " << result.message << "\n";
        }
        for (const QString& warning : result.warnings) {
            out << voidcare::cli::chip(QStringLiteral("[WARN]"), ChipKind::Warn, ctx.ansi) << " " << warning << "\n";
        }
        out.flush();
    }

    return result.exitCode;
}
