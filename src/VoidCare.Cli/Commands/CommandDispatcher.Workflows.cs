using VoidCare.Cli.Runtime;
using VoidCare.Core.Models;

namespace VoidCare.Cli.Commands;

public sealed partial class CommandDispatcher
{
    private async Task<CommandResult> RunPersistenceDisableAsync(IReadOnlyList<string> args, CommandOptions options, IUserPrompt prompt, CancellationToken cancellationToken)
    {
        var adminFailure = RequireAdmin("security persistence disable");
        if (adminFailure is not null)
        {
            return adminFailure;
        }

        var ids = GetIds(args);
        if (ids.Count == 0)
        {
            return Fail("security persistence disable", "Provide --id <ID> or --ids 1,2,3.", 2);
        }

        var cached = _stateStore.Read<PersistenceIndexState>(_paths.PersistenceIndexPath);
        if (cached is null || cached.Items.Count == 0)
        {
            return Fail("security persistence disable", "Persistence cache is missing. Run `voidcare security persistence list` first.", 2);
        }

        var current = await _persistenceService.EnumerateAsync(options.Verbose, cancellationToken: cancellationToken);
        var currentByStableKey = current.ToDictionary(static item => item.StableKey, StringComparer.OrdinalIgnoreCase);
        var selectedCached = cached.Items.Where(item => ids.Contains(item.Id)).ToArray();
        if (selectedCached.Length != ids.Count)
        {
            return Fail("security persistence disable", "One or more requested IDs are not in the cached persistence list.", 2);
        }

        var selectedCurrent = new List<PersistenceItem>();
        foreach (var item in selectedCached)
        {
            if (!currentByStableKey.TryGetValue(item.StableKey, out var currentItem))
            {
                return Fail("security persistence disable", $"Cached persistence item {item.Id} is stale. Refresh the list first.", 2);
            }

            selectedCurrent.Add(currentItem);
        }

        if (!options.DryRun && !prompt.Confirm($"Disable {selectedCurrent.Count} persistence item(s)?", options))
        {
            return Fail("security persistence disable", options.Json ? "Use --yes with --json for persistence disable." : "Action canceled by user.");
        }

        if (!await EnsureRestorePointAsync("Persistence disable", options, prompt, cancellationToken))
        {
            return Fail("security persistence disable", "Action canceled after restore point failure.");
        }

        var result = new CommandResult
        {
            Command = "security persistence disable",
            Summary = options.DryRun ? "Dry-run: persistence changes planned." : $"Processed {selectedCurrent.Count} persistence item(s).",
            Data = new { ids, items = selectedCurrent },
        };

        var failures = new List<string>();
        foreach (var item in selectedCurrent)
        {
            var outcome = await _persistenceService.DisableAsync(item, options.DryRun, options.Verbose, cancellationToken);
            result.Lines.Add(outcome.Message);
            if (!outcome.Success)
            {
                failures.Add(outcome.Message);
            }
        }

        if (failures.Count > 0)
        {
            result.Success = false;
            result.ExitCode = 1;
            result.Summary = $"Persistence disable completed with {failures.Count} failure(s).";
        }

        return result;
    }

    private async Task<CommandResult> RunSuspiciousScanAsync(IReadOnlyList<string> args, CommandOptions options, Action<ProgressEvent>? progress, CancellationToken cancellationToken)
    {
        var persistence = await _persistenceService.EnumerateAsync(options.Verbose, cancellationToken: cancellationToken);
        var quick = HasArg(args, "--quick") || !HasArg(args, "--full");
        IReadOnlyList<SuspiciousFileRecord> records;
        IReadOnlyList<string> roots;

        if (quick)
        {
            roots = ["<quick-scan>"];
            records = await _suspiciousFileService.ScanQuickAsync(persistence, options.Verbose, progress, cancellationToken);
        }
        else
        {
            var rawRoots = GetValue(args, "--roots");
            if (string.IsNullOrWhiteSpace(rawRoots))
            {
                return Fail("security suspicious scan", "Full suspicious scan requires --roots \"C:\\;D:\\\".", 2);
            }

            roots = rawRoots.Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            records = await _suspiciousFileService.ScanFullAsync(roots, persistence, options.Verbose, progress, cancellationToken);
        }

        _stateStore.Write(_paths.SuspiciousScanPath, new SuspiciousScanState(DateTimeOffset.UtcNow, roots, quick, records));

        var result = new CommandResult
        {
            Command = "security suspicious scan",
            Summary = records.Count == 0 ? "No heuristic suspicious files flagged." : $"Heuristic suspicious items flagged: {records.Count}.",
            Data = new { quick, roots, items = records },
        };
        result.Notices.Add(new ResultNotice(OutputSeverity.Info, "Results are heuristic-only and are not malware claims unless Defender detects them."));
        result.Tables.Add(new ResultTable(
            "Suspicious Files",
            ["ID", "Score", "Sig", "Size", "Modified", "SHA256", "Path", "Reasons"],
            records.Count == 0
                ? [["-", "-", "-", "-", "-", "-", "-", "None"]]
                : records.Select(item => (IReadOnlyList<string>)
                    [
                        item.Id.ToString(),
                        item.Score.ToString(),
                        item.SignatureText,
                        FormatBytes(item.Size),
                        item.Modified.ToLocalTime().ToString("yyyy-MM-dd HH:mm"),
                        Truncate(item.Sha256, 16),
                        Truncate(item.Path, 48),
                        Truncate(string.Join("; ", item.Reasons), 40),
                    ]).ToArray()));
        return result;
    }

