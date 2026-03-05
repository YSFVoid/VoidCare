#pragma once

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace voidcare::app {

enum class PageId {
    Dashboard = 0,
    Security,
    Optimize,
    Gaming,
    Apps,
    About,
};

struct ActionResult {
    bool success = false;
    QString message;
    int exitCode = 0;
    bool needsRestoreOverride = false;
    QString restoreDetail;
};

struct PersistenceEntryRow {
    QString id;
    QString sourceType;
    QString name;
    QString path;
    QString args;
    QString publisher;
    QString signatureStatus;
    bool enabled = false;
};

struct SuspiciousEntryRow {
    QString path;
    qint64 size = 0;
    QString created;
    QString modified;
    QString sha256;
    QString signatureStatus;
    int score = 0;
    QStringList reasons;
    QString persistenceRef;
};

struct QuarantineEntryRow {
    QString originalPath;
    QString quarantinePath;
    QString sha256;
    QString timestamp;
    QString signatureStatus;
    QStringList reasons;
};

struct InstalledAppRow {
    QString name;
    QString version;
    QString publisher;
};

struct UiSnapshot {
    QString currentPage;
    QString creditsText;
    QString footerText;
    QString warningBannerText;
    bool discordEnabled = true;
    QString discordChipText;
    QString discordAboutStatus;
    QString antivirusProviderName;
    QString antivirusStatus;
    bool defenderScanAvailable = false;
    bool defenderRemediationAvailable = false;
    bool externalScannerAvailable = false;
    QVector<PersistenceEntryRow> persistenceEntries;
    QVector<SuspiciousEntryRow> suspiciousEntries;
    QVector<QuarantineEntryRow> quarantineEntries;
    QVector<InstalledAppRow> installedApps;
    QStringList logs;
    QString healthSummary;
};

struct UiState {
    UiSnapshot snapshot;
    PageId page = PageId::Dashboard;
    QString statusText = QStringLiteral("Ready.");
    bool statusIsError = false;
    bool removeBloat = false;
    bool disableCopilot = false;
    QString customScanPath;
    QString fullScanRoots;
    QString externalScannerExe;
    QString externalScannerArgs;
    int selectedConfigIndex = 0;
    bool discordToggle = true;
    int selectedPersistenceIndex = -1;
    QSet<QString> selectedSuspicious;
    QSet<QString> selectedQuarantine;
};

ActionResult parseActionResult(const QVariantMap& map);
UiSnapshot parseSnapshot(const QVariantMap& map);

QString pageIdToString(PageId page);
PageId pageIdFromString(const QString& pageName);

}  // namespace voidcare::app
