#pragma once

#include <QObject>
#include <QScopedPointer>

#include <Windows.h>

#include "voidcare/core/types.h"

namespace voidcare::core {

class DiscordRpcService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool discordDetected READ discordDetected NOTIFY discordDetectedChanged)

public:
    explicit DiscordRpcService(QString clientId, QObject* parent = nullptr);
    ~DiscordRpcService() override;

    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] bool discordDetected() const;

    void setEnabled(bool enabled);
    void updatePresence(DiscordPresenceState state);

signals:
    void enabledChanged();
    void discordDetectedChanged();

private:
    bool ensureConnected();
    void closeConnection();
    bool sendFrame(qint32 op, const QByteArray& payload);
    bool readFrame(qint32* opOut, QByteArray* payloadOut);
    QByteArray activityPayload(DiscordPresenceState state) const;

    QString m_clientId;
    bool m_enabled = true;
    bool m_discordDetected = false;
    HANDLE m_pipe = INVALID_HANDLE_VALUE;
    qint64 m_startTimestamp = 0;
};

QByteArray encodeDiscordFrame(qint32 op, const QByteArray& payload);
bool decodeDiscordFrame(const QByteArray& frame, qint32* opOut, QByteArray* payloadOut);

}  // namespace voidcare::core
