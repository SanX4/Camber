using System.Security.Cryptography;
using System.Text;

namespace Camber.Core;

public sealed class PairingService(TimeProvider? clock = null)
{
    private readonly object gate = new();
    private readonly TimeProvider time = clock ?? TimeProvider.System;
    private byte[]? pendingHash;
    private DateTimeOffset expires;
    private int failures;
    private DateTimeOffset window;

    public PairingPayload Create(string pcId, string name, string endpoint, string fingerprint)
    {
        if (!Uri.TryCreate(endpoint, UriKind.Absolute, out var uri) || uri.Scheme != "https" ||
            uri.UserInfo.Length != 0 || uri.AbsolutePath != "/") throw new ArgumentException("A local HTTPS origin is required.");
        if (fingerprint.Length != 64 || !fingerprint.All(Uri.IsHexDigit)) throw new ArgumentException("Invalid certificate fingerprint.");
        lock (gate)
        {
            var secret = Convert.ToHexString(RandomNumberGenerator.GetBytes(32));
            pendingHash = SHA256.HashData(Encoding.UTF8.GetBytes(secret));
            expires = time.GetUtcNow().AddMinutes(2);
            return new(Protocol.Version, pcId, name, endpoint, fingerprint, secret, expires);
        }
    }

    public bool Consume(string secret, int version)
    {
        lock (gate)
        {
            var now = time.GetUtcNow();
            if (now - window >= TimeSpan.FromMinutes(1)) { failures = 0; window = now; }
            if (failures >= 5) return false;
            if (version != Protocol.Version || secret.Length != 64 || pendingHash is null || now >= expires)
            { failures++; return false; }
            var hash = SHA256.HashData(Encoding.UTF8.GetBytes(secret));
            if (!CryptographicOperations.FixedTimeEquals(hash, pendingHash)) { failures++; return false; }
            CryptographicOperations.ZeroMemory(pendingHash);
            pendingHash = null;
            return true;
        }
    }
}
