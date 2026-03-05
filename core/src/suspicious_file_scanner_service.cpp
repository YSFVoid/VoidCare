#include "voidcare/core/suspicious_file_scanner_service.h"

#include <Windows.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSet>

#include <cmath>

#include "voidcare/platform/windows/hash_utils.h"
#include "voidcare/platform/windows/signature_verifier.h"
#include "voidcare/platform/windows/windows_paths.h"

namespace voidcare::core {

namespace {

const QSet<QString> kSuspiciousExtensions = {
    QStringLiteral("exe"), QStringLiteral("dll"), QStringLiteral("scr"), QStringLiteral("com"),
    QStringLiteral("bat"), QStringLiteral("cmd"), QStringLiteral("ps1"), QStringLiteral("vbs"),
    QStringLiteral("js"),  QStringLiteral("jar"), QStringLiteral("lnk"),
};

const QSet<QString> kLikelyDocumentExt = {
    QStringLiteral("txt"), QStringLiteral("pdf"), QStringLiteral("doc"),  QStringLiteral("docx"),
    QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("jpg"), QStringLiteral("png"),
};

QString toHexSha(const QByteArray& hash) {
    return QString::fromLatin1(hash.toHex());
}

bool isHiddenOrSystem(const QString& path) {
    const DWORD attrs = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()));
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_HIDDEN) != 0 || (attrs & FILE_ATTRIBUTE_SYSTEM) != 0;
}

QString timestampFolder() {
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString random = QString::number(QRandomGenerator::global()->bounded(100000, 999999));
    return stamp + QStringLiteral("_") + random;
}

bool moveFileWithFallback(const QString& from, const QString& to) {
    QFile source(from);
    if (!source.exists()) {
        return false;
    }

    if (source.rename(to)) {
        return true;
    }

    if (QFile::copy(from, to)) {
        QFile::remove(from);
        return true;
    }

    return false;
}

}  // namespace

SuspiciousFileScannerService::SuspiciousFileScannerService(ProcessRunner* runner, QObject* parent)
    : QObject(parent)
    , m_runner(runner) {}

QVector<SuspiciousFileRecord> SuspiciousFileScannerService::scanQuick(
    const QVector<PersistenceEntry>& persistenceEntries,
    const LogCallback& logCallback,
    std::atomic_bool* cancelToken) const {
    QStringList roots = platform::windows::quickScanRoots();
    for (const auto& entry : persistenceEntries) {
        if (!entry.path.trimmed().isEmpty()) {
            roots << QFileInfo(entry.path).absolutePath();
            roots << entry.path;
        }
    }

    roots.removeDuplicates();
    return scanInternal(roots, persistenceEntries, logCallback, cancelToken);
}

QVector<SuspiciousFileRecord> SuspiciousFileScannerService::scanFull(
    const QStringList& roots,
    const QVector<PersistenceEntry>& persistenceEntries,
    const LogCallback& logCallback,
    std::atomic_bool* cancelToken) const {
    return scanInternal(roots, persistenceEntries, logCallback, cancelToken);
}

