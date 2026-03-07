namespace VoidCare.Core.Models;

public enum SignatureStatus
{
    Valid,
    Invalid,
    Unsigned,
    Unknown,
}

public sealed record AntivirusProductInfo(
    string Name,
    int ProductState,
    bool Active,
    bool UpToDate,
    string StatusText,
    string? ProductPath = null);

public sealed record DefenderStatusInfo(
    bool MpCmdRunAvailable,
    string? MpCmdRunPath,
    bool PowerShellStatusAvailable,
    IReadOnlyDictionary<string, string> Fields,
    string? RawPowerShellJson);

public sealed record PersistenceItem(
    int Id,
    string StableKey,
    string Type,
    string Name,
    string Path,
    string Args,
    SignatureStatus Signature,
    string SignatureText,
    string Location,
    string Reference,
    bool Enabled,
    string? Publisher = null);

public sealed record SuspiciousFileRecord(
    int Id,
    string StableKey,
    int Score,
    SignatureStatus SignatureStatus,
    string SignatureText,
    long Size,
    DateTimeOffset Modified,
    string Sha256,
    string Path,
    IReadOnlyList<string> Reasons,
    bool HiddenOrSystem,
    bool PersistenceLinked);

public sealed record QuarantineRecord(
    int Id,
    string OriginalPath,
    string QuarantinePath,
    string Sha256,
    DateTimeOffset Timestamp,
    IReadOnlyList<string> Reasons,
    string SignatureStatus,
    string ManifestPath);

public sealed record PersistenceIndexState(
    DateTimeOffset CreatedAt,
    IReadOnlyList<PersistenceItem> Items);

public sealed record SuspiciousScanState(
    DateTimeOffset CreatedAt,
    IReadOnlyList<string> Roots,
    bool Quick,
    IReadOnlyList<SuspiciousFileRecord> Items);
