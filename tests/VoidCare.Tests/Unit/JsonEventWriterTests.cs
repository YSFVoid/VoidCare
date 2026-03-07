using System.Text.Json;
using VoidCare.Cli.Runtime;

namespace VoidCare.Tests.Unit;

public sealed class JsonEventWriterTests
{
    [Fact]
    public void WritesResultEnvelopeAsJsonLine()
    {
        using var writer = new StringWriter();
        var jsonWriter = new JsonEventWriter(writer);

        jsonWriter.WriteResult(new CommandResult
        {
            Command = "status",
            Summary = "ok",
            Success = true,
            ExitCode = 0,
            Data = new { value = 1 },
        });

        var line = writer.ToString().Trim();
        using var doc = JsonDocument.Parse(line);
        Assert.Equal("result", doc.RootElement.GetProperty("type").GetString());
        Assert.Equal("status", doc.RootElement.GetProperty("command").GetString());
    }
}
