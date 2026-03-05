#include <QtTest>

#include "voidcare/core/suspicious_file_scanner_service.h"

class ManifestRoundtripTests : public QObject {
    Q_OBJECT

private slots:
    void jsonRoundtripPreservesFields();
};

void ManifestRoundtripTests::jsonRoundtripPreservesFields() {
    QVector<voidcare::core::QuarantineManifestEntry> entries;

    voidcare::core::QuarantineManifestEntry entry;
    entry.originalPath = QStringLiteral("C:/Users/User/Downloads/tool.exe");
    entry.quarantinePath = QStringLiteral("C:/ProgramData/VoidCare/Quarantine/a/tool.exe");
    entry.sha256 = QStringLiteral("abc123");
    entry.timestamp = QStringLiteral("2026-03-05T12:00:00Z");
    entry.reasons = {QStringLiteral("High-entropy filename pattern"),
                     QStringLiteral("Referenced by persistence entry")};
    entry.signatureStatus = QStringLiteral("Unsigned");
    entries.push_back(entry);

    const QByteArray json = voidcare::core::quarantineManifestToJson(entries);
    const QVector<voidcare::core::QuarantineManifestEntry> parsed =
        voidcare::core::quarantineManifestFromJson(json);

    QCOMPARE(parsed.size(), 1);
    QCOMPARE(parsed[0].originalPath, entry.originalPath);
    QCOMPARE(parsed[0].quarantinePath, entry.quarantinePath);
    QCOMPARE(parsed[0].sha256, entry.sha256);
    QCOMPARE(parsed[0].timestamp, entry.timestamp);
    QCOMPARE(parsed[0].signatureStatus, entry.signatureStatus);
    QCOMPARE(parsed[0].reasons, entry.reasons);
}

QTEST_MAIN(ManifestRoundtripTests)
#include "test_manifest_roundtrip.moc"
