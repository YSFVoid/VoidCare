#include <Windows.h>
#include <d3d11.h>
#include <shellscalingapi.h>
#include <tchar.h>
#include <wincodec.h>

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
#include <QByteArray>
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
    bool performancePreset = true;
    FontPack fonts;
    std::wstring exeDir;
    bool logsDrawerExpanded = false;
    ID3D11ShaderResourceView* logoTexture = nullptr;
    int logoWidth = 0;
    int logoHeight = 0;
    bool logoLoadAttempted = false;
};

bool loadTextureFromFileW(const std::wstring& path,
                          ID3D11Device* device,
                          ID3D11ShaderResourceView** outSrv,
                          int* outWidth,
                          int* outHeight) {
    if (device == nullptr || outSrv == nullptr || outWidth == nullptr || outHeight == nullptr) {
        return false;
    }

    *outSrv = nullptr;
    *outWidth = 0;
    *outHeight = 0;

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return false;
    }

    hr = factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) {
        factory->Release();
        return false;
    }

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        decoder->Release();
        factory->Release();
        return false;
    }

    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    hr = converter->Initialize(
        frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    frame->GetSize(&width, &height);
    if (width == 0 || height == 0) {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0);
    const UINT stride = width * 4u;
    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr)) {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subresource = {};
    subresource.pSysMem = pixels.data();
    subresource.SysMemPitch = stride;

    ID3D11Texture2D* texture = nullptr;
    hr = device->CreateTexture2D(&desc, &subresource, &texture);
    if (FAILED(hr) || texture == nullptr) {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(texture, &srvDesc, outSrv);
    texture->Release();

    converter->Release();
    frame->Release();
    decoder->Release();
    factory->Release();

    if (FAILED(hr) || *outSrv == nullptr) {
        return false;
    }

    *outWidth = static_cast<int>(width);
    *outHeight = static_cast<int>(height);
    return true;
}

void ensureSidebarLogoLoaded(RuntimeState* state) {
    if (state == nullptr || state->logoLoadAttempted) {
        return;
    }
    state->logoLoadAttempted = true;

    const std::filesystem::path logoPath = std::filesystem::path(state->exeDir) / "assets" / "logo_vt.png";
    if (!std::filesystem::exists(logoPath)) {
        return;
    }

    loadTextureFromFileW(logoPath.wstring(), g_pd3dDevice, &state->logoTexture, &state->logoWidth, &state->logoHeight);
}

void releaseSidebarLogo(RuntimeState* state) {
    if (state == nullptr) {
        return;
    }
    if (state->logoTexture != nullptr) {
        state->logoTexture->Release();
        state->logoTexture = nullptr;
    }
    state->logoWidth = 0;
    state->logoHeight = 0;
}

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

void renderProviderStatusChip(const voidcare::app::UiSnapshot& snapshot) {
    using voidcare::app::theme::ChipVariant;
    if (isLikelyUnavailableAntivirus(snapshot)) {
        voidcare::app::theme::StatusChip("Unavailable", ChipVariant::Danger);
    } else if (snapshot.defenderScanAvailable || snapshot.externalScannerAvailable) {
        voidcare::app::theme::StatusChip("Active", ChipVariant::Accent);
    } else {
        voidcare::app::theme::StatusChip("Passive", ChipVariant::Neutral);
    }
}

void renderLogsDrawer(RuntimeState* state, const bool allowClearButton) {
    if (state == nullptr) {
        return;
    }

    const char* label = state->logsDrawerExpanded ? "Hide Logs Drawer" : "Show Logs Drawer";
    if (ImGui::Button(label, ImVec2(150.0f, 32.0f))) {
        state->logsDrawerExpanded = !state->logsDrawerExpanded;
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.66f, 0.66f, 0.78f, 1.0f), "Monospace backend output");

    if (!state->logsDrawerExpanded) {
        return;
    }

    const float drawerHeight = std::clamp(ImGui::GetContentRegionAvail().y * 0.36f, 180.0f, 330.0f);
    if (voidcare::app::theme::BeginPanel(
            "logs_drawer_panel", "Logs Drawer", ImVec2(0.0f, drawerHeight), "Live backend stream")) {
        if (ImGui::Button("Copy Logs", ImVec2(110.0f, 32.0f))) {
            const QString joined = state->ui.snapshot.logs.join(QStringLiteral("\n"));
            const std::string utf8 = toUtf8(joined);
            ImGui::SetClipboardText(utf8.c_str());
        }
        if (allowClearButton) {
            ImGui::SameLine();
            if (ImGui::Button("Clear", ImVec2(86.0f, 32.0f))) {
                submitRequest(state, QStringLiteral("clear_logs"), {}, ActionKind::None, false, false, {});
            }
        }
        ImGui::Separator();
        ImGui::BeginChild("logs_console_area", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushFont(state->fonts.mono ? state->fonts.mono : ImGui::GetFont());
        for (const QString& line : state->ui.snapshot.logs) {
            const bool isError = line.contains(QStringLiteral("error"), Qt::CaseInsensitive) ||
                                 line.contains(QStringLiteral("failed"), Qt::CaseInsensitive);
            const bool isWarn = line.contains(QStringLiteral("warn"), Qt::CaseInsensitive);
            const ImVec4 color = isError ? ImVec4(0.97f, 0.48f, 0.49f, 1.0f)
                                         : (isWarn ? ImVec4(0.97f, 0.80f, 0.50f, 1.0f)
                                                   : ImVec4(0.83f, 0.83f, 0.92f, 1.0f));
            ImGui::TextColored(color, "%s", toUtf8(line).c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 12.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::PopFont();
        ImGui::EndChild();
    }
    voidcare::app::theme::EndPanel();
}

void renderDashboard(RuntimeState* state) {
    if (state == nullptr) {
        return;
    }

    if (ImGui::BeginTable(
            "dashboard_two_panel", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel("dashboard_security_panel",
                                             "Security",
                                             ImVec2(0.0f, 0.0f),
                                             "Provider, scan, persistence, suspicious files")) {
            if (voidcare::app::theme::BeginSettingRows("dashboard_security_rows")) {
                voidcare::app::theme::SettingRow("Provider status",
                                                 "Current antivirus mode",
                                                 [&]() { renderProviderStatusChip(state->ui.snapshot); });
                voidcare::app::theme::SettingRow("Provider",
                                                 "Detected product",
                                                 [&]() {
                                                     ImGui::TextUnformatted(
                                                         toShortUtf8(state->ui.snapshot.antivirusProviderName, 40).c_str());
                                                 });
                voidcare::app::theme::SettingRow("Quick scan",
                                                 "Defender quick scan",
                                                 [&]() {
                                                     const bool disabled = !state->ui.snapshot.defenderScanAvailable;
                                                     if (disabled) {
                                                         ImGui::BeginDisabled();
                                                     }
                                                     if (ImGui::Button("Run##dashboard_quick", ImVec2(120.0f, 32.0f))) {
                                                         submitRequest(state,
                                                                       QStringLiteral("run_defender_quick_scan"),
                                                                       {},
                                                                       ActionKind::Scan,
                                                                       false,
                                                                       false,
                                                                       {});
                                                     }
                                                     if (disabled) {
                                                         ImGui::EndDisabled();
                                                     }
                                                 });
                voidcare::app::theme::SettingRow("Full scan",
                                                 "Defender full scan",
                                                 [&]() {
                                                     const bool disabled = !state->ui.snapshot.defenderScanAvailable;
                                                     if (disabled) {
                                                         ImGui::BeginDisabled();
                                                     }
                                                     if (ImGui::Button("Run##dashboard_full", ImVec2(120.0f, 32.0f))) {
                                                         submitRequest(state,
                                                                       QStringLiteral("run_defender_full_scan"),
                                                                       {},
                                                                       ActionKind::Scan,
                                                                       false,
                                                                       false,
                                                                       {});
                                                     }
                                                     if (disabled) {
                                                         ImGui::EndDisabled();
                                                     }
                                                 });
                voidcare::app::theme::SettingRow("Persistence audit",
                                                 "Startup/tasks/services",
                                                 [&]() {
                                                     if (ImGui::Button("Refresh##dashboard_persist", ImVec2(120.0f, 32.0f))) {
                                                         submitRequest(state,
                                                                       QStringLiteral("refresh_persistence_audit"),
                                                                       {},
                                                                       ActionKind::None,
                                                                       false,
                                                                       false,
                                                                       {});
                                                     }
                                                     ImGui::SameLine();
                                                     ImGui::Text("%d rows", state->ui.snapshot.persistenceEntries.size());
                                                 });
                voidcare::app::theme::SettingRow("Suspicious files",
                                                 "Heuristic scan entry point",
                                                 [&]() {
                                                     if (ImGui::Button("Quick##dashboard_suspicious", ImVec2(86.0f, 32.0f))) {
                                                         submitRequest(state,
                                                                       QStringLiteral("run_quick_suspicious_scan"),
                                                                       {},
                                                                       ActionKind::Scan,
                                                                       false,
                                                                       false,
                                                                       {});
                                                     }
                                                     ImGui::SameLine();
                                                     if (ImGui::Button("Full##dashboard_suspicious", ImVec2(86.0f, 32.0f))) {
                                                         QVariantMap args;
                                                         args.insert(QStringLiteral("roots"),
                                                                     splitRoots(QString::fromUtf8(state->fullScanRoots.data())
                                                                                    .trimmed()));
                                                         submitRequest(state,
                                                                       QStringLiteral("run_full_suspicious_scan"),
                                                                       args,
                                                                       ActionKind::Scan,
                                                                       false,
                                                                       false,
                                                                       {});
                                                     }
                                                 });
            }
            voidcare::app::theme::EndSettingRows();
            ImGui::Separator();
            ImGui::TextWrapped("%s", toUtf8(state->ui.snapshot.antivirusStatus).c_str());
        }
        voidcare::app::theme::EndPanel();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel(
                "dashboard_optimize_panel", "Optimization", ImVec2(0.0f, 0.0f), "Preset, cleanup, and gaming")) {
            if (voidcare::app::theme::BeginSettingRows("dashboard_optimize_rows")) {
                voidcare::app::theme::SettingRow("Performance preset",
                                                 "Session-only preset toggle",
                                                 [&]() {
                                                     voidcare::app::theme::togglePill(
                                                         "dashboard_perf_toggle", &state->performancePreset, ImVec2(72.0f, 32.0f));
                                                 });
                voidcare::app::theme::SettingRow("Safe cleanup",
                                                 "Temp and recycle cleanup",
                                                 [&]() {
                                                     if (ImGui::Button("Run##dashboard_safe_opt", ImVec2(120.0f, 32.0f))) {
                                                         queueGuardedAction(state,
                                                                            QStringLiteral("Safe Optimization"),
                                                                            QStringLiteral("Run safe cleanup now?"),
                                                                            QStringLiteral("run_safe_optimization"),
                                                                            {},
                                                                            ActionKind::Optimize,
                                                                            false);
                                                     }
                                                 });
                voidcare::app::theme::SettingRow("Remove bloat",
                                                 "Aggressive option (default off)",
                                                 [&]() {
                                                     voidcare::app::theme::togglePill(
                                                         "dashboard_remove_bloat", &state->ui.removeBloat, ImVec2(72.0f, 32.0f));
                                                 });
                voidcare::app::theme::SettingRow("Disable Copilot",
                                                 "Aggressive option (default off)",
                                                 [&]() {
                                                     voidcare::app::theme::togglePill("dashboard_disable_copilot",
                                                                                      &state->ui.disableCopilot,
                                                                                      ImVec2(72.0f, 32.0f));
                                                 });
                voidcare::app::theme::SettingRow("Aggressive run",
                                                 "Run selected aggressive options",
                                                 [&]() {
                                                     if (ImGui::Button("Run##dashboard_aggressive_opt",
                                                                       ImVec2(120.0f, 32.0f))) {
                                                         QVariantMap args;
                                                         args.insert(QStringLiteral("removeBloat"), state->ui.removeBloat);
                                                         args.insert(QStringLiteral("disableCopilot"), state->ui.disableCopilot);
                                                         queueGuardedAction(state,
                                                                            QStringLiteral("Aggressive Optimization"),
                                                                            QStringLiteral(
                                                                                "Run aggressive optimization options?"),
                                                                            QStringLiteral("run_aggressive_optimization"),
                                                                            args,
                                                                            ActionKind::Optimize,
                                                                            false);
                                                     }
                                                 });
                voidcare::app::theme::SettingRow("Gaming boost",
                                                 "Enable or revert",
                                                 [&]() {
                                                     if (ImGui::Button("Enable##dashboard_game", ImVec2(84.0f, 32.0f))) {
                                                         queueGuardedAction(state,
                                                                            QStringLiteral("Enable Gaming Boost"),
                                                                            QStringLiteral(
                                                                                "Apply gaming boost settings?"),
                                                                            QStringLiteral("enable_gaming_boost"),
                                                                            {},
                                                                            ActionKind::None,
                                                                            false);
                                                     }
                                                     ImGui::SameLine();
                                                     if (ImGui::Button("Revert##dashboard_game", ImVec2(84.0f, 32.0f))) {
                                                         queueGuardedAction(state,
                                                                            QStringLiteral("Revert Gaming Boost"),
                                                                            QStringLiteral(
                                                                                "Revert gaming boost settings?"),
                                                                            QStringLiteral("revert_gaming_boost"),
                                                                            {},
                                                                            ActionKind::None,
                                                                            false);
                                                     }
                                                 });
                voidcare::app::theme::SettingRow("Health report",
                                                 "Refresh system health summary",
                                                 [&]() {
                                                     if (ImGui::Button("Refresh##dashboard_health", ImVec2(120.0f, 32.0f))) {
                                                         submitRequest(state,
                                                                       QStringLiteral("refresh_health_report"),
                                                                       {},
                                                                       ActionKind::None,
                                                                       false,
                                                                       false,
                                                                       {});
                                                     }
                                                 });
            }
            voidcare::app::theme::EndSettingRows();
            ImGui::Separator();
            ImGui::TextWrapped("%s", toUtf8(state->ui.snapshot.healthSummary).c_str());
        }
        voidcare::app::theme::EndPanel();
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
                          ImVec2(0.0f, 245.0f))) {
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
                          ImVec2(0.0f, 170.0f))) {
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
                          ImVec2(0.0f, 125.0f))) {
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

    voidcare::app::theme::StatusChip(kWarningText, voidcare::app::theme::ChipVariant::Warning);
    ImGui::Spacing();

    if (ImGui::BeginTable(
            "security_two_panel", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel(
                "security_left_panel", "Security", ImVec2(0.0f, 0.0f), "Defender + persistence")) {
            if (voidcare::app::theme::BeginSettingRows("security_left_rows")) {
                voidcare::app::theme::SettingRow("Provider status",
                                                 "Active, passive, or unavailable",
                                                 [&]() { renderProviderStatusChip(state->ui.snapshot); });
                voidcare::app::theme::SettingRow("Provider", "Detected antivirus", [&]() {
                    ImGui::TextUnformatted(toShortUtf8(state->ui.snapshot.antivirusProviderName, 36).c_str());
                });
                voidcare::app::theme::SettingRow("Custom scan path", "Path for Defender custom scan", [&]() {
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText("##custom_scan_path", state->customScanPath.data(), state->customScanPath.size());
                });
                voidcare::app::theme::SettingRow("Scan controls", "Quick / Full / Custom", [&]() {
                    const bool disabled = !state->ui.snapshot.defenderScanAvailable;
                    if (disabled) {
                        ImGui::BeginDisabled();
                    }
                    if (ImGui::Button("Quick##security", ImVec2(74.0f, 32.0f))) {
                        submitRequest(state, QStringLiteral("run_defender_quick_scan"), {}, ActionKind::Scan, false, false, {});
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Full##security", ImVec2(74.0f, 32.0f))) {
                        submitRequest(state, QStringLiteral("run_defender_full_scan"), {}, ActionKind::Scan, false, false, {});
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Custom##security", ImVec2(86.0f, 32.0f))) {
                        QVariantMap args;
                        args.insert(QStringLiteral("customPath"), QString::fromUtf8(state->customScanPath.data()).trimmed());
                        submitRequest(state, QStringLiteral("run_defender_custom_scan"), args, ActionKind::Scan, false, false, {});
                    }
                    if (disabled) {
                        ImGui::EndDisabled();
                    }
                });
                voidcare::app::theme::SettingRow("Auto-remediate", "Defender detections only", [&]() {
                    const bool disabled = !state->ui.snapshot.defenderRemediationAvailable;
                    if (disabled) {
                        ImGui::BeginDisabled();
                    }
                    if (ImGui::Button("Run##security_remediate", ImVec2(120.0f, 32.0f))) {
                        submitRequest(state,
                                      QStringLiteral("run_defender_auto_remediate"),
                                      {},
                                      ActionKind::Scan,
                                      false,
                                      false,
                                      {});
                    }
                    if (disabled) {
                        ImGui::EndDisabled();
                    }
                });
            }
            voidcare::app::theme::EndSettingRows();
            ImGui::Separator();
            if (ImGui::Button("Refresh Audit##security", ImVec2(120.0f, 32.0f))) {
                submitRequest(state, QStringLiteral("refresh_persistence_audit"), {}, ActionKind::None, false, false, {});
            }
            ImGui::SameLine();
            const bool hasSelection = state->ui.selectedPersistenceIndex >= 0 &&
                                      state->ui.selectedPersistenceIndex < state->ui.snapshot.persistenceEntries.size();
            if (!hasSelection) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Disable Selected##security", ImVec2(150.0f, 32.0f))) {
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
        voidcare::app::theme::EndPanel();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel(
                "security_right_panel", "Suspicious Files", ImVec2(0.0f, 0.0f), "Quarantine-first workflow")) {
            if (voidcare::app::theme::BeginSettingRows("security_right_rows")) {
                voidcare::app::theme::SettingRow("Full scan roots", "Semicolon separated paths", [&]() {
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText("##full_scan_roots", state->fullScanRoots.data(), state->fullScanRoots.size());
                });
                voidcare::app::theme::SettingRow("Suspicious scan", "Quick or full", [&]() {
                    if (ImGui::Button("Quick##security_suspicious", ImVec2(86.0f, 32.0f))) {
                        submitRequest(state, QStringLiteral("run_quick_suspicious_scan"), {}, ActionKind::Scan, false, false, {});
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Full##security_suspicious", ImVec2(86.0f, 32.0f))) {
                        QVariantMap args;
                        args.insert(QStringLiteral("roots"), splitRoots(QString::fromUtf8(state->fullScanRoots.data()).trimmed()));
                        submitRequest(state, QStringLiteral("run_full_suspicious_scan"), args, ActionKind::Scan, false, false, {});
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Refresh##security_suspicious", ImVec2(94.0f, 32.0f))) {
                        submitRequest(state, QStringLiteral("get_state"), {}, ActionKind::None, false, false, {});
                    }
                });
                voidcare::app::theme::SettingRow("Restore destination", "Optional destination override", [&]() {
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputText("##restore_destination", state->restoreDestination.data(), state->restoreDestination.size());
                });
                voidcare::app::theme::SettingRow("Actions", "Quarantine, restore, delete", [&]() {
                    if (ImGui::Button("Quarantine##security_action", ImVec2(96.0f, 32.0f))) {
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
                    if (ImGui::Button("Restore##security_action", ImVec2(80.0f, 32.0f))) {
                        QVariantMap args;
                        args.insert(QStringLiteral("quarantinePaths"), setToStringList(state->ui.selectedQuarantine));
                        args.insert(QStringLiteral("destinationOverride"), QString::fromUtf8(state->restoreDestination.data()).trimmed());
                        submitRequest(state, QStringLiteral("restore_quarantined"), args, ActionKind::None, false, false, {});
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Delete##security_action", ImVec2(78.0f, 32.0f))) {
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
                });
            }
            voidcare::app::theme::EndSettingRows();
            ImGui::Spacing();
            renderSuspiciousAndQuarantineTables(state, searchNeedle);
            if (ImGui::Button("Clear Suspicious Selection##security", ImVec2(190.0f, 30.0f))) {
                state->ui.selectedSuspicious.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Quarantine Selection##security", ImVec2(190.0f, 30.0f))) {
                state->ui.selectedQuarantine.clear();
            }
        }
        voidcare::app::theme::EndPanel();
        ImGui::EndTable();
    }
}

void renderOptimize(RuntimeState* state) {
    if (state == nullptr) {
        return;
    }

    if (ImGui::BeginTable(
            "optimize_two_panel", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel(
                "optimize_left_panel", "Optimization", ImVec2(0.0f, 0.0f), "Safe and aggressive controls")) {
            if (voidcare::app::theme::BeginSettingRows("optimize_left_rows")) {
                voidcare::app::theme::SettingRow("Performance preset", "Session-only preset", [&]() {
                    voidcare::app::theme::togglePill("optimize_perf_toggle", &state->performancePreset, ImVec2(72.0f, 32.0f));
                });
                voidcare::app::theme::SettingRow("Safe cleanup", "Temp and recycle cleanup", [&]() {
                    if (ImGui::Button("Run##optimize_safe", ImVec2(120.0f, 32.0f))) {
                        queueGuardedAction(state,
                                           QStringLiteral("Safe Optimization"),
                                           QStringLiteral("Run safe cleanup now?"),
                                           QStringLiteral("run_safe_optimization"),
                                           {},
                                           ActionKind::Optimize,
                                           false);
                    }
                });
                voidcare::app::theme::SettingRow("Remove bloat", "Aggressive option", [&]() {
                    voidcare::app::theme::togglePill("optimize_remove_bloat", &state->ui.removeBloat, ImVec2(72.0f, 32.0f));
                });
                voidcare::app::theme::SettingRow("Disable Copilot", "Aggressive option", [&]() {
                    voidcare::app::theme::togglePill("optimize_disable_copilot", &state->ui.disableCopilot, ImVec2(72.0f, 32.0f));
                });
                voidcare::app::theme::SettingRow("Aggressive run", "Run selected aggressive options", [&]() {
                    if (ImGui::Button("Run##optimize_aggressive", ImVec2(120.0f, 32.0f))) {
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
                });
            }
            voidcare::app::theme::EndSettingRows();
            ImGui::Separator();
            ImGui::TextWrapped("%s", toUtf8(state->ui.snapshot.healthSummary).c_str());
        }
        voidcare::app::theme::EndPanel();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel(
                "optimize_right_panel", "Companion Panel", ImVec2(0.0f, 0.0f), "Gaming and startup helpers")) {
            if (voidcare::app::theme::BeginSettingRows("optimize_right_rows")) {
                voidcare::app::theme::SettingRow("Gaming boost", "Enable or revert profile", [&]() {
                    if (ImGui::Button("Enable##optimize_game", ImVec2(84.0f, 32.0f))) {
                        queueGuardedAction(state,
                                           QStringLiteral("Enable Gaming Boost"),
                                           QStringLiteral("Apply gaming boost settings?"),
                                           QStringLiteral("enable_gaming_boost"),
                                           {},
                                           ActionKind::None,
                                           false);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Revert##optimize_game", ImVec2(84.0f, 32.0f))) {
                        queueGuardedAction(state,
                                           QStringLiteral("Revert Gaming Boost"),
                                           QStringLiteral("Revert gaming boost settings?"),
                                           QStringLiteral("revert_gaming_boost"),
                                           {},
                                           ActionKind::None,
                                           false);
                    }
                });
                voidcare::app::theme::SettingRow("Startup manager", "Open Security page", [&]() {
                    if (ImGui::Button("Open##optimize_security", ImVec2(120.0f, 32.0f))) {
                        navigateTo(state, voidcare::app::PageId::Security);
                    }
                });
                voidcare::app::theme::SettingRow("Refresh health", "Re-pull health summary", [&]() {
                    if (ImGui::Button("Refresh##optimize_health", ImVec2(120.0f, 32.0f))) {
                        submitRequest(state, QStringLiteral("refresh_health_report"), {}, ActionKind::None, false, false, {});
                    }
                });
                voidcare::app::theme::SettingRow("Session", "Pending actions", [&]() {
                    ImGui::Text("%d pending", state->pendingCount);
                });
            }
            voidcare::app::theme::EndSettingRows();
            ImGui::Separator();
            ImGui::Text("Last optimize: %s", toUtf8(state->lastOptimizeStamp).c_str());
            ImGui::Text("Discord RPC: %s", boolText(state->ui.discordToggle));
            ImGui::TextWrapped("%s", toUtf8(state->ui.statusText).c_str());
        }
        voidcare::app::theme::EndPanel();
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

    if (ImGui::BeginTable(
            "gaming_two_panel", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel(
                "gaming_left_panel", "Gaming", ImVec2(0.0f, 0.0f), "Boost actions and safety guidance")) {
            if (voidcare::app::theme::BeginSettingRows("gaming_left_rows")) {
                voidcare::app::theme::SettingRow("Enable boost", "Apply gaming profile", [&]() {
                    if (ImGui::Button("Enable##gaming", ImVec2(86.0f, 32.0f))) {
                        queueGuardedAction(state,
                                           QStringLiteral("Enable Gaming Boost"),
                                           QStringLiteral("Apply gaming boost settings?"),
                                           QStringLiteral("enable_gaming_boost"),
                                           {},
                                           ActionKind::None,
                                           false);
                    }
                });
                voidcare::app::theme::SettingRow("Revert boost", "Revert gaming profile", [&]() {
                    if (ImGui::Button("Revert##gaming", ImVec2(86.0f, 32.0f))) {
                        queueGuardedAction(state,
                                           QStringLiteral("Revert Gaming Boost"),
                                           QStringLiteral("Revert gaming boost settings?"),
                                           QStringLiteral("revert_gaming_boost"),
                                           {},
                                           ActionKind::None,
                                           false);
                    }
                });
                voidcare::app::theme::SettingRow("Safety", "Restore-point attempt enabled", [&]() {
                    voidcare::app::theme::StatusChip("Guarded", voidcare::app::theme::ChipVariant::Success);
                });
            }
            voidcare::app::theme::EndSettingRows();
            ImGui::Separator();
            ImGui::BulletText("Use Revert if game-specific settings conflict.");
            ImGui::BulletText("Boost and revert keep destructive-flow confirmations.");
        }
        voidcare::app::theme::EndPanel();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel(
                "gaming_right_panel", "Companion Panel", ImVec2(0.0f, 0.0f), "Session tuning controls")) {
            if (voidcare::app::theme::BeginSettingRows("gaming_right_rows")) {
                voidcare::app::theme::SettingRow("Frame limiter", "UI-only session knob", [&]() {
                    ImGui::SetNextItemWidth(-1.0f);
                    voidcare::app::theme::sliderBar("frame_limiter", &frameLimit, 60.0f, 240.0f, "%.0f FPS");
                });
                voidcare::app::theme::SettingRow("Image sharpness", "UI-only session knob", [&]() {
                    ImGui::SetNextItemWidth(-1.0f);
                    voidcare::app::theme::sliderBar("image_sharpness", &sharpness, 0.0f, 100.0f, "%.0f%%");
                });
                voidcare::app::theme::SettingRow("Disable notifications", "Session preference", [&]() {
                    voidcare::app::theme::togglePill("disable_notifications", &disableNotifications, ImVec2(72.0f, 32.0f));
                });
                voidcare::app::theme::SettingRow("Disable Game DVR", "Session preference", [&]() {
                    voidcare::app::theme::togglePill("disable_gamedvr", &disableGameDvr, ImVec2(72.0f, 32.0f));
                });
            }
            voidcare::app::theme::EndSettingRows();
            ImGui::Separator();
            ImGui::Text("CPU %.1f%% | RAM %.1f%% | Disk %.1f%%", state->cpuPercent, state->ramPercent, state->diskPercent);
            ImGui::TextWrapped("%s", toUtf8(state->ui.statusText).c_str());
        }
        voidcare::app::theme::EndPanel();
        ImGui::EndTable();
    }
}

void renderApps(RuntimeState* state, const QString& searchNeedle) {
    if (state == nullptr) {
        return;
    }

    if (ImGui::BeginTable(
            "apps_two_panel", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel("apps_left_panel",
                                             "Apps Manager",
                                             ImVec2(0.0f, 0.0f),
                                             "Inventory and launchers only")) {
            if (ImGui::Button("Refresh Apps##apps", ImVec2(130.0f, 32.0f))) {
                submitRequest(state, QStringLiteral("refresh_installed_apps"), {}, ActionKind::None, false, false, {});
            }
            ImGui::Spacing();
            if (ImGui::BeginTable("apps_table",
                                  3,
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                                      ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                                  ImVec2(0.0f, 0.0f))) {
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
        voidcare::app::theme::EndPanel();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel(
                "apps_right_panel", "Companion Panel", ImVec2(0.0f, 0.0f), "External launchers and status")) {
            if (voidcare::app::theme::BeginSettingRows("apps_rows")) {
                voidcare::app::theme::SettingRow("Open Apps Settings", "Windows settings launcher", [&]() {
                    if (ImGui::Button("Open##apps_settings", ImVec2(120.0f, 32.0f))) {
                        submitRequest(state, QStringLiteral("open_apps_settings"), {}, ActionKind::None, false, false, {});
                    }
                });
                voidcare::app::theme::SettingRow("Programs && Features", "Classic control panel", [&]() {
                    if (ImGui::Button("Open##programs_features", ImVec2(120.0f, 32.0f))) {
                        submitRequest(state,
                                      QStringLiteral("open_programs_features"),
                                      {},
                                      ActionKind::None,
                                      false,
                                      false,
                                      {});
                    }
                });
                voidcare::app::theme::SettingRow("Inventory size", "Current app rows", [&]() {
                    ImGui::Text("%d apps", state->ui.snapshot.installedApps.size());
                });
            }
            voidcare::app::theme::EndSettingRows();
            ImGui::Separator();
            ImGui::TextWrapped("Apps page is inventory + launcher only. No uninstall action is executed.");
            ImGui::TextWrapped("%s", toUtf8(state->ui.statusText).c_str());
        }
        voidcare::app::theme::EndPanel();
        ImGui::EndTable();
    }
}

void renderAbout(RuntimeState* state) {
    if (state == nullptr) {
        return;
    }

    if (ImGui::BeginTable(
            "about_two_panel", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel("about_left_panel",
                                             "About VoidCare",
                                             ImVec2(0.0f, 0.0f),
                                             "Offline admin optimization and security suite")) {
            ImGui::TextWrapped("VoidCare by VoidTools");
            ImGui::TextWrapped("%s", kCreditsText);
            ImGui::Separator();
            ImGui::TextWrapped("Admin-only, offline-only operation.");
            ImGui::TextWrapped("Defender-only auto-remediation. Suspicious scanning is heuristic-only.");
            ImGui::TextWrapped("Suspicious != confirmed malware. Review before deleting.");
        }
        voidcare::app::theme::EndPanel();

        ImGui::TableNextColumn();
        if (voidcare::app::theme::BeginPanel(
                "about_right_panel", "Discord RPC", ImVec2(0.0f, 0.0f), "Local IPC named pipes only")) {
            if (voidcare::app::theme::BeginSettingRows("about_rows")) {
                voidcare::app::theme::SettingRow("Current state", "Session RPC toggle", [&]() {
                    ImGui::Text("%s", boolText(state->ui.discordToggle));
                });
                voidcare::app::theme::SettingRow("Discord status", "Desktop client detection", [&]() {
                    ImGui::TextUnformatted(toShortUtf8(state->ui.snapshot.discordAboutStatus, 56).c_str());
                });
                voidcare::app::theme::SettingRow("Pipe range", "discord-ipc-0..9", [&]() {
                    ImGui::TextUnformatted("\\\\?\\pipe\\discord-ipc-*");
                });
                voidcare::app::theme::SettingRow("Session pending", "Active jobs", [&]() {
                    ImGui::Text("%d", state->pendingCount);
                });
                voidcare::app::theme::SettingRow("Last scan", "Timestamp", [&]() {
                    ImGui::TextUnformatted(toUtf8(state->lastScanStamp).c_str());
                });
                voidcare::app::theme::SettingRow("Last optimize", "Timestamp", [&]() {
                    ImGui::TextUnformatted(toUtf8(state->lastOptimizeStamp).c_str());
                });
            }
            voidcare::app::theme::EndSettingRows();
            ImGui::Separator();
            ImGui::TextWrapped("No web calls are used. RPC communicates over local named pipes.");
            ImGui::TextWrapped("%s", toUtf8(state->ui.statusText).c_str());
        }
        voidcare::app::theme::EndPanel();
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

    if (voidcare::app::theme::BeginPanel("topbar_panel", nullptr, ImVec2(0.0f, 74.0f))) {
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
    voidcare::app::theme::EndPanel();
}

void renderSidebar(RuntimeState* state,
                   const std::array<NavItem, 6>& navItems,
                   const bool iconFontLoaded) {
    if (state == nullptr) {
        return;
    }

    ensureSidebarLogoLoaded(state);

    if (voidcare::app::theme::BeginPanel("sidebar_panel", nullptr, ImVec2(88.0f, 0.0f))) {
        ImGui::SetCursorPosY(14.0f);
        if (state->logoTexture != nullptr && state->logoWidth > 0 && state->logoHeight > 0) {
            const float logoSize = 44.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - logoSize) * 0.5f);
            ImGui::Image(reinterpret_cast<ImTextureID>(state->logoTexture), ImVec2(logoSize, logoSize));
        } else {
            ImGui::SetCursorPosX(28.0f);
            ImGui::TextColored(ImVec4(0.56f, 0.49f, 1.0f, 1.0f), "VT");
        }
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

        const float bottomY = ImGui::GetWindowSize().y - 136.0f;
        ImGui::SetCursorPosY(std::max(bottomY, ImGui::GetCursorPosY() + 12.0f));
        voidcare::app::theme::StatusChip("Admin", voidcare::app::theme::ChipVariant::Accent);
        ImGui::Spacing();
        const QByteArray userUtf8 = qEnvironmentVariable("USERNAME").toUtf8();
        ImGui::TextColored(ImVec4(0.66f, 0.66f, 0.78f, 1.0f), "%s", userUtf8.constData());
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 72.0f);
        ImGui::TextColored(ImVec4(0.58f, 0.58f, 0.72f, 1.0f), "%s", kCreditsText);
        ImGui::PopTextWrapPos();
    }
    voidcare::app::theme::EndPanel();
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
    const HRESULT comInitResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitializeCom = SUCCEEDED(comInitResult);

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

        if (voidcare::app::theme::BeginPanel("content_surface", nullptr, ImVec2(0.0f, 0.0f))) {
            const QString searchNeedle = QString::fromUtf8(state.searchBuffer.data()).trimmed();
            ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.99f, 1.0f), "%s", pageName(state.ui.page));
            if (state.pendingCount > 0) {
                ImGui::SameLine();
                voidcare::app::theme::StatusChip("Working...", voidcare::app::theme::ChipVariant::Accent);
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
            ImGui::Spacing();
            renderLogsDrawer(&state, true);
            ImGui::EndChild();
        }
        voidcare::app::theme::EndPanel();
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
    releaseSidebarLogo(&state);

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    cleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    if (shouldUninitializeCom) {
        CoUninitialize();
    }

    return 0;
}

