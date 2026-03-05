using System.Collections.Concurrent;
using System.Collections.ObjectModel;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using VoidCare.Wpf.Models;
using VoidCare.Wpf.Services;

namespace VoidCare.Wpf.ViewModels;

public sealed class MainViewModel : ViewModelBase, IAsyncDisposable
{
    private readonly IBridgeClient _bridgeClient;
    private readonly INavigationService _navigationService;
    private readonly Dispatcher _dispatcher;
    private readonly DispatcherTimer _logFlushTimer;
    private readonly ConcurrentQueue<(string Line, bool IsError)> _logQueue = new();

    private string _currentPage = "Dashboard";
    private string _searchText = string.Empty;
    private string _selectedConfig = "New Config 1";
    private string _statusText = "Ready.";
    private bool _statusIsError;
    private bool _discordEnabled = true;
    private string _discordAboutStatus = string.Empty;
    private string _antivirusProviderName = string.Empty;
    private string _antivirusStatus = string.Empty;
    private bool _defenderScanAvailable;
    private bool _defenderRemediationAvailable;
    private bool _externalScannerAvailable;
    private string _healthSummary = string.Empty;
    private string _creditsText = "Developed by Ysf (Lone Wolf Developer)";
    private string _footerText = "VoidCare by VoidTools  |  Developed by Ysf (Lone Wolf Developer)";
    private string _warningBannerText = "Suspicious != confirmed malware. Review before deleting.";
    private string _cpuStatValue = $"{Environment.ProcessorCount} Threads";
    private string _ramStatValue = "Unavailable";
    private string _diskStatValue = "--";
    private string _lastScanText = "Not run in this session";
    private string _lastOptimizeText = "Not run in this session";
    private string _antivirusChipState = "Unavailable";
    private string _customScanPath = string.Empty;
    private string _fullScanRoots = string.Empty;
    private string _externalExe = string.Empty;
    private string _externalArgs = string.Empty;
    private bool _removeBloat;
    private bool _disableCopilot;
    private bool _suppressDiscordSync;
    private PersistenceEntryRow? _selectedPersistenceEntry;

    public MainViewModel()
        : this(new BridgeClient(), new NavigationService())
    {
    }

