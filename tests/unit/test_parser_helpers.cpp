#include <QtTest>

#include "voidcare/core/parser_utils.h"

class ParserHelpersTests : public QObject {
    Q_OBJECT

private slots:
    void parseCsvHandlesQuotedCommas();
    void splitExecutableParsesQuotedPath();
    void expandEnvironmentStringsResolvesVariables();
};

void ParserHelpersTests::parseCsvHandlesQuotedCommas() {
    const QString line = QStringLiteral("\"TaskName\",\"Task To Run\",\"Status\"");
    const QStringList cells = voidcare::core::parseCsvLine(line);
    QCOMPARE(cells.size(), 3);
    QCOMPARE(cells[0], QStringLiteral("TaskName"));
    QCOMPARE(cells[1], QStringLiteral("Task To Run"));
}

void ParserHelpersTests::splitExecutableParsesQuotedPath() {
    const auto split =
        voidcare::core::splitExecutableAndArgs(QStringLiteral("\"C:/Program Files/App/app.exe\" -silent"));
    QCOMPARE(split.first, QStringLiteral("C:/Program Files/App/app.exe"));
    QCOMPARE(split.second, QStringLiteral("-silent"));
}

void ParserHelpersTests::expandEnvironmentStringsResolvesVariables() {
    const QString expanded = voidcare::core::expandEnvironmentStrings(QStringLiteral("%SystemRoot%"));
    QVERIFY(!expanded.isEmpty());
    QVERIFY(expanded.contains(QStringLiteral("Windows"), Qt::CaseInsensitive));
}

QTEST_MAIN(ParserHelpersTests)
#include "test_parser_helpers.moc"
