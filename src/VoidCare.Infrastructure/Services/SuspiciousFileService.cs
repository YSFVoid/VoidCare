using System.Text.Json;
using VoidCare.Core.Models;
using VoidCare.Core.Services;

namespace VoidCare.Infrastructure.Services;

public sealed class SuspiciousFileService
{
    private readonly PathService _paths;
    private readonly FileSignatureVerifier _signatureVerifier;
    private readonly HashService _hashService;
    private readonly ProcessRunner _processRunner;

    public SuspiciousFileService(
        PathService paths,
        FileSignatureVerifier signatureVerifier,
        HashService hashService,
        ProcessRunner processRunner)
    {
        _paths = paths;
        _signatureVerifier = signatureVerifier;
        _hashService = hashService;
        _processRunner = processRunner;
    }

    public Task<IReadOnlyList<SuspiciousFileRecord>> ScanQuickAsync(
        IReadOnlyList<PersistenceItem> persistenceItems,
        bool verbose = false,
        Action<ProgressEvent>? progress = null,
        CancellationToken cancellationToken = default)
    {
        var roots = _paths.QuickSuspiciousRoots
            .Concat(persistenceItems.Select(static item => item.Path))
            .Where(static path => !string.IsNullOrWhiteSpace(path))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToArray();
        return ScanAsync(roots, persistenceItems, verbose, progress, cancellationToken);
    }

    public Task<IReadOnlyList<SuspiciousFileRecord>> ScanFullAsync(
        IReadOnlyList<string> roots,
        IReadOnlyList<PersistenceItem> persistenceItems,
        bool verbose = false,
        Action<ProgressEvent>? progress = null,
        CancellationToken cancellationToken = default)
        => ScanAsync(roots, persistenceItems, verbose, progress, cancellationToken);

    public async Task<(bool Success, string Message, string? Folder)> QuarantineAsync(
        IReadOnlyList<SuspiciousFileRecord> items,
        bool dryRun,
        bool verbose = false,
        Action<ProgressEvent>? progress = null,
        CancellationToken cancellationToken = default)
    {
        if (items.Count == 0)
        {
            return (false, "No suspicious files selected.", null);
        }

        var folderName = $"{DateTime.UtcNow:yyyyMMdd_HHmmss}_{Random.Shared.Next(100000, 999999)}";
        var folder = Path.Combine(_paths.QuarantineRoot, folderName);

        if (dryRun)
        {
            return (true, $"Would quarantine {items.Count} file(s) into {folder}", folder);
        }

        Directory.CreateDirectory(folder);
        var manifest = new List<QuarantineManifestRow>();

        foreach (var item in items)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (!File.Exists(item.Path))
            {
                progress?.Invoke(new ProgressEvent(OutputSeverity.Warn, $"Missing file skipped: {item.Path}"));
                continue;
            }

            var target = DeduplicateTargetPath(Path.Combine(folder, Path.GetFileName(item.Path)));
            MoveFile(item.Path, target);

            manifest.Add(new QuarantineManifestRow
            {
                OriginalPath = item.Path,
                QuarantinePath = target,
                Sha256 = item.Sha256,
                Timestamp = DateTimeOffset.UtcNow,
                Reasons = item.Reasons.ToList(),
                SignatureStatus = item.SignatureText,
            });
        }

        var manifestPath = Path.Combine(folder, "manifest.json");
        File.WriteAllText(manifestPath, JsonSerializer.Serialize(manifest, JsonDefaults.Options));
        await _processRunner.RunAsync("icacls.exe", [folder, "/inheritance:r", "/grant:r", "Administrators:(OI)(CI)F", "SYSTEM:(OI)(CI)F"], verbose: verbose, cancellationToken: cancellationToken);

