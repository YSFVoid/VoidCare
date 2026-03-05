#include "voidcare/core/types.h"

#include "voidcare/platform/windows/signature_verifier.h"

namespace voidcare::core {

QVariantMap persistenceEntryToVariant(const PersistenceEntry& entry) {
    QVariantMap map;
    map.insert(QStringLiteral("id"), entry.id);
    map.insert(QStringLiteral("sourceType"), entry.sourceType);
    map.insert(QStringLiteral("name"), entry.name);
    map.insert(QStringLiteral("path"), entry.path);
    map.insert(QStringLiteral("args"), entry.args);
    map.insert(QStringLiteral("publisher"), entry.publisher);
    map.insert(QStringLiteral("signatureStatus"),
               platform::windows::signatureStatusToString(entry.signatureStatus));
    map.insert(QStringLiteral("enabled"), entry.enabled);
    map.insert(QStringLiteral("rawReference"), entry.rawReference);
    return map;
}

QVariantMap suspiciousRecordToVariant(const SuspiciousFileRecord& record) {
    QVariantMap map;
    map.insert(QStringLiteral("path"), record.path);
    map.insert(QStringLiteral("size"), record.size);
    map.insert(QStringLiteral("created"), record.created.toString(Qt::ISODate));
    map.insert(QStringLiteral("modified"), record.modified.toString(Qt::ISODate));
    map.insert(QStringLiteral("sha256"), record.sha256);
    map.insert(QStringLiteral("signatureStatus"),
               platform::windows::signatureStatusToString(record.signatureStatus));
    map.insert(QStringLiteral("score"), record.score);
    map.insert(QStringLiteral("reasons"), record.reasons);
    map.insert(QStringLiteral("persistenceRef"), record.persistenceRef);
    map.insert(QStringLiteral("hiddenOrSystem"), record.hiddenOrSystem);
    return map;
}

QVariantMap installedAppToVariant(const InstalledAppInfo& app) {
    QVariantMap map;
    map.insert(QStringLiteral("name"), app.name);
    map.insert(QStringLiteral("version"), app.version);
    map.insert(QStringLiteral("publisher"), app.publisher);
    return map;
}

QString discordStateLabel(const DiscordPresenceState state) {
    switch (state) {
    case DiscordPresenceState::Dashboard:
        return QStringLiteral("from VoidTools");
    case DiscordPresenceState::Security:
        return QStringLiteral("Security Scan");
    case DiscordPresenceState::SuspiciousFiles:
        return QStringLiteral("Suspicious File Cleanup");
    case DiscordPresenceState::Optimize:
        return QStringLiteral("Optimization");
    case DiscordPresenceState::Gaming:
        return QStringLiteral("Gaming Boost");
    case DiscordPresenceState::Apps:
        return QStringLiteral("Apps Manager");
    default:
        return QStringLiteral("from VoidTools");
    }
}

}  // namespace voidcare::core