    public MainViewModel(IBridgeClient bridgeClient, INavigationService navigationService)
    {
        _bridgeClient = bridgeClient;
        _navigationService = navigationService;
        _dispatcher = Application.Current.Dispatcher;

        ConfigOptions = new ObservableCollection<string>
        {
            "New Config 1",
            "New Config 2",
            "New Config 3",
        };

        PageItems = new ObservableCollection<string>
        {
            "Dashboard",
            "Security",
            "Optimize",
            "Gaming",
            "Apps",
            "About",
        };

        PersistenceEntries = new ObservableCollection<PersistenceEntryRow>();
        SuspiciousEntries = new ObservableCollection<SuspiciousFileRow>();
        QuarantineEntries = new ObservableCollection<QuarantineEntryRow>();
        InstalledApps = new ObservableCollection<InstalledAppRow>();
        Logs = new ObservableCollection<string>();

        NavigateCommand = new AsyncRelayCommand(NavigateAsync);

        RefreshAntivirusCommand = new AsyncRelayCommand(_ => RunSimpleAsync("refresh_antivirus", "Antivirus status refreshed."));
        DefenderQuickScanCommand = new AsyncRelayCommand(_ => RunActionAsync("run_defender_quick_scan"), _ => DefenderScanAvailable);
        DefenderFullScanCommand = new AsyncRelayCommand(_ => RunActionAsync("run_defender_full_scan"), _ => DefenderScanAvailable);
        DefenderCustomScanCommand = new AsyncRelayCommand(_ => RunActionAsync("run_defender_custom_scan", new JsonObject
        {
            ["customPath"] = CustomScanPath,
        }), _ => DefenderScanAvailable);
        DefenderAutoRemediateCommand = new AsyncRelayCommand(_ => RunActionAsync("run_defender_auto_remediate"), _ => DefenderRemediationAvailable);

        ConfigureExternalScannerCommand = new AsyncRelayCommand(_ => ConfigureExternalScannerAsync(), _ => ExternalScannerAvailable);
        RunExternalScannerCommand = new AsyncRelayCommand(_ => RunActionAsync("run_external_scanner"), _ => ExternalScannerAvailable);

        RefreshPersistenceAuditCommand = new AsyncRelayCommand(_ => RunActionAsync("refresh_persistence_audit"));
        DisablePersistenceEntryCommand = new AsyncRelayCommand(_ => DisableSelectedPersistenceAsync(), _ => SelectedPersistenceEntry is not null);

        QuickSuspiciousScanCommand = new AsyncRelayCommand(_ => RunActionAsync("run_quick_suspicious_scan"));
        FullSuspiciousScanCommand = new AsyncRelayCommand(_ => RunFullSuspiciousScanAsync());
        QuarantineSelectedCommand = new AsyncRelayCommand(_ => QuarantineSelectedAsync());
        RestoreQuarantinedCommand = new AsyncRelayCommand(_ => RestoreSelectedAsync());
        DeleteQuarantinedCommand = new AsyncRelayCommand(_ => DeleteSelectedAsync());

        RunSafeOptimizationCommand = new AsyncRelayCommand(_ => RunGuardedAsync("run_safe_optimization", "Run safe optimization?", "VoidCare will attempt a restore point first."));
        RunAggressiveOptimizationCommand = new AsyncRelayCommand(_ => RunGuardedAsync("run_aggressive_optimization", "Run aggressive optimization?", "Optional bloat/policy actions may be applied.", new JsonObject
        {
            ["removeBloat"] = RemoveBloat,
            ["disableCopilot"] = DisableCopilot,
        }));

        EnableGamingBoostCommand = new AsyncRelayCommand(_ => RunGuardedAsync("enable_gaming_boost", "Enable gaming boost?", "A restore point will be attempted first."));
        RevertGamingBoostCommand = new AsyncRelayCommand(_ => RunGuardedAsync("revert_gaming_boost", "Revert gaming boost?", "A restore point will be attempted first."));

        RefreshAppsCommand = new AsyncRelayCommand(_ => RunActionAsync("refresh_installed_apps"));
        OpenAppsSettingsCommand = new AsyncRelayCommand(_ => RunOpenActionAsync("open_apps_settings", "Apps settings opened."));
        OpenProgramsFeaturesCommand = new AsyncRelayCommand(_ => RunOpenActionAsync("open_programs_features", "Programs and Features opened."));

        RefreshHealthCommand = new AsyncRelayCommand(_ => RunActionAsync("refresh_health_report"));
        ClearLogsCommand = new AsyncRelayCommand(_ => RunSimpleAsync("clear_logs", "Logs cleared."));

        _bridgeClient.EventReceived += OnBridgeEventReceived;

        _logFlushTimer = new DispatcherTimer(DispatcherPriority.Background, _dispatcher)
        {
            Interval = TimeSpan.FromMilliseconds(100),
        };
        _logFlushTimer.Tick += (_, _) => FlushLogQueue();
        _logFlushTimer.Start();

        RefreshMemorySnapshot();
        _ = InitializeAsync();
    }

    public ObservableCollection<string> ConfigOptions { get; }
    public ObservableCollection<string> PageItems { get; }

    public ObservableCollection<PersistenceEntryRow> PersistenceEntries { get; }
    public ObservableCollection<SuspiciousFileRow> SuspiciousEntries { get; }
    public ObservableCollection<QuarantineEntryRow> QuarantineEntries { get; }
    public ObservableCollection<InstalledAppRow> InstalledApps { get; }
    public ObservableCollection<string> Logs { get; }

    public ICommand NavigateCommand { get; }

    public ICommand RefreshAntivirusCommand { get; }
    public ICommand DefenderQuickScanCommand { get; }
    public ICommand DefenderFullScanCommand { get; }
    public ICommand DefenderCustomScanCommand { get; }
    public ICommand DefenderAutoRemediateCommand { get; }
    public ICommand ConfigureExternalScannerCommand { get; }
    public ICommand RunExternalScannerCommand { get; }

