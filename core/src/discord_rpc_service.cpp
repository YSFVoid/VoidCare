#include "voidcare/core/discord_rpc_service.h"

#include <Windows.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace voidcare::core {

namespace {

constexpr qint32 kOpHandshake = 0;
constexpr qint32 kOpFrame = 1;

QByteArray int32ToBytes(const qint32 value) {
    QByteArray bytes;
    bytes.resize(4);
    bytes[0] = static_cast<char>(value & 0xFF);
    bytes[1] = static_cast<char>((value >> 8) & 0xFF);
    bytes[2] = static_cast<char>((value >> 16) & 0xFF);
    bytes[3] = static_cast<char>((value >> 24) & 0xFF);
    return bytes;
}

qint32 bytesToInt32(const char* data) {
    return static_cast<qint32>(static_cast<unsigned char>(data[0])) |
           (static_cast<qint32>(static_cast<unsigned char>(data[1])) << 8) |
           (static_cast<qint32>(static_cast<unsigned char>(data[2])) << 16) |
           (static_cast<qint32>(static_cast<unsigned char>(data[3])) << 24);
}

}  // namespace

DiscordRpcService::DiscordRpcService(QString clientId, QObject* parent)
    : QObject(parent)
    , m_clientId(std::move(clientId))
    , m_startTimestamp(QDateTime::currentSecsSinceEpoch()) {}

DiscordRpcService::~DiscordRpcService() {
    closeConnection();
}

bool DiscordRpcService::isEnabled() const {
    return m_enabled;
}

bool DiscordRpcService::discordDetected() const {
    return m_discordDetected;
}

void DiscordRpcService::setEnabled(const bool enabled) {
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    if (!m_enabled) {
        closeConnection();
    }
    emit enabledChanged();
}

void DiscordRpcService::updatePresence(const DiscordPresenceState state) {
    if (!m_enabled || m_clientId.trimmed().isEmpty()) {
        return;
    }

    if (!ensureConnected()) {
        return;
    }

    if (!sendFrame(kOpFrame, activityPayload(state))) {
        closeConnection();
    }
}

bool DiscordRpcService::ensureConnected() {
    if (m_pipe != INVALID_HANDLE_VALUE) {
        return true;
    }

    if (m_clientId.trimmed().isEmpty()) {
        if (m_discordDetected) {
            m_discordDetected = false;
            emit discordDetectedChanged();
        }
        return false;
    }

    for (int i = 0; i < 10; ++i) {
        const QStringList candidates = {
            QStringLiteral("\\\\?\\pipe\\discord-ipc-%1").arg(i),
            QStringLiteral("\\\\.\\pipe\\discord-ipc-%1").arg(i),
        };

        HANDLE pipe = INVALID_HANDLE_VALUE;
        for (const QString& pipeName : candidates) {
            pipe = CreateFileW(reinterpret_cast<LPCWSTR>(pipeName.utf16()),
                               GENERIC_READ | GENERIC_WRITE,
                               0,
                               nullptr,
                               OPEN_EXISTING,
                               0,
                               nullptr);
            if (pipe != INVALID_HANDLE_VALUE) {
                break;
            }
        }
        if (pipe == INVALID_HANDLE_VALUE) {
            continue;
        }

        m_pipe = pipe;
        QJsonObject handshake;
        handshake.insert(QStringLiteral("v"), 1);
        handshake.insert(QStringLiteral("client_id"), m_clientId);
        const QByteArray payload = QJsonDocument(handshake).toJson(QJsonDocument::Compact);

        if (!sendFrame(kOpHandshake, payload)) {
            closeConnection();
            continue;
        }

        qint32 responseOp = 0;
        QByteArray responsePayload;
        if (!readFrame(&responseOp, &responsePayload)) {
            closeConnection();
            continue;
        }

        const bool wasDetected = m_discordDetected;
        m_discordDetected = true;
        if (!wasDetected) {
            emit discordDetectedChanged();
        }
        return true;
    }

    if (m_discordDetected) {
        m_discordDetected = false;
        emit discordDetectedChanged();
    }
    return false;
}

void DiscordRpcService::closeConnection() {
    if (m_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipe);
        m_pipe = INVALID_HANDLE_VALUE;
    }

    if (m_discordDetected) {
        m_discordDetected = false;
        emit discordDetectedChanged();
    }
}

bool DiscordRpcService::sendFrame(const qint32 op, const QByteArray& payload) {
    if (m_pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    const QByteArray frame = encodeDiscordFrame(op, payload);
    DWORD written = 0;
    const BOOL ok = WriteFile(m_pipe, frame.constData(), static_cast<DWORD>(frame.size()), &written, nullptr);
    return ok == TRUE && written == static_cast<DWORD>(frame.size());
}

bool DiscordRpcService::readFrame(qint32* opOut, QByteArray* payloadOut) {
    if (m_pipe == INVALID_HANDLE_VALUE || opOut == nullptr || payloadOut == nullptr) {
        return false;
    }

    QByteArray header;
    header.resize(8);
    DWORD headerRead = 0;
    if (!ReadFile(m_pipe, header.data(), 8, &headerRead, nullptr) || headerRead != 8) {
        return false;
    }

    const qint32 op = bytesToInt32(header.constData());
    const qint32 size = bytesToInt32(header.constData() + 4);
    if (size < 0 || size > (16 * 1024 * 1024)) {
        return false;
    }

    QByteArray payload;
    payload.resize(size);
    DWORD payloadRead = 0;
    if (size > 0 &&
        (!ReadFile(m_pipe, payload.data(), static_cast<DWORD>(size), &payloadRead, nullptr) ||
         payloadRead != static_cast<DWORD>(size))) {
        return false;
    }

    *opOut = op;
    *payloadOut = payload;
    return true;
}

QByteArray DiscordRpcService::activityPayload(const DiscordPresenceState state) const {
    QJsonObject activity;
    activity.insert(QStringLiteral("details"), QStringLiteral("Using Optimization Tool"));
    activity.insert(QStringLiteral("state"), discordStateLabel(state));

    QJsonObject timestamps;
    timestamps.insert(QStringLiteral("start"), static_cast<qint64>(m_startTimestamp));
    activity.insert(QStringLiteral("timestamps"), timestamps);

    QJsonObject args;
    args.insert(QStringLiteral("pid"), static_cast<qint64>(GetCurrentProcessId()));
    args.insert(QStringLiteral("activity"), activity);

    QJsonObject payload;
    payload.insert(QStringLiteral("cmd"), QStringLiteral("SET_ACTIVITY"));
    payload.insert(QStringLiteral("args"), args);
    payload.insert(QStringLiteral("nonce"), QUuid::createUuid().toString(QUuid::WithoutBraces));

    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

QByteArray encodeDiscordFrame(const qint32 op, const QByteArray& payload) {
    QByteArray frame;
    frame.reserve(8 + payload.size());
    frame.append(int32ToBytes(op));
    frame.append(int32ToBytes(payload.size()));
    frame.append(payload);
    return frame;
}

bool decodeDiscordFrame(const QByteArray& frame, qint32* opOut, QByteArray* payloadOut) {
    if (frame.size() < 8 || opOut == nullptr || payloadOut == nullptr) {
        return false;
    }

    const qint32 size = bytesToInt32(frame.constData() + 4);
    if (size < 0 || frame.size() < 8 + size) {
        return false;
    }

    *opOut = bytesToInt32(frame.constData());
    *payloadOut = frame.mid(8, size);
    return true;
}

}  // namespace voidcare::core
