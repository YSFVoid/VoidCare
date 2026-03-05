#include <Windows.h>
#include <d3d11.h>
#include <shellscalingapi.h>
#include <tchar.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "backend_dispatcher.h"
#include "ui_state.h"
#include "ui_theme.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

constexpr const char* kCreditsText = "Developed by Ysf (Lone Wolf Developer)";
constexpr const char* kWarningText = "Suspicious != confirmed malware. Review before deleting.";

constexpr const char* kIconDashboardFa = "\xef\x80\x95";
constexpr const char* kIconSecurityFa = "\xef\x8f\xad";
constexpr const char* kIconOptimizeFa = "\xef\x87\x9e";
constexpr const char* kIconGamingFa = "\xef\x84\x9b";
constexpr const char* kIconAppsFa = "\xef\x86\xb3";
constexpr const char* kIconAboutFa = "\xef\x84\xa9";
constexpr const char* kIconSearchFa = "\xef\x80\x82";
constexpr const char* kIconConfigFa = "\xef\x80\x93";

constexpr const char* kIconDashboardFallback = "D";
constexpr const char* kIconSecurityFallback = "S";
constexpr const char* kIconOptimizeFallback = "O";
constexpr const char* kIconGamingFallback = "G";
constexpr const char* kIconAppsFallback = "A";
constexpr const char* kIconAboutFallback = "i";
constexpr const char* kIconSearchFallback = "*";
constexpr const char* kIconConfigFallback = "C";

constexpr int kMaxLogLines = 1200;

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
UINT g_ResizeWidth = 0;
UINT g_ResizeHeight = 0;
float g_PendingDpiScale = 1.0f;
bool g_RequestDpiReload = false;

bool createDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    if (D3D11CreateDeviceAndSwapChain(nullptr,
                                      D3D_DRIVER_TYPE_HARDWARE,
                                      nullptr,
                                      createDeviceFlags,
                                      featureLevelArray,
                                      2,
                                      D3D11_SDK_VERSION,
                                      &sd,
                                      &g_pSwapChain,
                                      &g_pd3dDevice,
                                      &featureLevel,
                                      &g_pd3dDeviceContext) != S_OK) {
        return false;
    }
    return true;
}

void createRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer != nullptr) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void cleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