    private CommandResult BuildQuarantineList()
    {
        var items = _suspiciousFileService.ListQuarantine();
        var result = new CommandResult
        {
            Command = "security suspicious quarantine list",
            Summary = items.Count == 0 ? "No quarantined items found." : $"Quarantined items listed: {items.Count}.",
            Data = new { items },
        };
        result.Tables.Add(new ResultTable(
            "Quarantine",
            ["ID", "Timestamp", "Sig", "Original", "Quarantine", "SHA256"],
            items.Count == 0
                ? [["-", "-", "-", "-", "-", "-"]]
                : items.Select(item => (IReadOnlyList<string>)
                    [
                        item.Id.ToString(),
                        item.Timestamp.ToLocalTime().ToString("yyyy-MM-dd HH:mm"),
                        item.SignatureStatus,
                        Truncate(item.OriginalPath, 38),
                        Truncate(item.QuarantinePath, 38),
                        Truncate(item.Sha256, 16),
                    ]).ToArray()));
        return result;
    }

    private async Task<CommandResult> RunQuarantineAsync(IReadOnlyList<string> args, CommandOptions options, IUserPrompt prompt, Action<ProgressEvent>? progress, CancellationToken cancellationToken)
    {
        var ids = GetIds(args);
        if (ids.Count == 0)
        {
            return Fail("security suspicious quarantine", "Provide --ids 1,2,3.", 2);
        }

        var scan = _stateStore.Read<SuspiciousScanState>(_paths.SuspiciousScanPath);
        if (scan is null || scan.Items.Count == 0)
        {
            return Fail("security suspicious quarantine", "Suspicious scan cache is missing. Run a suspicious scan first.", 2);
        }

        var selected = scan.Items.Where(item => ids.Contains(item.Id)).ToArray();
        if (selected.Length != ids.Count)
        {
            return Fail("security suspicious quarantine", "One or more requested IDs are not in the cached suspicious scan.", 2);
        }

        if (selected.Any(item => !File.Exists(item.Path)))
        {
            return Fail("security suspicious quarantine", "One or more cached suspicious items no longer exist. Refresh the suspicious scan first.", 2);
        }

        if (!options.DryRun && !prompt.Confirm($"Quarantine {selected.Length} suspicious file(s)?", options))
        {
            return Fail("security suspicious quarantine", options.Json ? "Use --yes with --json for quarantine." : "Action canceled by user.");
        }

        var outcome = await _suspiciousFileService.QuarantineAsync(selected, options.DryRun, options.Verbose, progress, cancellationToken);
        var result = new CommandResult
        {
            Success = outcome.Success,
            ExitCode = outcome.Success ? 0 : 1,
            Command = "security suspicious quarantine",
            Summary = outcome.Message,
            Data = new { ids, folder = outcome.Folder },
        };
        if (outcome.Folder is not null)
        {
            result.Lines.Add($"Quarantine folder: {outcome.Folder}");
        }

        return result;
    }

