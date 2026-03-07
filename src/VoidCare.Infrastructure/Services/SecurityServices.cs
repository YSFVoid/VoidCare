using Microsoft.Win32;
using VoidCare.Core.Models;
using VoidCare.Core.Services;

namespace VoidCare.Infrastructure.Services;

public sealed class AntivirusDiscoveryService
{
    private readonly PowerShellRunner _powerShellRunner;

    public AntivirusDiscoveryService(PowerShellRunner powerShellRunner)
    {
        _powerShellRunner = powerShellRunner;
    }

    public async Task<IReadOnlyList<AntivirusProductInfo>> ListAsync(bool verbose = false, CancellationToken cancellationToken = default)
    {
        const string script = """
        $items = @(Get-CimInstance -Namespace root/SecurityCenter2 -ClassName AntiVirusProduct -ErrorAction SilentlyContinue |
            Select-Object @{Name='name';Expression={$_.displayName}},
                          @{Name='productState';Expression={[int]$_.productState}},
                          @{Name='productPath';Expression={$_.pathToSignedProductExe}})
        if ($items.Count -gt 0) { $items | ConvertTo-Json -Depth 4 -Compress }
        """;

        var items = await _powerShellRunner.RunJsonAsync<List<AntivirusDto>>(script, verbose, cancellationToken) ?? [];
        return items
            .Select(static dto =>
            {
                var active = (dto.ProductState & 0xF000) == 0x1000;
                var upToDate = (dto.ProductState & 0x00F0) == 0;
                var statusText = $"{(active ? "Active" : "Passive")} / {(upToDate ? "UpToDate" : "Stale")}";
                return new AntivirusProductInfo(dto.Name ?? "Unknown", dto.ProductState, active, upToDate, statusText, dto.ProductPath);
            })
            .OrderBy(static item => item.Name, StringComparer.OrdinalIgnoreCase)
            .ToArray();
    }

    private sealed class AntivirusDto
    {
        public string? Name { get; set; }
        public int ProductState { get; set; }
        public string? ProductPath { get; set; }
    }
}

public sealed class DefenderService
{
    private readonly ProcessRunner _processRunner;
    private readonly PowerShellRunner _powerShellRunner;

    public DefenderService(ProcessRunner processRunner, PowerShellRunner powerShellRunner)
    {
        _processRunner = processRunner;
        _powerShellRunner = powerShellRunner;
    }

    public string? FindMpCmdRunPath()
    {
        var direct = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Windows Defender", "MpCmdRun.exe");
        if (File.Exists(direct))
        {
            return direct;
        }

        var platformRoot = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "Microsoft", "Windows Defender", "Platform");
        if (!Directory.Exists(platformRoot))
        {
            return null;
        }

