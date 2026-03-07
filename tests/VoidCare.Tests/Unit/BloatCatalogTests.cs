using VoidCare.Core.Services;

namespace VoidCare.Tests.Unit;

public sealed class BloatCatalogTests
{
    [Fact]
    public void LoadsEmbeddedCatalog()
    {
        var catalog = new BloatCatalogService().Load();

        Assert.NotEmpty(catalog);
        Assert.Contains(catalog, item => item.Key == "appx-solitaire");
    }
}
