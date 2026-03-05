#include <QtTest/QtTest>

#include <QJsonDocument>
#include <QJsonObject>

#include "cli_common.h"

class CliRenderingTests final : public QObject {
    Q_OBJECT

private slots:
    void rendersAsciiTable() {
        const QString table = voidcare::cli::renderTable(
            {QStringLiteral("A"), QStringLiteral("B")},
            {{QStringLiteral("1"), QStringLiteral("two")}});
        QVERIFY(table.contains(QStringLiteral("| A ")));
        QVERIFY(table.contains(QStringLiteral("two")));
        QVERIFY(table.contains(QStringLiteral("+")));
    }

    void rendersJsonEnvelope() {
        voidcare::cli::CliResult result;
        result.success = true;
        result.exitCode = 0;
        result.command = QStringLiteral("status");
        result.message = QStringLiteral("ok");
        result.warnings = {QStringLiteral("warn")};
        result.plannedActions = {QStringLiteral("plan")};

        const QByteArray json = voidcare::cli::renderJsonEnvelope(result);
        const QJsonDocument doc = QJsonDocument::fromJson(json);
        QVERIFY(doc.isObject());
        const QJsonObject obj = doc.object();
        QCOMPARE(obj.value(QStringLiteral("command")).toString(), QStringLiteral("status"));
        QCOMPARE(obj.value(QStringLiteral("success")).toBool(), true);
        QCOMPARE(obj.value(QStringLiteral("warnings")).toArray().size(), 1);
        QCOMPARE(obj.value(QStringLiteral("plannedActions")).toArray().size(), 1);
    }
};

QTEST_MAIN(CliRenderingTests)
#include "test_cli_rendering.moc"

