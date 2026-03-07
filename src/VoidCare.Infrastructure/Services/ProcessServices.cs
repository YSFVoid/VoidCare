using System.Diagnostics;
using System.Text;
using System.Text.Json;
using VoidCare.Core.Models;

namespace VoidCare.Infrastructure.Services;

public sealed record ProcessRunResult(int ExitCode, string StandardOutput, string StandardError, string? ErrorMessage = null)
{
    public bool Success => string.IsNullOrWhiteSpace(ErrorMessage) && ExitCode == 0;
}

public sealed class ProcessRunner
{
    public async Task<ProcessRunResult> RunAsync(
        string fileName,
        IEnumerable<string> arguments,
        Action<ProgressEvent>? onOutput = null,
        bool verbose = false,
        string? workingDirectory = null,
        CancellationToken cancellationToken = default)
    {
        try
        {
            var startInfo = new ProcessStartInfo(fileName)
            {
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true,
                WorkingDirectory = workingDirectory ?? Environment.CurrentDirectory,
            };

            foreach (var argument in arguments)
            {
                startInfo.ArgumentList.Add(argument);
            }

            if (verbose)
            {
                onOutput?.Invoke(new ProgressEvent(OutputSeverity.Info, $"EXEC {fileName} {string.Join(' ', startInfo.ArgumentList)}"));
            }

            using var process = new Process { StartInfo = startInfo, EnableRaisingEvents = true };
            var stdOut = new StringBuilder();
            var stdErr = new StringBuilder();
            var stdoutDone = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
            var stderrDone = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);

            process.OutputDataReceived += (_, args) =>
            {
                if (args.Data is null)
                {
                    stdoutDone.TrySetResult();
                    return;
                }

                stdOut.AppendLine(args.Data);
                onOutput?.Invoke(new ProgressEvent(OutputSeverity.Info, args.Data));
            };

            process.ErrorDataReceived += (_, args) =>
            {
                if (args.Data is null)
                {
                    stderrDone.TrySetResult();
                    return;
                }

                stdErr.AppendLine(args.Data);
                onOutput?.Invoke(new ProgressEvent(OutputSeverity.Warn, args.Data));
            };

            if (!process.Start())
            {
                return new ProcessRunResult(-1, string.Empty, string.Empty, $"Failed to start {fileName}.");
            }

            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            await process.WaitForExitAsync(cancellationToken);
            await Task.WhenAll(stdoutDone.Task, stderrDone.Task);

            return new ProcessRunResult(process.ExitCode, stdOut.ToString().Trim(), stdErr.ToString().Trim());
        }
        catch (Exception ex)
        {
            return new ProcessRunResult(-1, string.Empty, string.Empty, ex.Message);
        }
    }
}

public sealed class PowerShellRunner
{
    private readonly ProcessRunner _processRunner;

    public PowerShellRunner(ProcessRunner processRunner)
    {
        _processRunner = processRunner;
    }

    public Task<ProcessRunResult> RunScriptAsync(
        string script,
        Action<ProgressEvent>? onOutput = null,
        bool verbose = false,
        CancellationToken cancellationToken = default)
        => _processRunner.RunAsync(
            "powershell.exe",
            ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script],
            onOutput,
            verbose,
            cancellationToken: cancellationToken);

    public async Task<T?> RunJsonAsync<T>(
        string script,
        bool verbose = false,
        CancellationToken cancellationToken = default)
    {
        var result = await RunScriptAsync(script, verbose: verbose, cancellationToken: cancellationToken);
        if (!result.Success || string.IsNullOrWhiteSpace(result.StandardOutput))
        {
            return default;
        }

        return JsonSerializer.Deserialize<T>(result.StandardOutput, JsonDefaults.Options);
    }
}
