using Microsoft.Win32;
using VoidCare.Core.Models;
using VoidCare.Core.Services;

namespace VoidCare.Infrastructure.Services;

public sealed class OptimizationService
{
    private readonly PathService _paths;
    private readonly ProcessRunner _processRunner;

    public OptimizationService(PathService paths, ProcessRunner processRunner)
    {
        _paths = paths;
        _processRunner = processRunner;
    }

    public Task<SafeCleanupSummary> RunSafeCleanupAsync(
        SafeCleanupOptions options,
        Action<ProgressEvent>? progress = null,
        CancellationToken cancellationToken = default)
    {
        var started = DateTimeOffset.UtcNow;
        var warnings = new List<string>();
        var targets = new List<string>();
        long filesScanned = 0;
        long filesDeleted = 0;
        long bytesFreed = 0;

        var roots = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            Path.GetTempPath(),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Temp"),
        };

        foreach (var root in _paths.CrashDumpRoots)
        {
            roots.Add(root);
        }

        if (options.IncludeWindowsTemp)
        {
            roots.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "Temp"));
        }

        if (options.IncludeBrowserCache)
        {
            foreach (var root in _paths.BrowserCacheRoots)
            {
                roots.Add(root);
            }
        }

        var cutoff = DateTime.UtcNow.AddDays(-Math.Max(options.OlderThanDays, 0));
        foreach (var root in roots)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (!Directory.Exists(root))
            {
                continue;
            }

            targets.Add(root);
            progress?.Invoke(new ProgressEvent(OutputSeverity.Info, $"Processing {root}"));
            foreach (var file in EnumerateFilesSafe(root))
            {
                cancellationToken.ThrowIfCancellationRequested();
                filesScanned++;

                try
                {
                    var info = new FileInfo(file);
                    if (info.LastWriteTimeUtc > cutoff)
                    {
                        continue;
                    }

                    bytesFreed += Math.Max(0, info.Length);
                    filesDeleted++;
                    if (!options.DryRun)
                    {
                        File.Delete(file);
                    }
                }
                catch (Exception ex)
                {
                    warnings.Add($"{file}: {ex.Message}");
                }
            }

            if (!options.DryRun)
            {
                RemoveEmptyDirectories(root);
            }
        }

        return Task.FromResult(new SafeCleanupSummary(
            warnings.Count == 0,
            filesScanned,
            filesDeleted,
            bytesFreed,
            DateTimeOffset.UtcNow - started,
            warnings,
            targets));
    }

    public async Task<(bool Success, string Message, IReadOnlyList<PerformanceChange> Changes)> ApplyPerformancePresetAsync(
        bool dryRun,
        bool verbose = false,
        CancellationToken cancellationToken = default)
    {
        var changes = new List<PerformanceChange>
        {
            new("Power Plan", "High performance"),
            new("Game Mode", "Enabled"),
        };

        if (dryRun)
        {
            return (true, "Dry-run: performance preset planned.", changes);
        }

        var power = await _processRunner.RunAsync("powercfg.exe", ["/setactive", "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c"], verbose: verbose, cancellationToken: cancellationToken);
        using var gameBar = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\GameBar", writable: true);
        gameBar?.SetValue("AutoGameModeEnabled", 1, RegistryValueKind.DWord);
        gameBar?.SetValue("AllowAutoGameMode", 1, RegistryValueKind.DWord);

        return (power.Success, power.Success ? "Applied safe performance preset." : "Performance preset completed with warnings.", changes);
    }

    public Task<(bool Success, string Message, IReadOnlyList<PerformanceChange> Changes)> DisableCopilotAsync(bool dryRun)
    {
        var changes = new List<PerformanceChange> { new("Windows Copilot", "Disabled policy set (best effort)") };
        if (dryRun)
        {
            return Task.FromResult<(bool, string, IReadOnlyList<PerformanceChange>)>((true, "Dry-run: Copilot disable planned.", changes));
        }

        using var hkcu = Registry.CurrentUser.CreateSubKey(@"Software\Policies\Microsoft\Windows\WindowsCopilot", writable: true);
        hkcu?.SetValue("TurnOffWindowsCopilot", 1, RegistryValueKind.DWord);
        using var hklm = Registry.LocalMachine.CreateSubKey(@"SOFTWARE\Policies\Microsoft\Windows\WindowsCopilot", writable: true);
        hklm?.SetValue("TurnOffWindowsCopilot", 1, RegistryValueKind.DWord);
        return Task.FromResult<(bool, string, IReadOnlyList<PerformanceChange>)>((true, "Applied Copilot policy.", changes));
    }

    private static IEnumerable<string> EnumerateFilesSafe(string root)
    {
        if (!Directory.Exists(root))
        {
            yield break;
        }

        var pending = new Stack<string>();
        pending.Push(root);

        while (pending.Count > 0)
        {
            var current = pending.Pop();
            IEnumerable<string> files;
            try
            {
                files = Directory.EnumerateFiles(current);
            }
            catch
            {
                continue;
            }

            foreach (var file in files)
            {
                yield return file;
            }

            IEnumerable<string> directories;
            try
            {
                directories = Directory.EnumerateDirectories(current);
            }
            catch
            {
                continue;
            }

            foreach (var directory in directories)
            {
                pending.Push(directory);
            }
        }
    }

    private static void RemoveEmptyDirectories(string root)
    {
        foreach (var directory in Directory.EnumerateDirectories(root, "*", SearchOption.AllDirectories)
                     .OrderByDescending(static path => path.Length))
        {
            try
            {
                if (!Directory.EnumerateFileSystemEntries(directory).Any())
                {
                    Directory.Delete(directory, false);
                }
            }
            catch
            {
            }
        }
    }
}