    public ICommand RefreshPersistenceAuditCommand { get; }
    public ICommand DisablePersistenceEntryCommand { get; }

    public ICommand QuickSuspiciousScanCommand { get; }
    public ICommand FullSuspiciousScanCommand { get; }
    public ICommand QuarantineSelectedCommand { get; }
    public ICommand RestoreQuarantinedCommand { get; }
    public ICommand DeleteQuarantinedCommand { get; }

    public ICommand RunSafeOptimizationCommand { get; }
    public ICommand RunAggressiveOptimizationCommand { get; }
    public ICommand EnableGamingBoostCommand { get; }
    public ICommand RevertGamingBoostCommand { get; }

    public ICommand RefreshAppsCommand { get; }
    public ICommand OpenAppsSettingsCommand { get; }
    public ICommand OpenProgramsFeaturesCommand { get; }

    public ICommand RefreshHealthCommand { get; }
    public ICommand ClearLogsCommand { get; }

    public string CurrentPage
    {
        get => _currentPage;
        private set => SetProperty(ref _currentPage, value);
    }

    public string SearchText
    {
        get => _searchText;
        set => SetProperty(ref _searchText, value);
    }

    public string SelectedConfig
    {
        get => _selectedConfig;
        set => SetProperty(ref _selectedConfig, value);
    }

    public string StatusText
    {
        get => _statusText;
        private set => SetProperty(ref _statusText, value);
    }

    public bool StatusIsError
    {
        get => _statusIsError;
        private set => SetProperty(ref _statusIsError, value);
    }

    public bool DiscordEnabled
    {
        get => _discordEnabled;
        set
        {
            if (_discordEnabled == value)
            {
                return;
            }

            SetProperty(ref _discordEnabled, value);
            if (!_suppressDiscordSync)
            {
                _ = SetDiscordEnabledAsync(value);
            }
        }
    }

    public string DiscordAboutStatus
    {
        get => _discordAboutStatus;
        private set => SetProperty(ref _discordAboutStatus, value);
    }

    public string AntivirusProviderName
    {
        get => _antivirusProviderName;
        private set => SetProperty(ref _antivirusProviderName, value);
    }

    public string AntivirusStatus
    {
        get => _antivirusStatus;
        private set => SetProperty(ref _antivirusStatus, value);
    }

    public bool DefenderScanAvailable
    {
        get => _defenderScanAvailable;
        private set
        {
            SetProperty(ref _defenderScanAvailable, value);
            NotifyCommandStateChanged();
        }
    }

    public bool DefenderRemediationAvailable
    {
        get => _defenderRemediationAvailable;
        private set
        {
            SetProperty(ref _defenderRemediationAvailable, value);
            NotifyCommandStateChanged();
        }
    }

    public bool ExternalScannerAvailable
    {
        get => _externalScannerAvailable;
        private set
        {
            SetProperty(ref _externalScannerAvailable, value);
            NotifyCommandStateChanged();
        }
    }

    public string HealthSummary
    {
        get => _healthSummary;
        private set => SetProperty(ref _healthSummary, value);
    }

    public string CreditsText
    {
        get => _creditsText;
        private set => SetProperty(ref _creditsText, value);
    }

    public string FooterText
    {
        get => _footerText;
        private set => SetProperty(ref _footerText, value);
    }

    public string WarningBannerText
    {
        get => _warningBannerText;
        private set => SetProperty(ref _warningBannerText, value);
    }

    public string CpuStatValue
    {
        get => _cpuStatValue;
        private set => SetProperty(ref _cpuStatValue, value);
    }

    public string RamStatValue
    {
        get => _ramStatValue;
        private set => SetProperty(ref _ramStatValue, value);
    }

    public string DiskStatValue
    {
        get => _diskStatValue;
        private set => SetProperty(ref _diskStatValue, value);
    }

    public string LastScanText
    {
        get => _lastScanText;
        private set => SetProperty(ref _lastScanText, value);
    }

    public string LastOptimizeText
    {
        get => _lastOptimizeText;
        private set => SetProperty(ref _lastOptimizeText, value);
    }

    public string AntivirusChipState
    {
        get => _antivirusChipState;
        private set => SetProperty(ref _antivirusChipState, value);
    }

