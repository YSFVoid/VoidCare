#include <QtTest>

#include <atomic>
#include <chrono>
#include <thread>

#include "voidcare/core/process_runner.h"

class ProcessRunnerIntegrationTests : public QObject {
    Q_OBJECT

private slots:
    void streamsOutput();
    void supportsCancellation();
};

void ProcessRunnerIntegrationTests::streamsOutput() {
    voidcare::core::ProcessRunner runner;

    voidcare::core::ProcessRunRequest request;
    request.executable = QStringLiteral("cmd.exe");
    request.arguments = {QStringLiteral("/c"), QStringLiteral("echo VoidCareRunner")};

    QStringList lines;
    const auto result = runner.run(request, [&lines](const QString& line, bool) {
        lines << line;
    });

    QVERIFY(result.success());
    QVERIFY(result.stdOut.contains(QStringLiteral("VoidCareRunner"), Qt::CaseInsensitive));
    QVERIFY(!lines.isEmpty());
}

void ProcessRunnerIntegrationTests::supportsCancellation() {
    voidcare::core::ProcessRunner runner;
    std::atomic_bool cancelToken{false};

    voidcare::core::ProcessRunRequest request;
    request.executable = QStringLiteral("powershell.exe");
    request.arguments = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-Command"),
        QStringLiteral("Start-Sleep -Seconds 6; Write-Output done"),
    };

    std::thread cancelThread([&cancelToken]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(350));
        cancelToken.store(true);
    });

    const auto result = runner.run(request, {}, &cancelToken);
    cancelThread.join();

    QVERIFY(result.canceled);
}

QTEST_MAIN(ProcessRunnerIntegrationTests)
#include "test_process_runner.moc"