void cleanupDeviceD3D() {
    cleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

std::string toUtf8(const QString& value) {
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

bool containsSearch(const QString& value, const QString& needle) {
    return needle.isEmpty() || value.contains(needle, Qt::CaseInsensitive);
}

QStringList setToStringList(const QSet<QString>& values) {
    QStringList out;
    out.reserve(values.size());
    for (const QString& value : values) {
        out.push_back(value);
    }
    return out;
}

QStringList splitRoots(const QString& raw) {
    QString normalized = raw;
    normalized.replace('\n', ';');
    normalized.replace(',', ';');
    const QStringList parts = normalized.split(';', Qt::SkipEmptyParts);
    QStringList roots;
    roots.reserve(parts.size());
    for (QString part : parts) {
        part = part.trimmed();
        if (!part.isEmpty()) {
            roots.push_back(part);
        }
    }
    return roots;
}

std::string toShortUtf8(const QString& value, const int maxChars = 120) {
    QString cut = value;
    if (cut.size() > maxChars) {
        cut = cut.left(maxChars - 1) + QStringLiteral("...");
    }
    return toUtf8(cut);
}

const char* boolText(const bool value) {
    return value ? "On" : "Off";
}

bool isLikelyUnavailableAntivirus(const voidcare::app::UiSnapshot& snapshot) {
    const QString provider = snapshot.antivirusProviderName.trimmed();
    const QString status = snapshot.antivirusStatus.trimmed();
    return provider.isEmpty() || provider.compare(QStringLiteral("None"), Qt::CaseInsensitive) == 0 ||
           status.contains(QStringLiteral("No AV"), Qt::CaseInsensitive);
}

QString nowStamp() {
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString formatFileSize(const qint64 bytes) {
    const double kb = static_cast<double>(bytes) / 1024.0;
    if (kb < 1024.0) {
        return QStringLiteral("%1 KB").arg(QString::number(kb, 'f', 1));
    }
    const double mb = kb / 1024.0;
    if (mb < 1024.0) {
        return QStringLiteral("%1 MB").arg(QString::number(mb, 'f', 1));
    }
    const double gb = mb / 1024.0;
    return QStringLiteral("%1 GB").arg(QString::number(gb, 'f', 1));
}

float clamp01(const float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

struct CpuSampler {
    ULARGE_INTEGER idlePrev = {};
    ULARGE_INTEGER kernelPrev = {};
    ULARGE_INTEGER userPrev = {};
    bool initialized = false;
};

float sampleCpuPercent(CpuSampler* sampler) {
    if (sampler == nullptr) {
        return 0.0f;
    }

    FILETIME idleTime;
    FILETIME kernelTime;
    FILETIME userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        return 0.0f;
    }

    ULARGE_INTEGER idle = {};
    ULARGE_INTEGER kernel = {};
    ULARGE_INTEGER user = {};
    idle.LowPart = idleTime.dwLowDateTime;
    idle.HighPart = idleTime.dwHighDateTime;
    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart = kernelTime.dwHighDateTime;
    user.LowPart = userTime.dwLowDateTime;
    user.HighPart = userTime.dwHighDateTime;

    if (!sampler->initialized) {
        sampler->idlePrev = idle;
        sampler->kernelPrev = kernel;
        sampler->userPrev = user;
        sampler->initialized = true;
        return 0.0f;
    }

    const unsigned long long idleDelta = idle.QuadPart - sampler->idlePrev.QuadPart;
    const unsigned long long kernelDelta = kernel.QuadPart - sampler->kernelPrev.QuadPart;
    const unsigned long long userDelta = user.QuadPart - sampler->userPrev.QuadPart;
    const unsigned long long totalDelta = kernelDelta + userDelta;

    sampler->idlePrev = idle;
    sampler->kernelPrev = kernel;
    sampler->userPrev = user;

    if (totalDelta == 0) {
        return 0.0f;
    }

    const float idleFraction = static_cast<float>(idleDelta) / static_cast<float>(totalDelta);
    return clamp01(1.0f - idleFraction) * 100.0f;
}

float sampleMemoryPercent() {
    MEMORYSTATUSEX statex = {};
    statex.dwLength = sizeof(statex);
    if (!GlobalMemoryStatusEx(&statex)) {
        return 0.0f;
    }
    return static_cast<float>(statex.dwMemoryLoad);
}

float sampleDiskPercent() {
    wchar_t windowsDir[MAX_PATH] = {};
    if (GetWindowsDirectoryW(windowsDir, MAX_PATH) == 0) {
        return 0.0f;
    }

    wchar_t rootPath[MAX_PATH] = {};
    if (GetVolumePathNameW(windowsDir, rootPath, MAX_PATH) == 0) {
        return 0.0f;
    }

    ULARGE_INTEGER freeBytes = {};
    ULARGE_INTEGER totalBytes = {};
    if (!GetDiskFreeSpaceExW(rootPath, &freeBytes, &totalBytes, nullptr) || totalBytes.QuadPart == 0) {
        return 0.0f;
    }

    const double used = static_cast<double>(totalBytes.QuadPart - freeBytes.QuadPart);
    const double pct = (used / static_cast<double>(totalBytes.QuadPart)) * 100.0;
    return static_cast<float>(pct);
}

std::wstring executableDirectory() {
    wchar_t pathBuffer[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, pathBuffer, MAX_PATH);
    std::filesystem::path path(pathBuffer);
    return path.parent_path().wstring();
}

struct FontPack {
    ImFont* ui = nullptr;
    ImFont* mono = nullptr;
    bool iconLoaded = false;
};

FontPack loadFonts(const float dpiScale, const std::wstring& exeDir) {
    FontPack pack;
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const float bodySize = 16.0f * dpiScale;
    const float monoSize = 14.0f * dpiScale;

    pack.ui = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", bodySize);
    if (pack.ui == nullptr) {
        pack.ui = io.Fonts->AddFontDefault();
    }

    pack.mono = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", monoSize);
    if (pack.mono == nullptr) {
        pack.mono = pack.ui;
    }

    std::filesystem::path iconPath = std::filesystem::path(exeDir) / "assets" / "fa-solid-900.ttf";
    if (std::filesystem::exists(iconPath)) {
        static const ImWchar iconRanges[] = {0xf000, 0xf8ff, 0};
        ImFontConfig cfg;
        cfg.MergeMode = true;
        cfg.PixelSnapH = true;
        cfg.GlyphMinAdvanceX = bodySize * 0.95f;
        const std::string iconPathNarrow = iconPath.string();
        if (io.Fonts->AddFontFromFileTTF(iconPathNarrow.c_str(), bodySize, &cfg, iconRanges) != nullptr) {
            pack.iconLoaded = true;
        }
    }

    io.FontDefault = pack.ui;
    return pack;
}

void drawStatusChip(const std::string& text, const ImVec4& bgColor, const ImVec4& textColor) {
    const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
    const ImVec2 padding(10.0f, 5.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);
    const ImRect rect(pos, pos + size);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(rect.Min, rect.Max, ImGui::GetColorU32(bgColor), size.y * 0.5f);
    drawList->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(ImVec4(bgColor.x, bgColor.y, bgColor.z, 0.85f)), size.y * 0.5f);
    drawList->AddText(ImVec2(rect.Min.x + padding.x, rect.Min.y + padding.y), ImGui::GetColorU32(textColor), text.c_str());
    ImGui::Dummy(size);
}

struct NavItem {
    voidcare::app::PageId page;
    const char* id;
    const char* label;
    const char* iconFa;
    const char* iconFallback;
};

enum class ActionKind {
    None,
    Scan,
    Optimize,
};

struct PendingRequest {
    QString method;
    ActionKind kind = ActionKind::None;
    bool guarded = false;
    bool overrideAttempt = false;
    QVariantMap baseArgs;
};

struct ConfirmDialogState {
    bool active = false;
    bool openNextFrame = false;
    bool overrideStage = false;
    QString title;
    QString message;
    QString method;
    QVariantMap args;
    ActionKind kind = ActionKind::None;
};

struct RuntimeState {
    voidcare::app::BackendDispatcher dispatcher;
    voidcare::app::UiState ui;
    std::unordered_map<quint64, PendingRequest> pending;
    int pendingCount = 0;
    bool initialized = false;
    QString lastScanStamp = QStringLiteral("Never");
    QString lastOptimizeStamp = QStringLiteral("Never");
    std::vector<float> cpuHistory;
    float cpuPercent = 0.0f;
    float ramPercent = 0.0f;
    float diskPercent = 0.0f;
    CpuSampler cpuSampler;
    std::chrono::steady_clock::time_point nextSampleTime = std::chrono::steady_clock::now();
    ConfirmDialogState confirmDialog;
    std::array<char, 256> searchBuffer = {};
    std::array<char, 512> customScanPath = {};
    std::array<char, 1024> fullScanRoots = {};
    std::array<char, 512> externalScannerExe = {};
    std::array<char, 512> externalScannerArgs = {};
    std::array<char, 512> restoreDestination = {};
    float pageFade = 1.0f;
    voidcare::app::PageId renderedPage = voidcare::app::PageId::Dashboard;
    int configIndex = 0;
    FontPack fonts;
    std::wstring exeDir;
};

void pruneSelections(voidcare::app::UiState* state) {
    if (state == nullptr) {
        return;
    }

    if (state->selectedPersistenceIndex >= state->snapshot.persistenceEntries.size()) {
        state->selectedPersistenceIndex = -1;
    }

    QSet<QString> suspiciousKeys;
    for (const auto& row : state->snapshot.suspiciousEntries) {
        suspiciousKeys.insert(row.path);
    }
    QSet<QString> quarantineKeys;
    for (const auto& row : state->snapshot.quarantineEntries) {
        quarantineKeys.insert(row.quarantinePath);
    }

    for (auto it = state->selectedSuspicious.begin(); it != state->selectedSuspicious.end();) {
        if (!suspiciousKeys.contains(*it)) {
            it = state->selectedSuspicious.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = state->selectedQuarantine.begin(); it != state->selectedQuarantine.end();) {
        if (!quarantineKeys.contains(*it)) {
            it = state->selectedQuarantine.erase(it);
        } else {
            ++it;
        }
    }
}

void setSnapshot(RuntimeState* state, const voidcare::app::UiSnapshot& snapshot) {
    if (state == nullptr) {
        return;
    }
    state->ui.snapshot = snapshot;
    state->ui.page = voidcare::app::pageIdFromString(snapshot.currentPage);
    state->ui.discordToggle = snapshot.discordEnabled;
    pruneSelections(&state->ui);
}

void setStatus(RuntimeState* state, const QString& text, const bool isError) {
    if (state == nullptr) {
        return;
    }
    state->ui.statusText = text;
    state->ui.statusIsError = isError;
}

quint64 submitRequest(RuntimeState* state,
                     const QString& method,
                     const QVariantMap& args,
                     const ActionKind kind,
                     const bool guarded,
                     const bool overrideAttempt,
                     const QVariantMap& baseArgs) {
    if (state == nullptr) {
        return 0;
    }

    const quint64 requestId = state->dispatcher.submit(method, args);
    if (requestId == 0) {
        setStatus(state, QStringLiteral("Failed to dispatch action: %1").arg(method), true);
        return 0;
    }

    PendingRequest pending;
    pending.method = method;
    pending.kind = kind;
    pending.guarded = guarded;
    pending.overrideAttempt = overrideAttempt;
    pending.baseArgs = baseArgs;
    state->pending.emplace(requestId, std::move(pending));
    state->pendingCount += 1;
    setStatus(state, QStringLiteral("Running: %1").arg(method), false);
    return requestId;
}

void queueGuardedAction(RuntimeState* state,
                        const QString& title,
                        const QString& message,
                        const QString& method,
                        const QVariantMap& baseArgs,
                        const ActionKind kind,
                        const bool overrideStage) {
    if (state == nullptr) {
        return;
    }

    state->confirmDialog.active = true;
    state->confirmDialog.openNextFrame = true;
    state->confirmDialog.overrideStage = overrideStage;
    state->confirmDialog.title = title;
    state->confirmDialog.message = message;
    state->confirmDialog.method = method;
    state->confirmDialog.args = baseArgs;
    state->confirmDialog.kind = kind;
}

void submitGuarded(RuntimeState* state, const bool overrideAttempt) {
    if (state == nullptr || !state->confirmDialog.active) {
        return;
    }

    QVariantMap args = state->confirmDialog.args;
    args.insert(QStringLiteral("initialConfirmed"), true);
    args.insert(QStringLiteral("proceedWithoutRestorePoint"), overrideAttempt);
    submitRequest(state,
                  state->confirmDialog.method,
                  args,
                  state->confirmDialog.kind,
                  true,
                  overrideAttempt,
                  state->confirmDialog.args);
    state->confirmDialog = {};
}

void processBackendEvents(RuntimeState* state) {
    if (state == nullptr) {
        return;
    }

    constexpr int maxEventsPerFrame = 256;
    int processed = 0;
    voidcare::app::BackendEvent event;
    while (processed < maxEventsPerFrame && state->dispatcher.pollEvent(&event)) {
        ++processed;
        switch (event.type) {
        case voidcare::app::BackendEventType::Initialized: {
            setSnapshot(state, event.snapshot);
            state->initialized = true;
            state->renderedPage = state->ui.page;
            state->pageFade = 1.0f;
            setStatus(state, QStringLiteral("Ready."), false);
            break;
        }
        case voidcare::app::BackendEventType::Snapshot: {
            if (!event.snapshot.currentPage.isEmpty()) {
                setSnapshot(state, event.snapshot);
            }
            break;
        }
        case voidcare::app::BackendEventType::LogLine: {
            state->ui.snapshot.logs.push_back(event.logLine);
            while (state->ui.snapshot.logs.size() > kMaxLogLines) {
                state->ui.snapshot.logs.removeFirst();
            }
            break;
        }
        case voidcare::app::BackendEventType::ActionResult: {
            if (!event.snapshot.currentPage.isEmpty()) {
                setSnapshot(state, event.snapshot);
            }

            PendingRequest pending;
            const auto pendingIt = state->pending.find(event.requestId);
            if (pendingIt != state->pending.end()) {
                pending = pendingIt->second;
                state->pending.erase(pendingIt);
                state->pendingCount = std::max(0, state->pendingCount - 1);
            }

            if (!event.actionResult.success && event.actionResult.needsRestoreOverride && pending.guarded &&
                !pending.overrideAttempt) {
                QString restoreMessage = event.actionResult.restoreDetail.trimmed();
                if (restoreMessage.isEmpty()) {
                    restoreMessage = event.actionResult.message;
                }
                queueGuardedAction(state,
                                   QStringLiteral("Restore Point Failed"),
                                   QStringLiteral("%1\n\nContinue anyway without restore point?")
                                       .arg(restoreMessage),
                                   pending.method,
                                   pending.baseArgs,
                                   pending.kind,
                                   true);
            }

            if (event.actionResult.success) {
                if (pending.kind == ActionKind::Scan) {
                    state->lastScanStamp = nowStamp();
                } else if (pending.kind == ActionKind::Optimize) {
                    state->lastOptimizeStamp = nowStamp();
                }
            }

            setStatus(state, event.actionResult.message, !event.actionResult.success);
            break;
        }
        case voidcare::app::BackendEventType::FatalError: {
            setStatus(state, event.message, true);
            break;
        }
        }
    }
}

const char* pageName(const voidcare::app::PageId page) {
    switch (page) {
    case voidcare::app::PageId::Dashboard:
        return "Dashboard";
    case voidcare::app::PageId::Security:
        return "Security";
    case voidcare::app::PageId::Optimize:
        return "Optimize";
    case voidcare::app::PageId::Gaming:
        return "Gaming";
    case voidcare::app::PageId::Apps:
        return "Apps";
    case voidcare::app::PageId::About:
        return "About";
    }
    return "Dashboard";
}

void navigateTo(RuntimeState* state, const voidcare::app::PageId target) {
    if (state == nullptr || state->ui.page == target) {
        return;
    }
    state->ui.page = target;
    state->pageFade = 0.0f;
    QVariantMap args;
    args.insert(QStringLiteral("page"), voidcare::app::pageIdToString(target));
    submitRequest(state, QStringLiteral("navigate"), args, ActionKind::None, false, false, {});
}

void updateSystemMetrics(RuntimeState* state) {
    if (state == nullptr) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now < state->nextSampleTime) {
        return;
    }

    state->cpuPercent = sampleCpuPercent(&state->cpuSampler);
    state->ramPercent = sampleMemoryPercent();
    state->diskPercent = sampleDiskPercent();

    state->cpuHistory.push_back(state->cpuPercent);
    if (state->cpuHistory.size() > 120) {
        state->cpuHistory.erase(state->cpuHistory.begin());
    }

    state->nextSampleTime = now + std::chrono::milliseconds(800);
}

void drawMetricTile(const char* title, const QString& value, const float height) {
    const ImVec2 tileSize((ImGui::GetContentRegionAvail().x - 24.0f) / 3.0f, height);
    ImGui::BeginChild(title, tileSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
    ImGui::TextColored(ImVec4(0.64f, 0.64f, 0.75f, 1.0f), "%s", title);
    const std::string valueUtf8 = toUtf8(value);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.98f, 1.0f));
    ImGui::SetWindowFontScale(1.1f);
    ImGui::TextUnformatted(valueUtf8.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::EndChild();
}

void renderLogsPanel(RuntimeState* state, const float height, const bool showClearButton) {
    if (state == nullptr) {
        return;
    }

    if (voidcare::app::theme::beginCard("logs_card", ImVec2(0.0f, height))) {
        voidcare::app::theme::cardHeader("Logs", "Live backend output");
        if (ImGui::Button("Copy Logs")) {
            const QString joined = state->ui.snapshot.logs.join(QStringLiteral("\n"));
            const std::string utf8 = toUtf8(joined);
            ImGui::SetClipboardText(utf8.c_str());
        }
        if (showClearButton) {
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                submitRequest(state, QStringLiteral("clear_logs"), {}, ActionKind::None, false, false, {});
            }
        }
        ImGui::Separator();
        ImGui::BeginChild("log_console", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushFont(state->fonts.mono ? state->fonts.mono : ImGui::GetFont());
        for (const QString& line : state->ui.snapshot.logs) {
            const std::string utf8 = toUtf8(line);
            ImGui::TextUnformatted(utf8.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 12.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::PopFont();
        ImGui::EndChild();
    }
    voidcare::app::theme::endCard();
}

void renderDashboard(RuntimeState* state) {
    if (state == nullptr) {
        return;
    }

    const float contentWidth = ImGui::GetContentRegionAvail().x;
    const bool twoColumns = contentWidth > 1120.0f;
    if (ImGui::BeginTable("dashboard_cards",
                          twoColumns ? 2 : 1,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("sys_health_card", ImVec2(0.0f, 320.0f))) {
            voidcare::app::theme::cardHeader("System Health", "Live CPU, RAM, and disk utilization");
            drawMetricTile("CPU", QStringLiteral("%1%").arg(QString::number(state->cpuPercent, 'f', 1)), 78.0f);
            ImGui::SameLine();
            drawMetricTile("RAM", QStringLiteral("%1%").arg(QString::number(state->ramPercent, 'f', 1)), 78.0f);
            ImGui::SameLine();
            drawMetricTile("Disk", QStringLiteral("%1%").arg(QString::number(state->diskPercent, 'f', 1)), 78.0f);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.78f, 1.0f), "Last scan:");
            ImGui::SameLine();
            ImGui::TextUnformatted(toUtf8(state->lastScanStamp).c_str());
            ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.78f, 1.0f), "Last optimize:");
            ImGui::SameLine();
            ImGui::TextUnformatted(toUtf8(state->lastOptimizeStamp).c_str());

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.78f, 1.0f), "CPU Trend");
            if (!state->cpuHistory.empty()) {
                ImGui::PlotLines("##cpu_history",
                                 state->cpuHistory.data(),
                                 static_cast<int>(state->cpuHistory.size()),
                                 0,
                                 nullptr,
                                 0.0f,
                                 100.0f,
                                 ImVec2(0.0f, 72.0f));
            } else {
                ImGui::BeginChild("cpu_empty", ImVec2(0.0f, 72.0f), true);
                ImGui::TextColored(ImVec4(0.60f, 0.60f, 0.74f, 1.0f), "Collecting metrics...");
                ImGui::EndChild();
            }
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("antivirus_card", ImVec2(0.0f, 320.0f))) {
            voidcare::app::theme::cardHeader("Antivirus", "Provider and scan controls");

            if (isLikelyUnavailableAntivirus(state->ui.snapshot)) {
                drawStatusChip("Unavailable", ImVec4(0.38f, 0.18f, 0.22f, 1.0f), ImVec4(1.0f, 0.84f, 0.84f, 1.0f));
            } else if (state->ui.snapshot.defenderScanAvailable || state->ui.snapshot.externalScannerAvailable) {
                drawStatusChip("Active", ImVec4(0.26f, 0.20f, 0.56f, 1.0f), ImVec4(0.95f, 0.93f, 1.0f, 1.0f));
            } else {
                drawStatusChip("Passive", ImVec4(0.25f, 0.28f, 0.36f, 1.0f), ImVec4(0.90f, 0.90f, 0.96f, 1.0f));
            }

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.78f, 1.0f), "Provider");
            ImGui::TextUnformatted(toUtf8(state->ui.snapshot.antivirusProviderName).c_str());
            ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.78f, 1.0f), "Status");
            ImGui::TextWrapped("%s", toUtf8(state->ui.snapshot.antivirusStatus).c_str());
            ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.78f, 1.0f), "Last scan");
            ImGui::TextUnformatted(toUtf8(state->lastScanStamp).c_str());

            ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 54.0f);
            if (!state->ui.snapshot.defenderScanAvailable) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Quick Scan", ImVec2(110.0f, 34.0f))) {
                submitRequest(state, QStringLiteral("run_defender_quick_scan"), {}, ActionKind::Scan, false, false, {});
            }
            ImGui::SameLine();
            if (ImGui::Button("Full Scan", ImVec2(110.0f, 34.0f))) {
                submitRequest(state, QStringLiteral("run_defender_full_scan"), {}, ActionKind::Scan, false, false, {});
            }
            if (!state->ui.snapshot.defenderScanAvailable) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            if (!state->ui.snapshot.defenderRemediationAvailable) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Auto-Remediate", ImVec2(130.0f, 34.0f))) {
                submitRequest(state,
                              QStringLiteral("run_defender_auto_remediate"),
                              {},
                              ActionKind::Scan,
                              false,
                              false,
                              {});
            }
            if (!state->ui.snapshot.defenderRemediationAvailable) {
                ImGui::EndDisabled();
            }
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("dashboard_actions_card", ImVec2(0.0f, 250.0f))) {
            voidcare::app::theme::cardHeader("Quick Actions", "Common operations");
            if (ImGui::Button("Refresh Antivirus", ImVec2(170.0f, 34.0f))) {
                submitRequest(state, QStringLiteral("refresh_antivirus"), {}, ActionKind::None, false, false, {});
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh Health", ImVec2(170.0f, 34.0f))) {
                submitRequest(state, QStringLiteral("refresh_health_report"), {}, ActionKind::None, false, false, {});
            }

            if (ImGui::Button("Open Security Page", ImVec2(170.0f, 34.0f))) {
                navigateTo(state, voidcare::app::PageId::Security);
            }
            ImGui::SameLine();
            if (ImGui::Button("Open Optimize Page", ImVec2(170.0f, 34.0f))) {
                navigateTo(state, voidcare::app::PageId::Optimize);
            }
            ImGui::Spacing();
            ImGui::TextWrapped("%s", toUtf8(state->ui.snapshot.healthSummary).c_str());
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        renderLogsPanel(state, 250.0f, false);
        ImGui::EndTable();
    }
}