    public string CustomScanPath
    {
        get => _customScanPath;
        set => SetProperty(ref _customScanPath, value);
    }

    public string FullScanRoots
    {
        get => _fullScanRoots;
        set => SetProperty(ref _fullScanRoots, value);
    }

    public string ExternalExe
    {
        get => _externalExe;
        set => SetProperty(ref _externalExe, value);
    }

    public string ExternalArgs
    {
        get => _externalArgs;
        set => SetProperty(ref _externalArgs, value);
    }

    public bool RemoveBloat
    {
        get => _removeBloat;
        set => SetProperty(ref _removeBloat, value);
    }

    public bool DisableCopilot
    {
        get => _disableCopilot;
        set => SetProperty(ref _disableCopilot, value);
    }

    public PersistenceEntryRow? SelectedPersistenceEntry
    {
        get => _selectedPersistenceEntry;
        set
        {
            SetProperty(ref _selectedPersistenceEntry, value);
            NotifyCommandStateChanged();
        }
    }

    public async ValueTask DisposeAsync()
    {
        _logFlushTimer.Stop();
        _bridgeClient.EventReceived -= OnBridgeEventReceived;
        await _bridgeClient.DisposeAsync();
    }

    private async Task InitializeAsync()
    {
        try
        {
            await _bridgeClient.ConnectAsync().ConfigureAwait(false);
            var response = await _bridgeClient.SendRequestAsync("get_initial_snapshot").ConfigureAwait(false);
            if (!response.Ok)
            {
                SetStatus(false, response.Error?["message"]?.GetValue<string>() ?? "Failed to load initial snapshot.");
                return;
            }

            if (response.Result is JsonObject result && result["state"] is JsonObject state)
            {
                await _dispatcher.InvokeAsync(() => ApplySnapshot(state));
            }

            SetStatus(true, "Bridge connected.");
        }
        catch (Exception ex)
        {
            SetStatus(false, $"Bridge connection failed: {ex.Message}");
        }
    }

    private async Task NavigateAsync(object? parameter)
    {
        var page = parameter as string;
        if (string.IsNullOrWhiteSpace(page))
        {
            return;
        }

        _navigationService.Navigate(page);
        CurrentPage = page;

        await _bridgeClient.SendRequestAsync("navigate", new JsonObject
        {
            ["page"] = page,
        }).ConfigureAwait(false);
    }

    private async Task SetDiscordEnabledAsync(bool enabled)
    {
        try
        {
            await RunSimpleAsync("set_discord_enabled", enabled ? "Discord RPC enabled." : "Discord RPC disabled.", new JsonObject
            {
                ["enabled"] = enabled,
            }).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            SetStatus(false, $"Failed to set Discord RPC state: {ex.Message}");
        }
    }

    private async Task ConfigureExternalScannerAsync()
    {
        var response = await _bridgeClient.SendRequestAsync("configure_external_scanner", new JsonObject
        {
            ["executable"] = ExternalExe,
            ["argsLine"] = ExternalArgs,
        }).ConfigureAwait(false);

        var success = response.Ok && (response.Result?["success"]?.GetValue<bool>() ?? false);
        var message = response.Result?["message"]?.GetValue<string>() ?? "External scanner command update failed.";
        SetStatus(success, message);
    }

    private async Task DisableSelectedPersistenceAsync()
    {
        if (SelectedPersistenceEntry is null)
        {
            return;
        }

        await RunGuardedAsync("disable_persistence_entry",
                              "Disable persistence entry?",
                              "A restore point will be attempted first.",
                              new JsonObject
                              {
                                  ["entryId"] = SelectedPersistenceEntry.Id,
                              }).ConfigureAwait(false);
    }

    private async Task RunFullSuspiciousScanAsync()
    {
        var roots = new JsonArray();
        foreach (var root in FullScanRoots.Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            roots.Add(root);
        }

        await RunActionAsync("run_full_suspicious_scan", new JsonObject
        {
            ["roots"] = roots,
        }).ConfigureAwait(false);
    }

