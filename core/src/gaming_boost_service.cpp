#include "voidcare/core/gaming_boost_service.h"

namespace voidcare::core {

GamingBoostService::GamingBoostService(ProcessRunner* runner, QObject* parent)
    : QObject(parent)
    , m_runner(runner) {}

AntivirusActionResult GamingBoostService::enableBoost(const LogCallback& logCallback) const {
    if (m_runner == nullptr) {
        return {false, QStringLiteral("Process runner unavailable."), -1};
    }

    bool ok = true;

    ProcessRunRequest powerPlan;
    powerPlan.executable = QStringLiteral("powercfg.exe");
    powerPlan.arguments = {QStringLiteral("/setactive"),
                           QStringLiteral("8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c")};
    const ProcessRunResult powerResult = m_runner->run(powerPlan);
    ok = ok && powerResult.success();

    if (logCallback) {
        logCallback(powerResult.success() ? QStringLiteral("High performance power plan applied.")
                                          : QStringLiteral("Failed to apply high performance plan."),
                    !powerResult.success());
    }

    ProcessRunRequest gameDvr;
    gameDvr.executable = QStringLiteral("powershell.exe");
    gameDvr.arguments = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        QStringLiteral("New-Item -Path 'HKCU:\\System\\GameConfigStore' -Force | Out-Null; Set-ItemProperty -Path 'HKCU:\\System\\GameConfigStore' -Name GameDVR_Enabled -Type DWord -Value 0"),
    };
    const ProcessRunResult gameDvrResult = m_runner->run(gameDvr);
    ok = ok && gameDvrResult.success();

    if (logCallback) {
        logCallback(gameDvrResult.success() ? QStringLiteral("Game DVR toggle updated.")
                                            : QStringLiteral("Game DVR toggle update reported warnings."),
                    !gameDvrResult.success());
    }

    return {ok,
            ok ? QStringLiteral("Gaming boost applied.")
               : QStringLiteral("Gaming boost completed with warnings."),
            ok ? 0 : 1};
}

AntivirusActionResult GamingBoostService::revertBoost(const LogCallback& logCallback) const {
    if (m_runner == nullptr) {
        return {false, QStringLiteral("Process runner unavailable."), -1};
    }

    bool ok = true;

    ProcessRunRequest powerPlan;
    powerPlan.executable = QStringLiteral("powercfg.exe");
    powerPlan.arguments = {QStringLiteral("/setactive"),
                           QStringLiteral("381b4222-f694-41f0-9685-ff5bb260df2e")};
    const ProcessRunResult powerResult = m_runner->run(powerPlan);
    ok = ok && powerResult.success();

    ProcessRunRequest gameDvr;
    gameDvr.executable = QStringLiteral("powershell.exe");
    gameDvr.arguments = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        QStringLiteral("New-Item -Path 'HKCU:\\System\\GameConfigStore' -Force | Out-Null; Set-ItemProperty -Path 'HKCU:\\System\\GameConfigStore' -Name GameDVR_Enabled -Type DWord -Value 1"),
    };
    const ProcessRunResult gameDvrResult = m_runner->run(gameDvr);
    ok = ok && gameDvrResult.success();

    if (logCallback) {
        logCallback(ok ? QStringLiteral("Gaming boost reverted.")
                       : QStringLiteral("Revert completed with warnings."),
                    !ok);
    }

    return {ok,
            ok ? QStringLiteral("Gaming settings reverted.")
               : QStringLiteral("Gaming settings revert reported warnings."),
            ok ? 0 : 1};
}

}  // namespace voidcare::core
