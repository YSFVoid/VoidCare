#include <QtTest>

#include "voidcare/core/suspicious_file_scanner_service.h"

class SuspiciousScoringTests : public QObject {
    Q_OBJECT

private slots:
    void scoreUsesConfiguredWeights();
    void detectsDoubleExtensions();
    void detectsHighEntropyNames();
};

void SuspiciousScoringTests::scoreUsesConfiguredWeights() {
    voidcare::core::SuspiciousFileRecord record;
    record.signatureStatus = voidcare::platform::windows::SignatureStatus::Unsigned;
    record.hiddenOrSystem = true;

    QStringList reasons;
    const int score = voidcare::core::SuspiciousFileScannerService::calculateHeuristicScore(
        record,
        true,
        true,
        false,
        true,
        &reasons);

    QCOMPARE(score, 10);
    QVERIFY(reasons.size() >= 3);
}

void SuspiciousScoringTests::detectsDoubleExtensions() {
    QVERIFY(voidcare::core::SuspiciousFileScannerService::hasDoubleExtensionPattern(
        QStringLiteral("invoice.pdf.exe")));
    QVERIFY(!voidcare::core::SuspiciousFileScannerService::hasDoubleExtensionPattern(
        QStringLiteral("normal.exe")));
}

void SuspiciousScoringTests::detectsHighEntropyNames() {
    QVERIFY(voidcare::core::SuspiciousFileScannerService::hasHighEntropyName(
        QStringLiteral("a1b2c3d4e5f6g7.exe")));
    QVERIFY(!voidcare::core::SuspiciousFileScannerService::hasHighEntropyName(
        QStringLiteral("aaaaaaaa.exe")));
}

QTEST_MAIN(SuspiciousScoringTests)
#include "test_suspicious_scoring.moc"