    private async Task QuarantineSelectedAsync()
    {
        var selected = SuspiciousEntries.Where(x => x.IsSelected).Select(x => x.Path).ToArray();
        if (selected.Length == 0)
        {
            SetStatus(false, "Select suspicious files first.");
            return;
        }

        var payload = new JsonObject
        {
            ["filePaths"] = new JsonArray(selected.Select(x => JsonValue.Create(x)).ToArray()),
        };

        await RunGuardedAsync("quarantine_selected", "Quarantine selected files?", "Files will be moved to quarantine.", payload).ConfigureAwait(false);
    }

    private async Task RestoreSelectedAsync()
    {
        var selected = QuarantineEntries.Where(x => x.IsSelected).Select(x => x.QuarantinePath).ToArray();
        if (selected.Length == 0)
        {
            SetStatus(false, "Select quarantined files first.");
            return;
        }

        await RunActionAsync("restore_quarantined", new JsonObject
        {
            ["quarantinePaths"] = new JsonArray(selected.Select(x => JsonValue.Create(x)).ToArray()),
            ["destinationOverride"] = string.Empty,
        }).ConfigureAwait(false);
    }

    private async Task DeleteSelectedAsync()
    {
        var selected = QuarantineEntries.Where(x => x.IsSelected).Select(x => x.QuarantinePath).ToArray();
        if (selected.Length == 0)
        {
            SetStatus(false, "Select quarantined files first.");
            return;
        }

        await RunGuardedAsync("delete_quarantined",
                              "Delete quarantined files permanently?",
                              "This cannot be undone. A restore point will be attempted first.",
                              new JsonObject
                              {
                                  ["quarantinePaths"] = new JsonArray(selected.Select(x => JsonValue.Create(x)).ToArray()),
                              }).ConfigureAwait(false);
    }

    private async Task RunOpenActionAsync(string method, string successMessage)
    {
        var response = await _bridgeClient.SendRequestAsync(method).ConfigureAwait(false);
        var success = response.Ok && (response.Result?["success"]?.GetValue<bool>() ?? false);
        SetStatus(success, success ? successMessage : "Operation failed.");
    }

    private async Task RunSimpleAsync(string method, string successMessage, JsonObject? args = null)
    {
        var response = await _bridgeClient.SendRequestAsync(method, args).ConfigureAwait(false);
        if (!response.Ok)
        {
            SetStatus(false, response.Error?["message"]?.GetValue<string>() ?? "Bridge request failed.");
            return;
        }

        SetStatus(true, successMessage);
    }

    private async Task RunActionAsync(string method, JsonObject? args = null)
    {
        var response = await _bridgeClient.SendRequestAsync(method, args).ConfigureAwait(false);
        if (!response.Ok)
        {
            SetStatus(false, response.Error?["message"]?.GetValue<string>() ?? "Bridge request failed.");
            return;
        }

        var result = ParseActionResult(response.Result);
        SetStatus(result.Success, result.Message);
        UpdateTimelineFromAction(method, result.Success);
        if (result.NeedsRestoreOverride)
        {
            await RunGuardedAsync(method, "Restore point failed", "Restore point could not be created. Continue anyway?", args, true, result.RestoreDetail).ConfigureAwait(false);
        }
    }

    private async Task RunGuardedAsync(string method,
                                       string confirmTitle,
                                       string confirmText,
                                       JsonObject? args = null,
                                       bool skipFirstConfirm = false,
                                       string restoreDetail = "")
    {
        if (!skipFirstConfirm)
        {
            var firstConfirm = await _dispatcher.InvokeAsync(() =>
                MessageBox.Show(confirmText, confirmTitle, MessageBoxButton.YesNo, MessageBoxImage.Warning) == MessageBoxResult.Yes);
            if (!firstConfirm)
            {
                return;
            }
        }

        var payload = args?.DeepClone() as JsonObject ?? new JsonObject();
        payload["initialConfirmed"] = true;
        payload["proceedWithoutRestorePoint"] = skipFirstConfirm;

        var response = await _bridgeClient.SendRequestAsync(method, payload).ConfigureAwait(false);
        if (!response.Ok)
        {
            SetStatus(false, response.Error?["message"]?.GetValue<string>() ?? "Bridge request failed.");
            return;
        }

        var result = ParseActionResult(response.Result);
        if (result.NeedsRestoreOverride && !skipFirstConfirm)
        {
            var details = string.IsNullOrWhiteSpace(result.RestoreDetail) ? "Restore point failed." : result.RestoreDetail;
            var secondConfirm = await _dispatcher.InvokeAsync(() =>
                MessageBox.Show($"{details}\n\nContinue anyway?", "Restore point failed", MessageBoxButton.YesNo, MessageBoxImage.Warning) == MessageBoxResult.Yes);
            if (!secondConfirm)
            {
                return;
            }

            await RunGuardedAsync(method, confirmTitle, confirmText, args, true, details).ConfigureAwait(false);
            return;
        }

        SetStatus(result.Success, result.Message);
        UpdateTimelineFromAction(method, result.Success);
    }