void renderPersistenceTable(RuntimeState* state, const QString& searchNeedle) {
    if (state == nullptr) {
        return;
    }

    if (ImGui::BeginTable("persistence_table",
                          5,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 235.0f))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.1f);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Publisher", ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn("Signature", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 78.0f);
        ImGui::TableHeadersRow();

        int visibleIndex = -1;
        for (int i = 0; i < state->ui.snapshot.persistenceEntries.size(); ++i) {
            const auto& row = state->ui.snapshot.persistenceEntries[i];
            const QString hay = row.name + QStringLiteral(" ") + row.path + QStringLiteral(" ") + row.publisher;
            if (!containsSearch(hay, searchNeedle)) {
                continue;
            }

            ++visibleIndex;
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool selected = (state->ui.selectedPersistenceIndex == i);
            if (ImGui::Selectable(toShortUtf8(row.name).c_str(),
                                  selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                state->ui.selectedPersistenceIndex = i;
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(toShortUtf8(row.sourceType, 42).c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(toShortUtf8(row.publisher, 44).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(toShortUtf8(row.signatureStatus, 26).c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(row.enabled ? "Yes" : "No");
            ImGui::PopID();
        }

        if (visibleIndex < 0) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImVec4(0.62f, 0.62f, 0.74f, 1.0f), "No persistence entries match the current filter.");
        }
        ImGui::EndTable();
    }
}

void renderSuspiciousAndQuarantineTables(RuntimeState* state, const QString& searchNeedle) {
    if (state == nullptr) {
        return;
    }

    ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.78f, 1.0f), "Suspicious Candidates");
    if (ImGui::BeginTable("suspicious_table",
                          5,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 185.0f))) {
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 2.6f);
        ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableSetupColumn("Signature", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Reasons", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < state->ui.snapshot.suspiciousEntries.size(); ++i) {
            const auto& row = state->ui.snapshot.suspiciousEntries[i];
            const QString reasonText = row.reasons.join(QStringLiteral(", "));
            const QString hay = row.path + QStringLiteral(" ") + reasonText + QStringLiteral(" ") + row.signatureStatus;
            if (!containsSearch(hay, searchNeedle)) {
                continue;
            }

            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool selected = state->ui.selectedSuspicious.contains(row.path);
            if (ImGui::Selectable(toShortUtf8(row.path, 92).c_str(),
                                  selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                if (selected) {
                    state->ui.selectedSuspicious.remove(row.path);
                } else {
                    state->ui.selectedSuspicious.insert(row.path);
                }
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", row.score);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(toShortUtf8(row.signatureStatus, 24).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(toUtf8(formatFileSize(row.size)).c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(toShortUtf8(reasonText, 54).c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.78f, 1.0f), "Quarantine Vault");
    if (ImGui::BeginTable("quarantine_table",
                          4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 130.0f))) {
        ImGui::TableSetupColumn("Quarantine Path", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Original Path", ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableSetupColumn("SHA256", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < state->ui.snapshot.quarantineEntries.size(); ++i) {
            const auto& row = state->ui.snapshot.quarantineEntries[i];
            const QString hay = row.quarantinePath + QStringLiteral(" ") + row.originalPath + QStringLiteral(" ") + row.sha256;
            if (!containsSearch(hay, searchNeedle)) {
                continue;
            }
            ImGui::PushID(i + 7000);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool selected = state->ui.selectedQuarantine.contains(row.quarantinePath);
            if (ImGui::Selectable(toShortUtf8(row.quarantinePath, 72).c_str(),
                                  selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                if (selected) {
                    state->ui.selectedQuarantine.remove(row.quarantinePath);
                } else {
                    state->ui.selectedQuarantine.insert(row.quarantinePath);
                }
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(toShortUtf8(row.originalPath, 62).c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(toShortUtf8(row.sha256, 36).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(toShortUtf8(row.timestamp, 26).c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void renderSecurity(RuntimeState* state, const QString& searchNeedle) {
    if (state == nullptr) {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.19f, 0.13f, 0.34f, 0.86f));
    ImGui::BeginChild("security_warning", ImVec2(0.0f, 42.0f), true);
    ImGui::TextColored(ImVec4(0.94f, 0.92f, 1.0f, 1.0f), "%s", kWarningText);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    const bool twoColumns = ImGui::GetContentRegionAvail().x > 1180.0f;
    if (ImGui::BeginTable("security_cards",
                          twoColumns ? 2 : 1,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("defender_scans_card", ImVec2(0.0f, 320.0f))) {
            voidcare::app::theme::cardHeader("Defender Scans", "Quick / Full / Custom");
            if (state->ui.snapshot.defenderScanAvailable) {
                drawStatusChip("Defender Available",
                               ImVec4(0.27f, 0.21f, 0.56f, 1.0f),
                               ImVec4(0.96f, 0.95f, 1.0f, 1.0f));
            } else {
                drawStatusChip("Defender Unavailable",
                               ImVec4(0.39f, 0.18f, 0.22f, 1.0f),
                               ImVec4(1.0f, 0.87f, 0.87f, 1.0f));
            }
            ImGui::Spacing();
            ImGui::TextWrapped("%s", toUtf8(state->ui.snapshot.antivirusStatus).c_str());
            ImGui::InputTextWithHint("##custom_scan_path",
                                     "Custom path for Defender custom scan",
                                     state->customScanPath.data(),
                                     state->customScanPath.size());

            if (!state->ui.snapshot.defenderScanAvailable) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Quick Scan", ImVec2(120.0f, 34.0f))) {
                submitRequest(state, QStringLiteral("run_defender_quick_scan"), {}, ActionKind::Scan, false, false, {});
            }
            ImGui::SameLine();
            if (ImGui::Button("Full Scan", ImVec2(120.0f, 34.0f))) {
                submitRequest(state, QStringLiteral("run_defender_full_scan"), {}, ActionKind::Scan, false, false, {});
            }
            ImGui::SameLine();
            if (ImGui::Button("Custom Scan", ImVec2(120.0f, 34.0f))) {
                QVariantMap args;
                args.insert(QStringLiteral("customPath"), QString::fromUtf8(state->customScanPath.data()).trimmed());
                submitRequest(state, QStringLiteral("run_defender_custom_scan"), args, ActionKind::Scan, false, false, {});
            }
            if (!state->ui.snapshot.defenderScanAvailable) {
                ImGui::EndDisabled();
            }
            if (!state->ui.snapshot.defenderRemediationAvailable) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Auto-Remediate", ImVec2(150.0f, 34.0f))) {
                submitRequest(state,
                              QStringLiteral("run_defender_auto_remediate"),
                              {},
                              ActionKind::Scan,
                              false,
                              false,
                              {});
            }
            if (!state->ui.snapshot.defenderRemediationAvailable) {
                ImGui::EndDisabled();
            }

            if (state->ui.snapshot.externalScannerAvailable) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.66f, 0.66f, 0.78f, 1.0f), "External Scanner (Session Only)");
                ImGui::InputTextWithHint("##external_exe", "Executable path", state->externalScannerExe.data(), state->externalScannerExe.size());
                ImGui::InputTextWithHint("##external_args", "Arguments", state->externalScannerArgs.data(), state->externalScannerArgs.size());
                if (ImGui::Button("Configure External", ImVec2(150.0f, 34.0f))) {
                    QVariantMap args;
                    args.insert(QStringLiteral("executable"), QString::fromUtf8(state->externalScannerExe.data()).trimmed());
                    args.insert(QStringLiteral("argsLine"), QString::fromUtf8(state->externalScannerArgs.data()).trimmed());
                    submitRequest(state, QStringLiteral("configure_external_scanner"), args, ActionKind::None, false, false, {});
                }
                ImGui::SameLine();
                if (ImGui::Button("Run External", ImVec2(130.0f, 34.0f))) {
                    submitRequest(state, QStringLiteral("run_external_scanner"), {}, ActionKind::Scan, false, false, {});
                }
            }
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("persistence_card", ImVec2(0.0f, 320.0f))) {
            voidcare::app::theme::cardHeader("Persistence Audit", "Startup folders, Run keys, tasks, services");
            if (ImGui::Button("Refresh Audit", ImVec2(130.0f, 34.0f))) {
                submitRequest(state, QStringLiteral("refresh_persistence_audit"), {}, ActionKind::None, false, false, {});
            }
            ImGui::SameLine();
            const bool hasSelection =
                state->ui.selectedPersistenceIndex >= 0 &&
                state->ui.selectedPersistenceIndex < state->ui.snapshot.persistenceEntries.size();
            if (!hasSelection) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Disable Selected", ImVec2(150.0f, 34.0f))) {
                const auto& row = state->ui.snapshot.persistenceEntries[state->ui.selectedPersistenceIndex];
                QVariantMap args;
                args.insert(QStringLiteral("entryId"), row.id);
                queueGuardedAction(state,
                                   QStringLiteral("Disable Persistence Entry"),
                                   QStringLiteral("Disable selected startup/persistence entry?\n\n%1").arg(row.name),
                                   QStringLiteral("disable_persistence_entry"),
                                   args,
                                   ActionKind::None,
                                   false);
            }
            if (!hasSelection) {
                ImGui::EndDisabled();
            }
            ImGui::Spacing();
            renderPersistenceTable(state, searchNeedle);
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("suspicious_card", ImVec2(0.0f, 380.0f))) {
            voidcare::app::theme::cardHeader("Suspicious Files", "Heuristic highlight only, quarantine-first");
            ImGui::InputTextWithHint("##full_scan_roots",
                                     "Full scan roots (semicolon-separated)",
                                     state->fullScanRoots.data(),
                                     state->fullScanRoots.size());
            if (ImGui::Button("Quick Scan", ImVec2(120.0f, 34.0f))) {
                submitRequest(state, QStringLiteral("run_quick_suspicious_scan"), {}, ActionKind::Scan, false, false, {});
            }
            ImGui::SameLine();
            if (ImGui::Button("Full Scan", ImVec2(120.0f, 34.0f))) {
                QVariantMap args;
                args.insert(QStringLiteral("roots"),
                            splitRoots(QString::fromUtf8(state->fullScanRoots.data()).trimmed()));
                submitRequest(state, QStringLiteral("run_full_suspicious_scan"), args, ActionKind::Scan, false, false, {});
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh Quarantine", ImVec2(150.0f, 34.0f))) {
                submitRequest(state, QStringLiteral("get_state"), {}, ActionKind::None, false, false, {});
            }
            ImGui::Spacing();

            renderSuspiciousAndQuarantineTables(state, searchNeedle);

            if (ImGui::Button("Quarantine Selected", ImVec2(170.0f, 34.0f))) {
                QVariantMap args;
                args.insert(QStringLiteral("filePaths"), setToStringList(state->ui.selectedSuspicious));
                queueGuardedAction(state,
                                   QStringLiteral("Quarantine Suspicious Files"),
                                   QStringLiteral("Move selected suspicious files to quarantine?"),
                                   QStringLiteral("quarantine_selected"),
                                   args,
                                   ActionKind::None,
                                   false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Suspicious Selection", ImVec2(190.0f, 34.0f))) {
                state->ui.selectedSuspicious.clear();
            }
            ImGui::Spacing();
            ImGui::InputTextWithHint("##restore_destination",
                                     "Restore destination override (optional)",
                                     state->restoreDestination.data(),
                                     state->restoreDestination.size());
            if (ImGui::Button("Restore Selected", ImVec2(150.0f, 34.0f))) {
                QVariantMap args;
                args.insert(QStringLiteral("quarantinePaths"), setToStringList(state->ui.selectedQuarantine));
                args.insert(QStringLiteral("destinationOverride"),
                            QString::fromUtf8(state->restoreDestination.data()).trimmed());
                submitRequest(state, QStringLiteral("restore_quarantined"), args, ActionKind::None, false, false, {});
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete Permanently", ImVec2(170.0f, 34.0f))) {
                QVariantMap args;
                args.insert(QStringLiteral("quarantinePaths"), setToStringList(state->ui.selectedQuarantine));
                queueGuardedAction(state,
                                   QStringLiteral("Delete Quarantined Files"),
                                   QStringLiteral("Permanently delete selected quarantined files? This cannot be undone."),
                                   QStringLiteral("delete_quarantined"),
                                   args,
                                   ActionKind::None,
                                   false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Quarantine Selection", ImVec2(190.0f, 34.0f))) {
                state->ui.selectedQuarantine.clear();
            }
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        renderLogsPanel(state, 380.0f, true);
        ImGui::EndTable();
    }
}

void renderOptimize(RuntimeState* state) {
    if (state == nullptr) {
        return;
    }

    const bool twoColumns = ImGui::GetContentRegionAvail().x > 1120.0f;
    if (ImGui::BeginTable("optimize_cards",
                          twoColumns ? 2 : 1,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("safe_opt_card", ImVec2(0.0f, 245.0f))) {
            voidcare::app::theme::cardHeader("Safe Optimization", "Temp cleanup and recycle bin");
            ImGui::TextWrapped("Safe actions still require confirmation and restore-point attempt.");
            if (ImGui::Button("Run Safe Cleanup", ImVec2(170.0f, 34.0f))) {
                queueGuardedAction(state,
                                   QStringLiteral("Safe Optimization"),
                                   QStringLiteral("Run safe cleanup now?"),
                                   QStringLiteral("run_safe_optimization"),
                                   {},
                                   ActionKind::Optimize,
                                   false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh Health", ImVec2(140.0f, 34.0f))) {
                submitRequest(state, QStringLiteral("refresh_health_report"), {}, ActionKind::None, false, false, {});
            }
            ImGui::Spacing();
            ImGui::TextWrapped("%s", toUtf8(state->ui.snapshot.healthSummary).c_str());
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("aggressive_opt_card", ImVec2(0.0f, 245.0f))) {
            voidcare::app::theme::cardHeader("Aggressive Optimization", "Optional and reversible guidance");
            ImGui::Checkbox("Remove selected bloat packages", &state->ui.removeBloat);
            ImGui::Checkbox("Disable Copilot (best-effort)", &state->ui.disableCopilot);
            if (ImGui::Button("Run Aggressive", ImVec2(170.0f, 34.0f))) {
                QVariantMap args;
                args.insert(QStringLiteral("removeBloat"), state->ui.removeBloat);
                args.insert(QStringLiteral("disableCopilot"), state->ui.disableCopilot);
                queueGuardedAction(state,
                                   QStringLiteral("Aggressive Optimization"),
                                   QStringLiteral("Run aggressive optimization options?"),
                                   QStringLiteral("run_aggressive_optimization"),
                                   args,
                                   ActionKind::Optimize,
                                   false);
            }
            ImGui::TextColored(ImVec4(0.67f, 0.67f, 0.78f, 1.0f),
                               "Unchecked by default. Use restore point if you need to revert.");
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        renderLogsPanel(state, 300.0f, true);

        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("optimization_tips_card", ImVec2(0.0f, 300.0f))) {
            voidcare::app::theme::cardHeader("Current Session", "Quick status");
            ImGui::Text("Pending jobs: %d", state->pendingCount);
            ImGui::Text("Last optimize: %s", toUtf8(state->lastOptimizeStamp).c_str());
            ImGui::Text("Discord RPC: %s", boolText(state->ui.discordToggle));
            ImGui::Spacing();
            ImGui::TextWrapped("%s", toUtf8(state->ui.statusText).c_str());
        }
        voidcare::app::theme::endCard();

        ImGui::EndTable();
    }
}

void renderGaming(RuntimeState* state) {
    if (state == nullptr) {
        return;
    }

    static float frameLimit = 144.0f;
    static float sharpness = 55.0f;
    static bool disableNotifications = true;
    static bool disableGameDvr = true;

    const bool twoColumns = ImGui::GetContentRegionAvail().x > 1120.0f;
    if (ImGui::BeginTable("gaming_cards",
                          twoColumns ? 2 : 1,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("gaming_core_card", ImVec2(0.0f, 250.0f))) {
            voidcare::app::theme::cardHeader("Gaming Boost", "High performance plan + safe toggles");
            if (ImGui::Button("Enable Gaming Boost", ImVec2(185.0f, 34.0f))) {
                queueGuardedAction(state,
                                   QStringLiteral("Enable Gaming Boost"),
                                   QStringLiteral("Apply gaming boost settings?"),
                                   QStringLiteral("enable_gaming_boost"),
                                   {},
                                   ActionKind::None,
                                   false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Revert Boost", ImVec2(150.0f, 34.0f))) {
                queueGuardedAction(state,
                                   QStringLiteral("Revert Gaming Boost"),
                                   QStringLiteral("Revert gaming boost settings?"),
                                   QStringLiteral("revert_gaming_boost"),
                                   {},
                                   ActionKind::None,
                                   false);
            }
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.78f, 1.0f), "Guidance");
            ImGui::BulletText("Use revert if game-specific power settings cause issues.");
            ImGui::BulletText("Restore points are attempted before applying changes.");
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("gaming_toggles_card", ImVec2(0.0f, 250.0f))) {
            voidcare::app::theme::cardHeader("Session Tweaks", "UI-only preset controls");
            ImGui::Text("Frame limiter");
            voidcare::app::theme::sliderBar("frame_limiter", &frameLimit, 60.0f, 240.0f, "%.0f FPS");
            ImGui::Text("Image sharpness");
            voidcare::app::theme::sliderBar("image_sharpness", &sharpness, 0.0f, 100.0f, "%.0f%%");
            ImGui::Text("Disable notifications");
            voidcare::app::theme::togglePill("disable_notifications", &disableNotifications, ImVec2(66.0f, 30.0f));
            ImGui::Text("Disable Game DVR");
            voidcare::app::theme::togglePill("disable_gamedvr", &disableGameDvr, ImVec2(66.0f, 30.0f));
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        renderLogsPanel(state, 300.0f, false);

        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("gaming_stats_card", ImVec2(0.0f, 300.0f))) {
            voidcare::app::theme::cardHeader("Runtime Stats", "Quick view");
            ImGui::Text("CPU: %.1f%%", state->cpuPercent);
            ImGui::Text("RAM: %.1f%%", state->ramPercent);
            ImGui::Text("Disk: %.1f%%", state->diskPercent);
            ImGui::Separator();
            ImGui::TextWrapped("%s", toUtf8(state->ui.statusText).c_str());
        }
        voidcare::app::theme::endCard();
        ImGui::EndTable();
    }
}

void renderApps(RuntimeState* state, const QString& searchNeedle) {
    if (state == nullptr) {
        return;
    }

    if (voidcare::app::theme::beginCard("apps_inventory_card", ImVec2(0.0f, 520.0f))) {
        voidcare::app::theme::cardHeader("Apps Manager", "Inventory and launchers only");
        if (ImGui::Button("Refresh Apps", ImVec2(130.0f, 34.0f))) {
            submitRequest(state, QStringLiteral("refresh_installed_apps"), {}, ActionKind::None, false, false, {});
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Apps Settings", ImVec2(165.0f, 34.0f))) {
            submitRequest(state, QStringLiteral("open_apps_settings"), {}, ActionKind::None, false, false, {});
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Programs && Features", ImVec2(205.0f, 34.0f))) {
            submitRequest(state, QStringLiteral("open_programs_features"), {}, ActionKind::None, false, false, {});
        }

        ImGui::Spacing();
        if (ImGui::BeginTable("apps_table",
                              3,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                              ImVec2(0.0f, 410.0f))) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.8f);
            ImGui::TableSetupColumn("Version", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableSetupColumn("Publisher", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < state->ui.snapshot.installedApps.size(); ++i) {
                const auto& row = state->ui.snapshot.installedApps[i];
                if (!containsSearch(row.name + QStringLiteral(" ") + row.publisher, searchNeedle)) {
                    continue;
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(toShortUtf8(row.name, 66).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(toShortUtf8(row.version, 24).c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(toShortUtf8(row.publisher, 40).c_str());
            }

            ImGui::EndTable();
        }
    }
    voidcare::app::theme::endCard();
}

void renderAbout(RuntimeState* state) {
    if (state == nullptr) {
        return;
    }

    const bool twoColumns = ImGui::GetContentRegionAvail().x > 1020.0f;
    if (ImGui::BeginTable("about_cards",
                          twoColumns ? 2 : 1,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("about_main_card", ImVec2(0.0f, 260.0f))) {
            voidcare::app::theme::cardHeader("VoidCare", "Offline admin optimization and security suite");
            ImGui::TextWrapped("VoidCare by VoidTools");
            ImGui::TextWrapped("%s", kCreditsText);
            ImGui::Separator();
            ImGui::TextWrapped("Admin-only, offline-only operation.");
            ImGui::TextWrapped("Defender-only auto-remediation. Suspicious scanning is heuristic-only.");
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("about_discord_card", ImVec2(0.0f, 260.0f))) {
            voidcare::app::theme::cardHeader("Discord Rich Presence", "Local IPC named pipes only");
            ImGui::Text("Current state: %s", boolText(state->ui.discordToggle));
            ImGui::TextWrapped("%s", toUtf8(state->ui.snapshot.discordAboutStatus).c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("Pipes: \\\\?\\pipe\\discord-ipc-0..9");
            ImGui::TextWrapped("No web calls are used.");
        }
        voidcare::app::theme::endCard();

        ImGui::TableNextColumn();
        renderLogsPanel(state, 280.0f, false);

        ImGui::TableNextColumn();
        if (voidcare::app::theme::beginCard("about_status_card", ImVec2(0.0f, 280.0f))) {
            voidcare::app::theme::cardHeader("Session Status", "Runtime details");
            ImGui::Text("Pending jobs: %d", state->pendingCount);
            ImGui::Text("Current page: %s", pageName(state->ui.page));
            ImGui::Text("Last scan: %s", toUtf8(state->lastScanStamp).c_str());
            ImGui::Text("Last optimize: %s", toUtf8(state->lastOptimizeStamp).c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", toUtf8(state->ui.statusText).c_str());
        }
        voidcare::app::theme::endCard();
        ImGui::EndTable();
    }
}

void renderConfirmModal(RuntimeState* state) {
    if (state == nullptr || !state->confirmDialog.active) {
        return;
    }

    if (state->confirmDialog.openNextFrame) {
        ImGui::OpenPopup("ConfirmActionModal");
        state->confirmDialog.openNextFrame = false;
    }

    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("ConfirmActionModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", toUtf8(state->confirmDialog.title).c_str());
        ImGui::Separator();
        ImGui::TextWrapped("%s", toUtf8(state->confirmDialog.message).c_str());
        ImGui::Spacing();
        const char* acceptText = state->confirmDialog.overrideStage ? "Continue Without Restore Point" : "Confirm";
        if (ImGui::Button(acceptText, ImVec2(240.0f, 36.0f))) {
            submitGuarded(state, state->confirmDialog.overrideStage);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(140.0f, 36.0f))) {
            state->confirmDialog = {};
            setStatus(state, QStringLiteral("Action canceled."), false);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    } else {
        state->confirmDialog = {};
    }
}

bool drawSearchBox(RuntimeState* state, const char* iconText) {
    if (state == nullptr) {
        return false;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(30.0f, 9.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.11f, 0.11f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.17f, 0.16f, 0.30f, 1.0f));
    ImGui::SetNextItemWidth(280.0f);
    const bool changed =
        ImGui::InputTextWithHint("##global_search", "Search tables, apps, paths...", state->searchBuffer.data(), state->searchBuffer.size());
    const ImVec2 min = ImGui::GetItemRectMin();
    ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 10.0f, min.y + 8.0f),
                                        ImGui::GetColorU32(ImVec4(0.63f, 0.63f, 0.74f, 1.0f)),
                                        iconText);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    return changed;
}

bool drawDiscordToggle(RuntimeState* state) {
    if (state == nullptr) {
        return false;
    }

    const bool enabled = state->ui.discordToggle;
    const char* label = enabled ? "Discord RPC: On" : "Discord RPC: Off";
    const ImVec2 size(160.0f, 34.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton("discord_rpc_pill", size);
    const bool hovered = ImGui::IsItemHovered();
    const ImRect rect(pos, pos + size);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec4 base = enabled ? ImVec4(0.31f, 0.24f, 0.67f, 1.0f) : ImVec4(0.17f, 0.17f, 0.24f, 1.0f);
    if (hovered) {
        base = ImVec4(base.x + 0.06f, base.y + 0.06f, base.z + 0.06f, 1.0f);
    }
    drawList->AddRectFilled(rect.Min, rect.Max, ImGui::GetColorU32(base), 16.0f);
    drawList->AddRect(rect.Min, rect.Max, ImGui::GetColorU32(ImVec4(0.46f, 0.38f, 0.93f, 0.75f)), 16.0f, 0, 1.0f);
    drawList->AddText(ImVec2(rect.Min.x + 12.0f, rect.Min.y + 9.0f),
                      ImGui::GetColorU32(ImVec4(0.95f, 0.95f, 1.0f, 1.0f)),
                      label);

    return pressed;
}

void renderTopBar(RuntimeState* state, const char* searchIcon, const char* configIcon) {
    if (state == nullptr) {
        return;
    }

    if (voidcare::app::theme::beginCard("topbar_card", ImVec2(0.0f, 74.0f))) {
        drawSearchBox(state, searchIcon);
        ImGui::SameLine();
        static const char* configs[] = {"New Config 1", "Performance", "Balanced"};
        ImGui::TextUnformatted(configIcon);
        ImGui::SameLine();
        voidcare::app::theme::chipCombo("config_chip",
                                        &state->configIndex,
                                        configs,
                                        static_cast<int>(std::size(configs)),
                                        ImVec2(180.0f, 34.0f));

        const float rightStart = ImGui::GetWindowPos().x + ImGui::GetWindowSize().x - 182.0f;
        ImGui::SameLine(rightStart);
        if (drawDiscordToggle(state)) {
            state->ui.discordToggle = !state->ui.discordToggle;
            QVariantMap args;
            args.insert(QStringLiteral("enabled"), state->ui.discordToggle);
            submitRequest(state, QStringLiteral("set_discord_enabled"), args, ActionKind::None, false, false, {});
        }
    }
    voidcare::app::theme::endCard();
}

void renderSidebar(RuntimeState* state,
                   const std::array<NavItem, 6>& navItems,
                   const bool iconFontLoaded) {
    if (state == nullptr) {
        return;
    }

    if (voidcare::app::theme::beginCard("sidebar_card", ImVec2(88.0f, 0.0f))) {
        ImGui::SetCursorPosY(18.0f);
        ImGui::SetCursorPosX(28.0f);
        ImGui::TextColored(ImVec4(0.56f, 0.49f, 1.0f, 1.0f), "VC");
        ImGui::Spacing();
        ImGui::Separator();

        for (const auto& item : navItems) {
            ImGui::SetCursorPosX(14.0f);
            const char* icon = iconFontLoaded ? item.iconFa : item.iconFallback;
            const bool selected = (state->ui.page == item.page);
            if (voidcare::app::theme::sidebarItem(icon, item.id, selected, ImVec2(56.0f, 48.0f))) {
                navigateTo(state, item.page);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", item.label);
            }
        }

        const float bottomY = ImGui::GetWindowSize().y - 102.0f;
        ImGui::SetCursorPosY(std::max(bottomY, ImGui::GetCursorPosY() + 12.0f));
        drawStatusChip("Admin", ImVec4(0.26f, 0.20f, 0.56f, 0.95f), ImVec4(0.97f, 0.96f, 1.0f, 1.0f));
        ImGui::SameLine();
        drawStatusChip("Offline", ImVec4(0.20f, 0.22f, 0.31f, 0.95f), ImVec4(0.92f, 0.92f, 0.96f, 1.0f));
        ImGui::Spacing();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 72.0f);
        ImGui::TextColored(ImVec4(0.66f, 0.66f, 0.78f, 1.0f), "Ysf");
        ImGui::PopTextWrapPos();
    }
    voidcare::app::theme::endCard();
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            g_ResizeWidth = static_cast<UINT>(LOWORD(lParam));
            g_ResizeHeight = static_cast<UINT>(HIWORD(lParam));
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) {
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (const RECT* suggestedRect = reinterpret_cast<RECT*>(lParam)) {
            SetWindowPos(hWnd,
                         nullptr,
                         suggestedRect->left,
                         suggestedRect->top,
                         suggestedRect->right - suggestedRect->left,
                         suggestedRect->bottom - suggestedRect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        g_PendingDpiScale = static_cast<float>(HIWORD(wParam)) / 96.0f;
        g_RequestDpiReload = true;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    int qtArgc = 1;
    char appName[] = "VoidCare";
    char* qtArgv[] = {appName, nullptr};
    QCoreApplication qtApp(qtArgc, qtArgv);

    WNDCLASSEXW wc = {sizeof(WNDCLASSEX),
                      CS_CLASSDC,
                      WndProc,
                      0L,
                      0L,
                      hInstance,
                      nullptr,
                      nullptr,
                      nullptr,
                      nullptr,
                      L"VoidCareImGuiWndClass",
                      nullptr};
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName,
                              L"VoidCare",
                              WS_OVERLAPPEDWINDOW,
                              100,
                              100,
                              1560,
                              940,
                              nullptr,
                              nullptr,
                              wc.hInstance,
                              nullptr);

    if (!createDeviceD3D(hwnd)) {
        cleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    createRenderTarget();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    RuntimeState state;
    state.exeDir = executableDirectory();
    g_PendingDpiScale = static_cast<float>(GetDpiForWindow(hwnd)) / 96.0f;
    if (g_PendingDpiScale <= 0.1f) {
        g_PendingDpiScale = 1.0f;
    }
    state.fonts = loadFonts(g_PendingDpiScale, state.exeDir);
    voidcare::app::theme::applyVoidCareStyle(g_PendingDpiScale);

    const QString defaultRoots = QStringLiteral("C:\\Users\\%1\\Downloads;C:\\Users\\%1\\Desktop;C:\\Users\\%1\\Documents")
                                     .arg(qEnvironmentVariable("USERNAME"));
    const QByteArray defaultRootsUtf8 = defaultRoots.toUtf8();
    std::memcpy(state.fullScanRoots.data(),
                defaultRootsUtf8.constData(),
                std::min(state.fullScanRoots.size() - 1, static_cast<std::size_t>(defaultRootsUtf8.size())));

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    if (!state.dispatcher.start()) {
        setStatus(&state, QStringLiteral("Failed to start backend dispatcher."), true);
    }

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                done = true;
            }
        }
        if (done) {
            break;
        }

        QCoreApplication::processEvents(QEventLoop::AllEvents, 4);
        processBackendEvents(&state);
        updateSystemMetrics(&state);

        if (g_RequestDpiReload) {
            g_RequestDpiReload = false;
            state.fonts = loadFonts(g_PendingDpiScale, state.exeDir);
            voidcare::app::theme::applyVoidCareStyle(g_PendingDpiScale);
            ImGui_ImplDX11_InvalidateDeviceObjects();
            ImGui_ImplDX11_CreateDeviceObjects();
        }

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            cleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            createRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        voidcare::app::theme::drawBackdrop(viewport->Pos, viewport->Size);

        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGuiWindowFlags rootFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("VoidCareRoot", nullptr, rootFlags);

        const char* searchIcon = state.fonts.iconLoaded ? kIconSearchFa : kIconSearchFallback;
        const char* configIcon = state.fonts.iconLoaded ? kIconConfigFa : kIconConfigFallback;

        static const std::array<NavItem, 6> navItems = {{
            {voidcare::app::PageId::Dashboard, "nav_dashboard", "Dashboard", kIconDashboardFa, kIconDashboardFallback},
            {voidcare::app::PageId::Security, "nav_security", "Security", kIconSecurityFa, kIconSecurityFallback},
            {voidcare::app::PageId::Optimize, "nav_optimize", "Optimize", kIconOptimizeFa, kIconOptimizeFallback},
            {voidcare::app::PageId::Gaming, "nav_gaming", "Gaming", kIconGamingFa, kIconGamingFallback},
            {voidcare::app::PageId::Apps, "nav_apps", "Apps", kIconAppsFa, kIconAppsFallback},
            {voidcare::app::PageId::About, "nav_about", "About", kIconAboutFa, kIconAboutFallback},
        }};

        renderSidebar(&state, navItems, state.fonts.iconLoaded);
        ImGui::SameLine();

        ImGui::BeginGroup();
        renderTopBar(&state, searchIcon, configIcon);
        ImGui::Spacing();

        if (state.renderedPage != state.ui.page) {
            state.renderedPage = state.ui.page;
            state.pageFade = 0.0f;
        }
        state.pageFade = std::min(1.0f, state.pageFade + ImGui::GetIO().DeltaTime * 4.8f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, state.pageFade);

        if (voidcare::app::theme::beginCard("content_surface", ImVec2(0.0f, 0.0f))) {
            const QString searchNeedle = QString::fromUtf8(state.searchBuffer.data()).trimmed();
            ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.99f, 1.0f), "%s", pageName(state.ui.page));
            if (state.pendingCount > 0) {
                ImGui::SameLine();
                drawStatusChip("Working...", ImVec4(0.27f, 0.21f, 0.56f, 0.95f), ImVec4(0.95f, 0.95f, 1.0f, 1.0f));
            }
            ImGui::Separator();

            ImGui::BeginChild("page_scroll", ImVec2(0.0f, 0.0f), false);
            switch (state.ui.page) {
            case voidcare::app::PageId::Dashboard:
                renderDashboard(&state);
                break;
            case voidcare::app::PageId::Security:
                renderSecurity(&state, searchNeedle);
                break;
            case voidcare::app::PageId::Optimize:
                renderOptimize(&state);
                break;
            case voidcare::app::PageId::Gaming:
                renderGaming(&state);
                break;
            case voidcare::app::PageId::Apps:
                renderApps(&state, searchNeedle);
                break;
            case voidcare::app::PageId::About:
                renderAbout(&state);
                break;
            }
            ImGui::EndChild();
        }
        voidcare::app::theme::endCard();
        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.64f, 0.64f, 0.75f, 1.0f), "%s", kCreditsText);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextColored(state.ui.statusIsError ? ImVec4(0.93f, 0.45f, 0.45f, 1.0f)
                                                  : ImVec4(0.68f, 0.68f, 0.80f, 1.0f),
                           "%s",
                           toUtf8(state.ui.statusText).c_str());

        ImGui::EndGroup();

        renderConfirmModal(&state);

        ImGui::End();

        ImGui::Render();
        const float clearColor[4] = {0.04f, 0.04f, 0.06f, 1.0f};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    state.dispatcher.stop();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    cleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