AntivirusActionResult SuspiciousFileScannerService::quarantineSelected(
    const QVector<SuspiciousFileRecord>& records,
    QVector<QuarantineManifestEntry>* manifestOut,
    QString* quarantineFolderOut) const {
    if (records.isEmpty()) {
        return {false, QStringLiteral("No suspicious files selected."), -1};
    }

    QDir rootDir(platform::windows::quarantineRoot());
    if (!rootDir.exists() && !rootDir.mkpath(QStringLiteral("."))) {
        return {false, QStringLiteral("Failed to create quarantine root directory."), -1};
    }

    const QString quarantineFolder = rootDir.filePath(timestampFolder());
    if (!QDir().mkpath(quarantineFolder)) {
        return {false, QStringLiteral("Failed to create quarantine folder."), -1};
    }

    QVector<QuarantineManifestEntry> manifestEntries;
    QStringList failures;

    for (const SuspiciousFileRecord& record : records) {
        QFileInfo fileInfo(record.path);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            failures << QStringLiteral("Missing file: %1").arg(record.path);
            continue;
        }

        QString targetName = fileInfo.fileName();
        QString targetPath = QDir(quarantineFolder).filePath(targetName);
        int suffix = 1;
        while (QFileInfo::exists(targetPath)) {
            targetPath = QDir(quarantineFolder).filePath(
                QStringLiteral("%1_%2").arg(targetName).arg(suffix++));
        }

        if (!moveFileWithFallback(record.path, targetPath)) {
            failures << QStringLiteral("Failed moving file to quarantine: %1").arg(record.path);
            continue;
        }

        QuarantineManifestEntry entry;
        entry.originalPath = QDir::fromNativeSeparators(record.path);
        entry.quarantinePath = QDir::fromNativeSeparators(targetPath);
        entry.sha256 = record.sha256;
        entry.timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        entry.reasons = record.reasons;
        entry.signatureStatus = platform::windows::signatureStatusToString(record.signatureStatus);
        manifestEntries.push_back(entry);
    }

    const QByteArray manifestJson = quarantineManifestToJson(manifestEntries);
    QFile manifestFile(QDir(quarantineFolder).filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {false, QStringLiteral("Failed to write quarantine manifest."), -1};
    }
    manifestFile.write(manifestJson);
    manifestFile.close();

    if (m_runner != nullptr) {
        ProcessRunRequest aclRequest;
        aclRequest.executable = QStringLiteral("icacls.exe");
        aclRequest.arguments = {
            quarantineFolder,
            QStringLiteral("/inheritance:r"),
            QStringLiteral("/grant:r"),
            QStringLiteral("Administrators:(OI)(CI)F"),
            QStringLiteral("SYSTEM:(OI)(CI)F"),
        };
        m_runner->run(aclRequest);
    }

    if (manifestOut != nullptr) {
        *manifestOut = manifestEntries;
    }
    if (quarantineFolderOut != nullptr) {
        *quarantineFolderOut = quarantineFolder;
    }

    AntivirusActionResult result;
    result.success = failures.isEmpty();
    result.exitCode = result.success ? 0 : 1;
    result.message = result.success
                         ? QStringLiteral("Selected files quarantined.")
                         : QStringLiteral("Quarantine completed with issues: %1")
                               .arg(failures.join(QStringLiteral("; ")));
    return result;
}

AntivirusActionResult SuspiciousFileScannerService::restoreFromManifest(
    const QVector<QuarantineManifestEntry>& entries,
    const QString& destinationOverride) const {
    if (entries.isEmpty()) {
        return {false, QStringLiteral("No quarantine entries selected for restore."), -1};
    }

    QStringList failures;
    for (const QuarantineManifestEntry& entry : entries) {
        const QString source = entry.quarantinePath;
        if (!QFileInfo::exists(source)) {
            failures << QStringLiteral("Missing quarantined file: %1").arg(source);
            continue;
        }

        QString destination = entry.originalPath;
        if (!destinationOverride.trimmed().isEmpty()) {
            destination = QDir(destinationOverride).filePath(QFileInfo(entry.originalPath).fileName());
        }

        QFileInfo destinationInfo(destination);
        QDir().mkpath(destinationInfo.absolutePath());

        if (QFileInfo::exists(destination)) {
            destination += QStringLiteral(".restored");
        }

        if (!moveFileWithFallback(source, destination)) {
            failures << QStringLiteral("Failed to restore file: %1").arg(entry.originalPath);
        }
    }

    AntivirusActionResult result;
    result.success = failures.isEmpty();
    result.exitCode = result.success ? 0 : 1;
    result.message = result.success ? QStringLiteral("Selected files restored.")
                                    : failures.join(QStringLiteral("; "));
    return result;
}