        return Directory.EnumerateDirectories(platformRoot)
            .OrderByDescending(static path => path, StringComparer.OrdinalIgnoreCase)
            .Select(path => Path.Combine(path, "MpCmdRun.exe"))
            .FirstOrDefault(File.Exists);
    }

    public async Task<DefenderStatusInfo> GetStatusAsync(bool verbose = false, CancellationToken cancellationToken = default)
    {
        const string script = """
        if (Get-Command Get-MpComputerStatus -ErrorAction SilentlyContinue) {
          $status = Get-MpComputerStatus
          [pscustomobject]@{
            AMServiceEnabled = [string]$status.AMServiceEnabled
            AntivirusEnabled = [string]$status.AntivirusEnabled
            AntispywareEnabled = [string]$status.AntispywareEnabled
            BehaviorMonitorEnabled = [string]$status.BehaviorMonitorEnabled
            RealTimeProtectionEnabled = [string]$status.RealTimeProtectionEnabled
            NISEnabled = [string]$status.NISEnabled
            IoavProtectionEnabled = [string]$status.IoavProtectionEnabled
          } | ConvertTo-Json -Depth 4 -Compress
        }
        """;

        var mpCmdRunPath = FindMpCmdRunPath();
        var raw = await _powerShellRunner.RunScriptAsync(script, verbose: verbose, cancellationToken: cancellationToken);
        var fields = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        if (!string.IsNullOrWhiteSpace(raw.StandardOutput))
        {
            try
            {
                using var document = System.Text.Json.JsonDocument.Parse(raw.StandardOutput);
                foreach (var property in document.RootElement.EnumerateObject())
                {
                    fields[property.Name] = property.Value.ToString();
                }
            }
            catch
            {
                fields["Raw"] = raw.StandardOutput.Trim();
            }
        }

        return new DefenderStatusInfo(
            mpCmdRunPath is not null,
            mpCmdRunPath,
            fields.Count > 0,
            fields,
            string.IsNullOrWhiteSpace(raw.StandardOutput) ? null : raw.StandardOutput.Trim());
    }

    public Task<ProcessRunResult> RunQuickScanAsync(Action<ProgressEvent>? progress, bool verbose = false, CancellationToken cancellationToken = default)
        => RunMpCommandAsync(["-Scan", "-ScanType", "1"], progress, verbose, cancellationToken);

    public Task<ProcessRunResult> RunFullScanAsync(Action<ProgressEvent>? progress, bool verbose = false, CancellationToken cancellationToken = default)
        => RunMpCommandAsync(["-Scan", "-ScanType", "2"], progress, verbose, cancellationToken);

    public Task<ProcessRunResult> RunPathScanAsync(string path, Action<ProgressEvent>? progress, bool verbose = false, CancellationToken cancellationToken = default)
        => RunMpCommandAsync(["-Scan", "-ScanType", "3", "-File", path], progress, verbose, cancellationToken);

    public Task<ProcessRunResult> RemediateAsync(Action<ProgressEvent>? progress, bool verbose = false, CancellationToken cancellationToken = default)
        => RemediateCoreAsync(progress, verbose, cancellationToken);

    private async Task<ProcessRunResult> RemediateCoreAsync(Action<ProgressEvent>? progress, bool verbose, CancellationToken cancellationToken)
    {
        const string availabilityScript = """
        $cmdlet = Get-Command Remove-MpThreat -ErrorAction SilentlyContinue
        $service = Get-Service -Name WinDefend -ErrorAction SilentlyContinue
        [pscustomobject]@{
          removeMpThreatAvailable = $null -ne $cmdlet
          serviceInstalled = $null -ne $service
          serviceRunning = $null -ne $service -and $service.Status -eq 'Running'
        } | ConvertTo-Json -Depth 4 -Compress
        """;

        var availability = await _powerShellRunner.RunJsonAsync<DefenderRemediationAvailability>(availabilityScript, verbose, cancellationToken);
        if (availability is null
            || !availability.RemoveMpThreatAvailable
            || !availability.ServiceInstalled
            || !availability.ServiceRunning)
        {
            return new ProcessRunResult(3, string.Empty, string.Empty, "Windows Defender is unavailable.");
        }

        const string remediationScript = """
        Remove-MpThreat -ErrorAction Stop | Out-String
        """;

        return await _powerShellRunner.RunScriptAsync(remediationScript, progress, verbose, cancellationToken);
    }

    private Task<ProcessRunResult> RunMpCommandAsync(IReadOnlyList<string> arguments, Action<ProgressEvent>? progress, bool verbose, CancellationToken cancellationToken)
    {
        var mpCmdRun = FindMpCmdRunPath();
        if (string.IsNullOrWhiteSpace(mpCmdRun))
        {
            return Task.FromResult(new ProcessRunResult(3, string.Empty, string.Empty, "Windows Defender is unavailable."));
        }

        return _processRunner.RunAsync(mpCmdRun, arguments, progress, verbose, cancellationToken: cancellationToken);
    }

    private sealed class DefenderRemediationAvailability
    {
        public bool RemoveMpThreatAvailable { get; set; }
        public bool ServiceInstalled { get; set; }
        public bool ServiceRunning { get; set; }
    }
}

public sealed class PersistenceService
{
    private readonly FileSignatureVerifier _signatureVerifier;
    private readonly ProcessRunner _processRunner;
    private readonly PowerShellRunner _powerShellRunner;
    private readonly PathService _paths;

