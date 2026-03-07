using System.Reflection;
using System.Runtime.InteropServices;
using Spectre.Console;
using VoidCare.Cli.Runtime;
using VoidCare.Core.Models;
using VoidCare.Core.Services;
using VoidCare.Infrastructure.Services;

namespace VoidCare.Cli.Commands;

public sealed partial class CommandDispatcher
{
    private readonly PathService _paths;
    private readonly StateStore _stateStore;
    private readonly ActionLogger _logger;
    private readonly AdminService _adminService;
    private readonly RestorePointService _restorePointService;
    private readonly ExplorerService _explorerService;
    private readonly AntivirusDiscoveryService _antivirusDiscoveryService;
    private readonly DefenderService _defenderService;
    private readonly PersistenceService _persistenceService;
    private readonly SuspiciousFileService _suspiciousFileService;
    private readonly OptimizationService _optimizationService;
    private readonly AppsService _appsService;
    private readonly BloatCatalogService _bloatCatalogService;

    public CommandDispatcher(
        PathService paths,
        StateStore stateStore,
        ActionLogger logger,
        AdminService adminService,
        RestorePointService restorePointService,
        ExplorerService explorerService,
        AntivirusDiscoveryService antivirusDiscoveryService,
        DefenderService defenderService,
        PersistenceService persistenceService,
        SuspiciousFileService suspiciousFileService,
        OptimizationService optimizationService,
        AppsService appsService,
        BloatCatalogService bloatCatalogService)
    {
        _paths = paths;
        _stateStore = stateStore;
        _logger = logger;
        _adminService = adminService;
        _restorePointService = restorePointService;
        _explorerService = explorerService;
        _antivirusDiscoveryService = antivirusDiscoveryService;
        _defenderService = defenderService;
        _persistenceService = persistenceService;
        _suspiciousFileService = suspiciousFileService;
        _optimizationService = optimizationService;
        _appsService = appsService;
        _bloatCatalogService = bloatCatalogService;
    }

    public bool IsLongRunning(IReadOnlyList<string> args)
    {
        if (args.Count == 0)
        {
            return false;
        }

        var joined = string.Join(' ', args).ToLowerInvariant();
        return joined.Contains("scan", StringComparison.Ordinal)
            || joined.StartsWith("optimize safe", StringComparison.Ordinal)
            || joined.StartsWith("optimize aggressive", StringComparison.Ordinal)
            || joined.StartsWith("apps list", StringComparison.Ordinal);
    }

    public string GetStatusText(IReadOnlyList<string> args)
    {
        if (args.Count == 0)
        {
            return "Working...";
        }

        var joined = string.Join(' ', args);
        return $"Running {joined}";
    }

