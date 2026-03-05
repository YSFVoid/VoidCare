#pragma once

#include <QThread>
#include <QVariantMap>

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>

#include "ui_state.h"

namespace voidcare::app {

enum class BackendEventType {
    Initialized,
    Snapshot,
    ActionResult,
    LogLine,
    FatalError,
};

struct BackendEvent {
    BackendEventType type = BackendEventType::FatalError;
    quint64 requestId = 0;
    QString scope;
    QString method;
    ActionResult actionResult;
    UiSnapshot snapshot;
    QString logLine;
    bool logIsError = false;
    QString message;
};

class BackendDispatcher {
public:
    BackendDispatcher();
    ~BackendDispatcher();

    BackendDispatcher(const BackendDispatcher&) = delete;
    BackendDispatcher& operator=(const BackendDispatcher&) = delete;

    bool start();
    void stop();

    quint64 submit(const QString& method, const QVariantMap& args = {});
    bool pollEvent(BackendEvent* outEvent);
    bool isRunning() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace voidcare::app
