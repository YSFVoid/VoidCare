namespace VoidCare.Wpf.Models;

public sealed class PersistenceEntryRow : RowModelBase
{
    public string Id { get; set; } = string.Empty;
    public string SourceType { get; set; } = string.Empty;
    public string Name { get; set; } = string.Empty;
    public string Path { get; set; } = string.Empty;
    public string Args { get; set; } = string.Empty;
    public string Publisher { get; set; } = string.Empty;
    public string SignatureStatus { get; set; } = string.Empty;
    public bool Enabled { get; set; }
}
