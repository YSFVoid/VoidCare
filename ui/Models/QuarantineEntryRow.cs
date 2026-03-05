namespace VoidCare.Wpf.Models;

public sealed class QuarantineEntryRow : RowModelBase
{
    public string OriginalPath { get; set; } = string.Empty;
    public string QuarantinePath { get; set; } = string.Empty;
    public string Sha256 { get; set; } = string.Empty;
    public string Timestamp { get; set; } = string.Empty;
    public string SignatureStatus { get; set; } = string.Empty;
    public string Reasons { get; set; } = string.Empty;
}
