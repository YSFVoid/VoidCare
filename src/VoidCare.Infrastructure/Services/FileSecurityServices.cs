using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using VoidCare.Core.Models;
using VoidCare.Infrastructure.Interop;

namespace VoidCare.Infrastructure.Services;

public sealed record SignatureInfo(SignatureStatus Status, string SignatureText, string? Publisher);

public sealed class HashService
{
    public string ComputeSha256(string path)
    {
        using var stream = File.OpenRead(path);
        var hash = SHA256.HashData(stream);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }
}

public sealed class FileSignatureVerifier
{
    public SignatureInfo Verify(string path)
    {
        try
        {
            if (!File.Exists(path))
            {
                return new SignatureInfo(SignatureStatus.Unknown, SignatureStatus.Unknown.ToString(), null);
            }

            var trustResult = WinTrustNative.VerifyEmbeddedSignature(path);
            var status = trustResult switch
            {
                WinTrustNative.ErrorSuccess => SignatureStatus.Valid,
                WinTrustNative.TrustENoSignature or
                WinTrustNative.TrustESubjectFormUnknown or
                WinTrustNative.TrustEProviderUnknown or
                WinTrustNative.CryptEFileError => SignatureStatus.Unsigned,
                _ => SignatureStatus.Invalid,
            };

            return new SignatureInfo(status, status.ToString(), TryGetPublisher(path));
        }
        catch
        {
            return new SignatureInfo(SignatureStatus.Unknown, SignatureStatus.Unknown.ToString(), null);
        }
    }

    private static string? TryGetPublisher(string path)
    {
        try
        {
            var certificate = X509Certificate.CreateFromSignedFile(path);
            var x509 = new X509Certificate2(certificate);
            return x509.GetNameInfo(X509NameType.SimpleName, false);
        }
        catch
        {
            return null;
        }
    }
}