    public async Task<CommandResult> ExecuteAsync(
        ParsedInvocation invocation,
        IUserPrompt prompt,
        Action<ProgressEvent>? progress,
        CancellationToken cancellationToken = default)
    {
        var args = invocation.Arguments;
        var options = invocation.Options;

        try
        {
            if (args.Count == 0 || IsCommand(args, "help") || IsCommand(args, "--help") || IsCommand(args, "-h"))
            {
                return BuildHelp();
            }

            if (IsCommand(args, "--version") || IsCommand(args, "version"))
            {
                return BuildVersion();
            }

            if (IsCommand(args, "about"))
            {
                return BuildAbout();
            }

            if (IsCommand(args, "status"))
            {
                return await BuildStatusAsync(options, cancellationToken);
            }

            if (IsCommand(args, "security", "av", "list"))
            {
                return await BuildAvListAsync(options, cancellationToken);
            }

            if (IsCommand(args, "security", "defender", "status"))
            {
                return await BuildDefenderStatusAsync(options, cancellationToken);
            }

            if (StartsWith(args, "security", "scan"))
            {
                return await RunSecurityScanAsync(args, options, progress, cancellationToken);
            }

            if (IsCommand(args, "security", "remediate"))
            {
                return await RunSecurityRemediateAsync(options, prompt, progress, cancellationToken);
            }

            if (IsCommand(args, "security", "persistence", "list"))
            {
                return await BuildPersistenceListAsync(options, progress, cancellationToken);
            }

            if (StartsWith(args, "security", "persistence", "disable"))
            {
                return await RunPersistenceDisableAsync(args, options, prompt, cancellationToken);
            }

            if (StartsWith(args, "security", "suspicious", "scan"))
            {
                return await RunSuspiciousScanAsync(args, options, progress, cancellationToken);
            }

            if (IsCommand(args, "security", "suspicious", "quarantine", "list"))
            {
                return BuildQuarantineList();
            }

            if (StartsWith(args, "security", "suspicious", "quarantine"))
            {
                return await RunQuarantineAsync(args, options, prompt, progress, cancellationToken);
            }

            if (StartsWith(args, "security", "suspicious", "restore"))
            {
                return RunQuarantineRestore(args, options, prompt);
            }

            if (StartsWith(args, "security", "suspicious", "delete"))
            {
                return await RunQuarantineDeleteAsync(args, options, prompt, cancellationToken);
            }

            if (StartsWith(args, "optimize", "safe"))
            {
                return await RunOptimizeSafeAsync(args, options, progress, cancellationToken);
            }

            if (StartsWith(args, "optimize", "performance"))
            {
                return await RunOptimizePerformanceAsync(args, options, prompt, cancellationToken);
            }

            if (StartsWith(args, "optimize", "aggressive"))
            {
                return await RunOptimizeAggressiveAsync(args, options, prompt, cancellationToken);
            }

            if (IsCommand(args, "optimize", "startup", "list"))
            {
                return await BuildStartupListAsync(options, cancellationToken);
            }

            if (StartsWith(args, "optimize", "startup", "disable"))
            {
                var remapped = new List<string> { "security", "persistence", "disable" };
                remapped.AddRange(args.Skip(3));
                return await RunPersistenceDisableAsync(remapped, options, prompt, cancellationToken);
            }

            if (StartsWith(args, "apps", "list"))
            {
                return await BuildAppsListAsync(args, options, cancellationToken);
            }

            if (IsCommand(args, "apps", "bloat", "list"))
            {
                return await BuildBloatListAsync(options, cancellationToken);
            }

            if (StartsWith(args, "apps", "bloat", "remove"))
            {
                return await RunBloatRemoveAsync(args, options, prompt, cancellationToken);
            }

            if (StartsWith(args, "logs", "show"))
            {
                return BuildLogTail(args);
            }

            if (IsCommand(args, "logs", "open"))
            {
                return RunLogsOpen();
            }

            return Fail(string.Join(' ', args), "Unknown command. Run `voidcare --help` for usage.", 2);
        }
        catch (Exception ex)
        {
            return Fail(string.Join(' ', args), ex.Message, 1);
        }
    }

    public void RecordOutcome(CommandResult result)
    {
        if (string.IsNullOrWhiteSpace(result.Command))
        {
            return;
        }

        _logger.Log(result.Success ? "OK" : "FAIL", result.Command, result.Summary);
        _stateStore.Write(_paths.LastActionPath, new LastActionState(result.Command, result.Success, result.ExitCode, result.Summary, DateTimeOffset.UtcNow));
    }

    private CommandResult BuildHelp()
    {
        var result = new CommandResult
        {
            Command = "help",
            Summary = "VoidCare CLI help",
            ShowBanner = true,
            Data = new
            {
                credits = AppMetadata.Credits,
                offlineOnly = AppMetadata.OfflineNotice,
            },
        };

        result.Lines.Add(AppMetadata.OfflineNotice);
        result.Lines.Add(string.Empty);
        result.Tables.Add(new ResultTable(
            "Core",
            ["Command", "Description"],
            [
                ["voidcare", "Launch the interactive numeric menu"],
                ["voidcare --help", "Show grouped command help"],
                ["voidcare --version", "Show app version and build"],
                ["voidcare about", "Show project information"],
                ["voidcare status", "Show environment and session status"],
            ]));
        result.Tables.Add(new ResultTable(
            "Security",
            ["Command", "Description"],
            [
                ["voidcare security av list", "List registered antivirus products"],
                ["voidcare security defender status", "Show Defender availability and status"],
                ["voidcare security scan --quick|--full|--path", "Run a Defender scan"],
                ["voidcare security persistence list", "Enumerate startup persistence"],
                ["voidcare security suspicious scan --quick|--full --roots", "Run heuristic suspicious-file scan"],
            ]));
        result.Tables.Add(new ResultTable(
            "Optimization / Apps / Logs",
            ["Command", "Description"],
            [
                ["voidcare optimize safe --dry-run", "Preview safe cleanup"],
                ["voidcare optimize performance --confirm", "Apply safe performance preset"],
                ["voidcare apps list --type all", "List installed apps"],
                ["voidcare apps bloat list", "Show conservative removable apps"],
                ["voidcare logs show --tail 50", "Show log tail"],
            ]));
        result.Lines.Add("Examples:");
        result.Lines.Add("  voidcare security suspicious scan --quick");
        result.Lines.Add("  voidcare optimize safe --days 3 --browser-cache --dry-run");
        result.Lines.Add("  voidcare apps bloat remove --ids 1,2 --confirm --yes");
        result.Lines.Add(string.Empty);
        result.Lines.Add(AppMetadata.Credits);
        return result;
    }

