using Spectre.Console;
using VoidCare.Cli.Runtime;
using VoidCare.Core.Models;

namespace VoidCare.Cli.Rendering;

public sealed class TerminalRenderer : IUserPrompt
{
    public void RenderBanner()
    {
        var banner = new FigletText("VoidCare").Color(Color.Cyan1);
        AnsiConsole.Write(banner);
        AnsiConsole.MarkupLine($"[grey]{AppMetadata.Subtitle}[/]");
        AnsiConsole.MarkupLine($"[grey]{AppMetadata.Credits}[/]");
        AnsiConsole.WriteLine();
    }

    public async Task<T> RunWithStatusAsync<T>(string status, Func<Task<T>> action)
    {
        return await AnsiConsole.Status()
            .Spinner(Spinner.Known.Star)
            .SpinnerStyle(Style.Parse("cyan"))
            .StartAsync(status, async _ => await action());
    }

    public void Render(CommandResult result, CommandOptions options)
    {
        if (result.ShowBanner && !options.NoBanner)
        {
            RenderBanner();
        }

        foreach (var line in result.Lines)
        {
            AnsiConsole.MarkupLine(Escape(line));
        }

        foreach (var table in result.Tables)
        {
            if (!string.IsNullOrWhiteSpace(table.Title))
            {
                AnsiConsole.Write(new Rule($"[grey]{Escape(table.Title!)}[/]").LeftJustified());
            }

            var rendered = new Table().Border(TableBorder.Rounded).Expand();
            foreach (var column in table.Columns)
            {
                rendered.AddColumn(new TableColumn(Escape(column)));
            }

            foreach (var row in table.Rows)
            {
                rendered.AddRow(row.Select(Escape).ToArray());
            }

            AnsiConsole.Write(rendered);
        }

        if (!options.Quiet)
        {
            foreach (var notice in result.Notices)
            {
                var style = notice.Severity switch
                {
                    OutputSeverity.Ok => "black on green",
                    OutputSeverity.Warn => "black on yellow",
                    OutputSeverity.Fail => "white on red",
                    _ => "black on deepskyblue1",
                };
                var label = notice.Severity switch
                {
                    OutputSeverity.Ok => "[OK]",
                    OutputSeverity.Warn => "[WARN]",
                    OutputSeverity.Fail => "[FAIL]",
                    _ => "[INFO]",
                };
                AnsiConsole.MarkupLine($"[{style}]{Escape(label)}[/] {Escape(notice.Text)}");
            }
        }

        if (!string.IsNullOrWhiteSpace(result.Summary))
        {
            var style = result.Success ? "black on green" : "white on red";
            var label = result.Success ? "[OK]" : "[FAIL]";
            AnsiConsole.MarkupLine($"[{style}]{Escape(label)}[/] {Escape(result.Summary)}");
        }
    }

    public bool Confirm(string prompt, CommandOptions options, bool strong = false)
    {
        if (options.Yes)
        {
            return true;
        }

        if (options.Json)
        {
            return false;
        }

        var fullPrompt = strong ? $"{prompt} This is destructive." : prompt;
        while (true)
        {
            Console.Write($"{fullPrompt} [y/n] (n): ");
            var input = Console.ReadLine()?.Trim();
            if (string.IsNullOrWhiteSpace(input))
            {
                Console.WriteLine();
                return false;
            }

            var normalized = input.ToLowerInvariant();
            Console.WriteLine();
            if (normalized is "y" or "yes")
            {
                return true;
            }

            if (normalized is "n" or "no")
            {
                return false;
            }

            AnsiConsole.MarkupLine("[black on yellow][[WARN]][/] Enter y/yes or n/no.");
        }
    }

    public string AskText(string prompt, string? defaultValue = null)
    {
        return defaultValue is null
            ? AnsiConsole.Ask<string>(prompt)
            : AnsiConsole.Ask(prompt, defaultValue);
    }

    public int AskInt(string prompt, int defaultValue, int minValue = 0, int maxValue = int.MaxValue)
    {
        while (true)
        {
            var value = AskText(prompt, defaultValue.ToString());
            if (int.TryParse(value, out var parsed) && parsed >= minValue && parsed <= maxValue)
            {
                return parsed;
            }

            AnsiConsole.MarkupLine("[black on yellow][[WARN]][/] Invalid number.");
        }
    }

    public void RenderProgress(ProgressEvent progress, bool verbose)
    {
        if (!verbose && progress.Severity == OutputSeverity.Info)
        {
            return;
        }

        var label = progress.Severity switch
        {
            OutputSeverity.Ok => $"[black on green]{Escape("[OK]")}[/]",
            OutputSeverity.Warn => $"[black on yellow]{Escape("[WARN]")}[/]",
            OutputSeverity.Fail => $"[white on red]{Escape("[FAIL]")}[/]",
            _ => $"[black on deepskyblue1]{Escape("[INFO]")}[/]",
        };
        AnsiConsole.MarkupLine($"{label} {Escape(progress.Message)}");
    }

    private static string Escape(string text) => Markup.Escape(text);
}
