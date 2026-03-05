#include "voidcare/platform/windows/hash_utils.h"

#include <Windows.h>
#include <bcrypt.h>

#include <QFile>

namespace voidcare::platform::windows {

QByteArray sha256File(const QString& filePath, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to open file: %1").arg(file.errorString());
        }
        return {};
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("BCryptOpenAlgorithmProvider failed.");
        }
        return {};
    }

    DWORD hashObjectLength = 0;
    ULONG outputLength = 0;
    if (BCryptGetProperty(algorithm,
                          BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&hashObjectLength),
                          sizeof(DWORD),
                          &outputLength,
                          0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to query hash object length.");
        }
        return {};
    }

    DWORD hashLength = 0;
    if (BCryptGetProperty(algorithm,
                          BCRYPT_HASH_LENGTH,
                          reinterpret_cast<PUCHAR>(&hashLength),
                          sizeof(DWORD),
                          &outputLength,
                          0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to query hash length.");
        }
        return {};
    }

    QByteArray hashObject;
    hashObject.resize(static_cast<int>(hashObjectLength));

    BCRYPT_HASH_HANDLE hashHandle = nullptr;
    if (BCryptCreateHash(algorithm,
                         &hashHandle,
                         reinterpret_cast<PUCHAR>(hashObject.data()),
                         hashObjectLength,
                         nullptr,
                         0,
                         0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to create hash handle.");
        }
        return {};
    }

    constexpr qint64 chunkSize = 1024 * 1024;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(chunkSize);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            BCryptDestroyHash(hashHandle);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Failed to read file while hashing.");
            }
            return {};
        }

        if (BCryptHashData(hashHandle,
                           reinterpret_cast<PUCHAR>(const_cast<char*>(chunk.constData())),
                           static_cast<ULONG>(chunk.size()),
                           0) != 0) {
            BCryptDestroyHash(hashHandle);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Failed to hash file chunk.");
            }
            return {};
        }
    }

    QByteArray hashResult;
    hashResult.resize(static_cast<int>(hashLength));
    if (BCryptFinishHash(hashHandle, reinterpret_cast<PUCHAR>(hashResult.data()), hashLength, 0) != 0) {
        BCryptDestroyHash(hashHandle);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to finalize hash.");
        }
        return {};
    }

    BCryptDestroyHash(hashHandle);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return hashResult;
}

}  // namespace voidcare::platform::windows