    private CommandResult BuildVersion()
    {
        var assemblyVersion = Assembly.GetExecutingAssembly().GetName().Version?.ToString() ?? "1.0.0";
        var build = GetBuildConfiguration();
        return new CommandResult
        {
            Command = "version",
            Summary = "Version information displayed.",
            ShowBanner = true,
            Data = new
            {
                version = assemblyVersion,
                build,
                runtime = AppMetadata.Runtime,
                credits = AppMetadata.Credits,
            },
            Lines =
            {
                $"Version: {assemblyVersion}",
                $"Build Configuration: {build}",
                $"Target Runtime: {AppMetadata.Runtime}",
                AppMetadata.OfflineNotice,
                AppMetadata.Credits,
            },
        };
    }

    private CommandResult BuildAbout()
    {
        return new CommandResult
        {
            Command = "about",
            Summary = "About information displayed.",
            ShowBanner = true,
            Data = new
            {
                name = AppMetadata.Name,
                subtitle = AppMetadata.Subtitle,
                offlineOnly = AppMetadata.OfflineNotice,
                credits = AppMetadata.Credits,
            },
            Lines =
            {
                "Premium Windows x64 terminal optimization and security workflow.",
                "Terminal-only, offline-first, and Defender-only for remediation.",
                AppMetadata.OfflineNotice,
                AppMetadata.Credits,
            },
        };
    }

    private async Task<CommandResult> BuildStatusAsync(CommandOptions options, CancellationToken cancellationToken)
    {
        var avProducts = await _antivirusDiscoveryService.ListAsync(options.Verbose, cancellationToken);
        var defender = await _defenderService.GetStatusAsync(options.Verbose, cancellationToken);
        var lastAction = _stateStore.Read<LastActionState>(_paths.LastActionPath);

        var result = new CommandResult
        {
            Command = "status",
            Summary = "Status collected.",
            Data = new
            {
                admin = _adminService.IsElevated(),
                osVersion = RuntimeInformation.OSDescription,
                architecture = RuntimeInformation.OSArchitecture.ToString(),
                defenderAvailable = defender.MpCmdRunAvailable,
                antivirus = avProducts,
                flags = options,
                lastAction,
            },
        };

        result.Lines.Add($"Admin: {(_adminService.IsElevated() ? "yes" : "no")}");
        result.Lines.Add($"OS Version: {RuntimeInformation.OSDescription}");
        result.Lines.Add($"Architecture: {RuntimeInformation.OSArchitecture}");
        result.Lines.Add($"Defender available: {(defender.MpCmdRunAvailable ? "yes" : "no")}");
        result.Lines.Add($"Last action summary: {lastAction?.Summary ?? "No action recorded yet."}");
        result.Tables.Add(new ResultTable(
            "Session Flags",
            ["Flag", "Enabled"],
            [
                ["--json", options.Json ? "Yes" : "No"],
                ["--quiet", options.Quiet ? "Yes" : "No"],
                ["--verbose", options.Verbose ? "Yes" : "No"],
                ["--yes", options.Yes ? "Yes" : "No"],
                ["--dry-run", options.DryRun ? "Yes" : "No"],
                ["--no-banner", options.NoBanner ? "Yes" : "No"],
            ]));
        result.Tables.Add(new ResultTable(
            "Registered Antivirus",
            ["Name", "State", "Mode", "Signatures"],
            avProducts.Count == 0
                ? [["None", "-", "-", "-"]]
                : avProducts.Select(item => (IReadOnlyList<string>)[item.Name, $"0x{item.ProductState:X}", item.Active ? "Active" : "Passive", item.UpToDate ? "UpToDate" : "Stale"]).ToArray()));
        return result;
    }

