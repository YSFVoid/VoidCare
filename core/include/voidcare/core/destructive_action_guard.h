#pragma once

#include <QObject>

#include "voidcare/core/restore_point_service.h"
#include "voidcare/core/types.h"

namespace voidcare::core {

class DestructiveActionGuard : public QObject {
public:
    explicit DestructiveActionGuard(RestorePointService* restorePointService, QObject* parent = nullptr);

    GuardOutcome evaluate(const QString& actionName,
                          bool userConfirmed,
                          bool proceedWhenRestoreFails) const;

private:
    RestorePointService* m_restorePointService = nullptr;
};

}  // namespace voidcare::core
