#include "voidcare/core/av_discovery_service.h"

#include <Windows.h>
#include <Wbemidl.h>

#include <comdef.h>

#pragma comment(lib, "wbemuuid.lib")

namespace voidcare::core {

namespace {

struct ComInitGuard {
    HRESULT hr = E_FAIL;

    ComInitGuard() {
        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }

    ~ComInitGuard() {
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }
};

QString variantToString(VARIANT* value) {
    if (value == nullptr) {
        return {};
    }
    if (value->vt == VT_BSTR && value->bstrVal != nullptr) {
        return QString::fromWCharArray(value->bstrVal);
    }
    _variant_t wrapped(*value);
    try {
        wrapped.ChangeType(VT_BSTR);
        return QString::fromWCharArray(wrapped.bstrVal);
    } catch (...) {
        return {};
    }
}

quint32 variantToUInt(VARIANT* value) {
    if (value == nullptr) {
        return 0;
    }
    if (value->vt == VT_UI4) {
        return value->ulVal;
    }
    if (value->vt == VT_I4) {
        return static_cast<quint32>(value->lVal);
    }
    if (value->vt == VT_BSTR && value->bstrVal != nullptr) {
        bool ok = false;
        quint32 parsed = QString::fromWCharArray(value->bstrVal).toUInt(&ok);
        return ok ? parsed : 0;
    }
    return 0;
}

}  // namespace

AvDiscoveryService::AvDiscoveryService(QObject* parent)
    : QObject(parent) {}

QVector<AntivirusProduct> AvDiscoveryService::discover() const {
    QVector<AntivirusProduct> results;

    ComInitGuard comGuard;
    if (FAILED(comGuard.hr)) {
        return results;
    }

    HRESULT hr = CoInitializeSecurity(nullptr,
                                      -1,
                                      nullptr,
                                      nullptr,
                                      RPC_C_AUTHN_LEVEL_DEFAULT,
                                      RPC_C_IMP_LEVEL_IMPERSONATE,
                                      nullptr,
                                      EOAC_NONE,
                                      nullptr);
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        return results;
    }

    IWbemLocator* locator = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator,
                          nullptr,
                          CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator,
                          reinterpret_cast<LPVOID*>(&locator));
    if (FAILED(hr) || locator == nullptr) {
        return results;
    }

    IWbemServices* services = nullptr;
    hr = locator->ConnectServer(_bstr_t(L"ROOT\\SecurityCenter2"),
                                nullptr,
                                nullptr,
                                nullptr,
                                0,
                                nullptr,
                                nullptr,
                                &services);
    if (FAILED(hr) || services == nullptr) {
        locator->Release();
        return results;
    }

    hr = CoSetProxyBlanket(services,
                           RPC_C_AUTHN_WINNT,
                           RPC_C_AUTHZ_NONE,
                           nullptr,
                           RPC_C_AUTHN_LEVEL_CALL,
                           RPC_C_IMP_LEVEL_IMPERSONATE,
                           nullptr,
                           EOAC_NONE);
    if (FAILED(hr)) {
        services->Release();
        locator->Release();
        return results;
    }

    IEnumWbemClassObject* enumerator = nullptr;
    hr = services->ExecQuery(_bstr_t(L"WQL"),
                             _bstr_t(L"SELECT displayName, productState FROM AntiVirusProduct"),
                             WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                             nullptr,
                             &enumerator);
    if (FAILED(hr) || enumerator == nullptr) {
        services->Release();
        locator->Release();
        return results;
    }

    IWbemClassObject* object = nullptr;
    ULONG returned = 0;
    while (enumerator->Next(WBEM_INFINITE, 1, &object, &returned) == S_OK && returned == 1) {
        VARIANT nameValue;
        VARIANT stateValue;
        VariantInit(&nameValue);
        VariantInit(&stateValue);

        object->Get(L"displayName", 0, &nameValue, nullptr, nullptr);
        object->Get(L"productState", 0, &stateValue, nullptr, nullptr);

        AntivirusProduct product;
        product.name = variantToString(&nameValue);
        product.rawState = variantToUInt(&stateValue);

        const quint8 realtimeState = static_cast<quint8>((product.rawState >> 16) & 0xFF);
        const quint8 signatureState = static_cast<quint8>(product.rawState & 0xFF);
        product.active = (realtimeState == 0x10 || realtimeState == 0x11);
        product.upToDate = (signatureState == 0x00);
        product.statusText = QStringLiteral("%1, %2")
                                 .arg(product.active ? QStringLiteral("Active")
                                                     : QStringLiteral("Inactive"))
                                 .arg(product.upToDate ? QStringLiteral("Up-to-date")
                                                       : QStringLiteral("Signatures stale"));
        results.push_back(product);

        VariantClear(&nameValue);
        VariantClear(&stateValue);
        object->Release();
    }

    enumerator->Release();
    services->Release();
    locator->Release();
    return results;
}

}  // namespace voidcare::core