    private CommandResult RunQuarantineRestore(IReadOnlyList<string> args, CommandOptions options, IUserPrompt prompt)
    {
        var id = GetSingleId(args);
        if (id <= 0)
        {
            return Fail("security suspicious restore", "Provide --id <ID>.", 2);
        }

        var target = _suspiciousFileService.ListQuarantine().FirstOrDefault(item => item.Id == id);
        if (target is null)
        {
            return Fail("security suspicious restore", "Quarantine ID not found.", 2);
        }

        var to = GetValue(args, "--to");
        if (!options.DryRun && !prompt.Confirm("Restore selected quarantined item?", options))
        {
            return Fail("security suspicious restore", options.Json ? "Use --yes with --json for restore." : "Action canceled by user.");
        }

        var outcome = _suspiciousFileService.Restore(target, to, options.DryRun);
        return new CommandResult
        {
            Success = outcome.Success,
            ExitCode = outcome.Success ? 0 : 1,
            Command = "security suspicious restore",
            Summary = outcome.Message,
            Data = new { id, to },
        };
    }

    private async Task<CommandResult> RunQuarantineDeleteAsync(IReadOnlyList<string> args, CommandOptions options, IUserPrompt prompt, CancellationToken cancellationToken)
    {
        var adminFailure = RequireAdmin("security suspicious delete");
        if (adminFailure is not null)
        {
            return adminFailure;
        }

        var id = GetSingleId(args);
        if (id <= 0)
        {
            return Fail("security suspicious delete", "Provide --id <ID>.", 2);
        }

        var target = _suspiciousFileService.ListQuarantine().FirstOrDefault(item => item.Id == id);
        if (target is null)
        {
            return Fail("security suspicious delete", "Quarantine ID not found.", 2);
        }

        if (!options.DryRun && !prompt.Confirm("Permanently delete selected quarantined item?", options, strong: true))
        {
            return Fail("security suspicious delete", options.Json ? "Use --yes with --json for delete." : "Action canceled by user.");
        }

        if (!await EnsureRestorePointAsync("Quarantine delete", options, prompt, cancellationToken))
        {
            return Fail("security suspicious delete", "Action canceled after restore point failure.");
        }

        var outcome = _suspiciousFileService.Delete(target, options.DryRun);
        return new CommandResult
        {
            Success = outcome.Success,
            ExitCode = outcome.Success ? 0 : 1,
            Command = "security suspicious delete",
            Summary = outcome.Message,
            Data = new { id },
        };
    }

    private async Task<CommandResult> RunOptimizeSafeAsync(IReadOnlyList<string> args, CommandOptions options, Action<ProgressEvent>? progress, CancellationToken cancellationToken)
    {
        var days = int.TryParse(GetValue(args, "--days"), out var parsedDays) ? parsedDays : 2;
        var cleanup = await _optimizationService.RunSafeCleanupAsync(
            new SafeCleanupOptions(days, HasArg(args, "--include-windows-temp"), HasArg(args, "--browser-cache"), options.DryRun),
            progress,
            cancellationToken);

        var result = new CommandResult
        {
            Success = cleanup.Success,
            ExitCode = cleanup.Success ? 0 : 1,
            Command = "optimize safe",
            Summary = options.DryRun ? "Dry-run: safe cleanup analysis completed." : "Safe cleanup completed.",
            Data = cleanup,
        };
        result.Lines.Add($"Files scanned: {cleanup.FilesScanned}");
        result.Lines.Add($"Files {(options.DryRun ? "to delete" : "deleted")}: {cleanup.FilesDeleted}");
        result.Lines.Add($"Bytes {(options.DryRun ? "to free" : "freed")}: {FormatBytes(cleanup.BytesFreed)}");
        result.Lines.Add($"Elapsed: {cleanup.Elapsed:g}");
        if (cleanup.Warnings.Count > 0)
        {
            foreach (var warning in cleanup.Warnings.Take(10))
            {
                result.Notices.Add(new ResultNotice(OutputSeverity.Warn, warning));
            }
        }
        return result;
    }

    private async Task<CommandResult> RunOptimizePerformanceAsync(IReadOnlyList<string> args, CommandOptions options, IUserPrompt prompt, CancellationToken cancellationToken)
    {
        var adminFailure = RequireAdmin("optimize performance");
        if (adminFailure is not null)
        {
            return adminFailure;
        }

        if (!HasArg(args, "--confirm"))
        {
            return Fail("optimize performance", "Performance preset requires --confirm.", 2);
        }

        if (!options.DryRun && !prompt.Confirm("Apply the safe performance preset?", options))
        {
            return Fail("optimize performance", options.Json ? "Use --yes with --json for performance preset." : "Action canceled by user.");
        }

        var outcome = await _optimizationService.ApplyPerformancePresetAsync(options.DryRun, options.Verbose, cancellationToken);
        var result = new CommandResult
        {
            Success = outcome.Success,
            ExitCode = outcome.Success ? 0 : 1,
            Command = "optimize performance",
            Summary = outcome.Message,
            Data = new { changes = outcome.Changes },
        };
        result.Tables.Add(new ResultTable(
            "Changes",
            ["Setting", "Value"],
            outcome.Changes.Select(change => (IReadOnlyList<string>)[change.Name, change.Value]).ToArray()));
        result.Lines.Add("Revert guidance: switch the power plan back and revert Game Bar values if needed.");
        return result;
    }

