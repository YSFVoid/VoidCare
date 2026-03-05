#pragma once

#include <QString>

namespace voidcare::platform::windows {

enum class SignatureStatus {
    Valid,
    Invalid,
    Unsigned,
    Unknown,
};

struct SignatureInfo {
    SignatureStatus status = SignatureStatus::Unknown;
    QString publisher;
    QString details;
};

SignatureInfo verifyFileSignature(const QString& filePath);
QString signatureStatusToString(SignatureStatus status);

}  // namespace voidcare::platform::windows
