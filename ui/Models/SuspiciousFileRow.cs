namespace VoidCare.Wpf.Models;

public sealed class SuspiciousFileRow : RowModelBase
{
    public string Path { get; set; } = string.Empty;
    public long Size { get; set; }
    public string Created { get; set; } = string.Empty;
    public string Modified { get; set; } = string.Empty;
    public string Sha256 { get; set; } = string.Empty;
    public string SignatureStatus { get; set; } = string.Empty;
    public int Score { get; set; }
    public string Reasons { get; set; } = string.Empty;
    public string PersistenceRef { get; set; } = string.Empty;
}