    private async Task<CommandResult> RunOptimizeAggressiveAsync(IReadOnlyList<string> args, CommandOptions options, IUserPrompt prompt, CancellationToken cancellationToken)
    {
        var adminFailure = RequireAdmin("optimize aggressive");
        if (adminFailure is not null)
        {
            return adminFailure;
        }

        if (!HasArg(args, "--confirm"))
        {
            return Fail("optimize aggressive", "Aggressive preset requires --confirm.", 2);
        }

        var removeBloatIds = HasArg(args, "--remove-bloat-ids") ? GetIdsFromValue(GetValue(args, "--remove-bloat-ids")) : [];
        var disableCopilot = HasArg(args, "--disable-copilot");
        var includeWindowsTemp = HasArg(args, "--include-windows-temp") || !HasArg(args, "--browser-cache");
        var includeBrowserCache = HasArg(args, "--browser-cache");

        if (!options.DryRun && !prompt.Confirm("Run aggressive optimization plan?", options, strong: true))
        {
            return Fail("optimize aggressive", options.Json ? "Use --yes with --json for aggressive mode." : "Action canceled by user.");
        }

        if (!await EnsureRestorePointAsync("Aggressive optimize", options, prompt, cancellationToken))
        {
            return Fail("optimize aggressive", "Action canceled after restore point failure.");
        }

        var result = new CommandResult
        {
            Command = "optimize aggressive",
            Summary = options.DryRun ? "Dry-run: aggressive plan prepared." : "Aggressive optimization completed.",
            Data = new
            {
                disableCopilot,
                includeWindowsTemp,
                includeBrowserCache,
                removeBloatIds,
            },
        };

        var cleanup = await _optimizationService.RunSafeCleanupAsync(new SafeCleanupOptions(2, includeWindowsTemp, includeBrowserCache, options.DryRun), cancellationToken: cancellationToken);
        result.Lines.Add($"Cleanup {(options.DryRun ? "would free" : "freed")}: {FormatBytes(cleanup.BytesFreed)}");
        if (disableCopilot)
        {
            var copilot = await _optimizationService.DisableCopilotAsync(options.DryRun);
            result.Lines.Add(copilot.Message);
        }

        if (removeBloatIds.Count > 0)
        {
            var candidates = await _appsService.FindBloatCandidatesAsync(_bloatCatalogService, options.Verbose, cancellationToken);
            foreach (var candidate in candidates.Where(item => removeBloatIds.Contains(item.Id)))
            {
                var removal = await _appsService.RemoveAsync(candidate, options.DryRun, options.Verbose, cancellationToken);
                result.Lines.Add(removal.Message);
                if (!removal.Success)
                {
                    result.Success = false;
                    result.ExitCode = 1;
                }
            }
        }

        return result;
    }

    private async Task<CommandResult> BuildAppsListAsync(IReadOnlyList<string> args, CommandOptions options, CancellationToken cancellationToken)
    {
        var type = GetValue(args, "--type") ?? "all";
        var apps = await _appsService.ListAsync(type, options.Verbose, cancellationToken);
        var result = new CommandResult
        {
            Command = "apps list",
            Summary = $"Apps listed: {apps.Count}.",
            Data = new { type, apps },
        };
        result.Tables.Add(new ResultTable(
            "Installed Apps",
            ["Name", "Publisher", "Version", "Type"],
            apps.Select(app => (IReadOnlyList<string>)[Truncate(app.Name, 36), Truncate(app.Publisher, 26), app.Version, app.Type]).ToArray()));
        return result;
    }

    private async Task<CommandResult> BuildBloatListAsync(CommandOptions options, CancellationToken cancellationToken)
    {
        var candidates = await _appsService.FindBloatCandidatesAsync(_bloatCatalogService, options.Verbose, cancellationToken);
        var result = new CommandResult
        {
            Command = "apps bloat list",
            Summary = candidates.Count == 0 ? "No conservative removable apps matched." : $"Conservative removable apps listed: {candidates.Count}.",
            Data = new { items = candidates },
        };
        result.Tables.Add(new ResultTable(
            "Bloat Candidates",
            ["ID", "Name", "Type", "Installed", "Reason"],
            candidates.Count == 0
                ? [["-", "-", "-", "-", "-"]]
                : candidates.Select(candidate => (IReadOnlyList<string>)[candidate.Id.ToString(), candidate.Catalog.DisplayName, candidate.Catalog.Type, candidate.InstalledApp.Name, candidate.Catalog.Reason]).ToArray()));
        return result;
    }