    private void OnBridgeEventReceived(object? sender, BridgeEvent bridgeEvent)
    {
        if (string.Equals(bridgeEvent.Event, "log", StringComparison.OrdinalIgnoreCase))
        {
            var line = bridgeEvent.Payload["line"]?.GetValue<string>();
            var isError = bridgeEvent.Payload["isError"]?.GetValue<bool>() ?? false;
            if (!string.IsNullOrWhiteSpace(line))
            {
                _logQueue.Enqueue((line!, isError));
            }
            return;
        }

        if (string.Equals(bridgeEvent.Event, "state", StringComparison.OrdinalIgnoreCase) &&
            bridgeEvent.Payload["state"] is JsonObject state)
        {
            _dispatcher.Invoke(() => ApplySnapshot(state));
        }
    }

    private void FlushLogQueue()
    {
        var count = 0;
        while (count < 100 && _logQueue.TryDequeue(out var item))
        {
            Logs.Add(item.Line);
            while (Logs.Count > 800)
            {
                Logs.RemoveAt(0);
            }
            count++;
        }
    }

    private void ApplySnapshot(JsonObject state)
    {
        CurrentPage = state["currentPage"]?.GetValue<string>() ?? CurrentPage;
        CreditsText = state["creditsText"]?.GetValue<string>() ?? CreditsText;
        FooterText = state["footerText"]?.GetValue<string>() ?? FooterText;
        WarningBannerText = state["warningBannerText"]?.GetValue<string>() ?? WarningBannerText;
        _suppressDiscordSync = true;
        DiscordEnabled = state["discordEnabled"]?.GetValue<bool>() ?? DiscordEnabled;
        _suppressDiscordSync = false;
        DiscordAboutStatus = state["discordAboutStatus"]?.GetValue<string>() ?? DiscordAboutStatus;
        AntivirusProviderName = state["antivirusProviderName"]?.GetValue<string>() ?? AntivirusProviderName;
        AntivirusStatus = state["antivirusStatus"]?.GetValue<string>() ?? AntivirusStatus;
        DefenderScanAvailable = state["defenderScanAvailable"]?.GetValue<bool>() ?? DefenderScanAvailable;
        DefenderRemediationAvailable = state["defenderRemediationAvailable"]?.GetValue<bool>() ?? DefenderRemediationAvailable;
        ExternalScannerAvailable = state["externalScannerAvailable"]?.GetValue<bool>() ?? ExternalScannerAvailable;
        HealthSummary = state["healthSummary"]?.GetValue<string>() ?? HealthSummary;
        UpdateHealthDerivedState();
        UpdateAntivirusChipState();

        ReplaceCollection(PersistenceEntries, state["persistenceEntries"] as JsonArray, MapPersistenceRow);
        ReplaceCollection(SuspiciousEntries, state["suspiciousEntries"] as JsonArray, MapSuspiciousRow);
        ReplaceCollection(QuarantineEntries, state["quarantineEntries"] as JsonArray, MapQuarantineRow);
        ReplaceCollection(InstalledApps, state["installedApps"] as JsonArray, MapAppRow);

        if (state["logs"] is JsonArray logs)
        {
            Logs.Clear();
            foreach (var entry in logs)
            {
                var line = entry?.GetValue<string>();
                if (!string.IsNullOrWhiteSpace(line))
                {
                    Logs.Add(line!);
                }
            }
        }
    }

