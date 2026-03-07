using System.Text.Json;
using VoidCare.Core.Models;

namespace VoidCare.Cli.Runtime;

public sealed class JsonEventWriter
{
    private readonly TextWriter _writer;

    public JsonEventWriter(TextWriter writer)
    {
        _writer = writer;
    }

    public void WriteProgress(ProgressEvent progress)
    {
        Write(new
        {
            type = "progress",
            timestamp = DateTimeOffset.UtcNow,
            level = progress.Severity.ToString().ToLowerInvariant(),
            message = progress.Message,
            percent = progress.Percent,
        });
    }

    public void WriteResult(CommandResult result)
    {
        Write(new
        {
            type = result.Success ? "result" : "error",
            success = result.Success,
            exitCode = result.ExitCode,
            command = result.Command,
            message = result.Summary,
            data = result.Data,
            notices = result.Notices.Select(static notice => new { level = notice.Severity.ToString().ToLowerInvariant(), text = notice.Text }),
            plannedActions = result.PlannedActions,
        });
    }

    private void Write(object payload)
    {
        _writer.WriteLine(JsonSerializer.Serialize(payload, JsonDefaults.CompactOptions));
        _writer.Flush();
    }
}
