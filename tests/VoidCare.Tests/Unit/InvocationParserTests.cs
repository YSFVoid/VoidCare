using VoidCare.Cli.Runtime;

namespace VoidCare.Tests.Unit;

public sealed class InvocationParserTests
{
    [Fact]
    public void ParsesGlobalFlagsAnywhere()
    {
        var parsed = InvocationParser.Parse(["security", "--json", "scan", "--quick", "--yes", "--dry-run", "--verbose"]);

        Assert.True(parsed.Options.Json);
        Assert.True(parsed.Options.Yes);
        Assert.True(parsed.Options.DryRun);
        Assert.True(parsed.Options.Verbose);
        Assert.Equal(["security", "scan", "--quick"], parsed.Arguments);
    }

    [Fact]
    public void LaunchesInteractiveWhenNoArgs()
    {
        var parsed = InvocationParser.Parse([]);
        Assert.True(parsed.LaunchInteractive);
    }

    [Fact]
    public void ParsesIdsAndDeduplicates()
    {
        var ids = InvocationParser.ParseIds("3,1,3,2");
        Assert.Equal([1, 2, 3], ids);
    }
}
