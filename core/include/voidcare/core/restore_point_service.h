#pragma once

#include <QObject>

#include "voidcare/core/process_runner.h"
#include "voidcare/core/types.h"

namespace voidcare::core {

class RestorePointService : public QObject {
public:
    explicit RestorePointService(ProcessRunner* runner, QObject* parent = nullptr);

    RestorePointResult createRestorePoint(const QString& actionLabel) const;

private:
    ProcessRunner* m_runner = nullptr;
};

}  // namespace voidcare::core