public sealed class AppsService
{
    private readonly PowerShellRunner _powerShellRunner;
    private readonly ProcessRunner _processRunner;

    public AppsService(PowerShellRunner powerShellRunner, ProcessRunner processRunner)
    {
        _powerShellRunner = powerShellRunner;
        _processRunner = processRunner;
    }

    public async Task<IReadOnlyList<InstalledAppInfo>> ListAsync(string type, bool verbose = false, CancellationToken cancellationToken = default)
    {
        var mode = string.IsNullOrWhiteSpace(type) ? "all" : type.ToLowerInvariant();
        var apps = new List<InstalledAppInfo>();
        if (mode is "all" or "win32")
        {
            apps.AddRange(ListWin32Apps());
        }

        if (mode is "all" or "appx")
        {
            apps.AddRange(await ListAppxAppsAsync(verbose, cancellationToken));
        }

        return apps
            .OrderBy(static app => app.Name, StringComparer.OrdinalIgnoreCase)
            .ThenBy(static app => app.Type, StringComparer.OrdinalIgnoreCase)
            .ToArray();
    }

    public async Task<IReadOnlyList<BloatCandidate>> FindBloatCandidatesAsync(BloatCatalogService catalogService, bool verbose = false, CancellationToken cancellationToken = default)
    {
        var installed = await ListAsync("all", verbose, cancellationToken);
        var catalog = catalogService.Load();
        var matches = new List<BloatCandidate>();

        foreach (var entry in catalog)
        {
            var match = installed.FirstOrDefault(app => IsCatalogMatch(entry, app));
            if (match is null)
            {
                continue;
            }

            matches.Add(new BloatCandidate(0, entry, match));
        }

        matches = matches.OrderBy(static item => item.Catalog.DisplayName, StringComparer.OrdinalIgnoreCase).ToList();
        for (var index = 0; index < matches.Count; index++)
        {
            matches[index] = matches[index] with { Id = index + 1 };
        }

        return matches;
    }

    public async Task<(bool Success, string Message)> RemoveAsync(BloatCandidate candidate, bool dryRun, bool verbose = false, CancellationToken cancellationToken = default)
    {
        if (candidate.InstalledApp.Type.Equals("appx", StringComparison.OrdinalIgnoreCase))
        {
            if (dryRun)
            {
                return (true, $"Would remove Appx package {candidate.InstalledApp.PackageFullName}");
            }

            var package = candidate.InstalledApp.PackageFullName?.Replace("'", "''", StringComparison.Ordinal);
            var result = await _powerShellRunner.RunScriptAsync($"Remove-AppxPackage -Package '{package}' -ErrorAction Stop", verbose: verbose, cancellationToken: cancellationToken);
            return result.Success
                ? (true, $"Removed {candidate.InstalledApp.Name}")
                : (false, string.IsNullOrWhiteSpace(result.StandardError) ? $"Failed to remove {candidate.InstalledApp.Name}" : result.StandardError);
        }

        var command = BuildWin32UninstallCommand(candidate.InstalledApp);
        if (string.IsNullOrWhiteSpace(command))
        {
            return (false, $"No uninstall command available for {candidate.InstalledApp.Name}");
        }

        if (dryRun)
        {
            return (true, $"Would run uninstall command for {candidate.InstalledApp.Name}");
        }

        var resultWin32 = await _processRunner.RunAsync("cmd.exe", ["/c", command], verbose: verbose, cancellationToken: cancellationToken);
        return resultWin32.Success
            ? (true, $"Removed {candidate.InstalledApp.Name}")
            : (false, string.IsNullOrWhiteSpace(resultWin32.StandardError) ? $"Failed to remove {candidate.InstalledApp.Name}" : resultWin32.StandardError);
    }

