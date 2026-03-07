using VoidCare.Core.Models;
using VoidCare.Core.Services;

namespace VoidCare.Tests.Unit;

public sealed class SuspiciousHeuristicsTests
{
    [Fact]
    public void DetectsDoubleExtension()
    {
        Assert.True(SuspiciousHeuristics.HasDoubleExtension("invoice.pdf.exe"));
        Assert.False(SuspiciousHeuristics.HasDoubleExtension("normal.exe"));
    }

    [Fact]
    public void DetectsRandomLookingNames()
    {
        Assert.True(SuspiciousHeuristics.LooksRandom("a1b2c3d4e5f6g7.exe"));
        Assert.False(SuspiciousHeuristics.LooksRandom("notepad.exe"));
    }

    [Fact]
    public void ScoresUsingConfiguredWeights()
    {
        var (score, reasons) = SuspiciousHeuristics.Score(SignatureStatus.Unsigned, true, true, true, false, true);

        Assert.Equal(10, score);
        Assert.True(reasons.Count >= 3);
    }
}
