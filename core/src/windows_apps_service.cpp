#include "voidcare/core/windows_apps_service.h"

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <vector>

namespace voidcare::core {

namespace {

struct UninstallRoot {
    HKEY key;
    QString path;
};

QString regString(HKEY key, const wchar_t* valueName) {
    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) || bytes == 0) {
        return {};
    }

    std::wstring buffer;
    buffer.resize(bytes / sizeof(wchar_t));
    if (RegQueryValueExW(key,
                         valueName,
                         nullptr,
                         &type,
                         reinterpret_cast<LPBYTE>(buffer.data()),
                         &bytes) != ERROR_SUCCESS) {
        return {};
    }

    if (!buffer.empty() && buffer.back() == L'\0') {
        buffer.pop_back();
    }

    return QString::fromStdWString(buffer).trimmed();
}

void appendFromUninstallKey(const UninstallRoot& root, QVector<InstalledAppInfo>* output) {
    if (output == nullptr) {
        return;
    }

    HKEY uninstall = nullptr;
    if (RegOpenKeyExW(root.key,
                      reinterpret_cast<LPCWSTR>(root.path.utf16()),
                      0,
                      KEY_ENUMERATE_SUB_KEYS,
                      &uninstall) != ERROR_SUCCESS) {
        return;
    }

    DWORD subKeyCount = 0;
    DWORD maxSubKeyLen = 0;
    RegQueryInfoKeyW(uninstall,
                     nullptr,
                     nullptr,
                     nullptr,
                     &subKeyCount,
                     &maxSubKeyLen,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr,
                     nullptr);

    std::vector<wchar_t> subKeyName(maxSubKeyLen + 2);
    for (DWORD i = 0; i < subKeyCount; ++i) {
        DWORD len = maxSubKeyLen + 1;
        if (RegEnumKeyExW(uninstall, i, subKeyName.data(), &len, nullptr, nullptr, nullptr, nullptr) !=
            ERROR_SUCCESS) {
            continue;
        }

        HKEY appKey = nullptr;
        if (RegOpenKeyExW(uninstall, subKeyName.data(), 0, KEY_QUERY_VALUE, &appKey) != ERROR_SUCCESS) {
            continue;
        }

        InstalledAppInfo info;
        info.name = regString(appKey, L"DisplayName");
        info.version = regString(appKey, L"DisplayVersion");
        info.publisher = regString(appKey, L"Publisher");
        RegCloseKey(appKey);

        if (!info.name.isEmpty()) {
            output->push_back(info);
        }
    }

    RegCloseKey(uninstall);
}

}  // namespace

WindowsAppsService::WindowsAppsService(QObject* parent)
    : QObject(parent) {}

QVector<InstalledAppInfo> WindowsAppsService::enumerateInstalledApps() const {
    QVector<InstalledAppInfo> apps;

    const std::array<UninstallRoot, 3> roots = {
        UninstallRoot{HKEY_LOCAL_MACHINE,
                      QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall")},
        UninstallRoot{HKEY_LOCAL_MACHINE,
                      QStringLiteral("SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall")},
        UninstallRoot{HKEY_CURRENT_USER,
                      QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall")},
    };

    for (const auto& root : roots) {
        appendFromUninstallKey(root, &apps);
    }

    std::sort(apps.begin(), apps.end(), [](const InstalledAppInfo& left, const InstalledAppInfo& right) {
        return left.name.toLower() < right.name.toLower();
    });

    return apps;
}

bool WindowsAppsService::openAppsSettings() const {
    const HINSTANCE result = ShellExecuteW(nullptr,
                                           L"open",
                                           L"ms-settings:appsfeatures",
                                           nullptr,
                                           nullptr,
                                           SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
}

bool WindowsAppsService::openProgramsAndFeatures() const {
    const HINSTANCE result = ShellExecuteW(nullptr,
                                           L"open",
                                           L"control.exe",
                                           L"appwiz.cpl",
                                           nullptr,
                                           SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
}

}  // namespace voidcare::core
