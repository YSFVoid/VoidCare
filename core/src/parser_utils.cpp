#include "voidcare/core/parser_utils.h"

#include <Windows.h>

#include <QDir>

namespace voidcare::core {

QPair<QString, QString> splitExecutableAndArgs(const QString& raw) {
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        return {QString(), QString()};
    }

    if (trimmed.startsWith('"')) {
        const int quoteEnd = trimmed.indexOf('"', 1);
        if (quoteEnd > 1) {
            const QString path = trimmed.mid(1, quoteEnd - 1);
            const QString args = trimmed.mid(quoteEnd + 1).trimmed();
            return {QDir::fromNativeSeparators(path), args};
        }
    }

    const int splitIndex = trimmed.indexOf(' ');
    if (splitIndex < 0) {
        return {QDir::fromNativeSeparators(trimmed), QString()};
    }

    const QString path = trimmed.left(splitIndex);
    const QString args = trimmed.mid(splitIndex + 1).trimmed();
    return {QDir::fromNativeSeparators(path), args};
}

QString expandEnvironmentStrings(const QString& value) {
    std::wstring input = value.toStdWString();
    DWORD size = ExpandEnvironmentStringsW(input.c_str(), nullptr, 0);
    if (size == 0) {
        return value;
    }

    std::wstring output;
    output.resize(size);
    ExpandEnvironmentStringsW(input.c_str(), output.data(), size);
    if (!output.empty() && output.back() == L'\0') {
        output.pop_back();
    }
    return QString::fromStdWString(output);
}

QStringList parseCsvLine(const QString& line) {
    QStringList output;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (c == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.append('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == ',' && !inQuotes) {
            output << current;
            current.clear();
        } else {
            current.append(c);
        }
    }
    output << current;

    for (QString& cell : output) {
        if (cell.startsWith('"') && cell.endsWith('"') && cell.size() >= 2) {
            cell = cell.mid(1, cell.size() - 2);
            cell.replace(QStringLiteral("\"\""), QStringLiteral("\""));
        }
        cell = cell.trimmed();
    }

    return output;
}

}  // namespace voidcare::core
