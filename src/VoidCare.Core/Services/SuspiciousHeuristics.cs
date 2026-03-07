using System.Security.Cryptography;
using System.Text.RegularExpressions;
using VoidCare.Core.Models;

namespace VoidCare.Core.Services;

public static class SuspiciousHeuristics
{
    private static readonly HashSet<string> SuspiciousExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".exe", ".dll", ".scr", ".com", ".bat", ".cmd", ".ps1", ".vbs", ".js", ".jar", ".lnk",
    };

    private static readonly HashSet<string> LikelyDocumentExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".txt", ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".jpg", ".png", ".zip",
    };

    public static IReadOnlySet<string> ExecutableExtensions => SuspiciousExtensions;

    public static bool IsCandidateExtension(string path)
        => SuspiciousExtensions.Contains(Path.GetExtension(path));

    public static bool HasDoubleExtension(string fileName)
    {
        var parts = fileName.Split('.', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        if (parts.Length < 3)
        {
            return false;
        }

        var last = "." + parts[^1];
        var previous = "." + parts[^2];
        return SuspiciousExtensions.Contains(last) && LikelyDocumentExtensions.Contains(previous);
    }

    public static bool LooksRandom(string fileName)
    {
        var name = Path.GetFileNameWithoutExtension(fileName);
        if (name.Length < 8)
        {
            return false;
        }

        if (Regex.IsMatch(name, @"^[a-z0-9]{10,}$", RegexOptions.IgnoreCase))
        {
            return true;
        }

        var counts = name.GroupBy(static c => char.ToLowerInvariant(c))
            .ToDictionary(static g => g.Key, static g => g.Count());

        var length = (double)name.Length;
        var entropy = 0d;
        foreach (var count in counts.Values)
        {
            var probability = count / length;
            entropy -= probability * Math.Log2(probability);
        }

        return entropy >= 3.8d;
    }

    public static (int Score, IReadOnlyList<string> Reasons) Score(
        SignatureStatus signature,
        bool userWritableRisk,
        bool hiddenOrSystem,
        bool doubleExtension,
        bool randomName,
        bool persistenceLinked)
    {
        var reasons = new List<string>();
        var score = 0;

        if (userWritableRisk && (signature == SignatureStatus.Invalid || signature == SignatureStatus.Unsigned))
        {
            score += 3;
            reasons.Add("Unsigned or invalid signature in user-writable location");
        }

        if (hiddenOrSystem)
        {
            score += 2;
            reasons.Add("Hidden/System attribute set unexpectedly");
        }

        if (doubleExtension)
        {
            score += 2;
            reasons.Add("Double extension naming pattern");
        }

        if (randomName)
        {
            score += 2;
            reasons.Add("Random-looking filename pattern");
        }

        if (persistenceLinked)
        {
            score += 3;
            reasons.Add("Referenced by persistence entry");
        }

        return (score, reasons);
    }

    public static string CreateStableKey(string value)
    {
        var bytes = SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(value));
        return Convert.ToHexString(bytes).ToLowerInvariant();
    }
}
