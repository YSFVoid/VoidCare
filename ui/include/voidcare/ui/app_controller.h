#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

#include "voidcare/core/antivirus_providers.h"
#include "voidcare/core/av_discovery_service.h"
#include "voidcare/core/destructive_action_guard.h"
#include "voidcare/core/discord_rpc_service.h"
#include "voidcare/core/gaming_boost_service.h"
#include "voidcare/core/optimization_service.h"
#include "voidcare/core/persistence_audit_service.h"
#include "voidcare/core/process_runner.h"
#include "voidcare/core/restore_point_service.h"
#include "voidcare/core/suspicious_file_scanner_service.h"
#include "voidcare/core/windows_apps_service.h"

namespace voidcare::ui {

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QString creditsText READ creditsText CONSTANT)
    Q_PROPERTY(QString footerText READ footerText CONSTANT)
    Q_PROPERTY(QString warningBannerText READ warningBannerText CONSTANT)
    Q_PROPERTY(bool discordEnabled READ discordEnabled WRITE setDiscordEnabled NOTIFY discordStateChanged)
    Q_PROPERTY(QString discordChipText READ discordChipText NOTIFY discordStateChanged)
    Q_PROPERTY(QString discordAboutStatus READ discordAboutStatus NOTIFY discordStateChanged)
    Q_PROPERTY(QString antivirusProviderName READ antivirusProviderName NOTIFY antivirusChanged)
    Q_PROPERTY(QString antivirusStatus READ antivirusStatus NOTIFY antivirusChanged)
    Q_PROPERTY(bool defenderScanAvailable READ defenderScanAvailable NOTIFY antivirusChanged)
    Q_PROPERTY(bool defenderRemediationAvailable READ defenderRemediationAvailable NOTIFY antivirusChanged)
    Q_PROPERTY(bool externalScannerAvailable READ externalScannerAvailable NOTIFY antivirusChanged)
    Q_PROPERTY(QVariantList persistenceEntries READ persistenceEntries NOTIFY persistenceEntriesChanged)
    Q_PROPERTY(QVariantList suspiciousEntries READ suspiciousEntries NOTIFY suspiciousEntriesChanged)
    Q_PROPERTY(QVariantList quarantineEntries READ quarantineEntries NOTIFY quarantineEntriesChanged)
    Q_PROPERTY(QVariantList installedApps READ installedApps NOTIFY installedAppsChanged)
    Q_PROPERTY(QStringList logs READ logs NOTIFY logsChanged)
    Q_PROPERTY(QString healthSummary READ healthSummary NOTIFY healthSummaryChanged)

public:
    explicit AppController(QObject* parent = nullptr);

    [[nodiscard]] QString currentPage() const;
    [[nodiscard]] QString creditsText() const;
    [[nodiscard]] QString footerText() const;
    [[nodiscard]] QString warningBannerText() const;
    [[nodiscard]] bool discordEnabled() const;
    [[nodiscard]] QString discordChipText() const;
    [[nodiscard]] QString discordAboutStatus() const;
    [[nodiscard]] QString antivirusProviderName() const;
    [[nodiscard]] QString antivirusStatus() const;
    [[nodiscard]] bool defenderScanAvailable() const;
    [[nodiscard]] bool defenderRemediationAvailable() const;
    [[nodiscard]] bool externalScannerAvailable() const;
    [[nodiscard]] QVariantList persistenceEntries() const;
    [[nodiscard]] QVariantList suspiciousEntries() const;
    [[nodiscard]] QVariantList quarantineEntries() const;
    [[nodiscard]] QVariantList installedApps() const;
    [[nodiscard]] QStringList logs() const;
    [[nodiscard]] QString healthSummary() const;

    Q_INVOKABLE void navigateTo(const QString& pageName);
    Q_INVOKABLE void refreshAntivirus();
    Q_INVOKABLE bool configureExternalScanner(const QString& executable, const QString& argsLine);
    Q_INVOKABLE QVariantMap runExternalScannerCommand();

    Q_INVOKABLE QVariantMap runDefenderQuickScan();
    Q_INVOKABLE QVariantMap runDefenderFullScan();
    Q_INVOKABLE QVariantMap runDefenderCustomScan(const QString& customPath);
    Q_INVOKABLE QVariantMap runDefenderAutoRemediate();

    Q_INVOKABLE QVariantMap refreshPersistenceAudit();
    Q_INVOKABLE QVariantMap disablePersistenceEntry(const QString& entryId,
                                                    bool initialConfirmed,
                                                    bool proceedWithoutRestorePoint);

    Q_INVOKABLE QVariantMap runQuickSuspiciousScan();
    Q_INVOKABLE QVariantMap runFullSuspiciousScan(const QStringList& roots);
    Q_INVOKABLE QVariantMap quarantineSelected(const QStringList& filePaths,
                                               bool initialConfirmed,
                                               bool proceedWithoutRestorePoint);
    Q_INVOKABLE QVariantMap restoreQuarantined(const QStringList& quarantinePaths,
                                               const QString& destinationOverride);
    Q_INVOKABLE QVariantMap deleteQuarantined(const QStringList& quarantinePaths,
                                              bool initialConfirmed,
                                              bool proceedWithoutRestorePoint);

    Q_INVOKABLE QVariantMap runSafeOptimization(bool initialConfirmed,
                                                bool proceedWithoutRestorePoint);
    Q_INVOKABLE QVariantMap runAggressiveOptimization(bool removeBloat,
                                                      bool disableCopilot,
                                                      bool initialConfirmed,
                                                      bool proceedWithoutRestorePoint);

    Q_INVOKABLE QVariantMap enableGamingBoost(bool initialConfirmed,
                                              bool proceedWithoutRestorePoint);
    Q_INVOKABLE QVariantMap revertGamingBoost(bool initialConfirmed,
                                              bool proceedWithoutRestorePoint);

    Q_INVOKABLE QVariantMap refreshInstalledApps();
    Q_INVOKABLE bool openAppsSettings();
    Q_INVOKABLE bool openProgramsAndFeatures();

    Q_INVOKABLE QVariantMap refreshHealthReport();
    Q_INVOKABLE void clearLogs();

