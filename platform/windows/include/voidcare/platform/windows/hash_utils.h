#pragma once

#include <QByteArray>
#include <QString>

namespace voidcare::platform::windows {

QByteArray sha256File(const QString& filePath, QString* errorMessage = nullptr);

}  // namespace voidcare::platform::windows
