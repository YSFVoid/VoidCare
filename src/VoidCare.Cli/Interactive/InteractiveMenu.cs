using Spectre.Console;
using VoidCare.Cli.Commands;
using VoidCare.Cli.Rendering;
using VoidCare.Cli.Runtime;
using VoidCare.Core.Models;

namespace VoidCare.Cli.Interactive;

public sealed class InteractiveMenu
{
    private readonly CommandDispatcher _dispatcher;
    private readonly TerminalRenderer _renderer;
    private readonly CommandOptions _options;

    public InteractiveMenu(CommandDispatcher dispatcher, TerminalRenderer renderer, CommandOptions options)
    {
        _dispatcher = dispatcher;
        _renderer = renderer;
        _options = options with { NoBanner = true };
    }

    public async Task<int> RunAsync(CancellationToken cancellationToken = default)
    {
        while (true)
        {
            RenderMenu("Main Menu", ["1) Status", "2) Security", "3) Optimization", "4) Apps", "5) Logs", "6) About", "0) Exit"]);
            switch (ReadMenuInput())
            {
                case "1":
                    await ExecuteAsync(["status"], cancellationToken);
                    break;
                case "2":
                    if (await RunSecurityMenuAsync(cancellationToken))
                    {
                        return 0;
                    }
                    break;
                case "3":
                    if (await RunOptimizationMenuAsync(cancellationToken))
                    {
                        return 0;
                    }
                    break;
                case "4":
                    if (await RunAppsMenuAsync(cancellationToken))
                    {
                        return 0;
                    }
                    break;
                case "5":
                    if (await RunLogsMenuAsync(cancellationToken))
                    {
                        return 0;
                    }
                    break;
                case "6":
                    await ExecuteAsync(["about"], cancellationToken);
                    break;
                case "0":
                case "q":
                case "quit":
                    return 0;
                default:
                    WarnInvalid();
                    break;
            }
        }
    }

    private async Task<bool> RunSecurityMenuAsync(CancellationToken cancellationToken)
    {
        while (true)
        {
            RenderMenu("Security", [
                "1) AV list",
                "2) Defender status",
                "3) Quick scan",
                "4) Full scan",
                "5) Custom path scan",
                "6) Remediate threats",
                "7) Persistence list",
                "8) Disable persistence item",
                "9) Quick suspicious scan",
                "10) Full suspicious scan",
                "11) Quarantine suspicious files",
                "12) Restore quarantined file",
                "13) Delete quarantined file",
                "0) Back"]);

            switch (ReadMenuInput())
            {
                case "1":
                    await ExecuteAsync(["security", "av", "list"], cancellationToken);
                    break;
                case "2":
                    await ExecuteAsync(["security", "defender", "status"], cancellationToken);
                    break;
                case "3":
                    await ExecuteAsync(["security", "scan", "--quick"], cancellationToken);
                    break;
                case "4":
                    await ExecuteAsync(["security", "scan", "--full"], cancellationToken);
                    break;
                case "5":
                    await ExecuteAsync(["security", "scan", "--path", PromptText("Custom path")], cancellationToken);
                    break;
                case "6":
                    await ExecuteAsync(["security", "remediate"], cancellationToken);
                    break;
                case "7":
                    await ExecuteAsync(["security", "persistence", "list"], cancellationToken);
                    break;
                case "8":
                    await ExecuteAsync(["security", "persistence", "disable", "--ids", PromptText("Persistence ID(s)")], cancellationToken);
                    break;
                case "9":
                    await ExecuteAsync(["security", "suspicious", "scan", "--quick"], cancellationToken);
                    break;
                case "10":
                    await ExecuteAsync(["security", "suspicious", "scan", "--full", "--roots", PromptText("Scan roots (C:\\;D:\\)")], cancellationToken);
                    break;
                case "11":
                    await ExecuteAsync(["security", "suspicious", "quarantine", "--ids", PromptText("Suspicious ID(s)")], cancellationToken);
                    break;
                case "12":
                    await ExecuteAsync(BuildRestoreArgs(), cancellationToken);
                    break;
                case "13":
                    await ExecuteAsync(["security", "suspicious", "delete", "--id", PromptText("Quarantine ID")], cancellationToken);
                    break;
                case "0":
                case "b":
                case "back":
                    return false;
                case "q":
                case "quit":
                    return true;
                default:
                    WarnInvalid();
                    break;
            }
        }
    }

    private async Task<bool> RunOptimizationMenuAsync(CancellationToken cancellationToken)
    {
        while (true)
        {
            RenderMenu("Optimization", [
                "1) Safe optimize",
                "2) Safe optimize with custom days",
                "3) Performance preset",
                "4) Aggressive preset",
                "5) Startup list",
                "6) Disable startup item",
                "0) Back"]);

            switch (ReadMenuInput())
            {
                case "1":
                    await ExecuteAsync(["optimize", "safe"], cancellationToken);
                    break;
                case "2":
                    await ExecuteAsync(["optimize", "safe", "--days", PromptText("Days")], cancellationToken);
                    break;
                case "3":
                    await ExecuteAsync(["optimize", "performance", "--confirm"], cancellationToken);
                    break;
                case "4":
                    await ExecuteAsync(["optimize", "aggressive", "--confirm"], cancellationToken);
                    break;
                case "5":
                    await ExecuteAsync(["optimize", "startup", "list"], cancellationToken);
                    break;
                case "6":
                    await ExecuteAsync(["optimize", "startup", "disable", "--ids", PromptText("Startup ID(s)")], cancellationToken);
                    break;
                case "0":
                case "b":
                case "back":
                    return false;
                case "q":
                case "quit":
                    return true;
                default:
                    WarnInvalid();
                    break;
            }
        }
    }

