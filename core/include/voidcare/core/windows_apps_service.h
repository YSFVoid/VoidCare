#pragma once

#include <QObject>
#include <QVector>

#include "voidcare/core/types.h"

namespace voidcare::core {

class WindowsAppsService : public QObject {
public:
    explicit WindowsAppsService(QObject* parent = nullptr);

    QVector<InstalledAppInfo> enumerateInstalledApps() const;
    bool openAppsSettings() const;
    bool openProgramsAndFeatures() const;
};

}  // namespace voidcare::core
