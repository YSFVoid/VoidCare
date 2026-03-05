#pragma once

#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

#include "voidcare/core/suspicious_file_scanner_service.h"

namespace voidcare::cli {

struct GlobalOptions {
    bool json = false;
    bool yes = false;
    bool dryRun = false;
};

struct ParseGlobalFlagsResult {
    GlobalOptions options;
    QStringList remainingArgs;
    QString error;
};

enum class ChipKind {
    Ok,
    Warn,
    Error,
    Info,
};

struct CliResult {
    bool success = true;
    int exitCode = 0;
    QString command;
    QString message;
    QJsonValue data;
    QStringList warnings;
    QStringList plannedActions;
    QStringList humanLines;
};

struct QuarantineListItem {
    int id = 0;
    core::QuarantineManifestEntry entry;
    QString manifestPath;
};

ParseGlobalFlagsResult parseGlobalFlags(const QStringList& args);
QVector<int> parseIdCsv(const QString& rawIds, bool* okOut = nullptr);

bool enableAnsiOutputIfSupported();
QString chip(const QString& text, ChipKind kind, bool ansi);

QString renderTable(const QStringList& headers, const QVector<QStringList>& rows);
QByteArray renderJsonEnvelope(const CliResult& result);
QString formatBytes(quint64 bytes);

QVector<QuarantineListItem> loadQuarantineIndex(const QString& quarantineRoot, QString* errorOut = nullptr);

}  // namespace voidcare::cli

