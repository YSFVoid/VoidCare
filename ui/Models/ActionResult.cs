namespace VoidCare.Wpf.Models;

public sealed class ActionResult
{
    public bool Success { get; set; }
    public string Message { get; set; } = string.Empty;
    public int ExitCode { get; set; }
    public bool NeedsRestoreOverride { get; set; }
    public string RestoreDetail { get; set; } = string.Empty;
}
