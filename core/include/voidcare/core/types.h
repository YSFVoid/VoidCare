#pragma once

#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

#include "voidcare/platform/windows/signature_verifier.h"

namespace voidcare::core {

enum class ProcessOutputStream {
    StdOut,
    StdErr,
};

struct ProcessOutputEvent {
    ProcessOutputStream stream = ProcessOutputStream::StdOut;
    QString text;
    QDateTime timestamp;
};

struct ProcessRunRequest {
    QString executable;
    QStringList arguments;
    QString workingDirectory;
    QMap<QString, QString> environment;
    bool mergeStdErr = false;
};

struct ProcessRunResult {
    int exitCode = -1;
    bool canceled = false;
    QString stdOut;
    QString stdErr;
    QString errorMessage;

    [[nodiscard]] bool success() const {
        return !canceled && errorMessage.isEmpty() && exitCode == 0;
    }
};

enum class RestorePointStatus {
    Success,
    Failed,
    SkippedByUser,
};

struct RestorePointResult {
    RestorePointStatus status = RestorePointStatus::Failed;
    QString message;
    QString detail;
};

struct GuardOutcome {
    bool proceed = false;
    bool needsRestoreOverride = false;
    bool restorePointCreated = false;
    QString message;
    RestorePointResult restorePoint;
};

struct AntivirusProduct {
    QString name;
    quint32 rawState = 0;
    bool active = false;
    bool upToDate = false;
    QString statusText;
};

struct AntivirusActionResult {
    bool success = false;
    QString message;
    int exitCode = -1;
};

using LogCallback = std::function<void(const QString& line, bool isError)>;

struct PersistenceEntry {
    QString id;
    QString sourceType;
    QString name;
    QString path;
    QString args;
    QString publisher;
    platform::windows::SignatureStatus signatureStatus = platform::windows::SignatureStatus::Unknown;
    bool enabled = true;
    QString rawReference;
};

struct SuspiciousFileRecord {
    QString path;
    qint64 size = 0;
    QDateTime created;
    QDateTime modified;
    QString sha256;
    platform::windows::SignatureStatus signatureStatus = platform::windows::SignatureStatus::Unknown;
    int score = 0;
    QStringList reasons;
    QString persistenceRef;
    bool hiddenOrSystem = false;
};

struct QuarantineManifestEntry {
    QString originalPath;
    QString quarantinePath;
    QString sha256;
    QString timestamp;
    QStringList reasons;
    QString signatureStatus;
};

enum class DiscordPresenceState {
    Dashboard,
    Security,
    SuspiciousFiles,
    Optimize,
    Gaming,
    Apps,
};

struct HealthReport {
    quint64 freeDiskBytes = 0;
    quint64 totalDiskBytes = 0;
    int startupItemCount = 0;
    QStringList heavyProcesses;
};

struct InstalledAppInfo {
    QString name;
    QString version;
    QString publisher;
};

QVariantMap persistenceEntryToVariant(const PersistenceEntry& entry);
QVariantMap suspiciousRecordToVariant(const SuspiciousFileRecord& record);
QVariantMap installedAppToVariant(const InstalledAppInfo& app);
QString discordStateLabel(DiscordPresenceState state);

}  // namespace voidcare::core