    private async Task<CommandResult> BuildAvListAsync(CommandOptions options, CancellationToken cancellationToken)
    {
        var products = await _antivirusDiscoveryService.ListAsync(options.Verbose, cancellationToken);
        var result = new CommandResult
        {
            Command = "security av list",
            Summary = products.Count == 0 ? "No AV registered." : "AV products enumerated.",
            Data = new { products },
        };
        result.Tables.Add(new ResultTable(
            "Registered Antivirus",
            ["Name", "ProductState", "Mode", "Signatures", "Decoded"],
            products.Count == 0
                ? [["None", "-", "-", "-", "-"]]
                : products.Select(item => (IReadOnlyList<string>)[item.Name, $"0x{item.ProductState:X}", item.Active ? "Active" : "Passive", item.UpToDate ? "UpToDate" : "Stale", item.StatusText]).ToArray()));
        return result;
    }

    private async Task<CommandResult> BuildDefenderStatusAsync(CommandOptions options, CancellationToken cancellationToken)
    {
        var status = await _defenderService.GetStatusAsync(options.Verbose, cancellationToken);
        var result = new CommandResult
        {
            Command = "security defender status",
            Summary = status.MpCmdRunAvailable ? "Defender status collected." : "Defender is unavailable.",
            Data = status,
        };

        result.Lines.Add($"MpCmdRun available: {(status.MpCmdRunAvailable ? "yes" : "no")}");
        result.Lines.Add($"MpCmdRun path: {status.MpCmdRunPath ?? "Unavailable"}");
        result.Lines.Add($"Get-MpComputerStatus available: {(status.PowerShellStatusAvailable ? "yes" : "no")}");
        if (status.Fields.Count > 0)
        {
            result.Tables.Add(new ResultTable(
                "Defender PowerShell Status",
                ["Field", "Value"],
                status.Fields.Select(static pair => (IReadOnlyList<string>)[pair.Key, pair.Value]).ToArray()));
        }
        else
        {
            result.Notices.Add(new ResultNotice(OutputSeverity.Warn, "Get-MpComputerStatus did not return data."));
        }

        return result;
    }

    private async Task<CommandResult> BuildPersistenceListAsync(CommandOptions options, Action<ProgressEvent>? progress, CancellationToken cancellationToken)
    {
        var items = await _persistenceService.EnumerateAsync(options.Verbose, progress, cancellationToken);
        _stateStore.Write(_paths.PersistenceIndexPath, new PersistenceIndexState(DateTimeOffset.UtcNow, items));
        var result = new CommandResult
        {
            Command = "security persistence list",
            Summary = $"Persistence entries listed: {items.Count}.",
            Data = new { items },
        };
        result.Tables.Add(new ResultTable(
            "Persistence Entries",
            ["ID", "Type", "Name", "Path", "Args", "Signature", "Location"],
            items.Select(item => (IReadOnlyList<string>)[item.Id.ToString(), item.Type, item.Name, Truncate(item.Path, 52), Truncate(item.Args, 24), item.SignatureText, Truncate(item.Location, 36)]).ToArray()));
        return result;
    }

    private async Task<CommandResult> BuildStartupListAsync(CommandOptions options, CancellationToken cancellationToken)
    {
        var items = await _persistenceService.EnumerateAsync(options.Verbose, cancellationToken: cancellationToken);
        var startup = items.Where(static item => item.Type is "StartupFolder" or "RegistryRun" or "ScheduledTask" or "Service").ToArray();
        var result = new CommandResult
        {
            Command = "optimize startup list",
            Summary = $"Startup items listed: {startup.Length}.",
            Data = new { items = startup },
        };
        result.Tables.Add(new ResultTable(
            "Startup Items",
            ["ID", "Type", "Name", "Path", "Signature"],
            startup.Select(item => (IReadOnlyList<string>)[item.Id.ToString(), item.Type, item.Name, Truncate(item.Path, 58), item.SignatureText]).ToArray()));
        return result;
    }

    private async Task<CommandResult> RunSecurityScanAsync(IReadOnlyList<string> args, CommandOptions options, Action<ProgressEvent>? progress, CancellationToken cancellationToken)
    {
        ProcessRunResult runResult;
        string mode;
        string? customPath = null;
        if (HasArg(args, "--quick"))
        {
            mode = "quick";
            runResult = await _defenderService.RunQuickScanAsync(progress, options.Verbose, cancellationToken);
        }
        else if (HasArg(args, "--full"))
        {
            mode = "full";
            runResult = await _defenderService.RunFullScanAsync(progress, options.Verbose, cancellationToken);
        }
        else
        {
            customPath = GetValue(args, "--path");
            if (string.IsNullOrWhiteSpace(customPath))
            {
                return Fail("security scan", "Specify --quick, --full, or --path <PATH>.", 2);
            }

            mode = "path";
            runResult = await _defenderService.RunPathScanAsync(customPath, progress, options.Verbose, cancellationToken);
        }

        var result = new CommandResult
        {
            Success = runResult.Success,
            ExitCode = runResult.Success ? 0 : (runResult.ExitCode == 0 ? 1 : runResult.ExitCode),
            Command = "security scan",
            Summary = runResult.Success ? $"Defender {mode} scan completed." : (runResult.ErrorMessage ?? "Defender scan failed."),
            Data = new { mode, path = customPath, exitCode = runResult.ExitCode, stdout = runResult.StandardOutput, stderr = runResult.StandardError },
        };
        if (!string.IsNullOrWhiteSpace(runResult.StandardOutput))
        {
            result.Lines.Add(runResult.StandardOutput);
        }

        if (!string.IsNullOrWhiteSpace(runResult.StandardError))
        {
            result.Notices.Add(new ResultNotice(OutputSeverity.Warn, runResult.StandardError));
        }

        return result;
    }