AntivirusActionResult SuspiciousFileScannerService::deletePermanentlyFromQuarantine(
    const QVector<QuarantineManifestEntry>& entries,
    QString* summaryOut) const {
    if (entries.isEmpty()) {
        return {false, QStringLiteral("No quarantined files selected."), -1};
    }

    QStringList summaryLines;
    QStringList failures;
    for (const QuarantineManifestEntry& entry : entries) {
        summaryLines << QStringLiteral("%1 | %2").arg(entry.quarantinePath, entry.sha256);
        QFile file(entry.quarantinePath);
        if (file.exists() && !file.remove()) {
            failures << QStringLiteral("Failed to delete: %1").arg(entry.quarantinePath);
        }
    }

    if (summaryOut != nullptr) {
        *summaryOut = summaryLines.join('\n');
    }

    AntivirusActionResult result;
    result.success = failures.isEmpty();
    result.exitCode = result.success ? 0 : 1;
    result.message = result.success ? QStringLiteral("Selected quarantined files permanently deleted.")
                                    : failures.join(QStringLiteral("; "));
    return result;
}

int SuspiciousFileScannerService::calculateHeuristicScore(const SuspiciousFileRecord& baseRecord,
                                                          const bool userWritable,
                                                          const bool hasDoubleExtension,
                                                          const bool entropyFlag,
                                                          const bool persistenceLinked,
                                                          QStringList* reasonsOut) {
    int score = 0;
    QStringList reasons;

    if (userWritable &&
        (baseRecord.signatureStatus == platform::windows::SignatureStatus::Unsigned ||
         baseRecord.signatureStatus == platform::windows::SignatureStatus::Invalid)) {
        score += 3;
        reasons << QStringLiteral("Unsigned or invalid signature in user-writable location");
    }

    if (baseRecord.hiddenOrSystem) {
        score += 2;
        reasons << QStringLiteral("Hidden/System attribute set unexpectedly");
    }

    if (hasDoubleExtension) {
        score += 2;
        reasons << QStringLiteral("Double extension naming pattern");
    }

    if (entropyFlag) {
        score += 2;
        reasons << QStringLiteral("High-entropy filename pattern");
    }

    if (persistenceLinked) {
        score += 3;
        reasons << QStringLiteral("Referenced by persistence entry");
    }

    if (reasonsOut != nullptr) {
        *reasonsOut = reasons;
    }
    return score;
}

bool SuspiciousFileScannerService::hasDoubleExtensionPattern(const QString& fileName) {
    const QString lower = fileName.toLower();
    const QStringList parts = lower.split('.');
    if (parts.size() < 3) {
        return false;
    }

    const QString last = parts.last();
    const QString previous = parts[parts.size() - 2];
    return kSuspiciousExtensions.contains(last) && kLikelyDocumentExt.contains(previous);
}

bool SuspiciousFileScannerService::hasHighEntropyName(const QString& fileName) {
    const QString base = QFileInfo(fileName).completeBaseName().toLower();
    if (base.size() < 8) {
        return false;
    }

    QMap<QChar, int> counts;
    for (QChar c : base) {
        counts[c] += 1;
    }

    const double length = static_cast<double>(base.size());
    double entropy = 0.0;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        const double p = static_cast<double>(it.value()) / length;
        entropy -= p * std::log2(p);
    }

    return entropy >= 3.8;
}