    private static void ReplaceCollection<T>(ObservableCollection<T> collection, JsonArray? source, Func<JsonObject, T> mapper)
    {
        if (source is null)
        {
            return;
        }

        collection.Clear();
        foreach (var node in source)
        {
            if (node is JsonObject obj)
            {
                collection.Add(mapper(obj));
            }
        }
    }

    private static PersistenceEntryRow MapPersistenceRow(JsonObject obj)
    {
        return new PersistenceEntryRow
        {
            Id = obj["id"]?.GetValue<string>() ?? string.Empty,
            SourceType = obj["sourceType"]?.GetValue<string>() ?? string.Empty,
            Name = obj["name"]?.GetValue<string>() ?? string.Empty,
            Path = obj["path"]?.GetValue<string>() ?? string.Empty,
            Args = obj["args"]?.GetValue<string>() ?? string.Empty,
            Publisher = obj["publisher"]?.GetValue<string>() ?? string.Empty,
            SignatureStatus = obj["signatureStatus"]?.GetValue<string>() ?? string.Empty,
            Enabled = obj["enabled"]?.GetValue<bool>() ?? false,
        };
    }

    private static SuspiciousFileRow MapSuspiciousRow(JsonObject obj)
    {
        var reasons = string.Empty;
        if (obj["reasons"] is JsonArray reasonsArray)
        {
            reasons = string.Join("; ", reasonsArray.Select(x => x?.GetValue<string>() ?? string.Empty));
        }

        return new SuspiciousFileRow
        {
            Path = obj["path"]?.GetValue<string>() ?? string.Empty,
            Size = obj["size"]?.GetValue<long>() ?? 0,
            Created = obj["created"]?.GetValue<string>() ?? string.Empty,
            Modified = obj["modified"]?.GetValue<string>() ?? string.Empty,
            Sha256 = obj["sha256"]?.GetValue<string>() ?? string.Empty,
            SignatureStatus = obj["signatureStatus"]?.GetValue<string>() ?? string.Empty,
            Score = obj["score"]?.GetValue<int>() ?? 0,
            Reasons = reasons,
            PersistenceRef = obj["persistenceRef"]?.GetValue<string>() ?? string.Empty,
        };
    }

    private static QuarantineEntryRow MapQuarantineRow(JsonObject obj)
    {
        var reasons = string.Empty;
        if (obj["reasons"] is JsonArray reasonsArray)
        {
            reasons = string.Join("; ", reasonsArray.Select(x => x?.GetValue<string>() ?? string.Empty));
        }

        return new QuarantineEntryRow
        {
            OriginalPath = obj["originalPath"]?.GetValue<string>() ?? string.Empty,
            QuarantinePath = obj["quarantinePath"]?.GetValue<string>() ?? string.Empty,
            Sha256 = obj["sha256"]?.GetValue<string>() ?? string.Empty,
            Timestamp = obj["timestamp"]?.GetValue<string>() ?? string.Empty,
            SignatureStatus = obj["signatureStatus"]?.GetValue<string>() ?? string.Empty,
            Reasons = reasons,
        };
    }

    private static InstalledAppRow MapAppRow(JsonObject obj)
    {
        return new InstalledAppRow
        {
            Name = obj["name"]?.GetValue<string>() ?? string.Empty,
            Version = obj["version"]?.GetValue<string>() ?? string.Empty,
            Publisher = obj["publisher"]?.GetValue<string>() ?? string.Empty,
        };
    }

    private static ActionResult ParseActionResult(JsonNode? node)
    {
        if (node is not JsonObject obj)
        {
            return new ActionResult
            {
                Success = false,
                Message = "Unexpected response payload.",
            };
        }

        return new ActionResult
        {
            Success = obj["success"]?.GetValue<bool>() ?? false,
            Message = obj["message"]?.GetValue<string>() ?? "Operation completed.",
            ExitCode = obj["exitCode"]?.GetValue<int>() ?? 0,
            NeedsRestoreOverride = obj["needsRestoreOverride"]?.GetValue<bool>() ?? false,
            RestoreDetail = obj["restoreDetail"]?.GetValue<string>() ?? string.Empty,
        };
    }