    public PersistenceService(FileSignatureVerifier signatureVerifier, ProcessRunner processRunner, PowerShellRunner powerShellRunner, PathService paths)
    {
        _signatureVerifier = signatureVerifier;
        _processRunner = processRunner;
        _powerShellRunner = powerShellRunner;
        _paths = paths;
    }

    public async Task<IReadOnlyList<PersistenceItem>> EnumerateAsync(bool verbose = false, Action<ProgressEvent>? progress = null, CancellationToken cancellationToken = default)
    {
        var items = new List<PersistenceItem>();
        items.AddRange(EnumerateStartupFolders());
        items.AddRange(EnumerateRunKeys());
        items.AddRange(await EnumerateScheduledTasksAsync(verbose, cancellationToken));
        items.AddRange(EnumerateAutoStartServices());

        var ordered = items
            .OrderBy(static item => item.Type, StringComparer.OrdinalIgnoreCase)
            .ThenBy(static item => item.Name, StringComparer.OrdinalIgnoreCase)
            .ThenBy(static item => item.Path, StringComparer.OrdinalIgnoreCase)
            .ToList();

        for (var index = 0; index < ordered.Count; index++)
        {
            ordered[index] = ordered[index] with { Id = index + 1 };
        }

        progress?.Invoke(new ProgressEvent(OutputSeverity.Info, $"Persistence entries enumerated: {ordered.Count}"));
        return ordered;
    }

    public async Task<(bool Success, string Message)> DisableAsync(PersistenceItem item, bool dryRun, bool verbose = false, CancellationToken cancellationToken = default)
    {
        return item.Type switch
        {
            "StartupFolder" => DisableStartupFile(item, dryRun),
            "RegistryRun" => DisableRegistryRun(item, dryRun),
            "ScheduledTask" => await DisableScheduledTaskAsync(item, dryRun, verbose, cancellationToken),
            "Service" => await DisableServiceAsync(item, dryRun, verbose, cancellationToken),
            _ => (false, $"Unsupported persistence type: {item.Type}."),
        };
    }

    private IEnumerable<PersistenceItem> EnumerateStartupFolders()
    {
        foreach (var folder in _paths.StartupFolders.Where(Directory.Exists))
        {
            foreach (var file in Directory.EnumerateFiles(folder))
            {
                var signature = _signatureVerifier.Verify(file);
                yield return new PersistenceItem(
                    0,
                    SuspiciousHeuristics.CreateStableKey($"startup|{Path.GetFullPath(file)}"),
                    "StartupFolder",
                    Path.GetFileName(file),
                    file,
                    string.Empty,
                    signature.Status,
                    signature.SignatureText,
                    folder,
                    file,
                    true,
                    signature.Publisher);
            }
        }
    }

    private IEnumerable<PersistenceItem> EnumerateRunKeys()
    {
        var locations = new[]
        {
            (Hive: Registry.CurrentUser, HiveName: "HKCU", KeyPath: @"Software\Microsoft\Windows\CurrentVersion\Run"),
            (Hive: Registry.CurrentUser, HiveName: "HKCU", KeyPath: @"Software\Microsoft\Windows\CurrentVersion\RunOnce"),
            (Hive: Registry.LocalMachine, HiveName: "HKLM", KeyPath: @"Software\Microsoft\Windows\CurrentVersion\Run"),
            (Hive: Registry.LocalMachine, HiveName: "HKLM", KeyPath: @"Software\Microsoft\Windows\CurrentVersion\RunOnce"),
        };

        foreach (var location in locations)
        {
            using var key = location.Hive.OpenSubKey(location.KeyPath, false);
            if (key is null)
            {
                continue;
            }

            foreach (var valueName in key.GetValueNames())
            {
                var raw = key.GetValue(valueName)?.ToString();
                if (string.IsNullOrWhiteSpace(raw))
                {
                    continue;
                }

                var (path, args) = SplitExecutableAndArguments(raw);
                var signature = File.Exists(path) ? _signatureVerifier.Verify(path) : new SignatureInfo(SignatureStatus.Unknown, SignatureStatus.Unknown.ToString(), null);
                yield return new PersistenceItem(
                    0,
                    SuspiciousHeuristics.CreateStableKey($"registry|{location.HiveName}|{location.KeyPath}|{valueName}"),
                    "RegistryRun",
                    valueName,
                    path,
                    args,
                    signature.Status,
                    signature.SignatureText,
                    $@"{location.HiveName}\{location.KeyPath}",
                    $"registry|{location.HiveName}|{location.KeyPath}|{valueName}",
                    true,
                    signature.Publisher);
            }
        }
    }