public slots:
    void setDiscordEnabled(bool enabled);

signals:
    void currentPageChanged();
    void discordStateChanged();
    void antivirusChanged();
    void persistenceEntriesChanged();
    void suspiciousEntriesChanged();
    void quarantineEntriesChanged();
    void installedAppsChanged();
    void logsChanged();
    void healthSummaryChanged();

private:
    QVariantMap guardOrBlock(const QString& actionName,
                             bool initialConfirmed,
                             bool proceedWithoutRestorePoint) const;
    QVariantMap makeActionResult(bool success,
                                 const QString& message,
                                 int exitCode = 0,
                                 bool needsRestoreOverride = false,
                                 const QString& restoreDetail = QString()) const;

    void appendLog(const QString& line, bool isError = false);
    void updateDiscordPresenceForCurrentPage();
    void rebuildPersistenceVariant();
    void rebuildSuspiciousVariant();
    void rebuildQuarantineVariant();
    void rebuildAppsVariant();

    QVector<core::SuspiciousFileRecord> resolveSuspiciousSelection(const QStringList& filePaths) const;
    QVector<core::QuarantineManifestEntry> resolveQuarantineSelection(const QStringList& quarantinePaths) const;

    std::unique_ptr<core::ProcessRunner> m_runner;
    std::unique_ptr<core::RestorePointService> m_restorePoints;
    std::unique_ptr<core::DestructiveActionGuard> m_destructiveGuard;
    std::unique_ptr<core::AvDiscoveryService> m_avDiscovery;
    std::unique_ptr<core::IAntivirusProvider> m_antivirusProvider;
    core::ExternalProvider* m_externalProvider = nullptr;
    std::unique_ptr<core::PersistenceAuditService> m_persistenceAudit;
    std::unique_ptr<core::SuspiciousFileScannerService> m_suspiciousScanner;
    std::unique_ptr<core::OptimizationService> m_optimization;
    std::unique_ptr<core::GamingBoostService> m_gaming;
    std::unique_ptr<core::WindowsAppsService> m_windowsApps;
    std::unique_ptr<core::DiscordRpcService> m_discord;

    QVector<core::AntivirusProduct> m_discoveredProducts;
    QVector<core::PersistenceEntry> m_persistenceRaw;
    QVector<core::SuspiciousFileRecord> m_suspiciousRaw;
    QVector<core::QuarantineManifestEntry> m_quarantineRaw;
    QVector<core::InstalledAppInfo> m_appsRaw;

    QVariantList m_persistenceVariant;
    QVariantList m_suspiciousVariant;
    QVariantList m_quarantineVariant;
    QVariantList m_appsVariant;

    QString m_currentPage = QStringLiteral("Dashboard");
    QString m_antivirusProviderName;
    QString m_antivirusStatus;
    QStringList m_logs;
    QString m_healthSummary;
    bool m_defenderScanAvailable = false;
    bool m_defenderRemediationAvailable = false;
    bool m_discordConfigured = false;
    QTimer* m_discordHeartbeat = nullptr;
};

}  // namespace voidcare::ui
