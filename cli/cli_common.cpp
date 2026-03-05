#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "cli_common.h"

#include <Windows.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <algorithm>

namespace voidcare::cli {

namespace {

QJsonArray toJsonArray(const QStringList& values) {
    QJsonArray out;
    for (const QString& value : values) {
        out.push_back(value);
    }
    return out;
}

QString padRight(const QString& value, const int width) {
    if (value.size() >= width) {
        return value;
    }
    return value + QString(width - value.size(), QLatin1Char(' '));
}

}  // namespace

ParseGlobalFlagsResult parseGlobalFlags(const QStringList& args) {
    ParseGlobalFlagsResult result;
    for (const QString& arg : args) {
        if (arg == QStringLiteral("--json")) {
            result.options.json = true;
            continue;
        }
        if (arg == QStringLiteral("--yes")) {
            result.options.yes = true;
            continue;
        }
        if (arg == QStringLiteral("--dry-run")) {
            result.options.dryRun = true;
            continue;
        }

        result.remainingArgs.push_back(arg);
    }
    return result;
}

QVector<int> parseIdCsv(const QString& rawIds, bool* okOut) {
    bool ok = true;
    QVector<int> ids;
    const QStringList parts = rawIds.split(',', Qt::SkipEmptyParts);
    for (QString part : parts) {
        part = part.trimmed();
        bool parsed = false;
        const int id = part.toInt(&parsed);
        if (!parsed || id <= 0) {
            ok = false;
            continue;
        }
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    if (okOut != nullptr) {
        *okOut = ok;
    }
    return ids;
}

bool enableAnsiOutputIfSupported() {
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
        return false;
    }

    DWORD mode = 0;
    if (!GetConsoleMode(handle, &mode)) {
        return false;
    }

    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
        return true;
    }

    const DWORD updatedMode = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(handle, updatedMode) != FALSE;
}

QString chip(const QString& text, const ChipKind kind, const bool ansi) {
    if (!ansi) {
        return text;
    }

    QString color;
    switch (kind) {
    case ChipKind::Ok:
        color = QStringLiteral("\x1b[32m");
        break;
    case ChipKind::Warn:
        color = QStringLiteral("\x1b[33m");
        break;
    case ChipKind::Error:
        color = QStringLiteral("\x1b[31m");
        break;
    case ChipKind::Info:
    default:
        color = QStringLiteral("\x1b[36m");
        break;
    }

    return color + text + QStringLiteral("\x1b[0m");
}

QString renderTable(const QStringList& headers, const QVector<QStringList>& rows) {
    if (headers.isEmpty()) {
        return {};
    }

    QVector<int> widths;
    widths.reserve(headers.size());
    for (const QString& header : headers) {
        widths.push_back(header.size());
    }

    for (const QStringList& row : rows) {
        for (int i = 0; i < headers.size(); ++i) {
            const QString cell = i < row.size() ? row[i] : QString();
            widths[i] = std::max(widths[i], static_cast<int>(cell.size()));
        }
    }

    QString separator = QStringLiteral("+");
    for (int width : widths) {
        separator += QString(width + 2, QLatin1Char('-')) + QStringLiteral("+");
    }

    QString output;
    QTextStream stream(&output);
    stream << separator << "\n|";
    for (int i = 0; i < headers.size(); ++i) {
        stream << " " << padRight(headers[i], widths[i]) << " |";
    }
    stream << "\n" << separator;

    for (const QStringList& row : rows) {
        stream << "\n|";
        for (int i = 0; i < headers.size(); ++i) {
            const QString cell = i < row.size() ? row[i] : QString();
            stream << " " << padRight(cell, widths[i]) << " |";
        }
    }
    stream << "\n" << separator;
    return output;
}

QByteArray renderJsonEnvelope(const CliResult& result) {
    QJsonObject root;
    root.insert(QStringLiteral("success"), result.success);
    root.insert(QStringLiteral("exitCode"), result.exitCode);
    root.insert(QStringLiteral("command"), result.command);
    root.insert(QStringLiteral("message"), result.message);
    root.insert(QStringLiteral("data"), result.data);
    root.insert(QStringLiteral("warnings"), toJsonArray(result.warnings));
    root.insert(QStringLiteral("plannedActions"), toJsonArray(result.plannedActions));
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

QString formatBytes(const quint64 bytes) {
    const double kb = static_cast<double>(bytes) / 1024.0;
    if (kb < 1024.0) {
        return QStringLiteral("%1 KB").arg(QString::number(kb, 'f', 1));
    }

    const double mb = kb / 1024.0;
    if (mb < 1024.0) {
        return QStringLiteral("%1 MB").arg(QString::number(mb, 'f', 1));
    }

    const double gb = mb / 1024.0;
    return QStringLiteral("%1 GB").arg(QString::number(gb, 'f', 2));
}

QVector<QuarantineListItem> loadQuarantineIndex(const QString& quarantineRoot, QString* errorOut) {
    QVector<QuarantineListItem> out;
    QDir rootDir(quarantineRoot);
    if (!rootDir.exists()) {
        if (errorOut != nullptr) {
            *errorOut = QStringLiteral("Quarantine root does not exist.");
        }
        return out;
    }

    QDirIterator it(quarantineRoot,
                    QStringList() << QStringLiteral("manifest.json"),
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString manifestPath = it.next();
        QFile file(manifestPath);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        const QByteArray json = file.readAll();
        const QVector<core::QuarantineManifestEntry> entries = core::quarantineManifestFromJson(json);
        for (const core::QuarantineManifestEntry& entry : entries) {
            QuarantineListItem item;
            item.entry = entry;
            item.manifestPath = manifestPath;
            out.push_back(item);
        }
    }

    std::sort(out.begin(), out.end(), [](const QuarantineListItem& left, const QuarantineListItem& right) {
        if (left.entry.timestamp == right.entry.timestamp) {
            return left.entry.quarantinePath < right.entry.quarantinePath;
        }
        return left.entry.timestamp < right.entry.timestamp;
    });

    for (int i = 0; i < out.size(); ++i) {
        out[i].id = i + 1;
    }

    if (errorOut != nullptr) {
        errorOut->clear();
    }
    return out;
}

}  // namespace voidcare::cli
