using VoidCare.Core.Models;

namespace VoidCare.Cli.Runtime;

public sealed record ResultNotice(OutputSeverity Severity, string Text);

public sealed record ResultTable(string? Title, IReadOnlyList<string> Columns, IReadOnlyList<IReadOnlyList<string>> Rows);

public sealed class CommandResult
{
    public bool Success { get; set; } = true;
    public int ExitCode { get; set; }
    public string Command { get; set; } = string.Empty;
    public string Summary { get; set; } = string.Empty;
    public bool ShowBanner { get; set; }
    public List<string> Lines { get; } = [];
    public List<ResultTable> Tables { get; } = [];
    public List<ResultNotice> Notices { get; } = [];
    public List<string> PlannedActions { get; } = [];
    public object? Data { get; set; }
}

public sealed record ParsedInvocation(CommandOptions Options, IReadOnlyList<string> Arguments, bool LaunchInteractive);

public interface IUserPrompt
{
    bool Confirm(string prompt, CommandOptions options, bool strong = false);
    string AskText(string prompt, string? defaultValue = null);
    int AskInt(string prompt, int defaultValue, int minValue = 0, int maxValue = int.MaxValue);
}