    private async Task<IEnumerable<PersistenceItem>> EnumerateScheduledTasksAsync(bool verbose, CancellationToken cancellationToken)
    {
        const string script = """
        if (Get-Command Get-ScheduledTask -ErrorAction SilentlyContinue) {
          $rows = @()
          foreach ($task in Get-ScheduledTask) {
            if ($task.Actions.Count -eq 0) {
              $rows += [pscustomobject]@{
                taskName = "$($task.TaskPath)$($task.TaskName)"
                execute = ""
                arguments = ""
                state = [string]$task.State
              }
            } else {
              foreach ($action in $task.Actions) {
                $rows += [pscustomobject]@{
                  taskName = "$($task.TaskPath)$($task.TaskName)"
                  execute = $action.Execute
                  arguments = $action.Arguments
                  state = [string]$task.State
                }
              }
            }
          }
          @($rows) | ConvertTo-Json -Depth 6 -Compress
        }
        """;

        var rows = await _powerShellRunner.RunJsonAsync<List<ScheduledTaskDto>>(script, verbose, cancellationToken) ?? [];
        return rows
            .Where(static row => !string.IsNullOrWhiteSpace(row.TaskName))
            .Select(row =>
            {
                var signature = File.Exists(row.Execute ?? string.Empty)
                    ? _signatureVerifier.Verify(row.Execute!)
                    : new SignatureInfo(SignatureStatus.Unknown, SignatureStatus.Unknown.ToString(), null);
                return new PersistenceItem(
                    0,
                    SuspiciousHeuristics.CreateStableKey($"task|{row.TaskName}"),
                    "ScheduledTask",
                    row.TaskName!,
                    row.Execute ?? string.Empty,
                    row.Arguments ?? string.Empty,
                    signature.Status,
                    signature.SignatureText,
                    row.TaskName!,
                    row.TaskName!,
                    !string.Equals(row.State, "Disabled", StringComparison.OrdinalIgnoreCase),
                    signature.Publisher);
            });
    }

    private IEnumerable<PersistenceItem> EnumerateAutoStartServices()
    {
        using var servicesKey = Registry.LocalMachine.OpenSubKey(@"SYSTEM\CurrentControlSet\Services", false);
        if (servicesKey is null)
        {
            yield break;
        }

        foreach (var serviceName in servicesKey.GetSubKeyNames())
        {
            using var serviceKey = servicesKey.OpenSubKey(serviceName, false);
            if (serviceKey is null)
            {
                continue;
            }

            if (serviceKey.GetValue("Start") is not int startMode || startMode != 2)
            {
                continue;
            }

            var imagePath = Environment.ExpandEnvironmentVariables(serviceKey.GetValue("ImagePath")?.ToString() ?? string.Empty);
            var (path, args) = SplitExecutableAndArguments(imagePath);
            var signature = File.Exists(path) ? _signatureVerifier.Verify(path) : new SignatureInfo(SignatureStatus.Unknown, SignatureStatus.Unknown.ToString(), null);
            yield return new PersistenceItem(
                0,
                SuspiciousHeuristics.CreateStableKey($"service|{serviceName}"),
                "Service",
                serviceName,
                path,
                args,
                signature.Status,
                signature.SignatureText,
                @"HKLM\SYSTEM\CurrentControlSet\Services",
                serviceName,
                true,
                signature.Publisher);
        }
    }