    private async Task<CommandResult> RunBloatRemoveAsync(IReadOnlyList<string> args, CommandOptions options, IUserPrompt prompt, CancellationToken cancellationToken)
    {
        var adminFailure = RequireAdmin("apps bloat remove");
        if (adminFailure is not null)
        {
            return adminFailure;
        }

        if (!HasArg(args, "--confirm"))
        {
            return Fail("apps bloat remove", "Bloat removal requires --confirm.", 2);
        }

        var ids = GetIds(args);
        if (ids.Count == 0)
        {
            return Fail("apps bloat remove", "Provide --ids 1,2,3.", 2);
        }

        var candidates = await _appsService.FindBloatCandidatesAsync(_bloatCatalogService, options.Verbose, cancellationToken);
        var selected = candidates.Where(item => ids.Contains(item.Id)).ToArray();
        if (selected.Length != ids.Count)
        {
            return Fail("apps bloat remove", "One or more requested IDs are not in the current bloat list.", 2);
        }

        if (!options.DryRun && !prompt.Confirm($"Remove {selected.Length} curated bloat app(s)?", options, strong: true))
        {
            return Fail("apps bloat remove", options.Json ? "Use --yes with --json for bloat removal." : "Action canceled by user.");
        }

        if (!await EnsureRestorePointAsync("Bloat remove", options, prompt, cancellationToken))
        {
            return Fail("apps bloat remove", "Action canceled after restore point failure.");
        }

        var result = new CommandResult
        {
            Command = "apps bloat remove",
            Summary = options.DryRun ? "Dry-run: bloat removal planned." : $"Processed {selected.Length} bloat removal request(s).",
            Data = new { ids },
        };
        foreach (var candidate in selected)
        {
            var removal = await _appsService.RemoveAsync(candidate, options.DryRun, options.Verbose, cancellationToken);
            result.Lines.Add(removal.Message);
            if (!removal.Success)
            {
                result.Success = false;
                result.ExitCode = 1;
            }
        }

        return result;
    }

    private CommandResult BuildLogTail(IReadOnlyList<string> args)
    {
        var tail = int.TryParse(GetValue(args, "--tail"), out var parsedTail) ? parsedTail : 50;
        var lines = _logger.ReadTail(tail);
        var result = new CommandResult
        {
            Command = "logs show",
            Summary = lines.Count == 0 ? "Log file is empty." : $"Showing last {lines.Count} log line(s).",
            Data = new { tail, lines },
        };
        foreach (var line in lines)
        {
            result.Lines.Add(line);
        }
        return result;
    }

    private CommandResult RunLogsOpen()
    {
        var opened = _explorerService.OpenFolder(_paths.LogsRoot);
        return new CommandResult
        {
            Success = opened,
            ExitCode = opened ? 0 : 1,
            Command = "logs open",
            Summary = opened ? "Opened logs folder." : "Failed to open logs folder.",
            Data = new { folder = _paths.LogsRoot },
        };
    }

    private static IReadOnlyList<int> GetIds(IReadOnlyList<string> args)
    {
        var single = GetValue(args, "--id");
        if (int.TryParse(single, out var singleId) && singleId > 0)
        {
            return [singleId];
        }

        return GetIdsFromValue(GetValue(args, "--ids"));
    }

    private static int GetSingleId(IReadOnlyList<string> args)
        => int.TryParse(GetValue(args, "--id"), out var id) ? id : 0;

    private static IReadOnlyList<int> GetIdsFromValue(string? csv)
        => string.IsNullOrWhiteSpace(csv) ? [] : InvocationParser.ParseIds(csv).ToArray();

    private static string FormatBytes(long bytes)
    {
        if (bytes < 1024)
        {
            return $"{bytes} B";
        }

        var kb = bytes / 1024d;
        if (kb < 1024)
        {
            return $"{kb:F1} KB";
        }

        var mb = kb / 1024d;
        if (mb < 1024)
        {
            return $"{mb:F1} MB";
        }

        return $"{mb / 1024d:F2} GB";
    }
}
