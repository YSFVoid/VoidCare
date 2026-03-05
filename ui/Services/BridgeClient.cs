using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace VoidCare.Wpf.Services;

public sealed class BridgeClient : IBridgeClient
{
    private readonly object _writeLock = new();
    private readonly ConcurrentDictionary<string, TaskCompletionSource<BridgeResponse>> _pending = new();
    private readonly JsonSerializerOptions _jsonOptions = new(JsonSerializerDefaults.Web);

    private Process? _process;
    private StreamWriter? _writer;
    private CancellationTokenSource? _readCts;
    private Task? _stdoutTask;
    private Task? _stderrTask;

    public event EventHandler<BridgeEvent>? EventReceived;

    public bool IsConnected => _process is { HasExited: false };

    public async Task ConnectAsync(CancellationToken cancellationToken = default)
    {
        if (IsConnected)
        {
            return;
        }

        await DisconnectAsync().ConfigureAwait(false);

        var bridgePath = Path.Combine(AppContext.BaseDirectory, "VoidCare.Bridge.exe");
        if (!File.Exists(bridgePath))
        {
            throw new FileNotFoundException("VoidCare.Bridge.exe was not found.", bridgePath);
        }

        var startInfo = new ProcessStartInfo
        {
            FileName = bridgePath,
            CreateNoWindow = true,
            UseShellExecute = false,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            WorkingDirectory = AppContext.BaseDirectory,
        };

        var process = new Process
        {
            StartInfo = startInfo,
            EnableRaisingEvents = true,
        };

        process.Exited += (_, _) =>
        {
            FailPending("Bridge process exited.");
        };

        if (!process.Start())
        {
            throw new InvalidOperationException("Failed to start VoidCare.Bridge.exe.");
        }

        _process = process;
        _writer = process.StandardInput;
        _writer.AutoFlush = true;

        _readCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        _stdoutTask = Task.Run(() => ReadStdOutLoopAsync(process.StandardOutput, _readCts.Token), _readCts.Token);
        _stderrTask = Task.Run(() => ReadStdErrLoopAsync(process.StandardError, _readCts.Token), _readCts.Token);
    }

    public async Task DisconnectAsync()
    {
        try
        {
            _readCts?.Cancel();
        }
        catch
        {
        }

        if (_process is { HasExited: false })
        {
            try
            {
                _process.Kill(true);
            }
            catch
            {
            }
        }

        if (_stdoutTask is not null)
        {
            try
            {
                await _stdoutTask.ConfigureAwait(false);
            }
            catch
            {
            }
        }

        if (_stderrTask is not null)
        {
            try
            {
                await _stderrTask.ConfigureAwait(false);
            }
            catch
            {
            }
        }

        _stdoutTask = null;
        _stderrTask = null;

        _writer?.Dispose();
        _writer = null;

        _process?.Dispose();
        _process = null;

        _readCts?.Dispose();
        _readCts = null;

        FailPending("Bridge disconnected.");
    }

    public async Task<BridgeResponse> SendRequestAsync(string method, JsonObject? args = null, CancellationToken cancellationToken = default)
    {
        await ConnectAsync(cancellationToken).ConfigureAwait(false);

        if (_writer is null)
        {
            throw new InvalidOperationException("Bridge writer is not available.");
        }

        var requestId = Guid.NewGuid().ToString("N");
        var request = new BridgeRequest
        {
            Id = requestId,
            Method = method,
            Args = args ?? new JsonObject(),
        };

        var tcs = new TaskCompletionSource<BridgeResponse>(TaskCreationOptions.RunContinuationsAsynchronously);
        if (!_pending.TryAdd(requestId, tcs))
        {
            throw new InvalidOperationException("Failed to register bridge request.");
        }

        var payload = JsonSerializer.Serialize(request, _jsonOptions);
        lock (_writeLock)
        {
            _writer.WriteLine(payload);
            _writer.Flush();
        }

        using var registration = cancellationToken.Register(() =>
        {
            if (_pending.TryRemove(requestId, out var pending))
            {
                pending.TrySetCanceled(cancellationToken);
            }
        });

        return await tcs.Task.ConfigureAwait(false);
    }

    public async ValueTask DisposeAsync()
    {
        await DisconnectAsync().ConfigureAwait(false);
    }

    private async Task ReadStdOutLoopAsync(StreamReader reader, CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            var line = await reader.ReadLineAsync(cancellationToken).ConfigureAwait(false);
            if (line is null)
            {
                break;
            }

            ProcessBridgeLine(line);
        }
    }

    private async Task ReadStdErrLoopAsync(StreamReader reader, CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            var line = await reader.ReadLineAsync(cancellationToken).ConfigureAwait(false);
            if (line is null)
            {
                break;
            }

            EventReceived?.Invoke(this, new BridgeEvent
            {
                Event = "log",
                Payload = new JsonObject
                {
                    ["line"] = line,
                    ["isError"] = true,
                },
            });
        }
    }

    private void ProcessBridgeLine(string line)
    {
        JsonNode? node;
        try
        {
            node = JsonNode.Parse(line);
        }
        catch
        {
            return;
        }

        if (node is not JsonObject obj)
        {
            return;
        }

        var type = obj["type"]?.GetValue<string>();
        if (string.Equals(type, "response", StringComparison.OrdinalIgnoreCase))
        {
            var response = new BridgeResponse
            {
                Id = obj["id"]?.GetValue<string>() ?? string.Empty,
                Ok = obj["ok"]?.GetValue<bool>() ?? false,
                Result = obj["result"],
                Error = obj["error"] as JsonObject,
            };

            if (_pending.TryRemove(response.Id, out var pending))
            {
                pending.TrySetResult(response);
            }
            return;
        }

        if (string.Equals(type, "event", StringComparison.OrdinalIgnoreCase))
        {
            EventReceived?.Invoke(this, new BridgeEvent
            {
                Event = obj["event"]?.GetValue<string>() ?? string.Empty,
                Payload = obj["payload"] as JsonObject ?? new JsonObject(),
            });
        }
    }

    private void FailPending(string message)
    {
        foreach (var item in _pending)
        {
            if (_pending.TryRemove(item.Key, out var pending))
            {
                pending.TrySetException(new IOException(message));
            }
        }
    }
}

