#include "voidcare/platform/windows/signature_verifier.h"

#include <Windows.h>
#include <wintrust.h>
#include <Softpub.h>

#include <QByteArray>
#include <QFileInfo>

namespace voidcare::platform::windows {

namespace {

QString companyNameFromVersion(const QString& filePath) {
    const std::wstring widePath = filePath.toStdWString();
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(widePath.c_str(), &handle);
    if (size == 0) {
        return {};
    }

    QByteArray versionData;
    versionData.resize(static_cast<int>(size));
    if (!GetFileVersionInfoW(widePath.c_str(), 0, size, versionData.data())) {
        return {};
    }

    struct LangCodePage {
        WORD language;
        WORD codePage;
    };

    LangCodePage* translations = nullptr;
    UINT translationsSize = 0;
    if (!VerQueryValueW(versionData.data(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<LPVOID*>(&translations), &translationsSize) ||
        translationsSize < sizeof(LangCodePage)) {
        return {};
    }

    const QString subBlock = QStringLiteral("\\StringFileInfo\\%1%2\\CompanyName")
                                 .arg(QString::number(translations[0].language, 16).rightJustified(4, '0'))
                                 .arg(QString::number(translations[0].codePage, 16).rightJustified(4, '0'));

    LPVOID value = nullptr;
    UINT valueSize = 0;
    std::wstring subBlockW = subBlock.toStdWString();
    if (!VerQueryValueW(versionData.data(), subBlockW.c_str(), &value, &valueSize) || valueSize == 0) {
        return {};
    }

    return QString::fromWCharArray(static_cast<wchar_t*>(value)).trimmed();
}

}  // namespace

SignatureInfo verifyFileSignature(const QString& filePath) {
    SignatureInfo info;
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        info.status = SignatureStatus::Unknown;
        info.details = QStringLiteral("File not found.");
        return info;
    }

    WINTRUST_FILE_INFO fileData = {};
    std::wstring widePath = filePath.toStdWString();
    fileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileData.pcwszFilePath = widePath.c_str();

    WINTRUST_DATA trustData = {};
    trustData.cbStruct = sizeof(WINTRUST_DATA);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileData;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;
    trustData.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL | WTD_DISABLE_MD2_MD4;

    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG verifyStatus = WinVerifyTrust(nullptr, &policyGUID, &trustData);

    if (verifyStatus == ERROR_SUCCESS) {
        info.status = SignatureStatus::Valid;
        info.details = QStringLiteral("Signature is valid.");
        info.publisher = companyNameFromVersion(filePath);
    } else if (verifyStatus == TRUST_E_NOSIGNATURE || verifyStatus == TRUST_E_PROVIDER_UNKNOWN ||
               verifyStatus == TRUST_E_SUBJECT_FORM_UNKNOWN) {
        info.status = SignatureStatus::Unsigned;
        info.details = QStringLiteral("File is unsigned.");
    } else if (verifyStatus == TRUST_E_EXPLICIT_DISTRUST || verifyStatus == CERT_E_REVOKED ||
               verifyStatus == TRUST_E_SUBJECT_NOT_TRUSTED || verifyStatus == CRYPT_E_SECURITY_SETTINGS) {
        info.status = SignatureStatus::Invalid;
        info.details = QStringLiteral("Signature exists but is not trusted.");
    } else {
        info.status = SignatureStatus::Unknown;
        info.details = QStringLiteral("Signature verification returned status 0x%1")
                           .arg(QString::number(static_cast<qulonglong>(verifyStatus), 16));
    }

    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policyGUID, &trustData);

    if (info.publisher.isEmpty()) {
        info.publisher = QStringLiteral("Unknown");
    }

    return info;
}

QString signatureStatusToString(const SignatureStatus status) {
    switch (status) {
    case SignatureStatus::Valid:
        return QStringLiteral("Valid");
    case SignatureStatus::Invalid:
        return QStringLiteral("Invalid");
    case SignatureStatus::Unsigned:
        return QStringLiteral("Unsigned");
    case SignatureStatus::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

}  // namespace voidcare::platform::windows
