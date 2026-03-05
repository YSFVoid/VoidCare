#include <QtTest>

#include <QJsonDocument>
#include <QJsonObject>

#include "voidcare/core/discord_rpc_service.h"

class DiscordCodecTests : public QObject {
    Q_OBJECT

private slots:
    void encodeDecodeFrameRoundtrip();
};

void DiscordCodecTests::encodeDecodeFrameRoundtrip() {
    QJsonObject payloadObj;
    payloadObj.insert(QStringLiteral("cmd"), QStringLiteral("SET_ACTIVITY"));
    payloadObj.insert(QStringLiteral("nonce"), QStringLiteral("123"));

    const QByteArray payload = QJsonDocument(payloadObj).toJson(QJsonDocument::Compact);
    const QByteArray frame = voidcare::core::encodeDiscordFrame(1, payload);

    qint32 op = -1;
    QByteArray decodedPayload;
    QVERIFY(voidcare::core::decodeDiscordFrame(frame, &op, &decodedPayload));
    QCOMPARE(op, 1);
    QCOMPARE(decodedPayload, payload);
}

QTEST_MAIN(DiscordCodecTests)
#include "test_discord_codec.moc"
