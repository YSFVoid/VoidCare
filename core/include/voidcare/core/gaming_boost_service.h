#pragma once

#include <QObject>

#include "voidcare/core/process_runner.h"
#include "voidcare/core/types.h"

namespace voidcare::core {

class GamingBoostService : public QObject {
public:
    explicit GamingBoostService(ProcessRunner* runner, QObject* parent = nullptr);

    AntivirusActionResult enableBoost(const LogCallback& logCallback = {}) const;
    AntivirusActionResult revertBoost(const LogCallback& logCallback = {}) const;

private:
    ProcessRunner* m_runner = nullptr;
};

}  // namespace voidcare::core