    private static bool IsCatalogMatch(BloatCatalogEntry entry, InstalledAppInfo app)
    {
        if (!entry.Type.Equals(app.Type, StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        return entry.MatchMode switch
        {
            "packageName" => (app.PackageFullName?.Contains(entry.Match, StringComparison.OrdinalIgnoreCase) ?? false)
                || (app.FamilyName?.Contains(entry.Match, StringComparison.OrdinalIgnoreCase) ?? false),
            "nameContains" => app.Name.Contains(entry.Match, StringComparison.OrdinalIgnoreCase),
            _ => false,
        };
    }

    private static string? BuildWin32UninstallCommand(InstalledAppInfo app)
    {
        var command = string.IsNullOrWhiteSpace(app.QuietUninstallCommand) ? app.UninstallCommand : app.QuietUninstallCommand;
        if (string.IsNullOrWhiteSpace(command))
        {
            return null;
        }

        if (command.Contains("MsiExec", StringComparison.OrdinalIgnoreCase))
        {
            command = command.Replace("/I", "/X", StringComparison.OrdinalIgnoreCase);
            if (!command.Contains("/quiet", StringComparison.OrdinalIgnoreCase))
            {
                command += " /quiet /norestart";
            }
        }

        return command;
    }

    private IEnumerable<InstalledAppInfo> ListWin32Apps()
    {
        var locations = new[]
        {
            (Hive: Registry.LocalMachine, Path: @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"),
            (Hive: Registry.LocalMachine, Path: @"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall"),
            (Hive: Registry.CurrentUser, Path: @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"),
        };

        foreach (var location in locations)
        {
            using var key = location.Hive.OpenSubKey(location.Path, false);
            if (key is null)
            {
                continue;
            }

            foreach (var subKeyName in key.GetSubKeyNames())
            {
                using var appKey = key.OpenSubKey(subKeyName, false);
                if (appKey is null)
                {
                    continue;
                }

                var name = appKey.GetValue("DisplayName")?.ToString();
                if (string.IsNullOrWhiteSpace(name))
                {
                    continue;
                }

                var publisher = appKey.GetValue("Publisher")?.ToString() ?? string.Empty;
                var version = appKey.GetValue("DisplayVersion")?.ToString() ?? string.Empty;
                var uninstall = appKey.GetValue("UninstallString")?.ToString();
                var quietUninstall = appKey.GetValue("QuietUninstallString")?.ToString();
                yield return new InstalledAppInfo(
                    SuspiciousHeuristics.CreateStableKey($"win32|{name}|{publisher}|{version}"),
                    name,
                    publisher,
                    version,
                    "win32",
                    uninstall,
                    quietUninstall);
            }
        }
    }

    private async Task<IEnumerable<InstalledAppInfo>> ListAppxAppsAsync(bool verbose, CancellationToken cancellationToken)
    {
        const string script = """
        $apps = @(Get-AppxPackage -ErrorAction SilentlyContinue | Select-Object Name, PackageFullName, Publisher, Version, PackageFamilyName)
        if ($apps.Count -gt 0) { $apps | ConvertTo-Json -Depth 4 -Compress }
        """;

        var rows = await _powerShellRunner.RunJsonAsync<List<AppxDto>>(script, verbose, cancellationToken) ?? [];
        return rows.Select(row => new InstalledAppInfo(
            SuspiciousHeuristics.CreateStableKey($"appx|{row.Name}|{row.PackageFullName}"),
            row.Name ?? "Unknown",
            row.Publisher ?? string.Empty,
            row.Version ?? string.Empty,
            "appx",
            PackageFullName: row.PackageFullName,
            FamilyName: row.PackageFamilyName));
    }

    private sealed class AppxDto
    {
        public string? Name { get; set; }
        public string? PackageFullName { get; set; }
        public string? Publisher { get; set; }
        public string? Version { get; set; }
        public string? PackageFamilyName { get; set; }
    }
}
