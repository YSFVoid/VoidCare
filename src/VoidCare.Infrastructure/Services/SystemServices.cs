using System.Diagnostics;
using System.Security.Principal;
using System.Text.Json;
using VoidCare.Core.Models;

namespace VoidCare.Infrastructure.Services;

public sealed class PathService
{
    public string Root => Ensure(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "VoidCare"));
    public string LogsRoot => Ensure(Path.Combine(Root, "Logs"));
    public string StateRoot => Ensure(Path.Combine(Root, "State"));
    public string QuarantineRoot => Ensure(Path.Combine(Root, "Quarantine"));

    public string LastActionPath => Path.Combine(StateRoot, "last-action.json");
    public string PersistenceIndexPath => Path.Combine(StateRoot, "persistence-index.json");
    public string SuspiciousScanPath => Path.Combine(StateRoot, "suspicious-scan.json");

    public IReadOnlyList<string> StartupFolders =>
    [
        Environment.GetFolderPath(Environment.SpecialFolder.Startup),
        Environment.GetFolderPath(Environment.SpecialFolder.CommonStartup),
    ];

    public IReadOnlyList<string> QuickSuspiciousRoots =>
    [
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads"),
        Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
        Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
        Path.GetTempPath(),
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Temp"),
        .. StartupFolders,
    ];

    public IReadOnlyList<string> BrowserCacheRoots =>
    [
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Google", "Chrome", "User Data", "Default", "Cache"),
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Microsoft", "Edge", "User Data", "Default", "Cache"),
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Mozilla", "Firefox", "Profiles"),
    ];

    public IReadOnlyList<string> CrashDumpRoots =>
    [
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "CrashDumps"),
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "D3DSCache"),
    ];

    public bool IsUserWritableRiskLocation(string path)
    {
        var normalized = Normalize(path);
        var riskyRoots = new[]
        {
            Normalize(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile)),
            Normalize(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData)),
            Normalize(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData)),
            Normalize(Path.GetTempPath()),
        };

        return riskyRoots.Any(root => normalized.StartsWith(root, StringComparison.OrdinalIgnoreCase));
    }

    public string Normalize(string path) => Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar);

    private static string Ensure(string path)
    {
        Directory.CreateDirectory(path);
        return path;
    }
}

public sealed class StateStore
{
    private readonly PathService _paths;

    public StateStore(PathService paths)
    {
        _paths = paths;
    }

    public void Write<T>(string path, T value)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, JsonSerializer.Serialize(value, JsonDefaults.Options));
    }

    public T? Read<T>(string path)
    {
        if (!File.Exists(path))
        {
            return default;
        }

        return JsonSerializer.Deserialize<T>(File.ReadAllText(path), JsonDefaults.Options);
    }

    public bool Exists(string path) => File.Exists(path);

    public void Delete(string path)
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}

public sealed class ActionLogger
{
    private readonly PathService _paths;

    public ActionLogger(PathService paths)
    {
        _paths = paths;
    }

    public void Log(string level, string command, string message)
    {
        Directory.CreateDirectory(_paths.LogsRoot);
        var line = $"{DateTimeOffset.UtcNow:O}|{level}|{command}|{message.Replace(Environment.NewLine, " ")}";
        File.AppendAllLines(Path.Combine(_paths.LogsRoot, "voidcare.log"), [line]);
    }

    public IReadOnlyList<string> ReadTail(int tail)
    {
        var path = Path.Combine(_paths.LogsRoot, "voidcare.log");
        if (!File.Exists(path))
        {
            return [];
        }

        return File.ReadLines(path).TakeLast(Math.Max(tail, 1)).ToArray();
    }
}

public sealed class AdminService
{
    public bool IsElevated()
    {
        using var identity = WindowsIdentity.GetCurrent();
        var principal = new WindowsPrincipal(identity);
        return principal.IsInRole(WindowsBuiltInRole.Administrator);
    }
}

public sealed record RestorePointOutcome(bool Success, string Message, string? Detail = null);

public sealed class RestorePointService
{
    private readonly PowerShellRunner _powerShellRunner;

    public RestorePointService(PowerShellRunner powerShellRunner)
    {
        _powerShellRunner = powerShellRunner;
    }

    public async Task<RestorePointOutcome> CreateAsync(string actionLabel, bool verbose = false, CancellationToken cancellationToken = default)
    {
        var safeLabel = actionLabel.Replace("'", "''", StringComparison.Ordinal);
        var script = $"Checkpoint-Computer -Description 'VoidCare {safeLabel}' -RestorePointType 'MODIFY_SETTINGS'";
        var result = await _powerShellRunner.RunScriptAsync(script, verbose: verbose, cancellationToken: cancellationToken);
        if (result.Success)
        {
            return new RestorePointOutcome(true, "Restore point created.");
        }

        var detail = string.Join(Environment.NewLine, new[] { result.ErrorMessage, result.StandardError, result.StandardOutput }
            .Where(static text => !string.IsNullOrWhiteSpace(text)));
        return new RestorePointOutcome(false, "Failed to create restore point.", detail);
    }
}

public sealed class ExplorerService
{
    public bool OpenFolder(string path)
    {
        try
        {
            Process.Start(new ProcessStartInfo("explorer.exe", $"\"{path}\"")
            {
                UseShellExecute = true,
            });
            return true;
        }
        catch
        {
            return false;
        }
    }
}