        return (true, $"Quarantined {manifest.Count} file(s).", folder);
    }

    public IReadOnlyList<QuarantineRecord> ListQuarantine()
    {
        var rows = new List<QuarantineRecord>();
        if (!Directory.Exists(_paths.QuarantineRoot))
        {
            return rows;
        }

        foreach (var manifestPath in Directory.EnumerateFiles(_paths.QuarantineRoot, "manifest.json", SearchOption.AllDirectories))
        {
            var manifest = JsonSerializer.Deserialize<List<QuarantineManifestRow>>(File.ReadAllText(manifestPath), JsonDefaults.Options) ?? [];
            rows.AddRange(manifest.Select(row => new QuarantineRecord(
                0,
                row.OriginalPath,
                row.QuarantinePath,
                row.Sha256,
                row.Timestamp,
                row.Reasons,
                row.SignatureStatus,
                manifestPath)));
        }

        rows = rows
            .OrderBy(static row => row.Timestamp)
            .ThenBy(static row => row.QuarantinePath, StringComparer.OrdinalIgnoreCase)
            .ToList();

        for (var index = 0; index < rows.Count; index++)
        {
            rows[index] = rows[index] with { Id = index + 1 };
        }

        return rows;
    }

    public (bool Success, string Message) Restore(QuarantineRecord item, string? destination, bool dryRun)
    {
        var target = string.IsNullOrWhiteSpace(destination)
            ? item.OriginalPath
            : Path.Combine(destination, Path.GetFileName(item.OriginalPath));

        if (dryRun)
        {
            return (true, $"Would restore {item.QuarantinePath} -> {target}");
        }

        Directory.CreateDirectory(Path.GetDirectoryName(target)!);
        var actualTarget = File.Exists(target) ? target + ".restored" : target;
        MoveFile(item.QuarantinePath, actualTarget);
        UpdateManifest(item);
        return (true, $"Restored {actualTarget}");
    }

    public (bool Success, string Message) Delete(QuarantineRecord item, bool dryRun)
    {
        if (dryRun)
        {
            return (true, $"Would permanently delete {item.QuarantinePath}");
        }

        if (File.Exists(item.QuarantinePath))
        {
            File.Delete(item.QuarantinePath);
        }

        UpdateManifest(item);
        return (true, $"Deleted {item.QuarantinePath}");
    }

    private Task<IReadOnlyList<SuspiciousFileRecord>> ScanAsync(
        IReadOnlyList<string> roots,
        IReadOnlyList<PersistenceItem> persistenceItems,
        bool verbose,
        Action<ProgressEvent>? progress,
        CancellationToken cancellationToken)
    {
        var visited = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var persistencePaths = persistenceItems
            .Where(static item => !string.IsNullOrWhiteSpace(item.Path))
            .Select(static item => item.Path)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        var results = new List<SuspiciousFileRecord>();

        foreach (var root in roots.Where(static root => !string.IsNullOrWhiteSpace(root)).Distinct(StringComparer.OrdinalIgnoreCase))
        {
            cancellationToken.ThrowIfCancellationRequested();
            progress?.Invoke(new ProgressEvent(OutputSeverity.Info, $"Scanning {root}"));

            foreach (var file in EnumerateFilesSafe(root))
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (!visited.Add(file) || !SuspiciousHeuristics.IsCandidateExtension(file))
                {
                    continue;
                }

                try
                {
                    var signature = _signatureVerifier.Verify(file);
                    var hiddenOrSystem = (File.GetAttributes(file) & (FileAttributes.Hidden | FileAttributes.System)) != 0;
                    var (score, reasons) = SuspiciousHeuristics.Score(
                        signature.Status,
                        _paths.IsUserWritableRiskLocation(file),
                        hiddenOrSystem,
                        SuspiciousHeuristics.HasDoubleExtension(Path.GetFileName(file)),
                        SuspiciousHeuristics.LooksRandom(Path.GetFileName(file)),
                        persistencePaths.Contains(file));

                    if (score < 4)
                    {
                        continue;
                    }

                    var info = new FileInfo(file);
                    results.Add(new SuspiciousFileRecord(
                        0,
                        SuspiciousHeuristics.CreateStableKey(file.ToLowerInvariant()),
                        score,
                        signature.Status,
                        signature.SignatureText,
                        info.Exists ? info.Length : 0,
                        info.Exists ? new DateTimeOffset(info.LastWriteTimeUtc) : DateTimeOffset.MinValue,
                        info.Exists ? _hashService.ComputeSha256(file) : "unavailable",
                        file,
                        reasons,
                        hiddenOrSystem,
                        persistencePaths.Contains(file)));

                    if (verbose)
                    {
                        progress?.Invoke(new ProgressEvent(OutputSeverity.Info, $"Flagged suspicious item: {file} (score {score})"));
                    }
                }
                catch (Exception ex)
                {
                    if (verbose)
                    {
                        progress?.Invoke(new ProgressEvent(OutputSeverity.Warn, $"Skipped {file}: {ex.Message}"));
                    }
                }
            }
        }

        var ordered = results
            .OrderByDescending(static item => item.Score)
            .ThenBy(static item => item.Path, StringComparer.OrdinalIgnoreCase)
            .ToList();

        for (var index = 0; index < ordered.Count; index++)
        {
            ordered[index] = ordered[index] with { Id = index + 1 };
        }

        return Task.FromResult<IReadOnlyList<SuspiciousFileRecord>>(ordered);
    }

    private static IEnumerable<string> EnumerateFilesSafe(string root)
    {
        if (File.Exists(root))
        {
            yield return Path.GetFullPath(root);
            yield break;
        }

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
                yield return Path.GetFullPath(file);
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

    private static void MoveFile(string source, string target)
    {
        try
        {
            File.Move(source, target);
        }
        catch
        {
            File.Copy(source, target, overwrite: false);
            File.Delete(source);
        }
    }

    private static string DeduplicateTargetPath(string target)
    {
        if (!File.Exists(target))
        {
            return target;
        }

        var directory = Path.GetDirectoryName(target)!;
        var fileNameWithoutExtension = Path.GetFileNameWithoutExtension(target);
        var extension = Path.GetExtension(target);
        var counter = 1;
        string candidate;
        do
        {
            candidate = Path.Combine(directory, $"{fileNameWithoutExtension}_{counter++}{extension}");
        }
        while (File.Exists(candidate));

        return candidate;
    }

    private void UpdateManifest(QuarantineRecord item)
    {
        var manifest = JsonSerializer.Deserialize<List<QuarantineManifestRow>>(File.ReadAllText(item.ManifestPath), JsonDefaults.Options) ?? [];
        manifest = manifest
            .Where(row => !string.Equals(row.QuarantinePath, item.QuarantinePath, StringComparison.OrdinalIgnoreCase))
            .ToList();

        if (manifest.Count == 0)
        {
            File.Delete(item.ManifestPath);
            var directory = Path.GetDirectoryName(item.ManifestPath);
            if (directory is not null && Directory.Exists(directory) && !Directory.EnumerateFileSystemEntries(directory).Any())
            {
                Directory.Delete(directory, false);
            }
            return;
        }

        File.WriteAllText(item.ManifestPath, JsonSerializer.Serialize(manifest, JsonDefaults.Options));
    }

    private sealed class QuarantineManifestRow
    {
        public string OriginalPath { get; set; } = string.Empty;
        public string QuarantinePath { get; set; } = string.Empty;
        public string Sha256 { get; set; } = string.Empty;
        public DateTimeOffset Timestamp { get; set; }
        public List<string> Reasons { get; set; } = [];
        public string SignatureStatus { get; set; } = string.Empty;
    }
}