    private void SetStatus(bool success, string message)
    {
        _dispatcher.Invoke(() =>
        {
            StatusIsError = !success;
            StatusText = message;
        });
    }

    private void NotifyCommandStateChanged()
    {
        foreach (var command in new[]
                 {
                     DefenderQuickScanCommand,
                     DefenderFullScanCommand,
                     DefenderCustomScanCommand,
                     DefenderAutoRemediateCommand,
                     ConfigureExternalScannerCommand,
                     RunExternalScannerCommand,
                     DisablePersistenceEntryCommand,
                 })
        {
            if (command is AsyncRelayCommand asyncRelay)
            {
                asyncRelay.NotifyCanExecuteChanged();
            }
        }
    }

    private void UpdateTimelineFromAction(string method, bool success)
    {
        if (!success)
        {
            return;
        }

        var now = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
        if (method is "run_defender_quick_scan" or "run_defender_full_scan" or "run_defender_custom_scan" or "run_external_scanner" or "run_quick_suspicious_scan" or "run_full_suspicious_scan")
        {
            LastScanText = now;
        }

        if (method is "run_safe_optimization" or "run_aggressive_optimization")
        {
            LastOptimizeText = now;
        }

        if (method is "refresh_health_report")
        {
            UpdateHealthDerivedState();
        }
    }

    private void UpdateAntivirusChipState()
    {
        if (DefenderScanAvailable || DefenderRemediationAvailable)
        {
            AntivirusChipState = "Active";
            return;
        }

        if (ExternalScannerAvailable)
        {
            AntivirusChipState = "Passive";
            return;
        }

        if (!string.IsNullOrWhiteSpace(AntivirusProviderName) &&
            !AntivirusProviderName.Contains("none", StringComparison.OrdinalIgnoreCase) &&
            !AntivirusProviderName.Contains("not detected", StringComparison.OrdinalIgnoreCase))
        {
            AntivirusChipState = "Passive";
            return;
        }

        AntivirusChipState = "Unavailable";
    }

    private void UpdateHealthDerivedState()
    {
        CpuStatValue = $"{Environment.ProcessorCount} Threads";
        RefreshMemorySnapshot();

        var summary = HealthSummary ?? string.Empty;
        const string diskPrefix = "Disk Free:";
        var prefixIndex = summary.IndexOf(diskPrefix, StringComparison.OrdinalIgnoreCase);
        if (prefixIndex >= 0)
        {
            var start = prefixIndex + diskPrefix.Length;
            var end = summary.IndexOf('|', start);
            var segment = end >= 0 ? summary[start..end] : summary[start..];
            var parsed = segment.Trim();
            if (!string.IsNullOrWhiteSpace(parsed))
            {
                DiskStatValue = parsed;
                return;
            }
        }

        DiskStatValue = "--";
    }

    private void RefreshMemorySnapshot()
    {
        var status = new MEMORYSTATUSEX();
        status.dwLength = (uint)Marshal.SizeOf<MEMORYSTATUSEX>();

        if (!GlobalMemoryStatusEx(ref status) || status.ullTotalPhys == 0)
        {
            RamStatValue = "Unavailable";
            return;
        }

        var totalGb = status.ullTotalPhys / (1024d * 1024d * 1024d);
        var availableGb = status.ullAvailPhys / (1024d * 1024d * 1024d);
        var usedGb = Math.Max(0, totalGb - availableGb);
        RamStatValue = $"{usedGb:0.#} / {totalGb:0.#} GB";
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GlobalMemoryStatusEx(ref MEMORYSTATUSEX lpBuffer);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
    private struct MEMORYSTATUSEX
    {
        public uint dwLength;
        public uint dwMemoryLoad;
        public ulong ullTotalPhys;
        public ulong ullAvailPhys;
        public ulong ullTotalPageFile;
        public ulong ullAvailPageFile;
        public ulong ullTotalVirtual;
        public ulong ullAvailVirtual;
        public ulong ullAvailExtendedVirtual;
    }
}
