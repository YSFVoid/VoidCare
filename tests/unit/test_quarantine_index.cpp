#include <QtTest/QtTest>

#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "cli_common.h"
#include "voidcare/core/suspicious_file_scanner_service.h"

class QuarantineIndexTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsManifestEntriesWithStableIds() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString folder = dir.filePath(QStringLiteral("Q1"));
        QVERIFY(QDir().mkpath(folder));

        QVector<voidcare::core::QuarantineManifestEntry> manifest;
        voidcare::core::QuarantineManifestEntry entryA;
        entryA.originalPath = QStringLiteral("C:/a.exe");
        entryA.quarantinePath = QStringLiteral("C:/Q/a.exe");
        entryA.sha256 = QStringLiteral("aaa");
        entryA.timestamp = QStringLiteral("2026-01-01T01:00:00Z");
        entryA.signatureStatus = QStringLiteral("Unsigned");
        manifest.push_back(entryA);

        voidcare::core::QuarantineManifestEntry entryB;
        entryB.originalPath = QStringLiteral("C:/b.exe");
        entryB.quarantinePath = QStringLiteral("C:/Q/b.exe");
        entryB.sha256 = QStringLiteral("bbb");
        entryB.timestamp = QStringLiteral("2026-01-01T02:00:00Z");
        entryB.signatureStatus = QStringLiteral("Unsigned");
        manifest.push_back(entryB);

        QFile file(QDir(folder).filePath(QStringLiteral("manifest.json")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(voidcare::core::quarantineManifestToJson(manifest));
        file.close();

        QString error;
        const QVector<voidcare::cli::QuarantineListItem> items =
            voidcare::cli::loadQuarantineIndex(dir.path(), &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(items.size(), 2);
        QCOMPARE(items[0].id, 1);
        QCOMPARE(items[1].id, 2);
        QCOMPARE(items[0].entry.originalPath, QStringLiteral("C:/a.exe"));
        QCOMPARE(items[1].entry.originalPath, QStringLiteral("C:/b.exe"));
    }
};

QTEST_MAIN(QuarantineIndexTests)
#include "test_quarantine_index.moc"
