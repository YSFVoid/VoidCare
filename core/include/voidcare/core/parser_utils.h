#pragma once

#include <QPair>
#include <QString>
#include <QStringList>

namespace voidcare::core {

QStringList parseCsvLine(const QString& line);
QPair<QString, QString> splitExecutableAndArgs(const QString& raw);
QString expandEnvironmentStrings(const QString& value);

}  // namespace voidcare::core
