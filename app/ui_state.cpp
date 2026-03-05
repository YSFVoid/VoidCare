#include "ui_state.h"

namespace voidcare::app {

namespace {

QStringList toStringList(const QVariant& value) {
    if (value.canConvert<QStringList>()) {
        return value.toStringList();
    }

    QStringList out;
    const QVariantList list = value.toList();
    out.reserve(list.size());
    for (const QVariant& item : list) {
        out.push_back(item.toString());
    }
    return out;
}

QVector<PersistenceEntryRow> parsePersistenceEntries(const QVariant& value) {
    QVector<PersistenceEntryRow> rows;
    const QVariantList list = value.toList();
    rows.reserve(list.size());
    for (const QVariant& item : list) {
        const QVariantMap rowMap = item.toMap();
        PersistenceEntryRow row;
        row.id = rowMap.value(QStringLiteral("id")).toString();
        row.sourceType = rowMap.value(QStringLiteral("sourceType")).toString();
        row.name = rowMap.value(QStringLiteral("name")).toString();
        row.path = rowMap.value(QStringLiteral("path")).toString();
        row.args = rowMap.value(QStringLiteral("args")).toString();
        row.publisher = rowMap.value(QStringLiteral("publisher")).toString();
        row.signatureStatus = rowMap.value(QStringLiteral("signatureStatus")).toString();
        row.enabled = rowMap.value(QStringLiteral("enabled")).toBool();
        rows.push_back(row);
    }
    return rows;
}

QVector<SuspiciousEntryRow> parseSuspiciousEntries(const QVariant& value) {
    QVector<SuspiciousEntryRow> rows;
    const QVariantList list = value.toList();
    rows.reserve(list.size());
    for (const QVariant& item : list) {
        const QVariantMap rowMap = item.toMap();
        SuspiciousEntryRow row;
        row.path = rowMap.value(QStringLiteral("path")).toString();
        row.size = rowMap.value(QStringLiteral("size")).toLongLong();
        row.created = rowMap.value(QStringLiteral("created")).toString();
        row.modified = rowMap.value(QStringLiteral("modified")).toString();
        row.sha256 = rowMap.value(QStringLiteral("sha256")).toString();
        row.signatureStatus = rowMap.value(QStringLiteral("signatureStatus")).toString();
        row.score = rowMap.value(QStringLiteral("score")).toInt();
        row.reasons = toStringList(rowMap.value(QStringLiteral("reasons")));
        row.persistenceRef = rowMap.value(QStringLiteral("persistenceRef")).toString();
        rows.push_back(row);
    }
    return rows;
}

QVector<QuarantineEntryRow> parseQuarantineEntries(const QVariant& value) {
    QVector<QuarantineEntryRow> rows;
    const QVariantList list = value.toList();
    rows.reserve(list.size());
    for (const QVariant& item : list) {
        const QVariantMap rowMap = item.toMap();
        QuarantineEntryRow row;
        row.originalPath = rowMap.value(QStringLiteral("originalPath")).toString();
        row.quarantinePath = rowMap.value(QStringLiteral("quarantinePath")).toString();
        row.sha256 = rowMap.value(QStringLiteral("sha256")).toString();
        row.timestamp = rowMap.value(QStringLiteral("timestamp")).toString();
        row.signatureStatus = rowMap.value(QStringLiteral("signatureStatus")).toString();
        row.reasons = toStringList(rowMap.value(QStringLiteral("reasons")));
        rows.push_back(row);
    }
    return rows;
}

QVector<InstalledAppRow> parseInstalledApps(const QVariant& value) {
    QVector<InstalledAppRow> rows;
    const QVariantList list = value.toList();
    rows.reserve(list.size());
    for (const QVariant& item : list) {
        const QVariantMap rowMap = item.toMap();
        InstalledAppRow row;
        row.name = rowMap.value(QStringLiteral("name")).toString();
        row.version = rowMap.value(QStringLiteral("version")).toString();
        row.publisher = rowMap.value(QStringLiteral("publisher")).toString();
        rows.push_back(row);
    }
    return rows;
}

}  // namespace

ActionResult parseActionResult(const QVariantMap& map) {
    ActionResult result;
    result.success = map.value(QStringLiteral("success")).toBool();
    result.message = map.value(QStringLiteral("message")).toString();
    result.exitCode = map.value(QStringLiteral("exitCode")).toInt();
    result.needsRestoreOverride = map.value(QStringLiteral("needsRestoreOverride")).toBool();
    result.restoreDetail = map.value(QStringLiteral("restoreDetail")).toString();
    return result;
}

UiSnapshot parseSnapshot(const QVariantMap& map) {
    UiSnapshot snapshot;
    snapshot.currentPage = map.value(QStringLiteral("currentPage")).toString();
    snapshot.creditsText = map.value(QStringLiteral("creditsText")).toString();
    snapshot.footerText = map.value(QStringLiteral("footerText")).toString();
    snapshot.warningBannerText = map.value(QStringLiteral("warningBannerText")).toString();
    const auto discordEnabled = map.find(QStringLiteral("discordEnabled"));
    snapshot.discordEnabled =
        (discordEnabled == map.end() || !discordEnabled->isValid()) ? true : discordEnabled->toBool();
    snapshot.discordChipText = map.value(QStringLiteral("discordChipText")).toString();
    snapshot.discordAboutStatus = map.value(QStringLiteral("discordAboutStatus")).toString();
    snapshot.antivirusProviderName = map.value(QStringLiteral("antivirusProviderName")).toString();
    snapshot.antivirusStatus = map.value(QStringLiteral("antivirusStatus")).toString();
    snapshot.defenderScanAvailable = map.value(QStringLiteral("defenderScanAvailable")).toBool();
    snapshot.defenderRemediationAvailable =
        map.value(QStringLiteral("defenderRemediationAvailable")).toBool();
    snapshot.externalScannerAvailable = map.value(QStringLiteral("externalScannerAvailable")).toBool();
    snapshot.persistenceEntries = parsePersistenceEntries(map.value(QStringLiteral("persistenceEntries")));
    snapshot.suspiciousEntries = parseSuspiciousEntries(map.value(QStringLiteral("suspiciousEntries")));
    snapshot.quarantineEntries = parseQuarantineEntries(map.value(QStringLiteral("quarantineEntries")));
    snapshot.installedApps = parseInstalledApps(map.value(QStringLiteral("installedApps")));
    snapshot.logs = toStringList(map.value(QStringLiteral("logs")));
    snapshot.healthSummary = map.value(QStringLiteral("healthSummary")).toString();
    return snapshot;
}

QString pageIdToString(const PageId page) {
    switch (page) {
    case PageId::Dashboard:
        return QStringLiteral("Dashboard");
    case PageId::Security:
        return QStringLiteral("Security");
    case PageId::Optimize:
        return QStringLiteral("Optimize");
    case PageId::Gaming:
        return QStringLiteral("Gaming");
    case PageId::Apps:
        return QStringLiteral("Apps");
    case PageId::About:
        return QStringLiteral("About");
    }

    return QStringLiteral("Dashboard");
}

PageId pageIdFromString(const QString& pageName) {
    if (pageName.compare(QStringLiteral("Security"), Qt::CaseInsensitive) == 0) {
        return PageId::Security;
    }
    if (pageName.compare(QStringLiteral("Optimize"), Qt::CaseInsensitive) == 0) {
        return PageId::Optimize;
    }
    if (pageName.compare(QStringLiteral("Gaming"), Qt::CaseInsensitive) == 0) {
        return PageId::Gaming;
    }
    if (pageName.compare(QStringLiteral("Apps"), Qt::CaseInsensitive) == 0) {
        return PageId::Apps;
    }
    if (pageName.compare(QStringLiteral("About"), Qt::CaseInsensitive) == 0) {
        return PageId::About;
    }
    return PageId::Dashboard;
}

}  // namespace voidcare::app
