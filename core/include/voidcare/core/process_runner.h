#pragma once

#include <QObject>

#include <atomic>

#include "voidcare/core/types.h"

namespace voidcare::core {

class ProcessRunner : public QObject {
public:
    explicit ProcessRunner(QObject* parent = nullptr);

    ProcessRunResult run(const ProcessRunRequest& request,
                         const LogCallback& logCallback = {},
                         std::atomic_bool* cancelToken = nullptr) const;
};

}  // namespace voidcare::core
