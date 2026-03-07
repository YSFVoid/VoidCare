using System.Text.Json;
using System.Text.Json.Serialization;

namespace VoidCare.Core.Models;

public static class AppMetadata
{
    public const string Name = "VoidCare";
    public const string Subtitle = "VoidTools • Windows x64 • Offline-only";
    public const string Credits = "Developed by Ysf (Lone Wolf Developer)";
    public const string OfflineNotice = "Offline-only: no HTTP/web calls, downloads, or telemetry.";
    public const string Runtime = "win-x64";
}

public sealed record CommandOptions(
    bool Json = false,
    bool Quiet = false,
    bool Verbose = false,
    bool Yes = false,
    bool DryRun = false,
    bool NoBanner = false);

public enum OutputSeverity
{
    Ok,
    Warn,
    Fail,
    Info,
}

public sealed record LastActionState(
    string Command,
    bool Success,
    int ExitCode,
    string Summary,
    DateTimeOffset Timestamp);

public sealed record ProgressEvent(OutputSeverity Severity, string Message, double? Percent = null);

public static class JsonDefaults
{
    public static readonly JsonSerializerOptions Options = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };

    public static readonly JsonSerializerOptions CompactOptions = new()
    {
        WriteIndented = false,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };
}