    private async Task<bool> RunAppsMenuAsync(CancellationToken cancellationToken)
    {
        while (true)
        {
            RenderMenu("Apps", [
                "1) List all apps",
                "2) List Win32 apps",
                "3) List Appx apps",
                "4) Bloat list",
                "5) Remove bloat by IDs",
                "0) Back"]);

            switch (ReadMenuInput())
            {
                case "1":
                    await ExecuteAsync(["apps", "list", "--type", "all"], cancellationToken);
                    break;
                case "2":
                    await ExecuteAsync(["apps", "list", "--type", "win32"], cancellationToken);
                    break;
                case "3":
                    await ExecuteAsync(["apps", "list", "--type", "appx"], cancellationToken);
                    break;
                case "4":
                    await ExecuteAsync(["apps", "bloat", "list"], cancellationToken);
                    break;
                case "5":
                    await ExecuteAsync(["apps", "bloat", "remove", "--ids", PromptText("Bloat ID(s)"), "--confirm"], cancellationToken);
                    break;
                case "0":
                case "b":
                case "back":
                    return false;
                case "q":
                case "quit":
                    return true;
                default:
                    WarnInvalid();
                    break;
            }
        }
    }

    private async Task<bool> RunLogsMenuAsync(CancellationToken cancellationToken)
    {
        while (true)
        {
            RenderMenu("Logs", ["1) Show tail 50", "2) Show tail custom", "3) Open logs folder", "0) Back"]);
            switch (ReadMenuInput())
            {
                case "1":
                    await ExecuteAsync(["logs", "show", "--tail", "50"], cancellationToken);
                    break;
                case "2":
                    await ExecuteAsync(["logs", "show", "--tail", PromptText("Tail count")], cancellationToken);
                    break;
                case "3":
                    await ExecuteAsync(["logs", "open"], cancellationToken);
                    break;
                case "0":
                case "b":
                case "back":
                    return false;
                case "q":
                case "quit":
                    return true;
                default:
                    WarnInvalid();
                    break;
            }
        }
    }

    private async Task ExecuteAsync(IReadOnlyList<string> args, CancellationToken cancellationToken)
    {
        var invocation = new ParsedInvocation(_options, args, false);
        Action<ProgressEvent> progress = evt => _renderer.RenderProgress(evt, _options.Verbose);
        var useStatus = _dispatcher.IsLongRunning(args) && !RequiresInteractivePrompt(args, _options);
        var result = useStatus
            ? await _renderer.RunWithStatusAsync(_dispatcher.GetStatusText(args), () => _dispatcher.ExecuteAsync(invocation, _renderer, progress, cancellationToken))
            : await _dispatcher.ExecuteAsync(invocation, _renderer, progress, cancellationToken);
        _renderer.Render(result, _options);
        _dispatcher.RecordOutcome(result);
        AnsiConsole.WriteLine();
    }

    private static void RenderMenu(string title, IReadOnlyList<string> lines)
    {
        AnsiConsole.Write(new Rule($"[cyan]{title}[/]").LeftJustified());
        foreach (var line in lines)
        {
            AnsiConsole.MarkupLine(line);
        }
    }

    private static string ReadMenuInput()
        => (Console.ReadLine() ?? string.Empty).Trim().ToLowerInvariant();

    private static void WarnInvalid()
        => AnsiConsole.MarkupLine("[black on yellow][[WARN]][/] Invalid menu input.");

    private static string PromptText(string label)
    {
        AnsiConsole.Markup($"{label}: ");
        return Console.ReadLine()?.Trim() ?? string.Empty;
    }

    private static IReadOnlyList<string> BuildRestoreArgs()
    {
        var id = PromptText("Quarantine ID");
        var destination = PromptText("Restore target (leave empty for original path)");
        return string.IsNullOrWhiteSpace(destination)
            ? ["security", "suspicious", "restore", "--id", id]
            : ["security", "suspicious", "restore", "--id", id, "--to", destination];
    }

    private static bool RequiresInteractivePrompt(IReadOnlyList<string> args, CommandOptions options)
    {
        if (options.Yes || options.Json)
        {
            return false;
        }

        if (StartsWith(args, "security", "remediate"))
        {
            return true;
        }

        if (StartsWith(args, "security", "persistence", "disable"))
        {
            return !options.DryRun;
        }

        if (StartsWith(args, "security", "suspicious", "quarantine"))
        {
            return !options.DryRun;
        }

        if (StartsWith(args, "security", "suspicious", "restore"))
        {
            return !options.DryRun;
        }

        if (StartsWith(args, "security", "suspicious", "delete"))
        {
            return !options.DryRun;
        }

        if (StartsWith(args, "optimize", "performance"))
        {
            return !options.DryRun;
        }

        if (StartsWith(args, "optimize", "aggressive"))
        {
            return !options.DryRun;
        }

        if (StartsWith(args, "apps", "bloat", "remove"))
        {
            return !options.DryRun;
        }

        return false;
    }

    private static bool StartsWith(IReadOnlyList<string> args, params string[] tokens)
        => args.Count >= tokens.Length && tokens.Select((value, index) => args[index].Equals(value, StringComparison.OrdinalIgnoreCase)).All(static value => value);
}