    private static (string Path, string Args) SplitExecutableAndArguments(string command)
    {
        var text = command.Trim();
        if (string.IsNullOrWhiteSpace(text))
        {
            return (string.Empty, string.Empty);
        }

        if (text.StartsWith('"'))
        {
            var closingIndex = text.IndexOf('"', 1);
            if (closingIndex > 1)
            {
                return (text[1..closingIndex], text[(closingIndex + 1)..].Trim());
            }
        }

        var firstSpace = text.IndexOf(' ');
        return firstSpace < 0 ? (text, string.Empty) : (text[..firstSpace], text[(firstSpace + 1)..].Trim());
    }

    private static (bool Success, string Message) DisableStartupFile(PersistenceItem item, bool dryRun)
    {
        var source = item.Reference;
        var target = source + ".disabled";
        if (dryRun)
        {
            return (true, $"Would rename {source} -> {target}");
        }

        File.Move(source, target, overwrite: false);
        return (true, $"Renamed startup entry to {Path.GetFileName(target)}");
    }

    private static (bool Success, string Message) DisableRegistryRun(PersistenceItem item, bool dryRun)
    {
        var parts = item.Reference.Split('|');
        if (parts.Length != 4)
        {
            return (false, "Invalid registry reference.");
        }

        var hive = parts[1] switch
        {
            "HKCU" => Registry.CurrentUser,
            "HKLM" => Registry.LocalMachine,
            _ => null,
        };

        if (hive is null)
        {
            return (false, "Unsupported registry hive.");
        }

        var subKey = parts[2];
        var valueName = parts[3];
        var backupPath = $@"Software\VoidCare\Backups\Run\{DateTime.UtcNow:yyyyMMddHHmmss}";
        if (dryRun)
        {
            return (true, $"Would move {valueName} from {parts[1]}\\{subKey} to {parts[1]}\\{backupPath}");
        }

        using var sourceKey = hive.OpenSubKey(subKey, writable: true);
        if (sourceKey is null)
        {
            return (false, "Registry source key not found.");
        }

        var currentValue = sourceKey.GetValue(valueName);
        var valueKind = sourceKey.GetValueKind(valueName);
        if (currentValue is null)
        {
            return (false, "Registry value not found.");
        }

        using var backupKey = hive.CreateSubKey(backupPath, writable: true);
        backupKey?.SetValue(valueName, currentValue, valueKind);
        sourceKey.DeleteValue(valueName, throwOnMissingValue: false);
        return (true, $"Moved registry entry to backup key {parts[1]}\\{backupPath}");
    }

    private async Task<(bool Success, string Message)> DisableScheduledTaskAsync(PersistenceItem item, bool dryRun, bool verbose, CancellationToken cancellationToken)
    {
        if (dryRun)
        {
            return (true, $"Would disable scheduled task {item.Reference}");
        }

        var result = await _processRunner.RunAsync("schtasks.exe", ["/Change", "/TN", item.Reference, "/Disable"], verbose: verbose, cancellationToken: cancellationToken);
        return result.Success
            ? (true, $"Disabled scheduled task {item.Reference}")
            : (false, string.IsNullOrWhiteSpace(result.StandardError) ? "Failed to disable scheduled task." : result.StandardError);
    }

    private async Task<(bool Success, string Message)> DisableServiceAsync(PersistenceItem item, bool dryRun, bool verbose, CancellationToken cancellationToken)
    {
        if (dryRun)
        {
            return (true, $"Would set service {item.Reference} to manual start");
        }

        var result = await _processRunner.RunAsync("sc.exe", ["config", item.Reference, "start=", "demand"], verbose: verbose, cancellationToken: cancellationToken);
        return result.Success
            ? (true, $"Set service {item.Reference} to manual start")
            : (false, string.IsNullOrWhiteSpace(result.StandardError) ? "Failed to change service start type." : result.StandardError);
    }

    private sealed class ScheduledTaskDto
    {
        public string? TaskName { get; set; }
        public string? Execute { get; set; }
        public string? Arguments { get; set; }
        public string? State { get; set; }
    }

}
