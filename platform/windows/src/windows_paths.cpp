#include "voidcare/platform/windows/windows_paths.h"

#include <algorithm>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace voidcare::platform::windows {

QString programDataRoot() {
    const QString base = qEnvironmentVariable("ProgramData", QStringLiteral("C:/ProgramData"));
    return QDir::cleanPath(base + QStringLiteral("/VoidCare"));
}

QString quarantineRoot() {
    return QDir::cleanPath(programDataRoot() + QStringLiteral("/Quarantine"));
}

QStringList startupFolders() {
    QStringList folders;
    folders << QDir::cleanPath(qEnvironmentVariable(
                   "ProgramData",
                   QStringLiteral("C:/ProgramData")) +
               QStringLiteral("/Microsoft/Windows/Start Menu/Programs/Startup"));

    const QString appData = qEnvironmentVariable("APPDATA");
    if (!appData.isEmpty()) {
        folders << QDir::cleanPath(appData + QStringLiteral("/Microsoft/Windows/Start Menu/Programs/Startup"));
    }
    folders.removeDuplicates();
    return folders;
}

QStringList quickScanRoots() {
    QStringList roots;
    roots << QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
          << QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
          << QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
          << QDir::tempPath();

    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty()) {
        roots << QDir::cleanPath(localAppData + QStringLiteral("/Temp"));
    }

    roots << startupFolders();

    roots.removeDuplicates();
    roots.erase(std::remove_if(roots.begin(), roots.end(), [](const QString& root) {
                    return root.isEmpty() || !QFileInfo::exists(root);
                }),
                roots.end());
    return roots;
}

bool isUserWritableLocation(const QString& filePath) {
    const QString normalized = QDir::cleanPath(filePath).toLower();
    const QString userProfile = QDir::cleanPath(qEnvironmentVariable("USERPROFILE")).toLower();
    const QString appData = QDir::cleanPath(qEnvironmentVariable("APPDATA")).toLower();
    const QString localAppData = QDir::cleanPath(qEnvironmentVariable("LOCALAPPDATA")).toLower();
    const QString tempPath = QDir::cleanPath(QDir::tempPath()).toLower();

    return (!userProfile.isEmpty() && normalized.startsWith(userProfile)) ||
           (!appData.isEmpty() && normalized.startsWith(appData)) ||
           (!localAppData.isEmpty() && normalized.startsWith(localAppData)) ||
           (!tempPath.isEmpty() && normalized.startsWith(tempPath));
}

}  // namespace voidcare::platform::windows
