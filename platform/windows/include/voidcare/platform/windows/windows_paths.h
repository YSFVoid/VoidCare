#pragma once

#include <QString>
#include <QStringList>

namespace voidcare::platform::windows {

QString programDataRoot();
QString quarantineRoot();
QStringList startupFolders();
QStringList quickScanRoots();
bool isUserWritableLocation(const QString& filePath);

}  // namespace voidcare::platform::windows
