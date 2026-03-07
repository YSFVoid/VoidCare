using System.Runtime.InteropServices;

namespace VoidCare.Infrastructure.Interop;

internal static class WinTrustNative
{
    internal const uint ErrorSuccess = 0;
    internal const uint TrustENoSignature = 0x800B0100;
    internal const uint TrustESubjectFormUnknown = 0x800B0003;
    internal const uint TrustEProviderUnknown = 0x800B0001;
    internal const uint CryptEFileError = 0x80092003;

    private static readonly Guid ActionGenericVerifyV2 = new("00AAC56B-CD44-11d0-8CC2-00C04FC295EE");

    [DllImport("wintrust.dll", ExactSpelling = true, PreserveSig = true, SetLastError = true)]
    private static extern uint WinVerifyTrust(
        IntPtr hwnd,
        [MarshalAs(UnmanagedType.LPStruct)] Guid actionId,
        IntPtr data);

    internal static uint VerifyEmbeddedSignature(string filePath)
    {
        var fileInfo = new WinTrustFileInfo(filePath);
        var fileInfoPointer = IntPtr.Zero;
        var dataPointer = IntPtr.Zero;

        try
        {
            fileInfoPointer = Marshal.AllocHGlobal(Marshal.SizeOf<WinTrustFileInfo>());
            Marshal.StructureToPtr(fileInfo, fileInfoPointer, false);

            var trustData = new WinTrustData(fileInfoPointer);
            dataPointer = Marshal.AllocHGlobal(Marshal.SizeOf<WinTrustData>());
            Marshal.StructureToPtr(trustData, dataPointer, false);

            return WinVerifyTrust(IntPtr.Zero, ActionGenericVerifyV2, dataPointer);
        }
        finally
        {
            if (fileInfo.FilePathPointer != IntPtr.Zero)
            {
                Marshal.FreeCoTaskMem(fileInfo.FilePathPointer);
            }

            if (dataPointer != IntPtr.Zero)
            {
                Marshal.DestroyStructure<WinTrustData>(dataPointer);
                Marshal.FreeHGlobal(dataPointer);
            }

            if (fileInfoPointer != IntPtr.Zero)
            {
                Marshal.DestroyStructure<WinTrustFileInfo>(fileInfoPointer);
                Marshal.FreeHGlobal(fileInfoPointer);
            }
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct WinTrustFileInfo
    {
        public uint StructSize;
        public IntPtr FilePathPointer;
        public IntPtr FileHandle;
        public IntPtr KnownSubject;

        public WinTrustFileInfo(string filePath)
        {
            StructSize = (uint)Marshal.SizeOf<WinTrustFileInfo>();
            FilePathPointer = Marshal.StringToCoTaskMemUni(filePath);
            FileHandle = IntPtr.Zero;
            KnownSubject = IntPtr.Zero;
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct WinTrustData
    {
        public uint StructSize;
        public IntPtr PolicyCallbackData;
        public IntPtr SIPClientData;
        public WinTrustDataUIChoice UIChoice;
        public WinTrustDataRevocationChecks RevocationChecks;
        public WinTrustDataChoice UnionChoice;
        public IntPtr FileInfoPointer;
        public WinTrustDataStateAction StateAction;
        public IntPtr StateData;
        public string? URLReference;
        public WinTrustDataProvFlags ProvFlags;
        public WinTrustDataUIContext UIContext;

        public WinTrustData(IntPtr fileInfoPointer)
        {
            StructSize = (uint)Marshal.SizeOf<WinTrustData>();
            PolicyCallbackData = IntPtr.Zero;
            SIPClientData = IntPtr.Zero;
            UIChoice = WinTrustDataUIChoice.None;
            RevocationChecks = WinTrustDataRevocationChecks.None;
            UnionChoice = WinTrustDataChoice.File;
            FileInfoPointer = fileInfoPointer;
            StateAction = WinTrustDataStateAction.Ignore;
            StateData = IntPtr.Zero;
            URLReference = null;
            ProvFlags = WinTrustDataProvFlags.RevocationCheckNone;
            UIContext = WinTrustDataUIContext.Execute;
        }
    }

    private enum WinTrustDataUIChoice : uint
    {
        All = 1,
        None = 2,
        NoBad = 3,
        NoGood = 4,
    }

    private enum WinTrustDataRevocationChecks : uint
    {
        None = 0,
        WholeChain = 1,
    }

    private enum WinTrustDataChoice : uint
    {
        File = 1,
        Catalog = 2,
        Blob = 3,
        Signer = 4,
        Certificate = 5,
    }

    private enum WinTrustDataStateAction : uint
    {
        Ignore = 0,
        Verify = 1,
        Close = 2,
        AutoCache = 3,
        AutoCacheFlush = 4,
    }

    [Flags]
    private enum WinTrustDataProvFlags : uint
    {
        UseIe4TrustFlag = 0x1,
        NoIe4ChainFlag = 0x2,
        NoPolicyUsageFlag = 0x4,
        RevocationCheckNone = 0x10,
        RevocationCheckEndCert = 0x20,
        RevocationCheckChain = 0x40,
        RevocationCheckChainExcludeRoot = 0x80,
        SaferFlag = 0x100,
        HashOnlyFlag = 0x200,
        UseDefaultOsverCheck = 0x400,
        LifetimeSigningFlag = 0x800,
        CacheOnlyUrlRetrieval = 0x1000,
    }

    private enum WinTrustDataUIContext : uint
    {
        Execute = 0,
        Install = 1,
    }
}