    private async Task<CommandResult> RunSecurityRemediateAsync(CommandOptions options, IUserPrompt prompt, Action<ProgressEvent>? progress, CancellationToken cancellationToken)
    {
        var adminFailure = RequireAdmin("security remediate");
        if (adminFailure is not null)
        {
            return adminFailure;
        }

        if (!prompt.Confirm("Run Defender remediation for Defender-detected threats?", options))
        {
            return Fail("security remediate", options.Json ? "Use --yes with --json for remediation." : "Action canceled by user.");
        }

        var runResult = await _defenderService.RemediateAsync(progress, options.Verbose, cancellationToken);
        var result = new CommandResult
        {
            Success = runResult.Success,
            ExitCode = runResult.Success ? 0 : (runResult.ExitCode == 0 ? 1 : runResult.ExitCode),
            Command = "security remediate",
            Summary = runResult.Success ? "Defender remediation completed." : (runResult.ErrorMessage ?? "Defender remediation failed."),
            Data = new { stdout = runResult.StandardOutput, stderr = runResult.StandardError },
        };

        if (!string.IsNullOrWhiteSpace(runResult.StandardOutput))
        {
            result.Lines.Add(runResult.StandardOutput);
        }

        if (!string.IsNullOrWhiteSpace(runResult.StandardError))
        {
            result.Notices.Add(new ResultNotice(OutputSeverity.Warn, runResult.StandardError));
        }

        return result;
    }

    private static string GetBuildConfiguration()
    {
#if DEBUG
        return "Debug";
#else
        return "Release";
#endif
    }

    private static bool IsCommand(IReadOnlyList<string> args, params string[] tokens)
        => args.Count == tokens.Length && tokens.Select((value, index) => args[index].Equals(value, StringComparison.OrdinalIgnoreCase)).All(static value => value);

    private static bool StartsWith(IReadOnlyList<string> args, params string[] tokens)
        => args.Count >= tokens.Length && tokens.Select((value, index) => args[index].Equals(value, StringComparison.OrdinalIgnoreCase)).All(static value => value);

    private static bool HasArg(IReadOnlyList<string> args, string flag)
        => args.Any(arg => arg.Equals(flag, StringComparison.OrdinalIgnoreCase));

    private static string? GetValue(IReadOnlyList<string> args, string flag)
    {
        for (var index = 0; index < args.Count - 1; index++)
        {
            if (args[index].Equals(flag, StringComparison.OrdinalIgnoreCase))
            {
                return args[index + 1];
            }
        }

        return null;
    }

    private CommandResult? RequireAdmin(string command)
        => _adminService.IsElevated()
            ? null
            : Fail(command, "Administrator privileges are required for this command.", 2);

    private async Task<bool> EnsureRestorePointAsync(string actionLabel, CommandOptions options, IUserPrompt prompt, CancellationToken cancellationToken)
    {
        if (options.DryRun)
        {
            return true;
        }

        var restorePoint = await _restorePointService.CreateAsync(actionLabel, options.Verbose, cancellationToken);
        if (restorePoint.Success)
        {
            return true;
        }

        return prompt.Confirm($"Restore point failed: {restorePoint.Detail ?? restorePoint.Message}. Continue without restore point?", options, strong: true);
    }

    private static string Truncate(string text, int maxLength)
        => text.Length <= maxLength ? text : text[..(maxLength - 3)] + "...";

    private static CommandResult Fail(string command, string message, int exitCode = 1)
        => new()
        {
            Success = false,
            ExitCode = exitCode,
            Command = command,
            Summary = message,
            Data = new { error = message },
        };
}
