using VoidCare.Core.Models;

namespace VoidCare.Cli.Runtime;

public static class InvocationParser
{
    private static readonly HashSet<string> GlobalFlags = new(StringComparer.OrdinalIgnoreCase)
    {
        "--json",
        "--quiet",
        "--verbose",
        "--yes",
        "--dry-run",
        "--no-banner",
    };

    public static ParsedInvocation Parse(string[] args)
    {
        var remaining = new List<string>();
        var options = new CommandOptions();

        foreach (var arg in args)
        {
            switch (arg)
            {
                case "--json":
                    options = options with { Json = true };
                    break;
                case "--quiet":
                    options = options with { Quiet = true };
                    break;
                case "--verbose":
                    options = options with { Verbose = true };
                    break;
                case "--yes":
                    options = options with { Yes = true };
                    break;
                case "--dry-run":
                    options = options with { DryRun = true };
                    break;
                case "--no-banner":
                    options = options with { NoBanner = true };
                    break;
                default:
                    remaining.Add(arg);
                    break;
            }
        }

        var launchInteractive = remaining.Count == 0
            || remaining.Count == 1 && (remaining[0].Equals("menu", StringComparison.OrdinalIgnoreCase)
                                         || remaining[0].Equals("interactive", StringComparison.OrdinalIgnoreCase));

        return new ParsedInvocation(options, remaining, launchInteractive);
    }

    public static IReadOnlyList<int> ParseIds(string csv)
    {
        return csv
            .Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
            .Select(static part => int.TryParse(part, out var value) ? value : -1)
            .Where(static value => value > 0)
            .Distinct()
            .Order()
            .ToArray();
    }
}