QVector<SuspiciousFileRecord> SuspiciousFileScannerService::scanInternal(
    const QStringList& roots,
    const QVector<PersistenceEntry>& persistenceEntries,
    const LogCallback& logCallback,
    std::atomic_bool* cancelToken) const {
    QVector<SuspiciousFileRecord> suspicious;

    QSet<QString> persistencePaths;
    QMap<QString, QString> persistenceByPath;
    for (const auto& entry : persistenceEntries) {
        const QString normalized = QDir::cleanPath(entry.path).toLower();
        if (!normalized.isEmpty()) {
            persistencePaths.insert(normalized);
            persistenceByPath.insert(normalized, entry.name);
        }
    }

    QSet<QString> visitedFiles;
    const auto inspectCandidate = [&](const QString& path) {
        const QString normalized = path.toLower();
        if (visitedFiles.contains(normalized)) {
            return;
        }
        visitedFiles.insert(normalized);

        QFileInfo fileInfo(path);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            return;
        }

        const QString extension = fileInfo.suffix().toLower();
        if (!kSuspiciousExtensions.contains(extension)) {
            return;
        }

        SuspiciousFileRecord record;
        record.path = path;
        record.size = fileInfo.size();
        record.created = fileInfo.birthTime();
        record.modified = fileInfo.lastModified();
        record.hiddenOrSystem = isHiddenOrSystem(path);

        QString hashError;
        const QByteArray hash = platform::windows::sha256File(path, &hashError);
        record.sha256 = hash.isEmpty() ? QStringLiteral("unavailable") : toHexSha(hash);

        const platform::windows::SignatureInfo signature =
            platform::windows::verifyFileSignature(path);
        record.signatureStatus = signature.status;

        const bool userWritable = platform::windows::isUserWritableLocation(path);
        const bool doubleExt = hasDoubleExtensionPattern(fileInfo.fileName());
        const bool entropyFlag = hasHighEntropyName(fileInfo.fileName());
        const bool persistenceLinked = persistencePaths.contains(normalized);
        if (persistenceLinked) {
            record.persistenceRef = persistenceByPath.value(normalized);
        }

        QStringList reasons;
        record.score = calculateHeuristicScore(
            record, userWritable, doubleExt, entropyFlag, persistenceLinked, &reasons);
        record.reasons = reasons;

        if (record.score >= 4) {
            suspicious.push_back(record);
            if (logCallback) {
                logCallback(QStringLiteral("Suspicious item: %1 (score %2)")
                                .arg(record.path)
                                .arg(record.score),
                            false);
            }
        }
    };

    for (const QString& root : roots) {
        if (cancelToken != nullptr && cancelToken->load()) {
            break;
        }

        QFileInfo rootInfo(root);
        if (!rootInfo.exists()) {
            continue;
        }

        if (logCallback) {
            logCallback(QStringLiteral("Scanning: %1").arg(rootInfo.absoluteFilePath()), false);
        }

        if (rootInfo.isFile()) {
            inspectCandidate(rootInfo.absoluteFilePath());
            continue;
        }

        QDirIterator iterator(rootInfo.absoluteFilePath(),
                              QDir::Files | QDir::NoDotAndDotDot,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            if (cancelToken != nullptr && cancelToken->load()) {
                return suspicious;
            }

            const QString path = QDir::cleanPath(iterator.next());
            inspectCandidate(path);
        }
    }

    if (logCallback) {
        logCallback(QStringLiteral("Suspicious scan completed. Items flagged: %1")
                        .arg(suspicious.size()),
                    false);
    }

    return suspicious;
}

QByteArray quarantineManifestToJson(const QVector<QuarantineManifestEntry>& entries) {
    QJsonArray array;
    for (const QuarantineManifestEntry& entry : entries) {
        QJsonObject obj;
        obj.insert(QStringLiteral("originalPath"), entry.originalPath);
        obj.insert(QStringLiteral("quarantinePath"), entry.quarantinePath);
        obj.insert(QStringLiteral("sha256"), entry.sha256);
        obj.insert(QStringLiteral("timestamp"), entry.timestamp);
        obj.insert(QStringLiteral("signatureStatus"), entry.signatureStatus);

        QJsonArray reasons;
        for (const QString& reason : entry.reasons) {
            reasons.push_back(reason);
        }
        obj.insert(QStringLiteral("reasons"), reasons);
        array.push_back(obj);
    }

    QJsonDocument document(array);
    return document.toJson(QJsonDocument::Indented);
}

QVector<QuarantineManifestEntry> quarantineManifestFromJson(const QByteArray& json) {
    QVector<QuarantineManifestEntry> entries;
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isArray()) {
        return entries;
    }

    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        QuarantineManifestEntry entry;
        entry.originalPath = obj.value(QStringLiteral("originalPath")).toString();
        entry.quarantinePath = obj.value(QStringLiteral("quarantinePath")).toString();
        entry.sha256 = obj.value(QStringLiteral("sha256")).toString();
        entry.timestamp = obj.value(QStringLiteral("timestamp")).toString();
        entry.signatureStatus = obj.value(QStringLiteral("signatureStatus")).toString();

        const QJsonArray reasons = obj.value(QStringLiteral("reasons")).toArray();
        for (const QJsonValue& reason : reasons) {
            entry.reasons.push_back(reason.toString());
        }

        entries.push_back(entry);
    }

    return entries;
}

}  // namespace voidcare::core
