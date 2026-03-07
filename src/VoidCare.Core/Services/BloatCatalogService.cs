using System.Reflection;
using System.Text.Json;
using VoidCare.Core.Models;

namespace VoidCare.Core.Services;

public sealed class BloatCatalogService
{
    private const string ResourceName = "VoidCare.Core.Data.BloatCatalog.json";
    private IReadOnlyList<BloatCatalogEntry>? _cache;

    public IReadOnlyList<BloatCatalogEntry> Load()
    {
        if (_cache is not null)
        {
            return _cache;
        }

        using var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(ResourceName)
            ?? throw new InvalidOperationException($"Missing embedded resource: {ResourceName}");
        using var reader = new StreamReader(stream);
        var json = reader.ReadToEnd();
        _cache = JsonSerializer.Deserialize<List<BloatCatalogEntry>>(json, JsonDefaults.Options)
            ?? [];
        return _cache;
    }
}
