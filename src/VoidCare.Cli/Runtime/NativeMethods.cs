using System.Runtime.InteropServices;

namespace VoidCare.Cli.Runtime;

internal static class NativeMethods
{
    [DllImport("kernel32.dll")]
    internal static extern uint GetConsoleProcessList([Out] uint[] processList, int processCount);
}
