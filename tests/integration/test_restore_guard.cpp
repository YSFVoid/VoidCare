#include <QtTest>

#include "voidcare/core/destructive_action_guard.h"

class RestoreGuardIntegrationTests : public QObject {
    Q_OBJECT

private slots:
    void blocksWhenRestorePointFailsWithoutOverride();
    void allowsWhenOverrideProvided();
};

void RestoreGuardIntegrationTests::blocksWhenRestorePointFailsWithoutOverride() {
    voidcare::core::RestorePointService restoreService(nullptr);
    voidcare::core::DestructiveActionGuard guard(&restoreService);

    const auto outcome = guard.evaluate(QStringLiteral("Danger action"), true, false);
    QVERIFY(!outcome.proceed);
    QVERIFY(outcome.needsRestoreOverride);
}

void RestoreGuardIntegrationTests::allowsWhenOverrideProvided() {
    voidcare::core::RestorePointService restoreService(nullptr);
    voidcare::core::DestructiveActionGuard guard(&restoreService);

    const auto outcome = guard.evaluate(QStringLiteral("Danger action"), true, true);
    QVERIFY(outcome.proceed);
    QVERIFY(!outcome.needsRestoreOverride);
}

QTEST_MAIN(RestoreGuardIntegrationTests)
#include "test_restore_guard.moc"
