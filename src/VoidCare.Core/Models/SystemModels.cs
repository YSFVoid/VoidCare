namespace VoidCare.Core.Models;

public sealed record SafeCleanupOptions(
    int OlderThanDays,
    bool IncludeWindowsTemp,
    bool IncludeBrowserCache,
    bool DryRun);

public sealed record SafeCleanupSummary(
    bool Success,
    long FilesScanned,
    long FilesDeleted,
    long BytesFreed,
    TimeSpan Elapsed,
    IReadOnlyList<string> Warnings,
    IReadOnlyList<string> Targets);

public sealed record PerformanceChange(string Name, string Value);

public sealed record InstalledAppInfo(
    string StableKey,
    string Name,
    string Publisher,
    string Version,
    string Type,
    string? UninstallCommand = null,
    string? QuietUninstallCommand = null,
    string? PackageFullName = null,
    string? FamilyName = null);

public sealed record BloatCatalogEntry(
    string Key,
    string DisplayName,
    string Type,
    string Match,
    string MatchMode,
    string Reason);

public sealed record BloatCandidate(
    int Id,
    BloatCatalogEntry Catalog,
    InstalledAppInfo InstalledApp);

public sealed record LogEntry(
    DateTimeOffset Timestamp,
    string Level,
    string Command,
    string Message);
