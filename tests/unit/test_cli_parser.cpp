#include <QtTest/QtTest>

#include "cli_common.h"

class CliParserTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesGlobalFlagsAnywhere() {
        const QStringList args = {
            QStringLiteral("security"),
            QStringLiteral("--json"),
            QStringLiteral("scan"),
            QStringLiteral("--quick"),
            QStringLiteral("--yes"),
            QStringLiteral("--dry-run"),
        };

        const auto parsed = voidcare::cli::parseGlobalFlags(args);
        QCOMPARE(parsed.options.json, true);
        QCOMPARE(parsed.options.yes, true);
        QCOMPARE(parsed.options.dryRun, true);
        QCOMPARE(parsed.remainingArgs,
                 QStringList({QStringLiteral("security"), QStringLiteral("scan"), QStringLiteral("--quick")}));
    }

    void parsesIdCsvAndDeduplicates() {
        bool ok = false;
        const QVector<int> ids = voidcare::cli::parseIdCsv(QStringLiteral("3,1,3,2"), &ok);
        QCOMPARE(ok, true);
        QCOMPARE(ids, QVector<int>({1, 2, 3}));
    }

    void detectsInvalidIdCsv() {
        bool ok = true;
        const QVector<int> ids = voidcare::cli::parseIdCsv(QStringLiteral("a,2"), &ok);
        QCOMPARE(ok, false);
        QCOMPARE(ids, QVector<int>({2}));
    }
};

QTEST_MAIN(CliParserTests)
#include "test_cli_parser.moc"

