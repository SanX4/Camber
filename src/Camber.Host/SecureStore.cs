using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text.Json;
using Camber.Core;

namespace Camber.Host;

public sealed class SecureStore
{
#if CAMBER_ISOLATED_TEST_HOST
    private readonly string root = TestDataPaths.Root;
#else
    private readonly string root = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Camber");
#endif
    private readonly object gate = new();
    public string PcId { get; }
    private Dictionary<string, StoredDevice> devices;
    public SecureStore()
    {
        Directory.CreateDirectory(root);
        PcId = Read<string>("identity") ?? Guid.NewGuid().ToString("N");
        Write("identity", PcId);
        devices = Read<Dictionary<string, StoredDevice>>("devices") ?? [];
    }
    public X509Certificate2 Certificate()
    {
        var pfx = Read<byte[]>("certificate");
        // Schannel's server credential acquisition requires a Windows key container.
        if (pfx is not null) return X509CertificateLoader.LoadPkcs12(pfx, null, X509KeyStorageFlags.UserKeySet);
        using var rsa = RSA.Create(3072);
        var request = new CertificateRequest("CN=Camber Local Camera", rsa, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
        request.CertificateExtensions.Add(new X509KeyUsageExtension(X509KeyUsageFlags.DigitalSignature | X509KeyUsageFlags.KeyEncipherment, true));
        using var certificate = request.CreateSelfSigned(DateTimeOffset.UtcNow.AddDays(-1), DateTimeOffset.UtcNow.AddYears(2));
        pfx = certificate.Export(X509ContentType.Pfx);
        Write("certificate", pfx);
        return X509CertificateLoader.LoadPkcs12(pfx, null, X509KeyStorageFlags.UserKeySet);
    }
    public (Device Device, string Token) Add(string name, string platform)
    {
        lock (gate)
        {
            var device = new Device(Guid.NewGuid().ToString("N"), name, platform, DateTimeOffset.UtcNow);
            var token = Convert.ToHexString(RandomNumberGenerator.GetBytes(32));
            devices[device.Id] = new(device, Convert.ToHexString(SHA256.HashData(Convert.FromHexString(token))));
            Write("devices", devices);
            return (device, token);
        }
    }
    public bool Authenticate(string id, string token)
    {
        lock (gate)
        {
            if (!devices.TryGetValue(id, out var entry) || token.Length != 64 || !token.All(Uri.IsHexDigit)) return false;
            return CryptographicOperations.FixedTimeEquals(Convert.FromHexString(entry.TokenHash), SHA256.HashData(Convert.FromHexString(token)));
        }
    }
    public Device[] Devices { get { lock (gate) return devices.Values.Select(x => x.Device).ToArray(); } }
    public void Revoke(string id) { lock (gate) { devices.Remove(id); Write("devices", devices); } }
    private T? Read<T>(string name)
    {
        var path = Path.Combine(root, name + ".bin");
        if (!File.Exists(path)) return default;
        var bytes = ProtectedData.Unprotect(File.ReadAllBytes(path), null, DataProtectionScope.CurrentUser);
        try { return JsonSerializer.Deserialize<T>(bytes); } finally { CryptographicOperations.ZeroMemory(bytes); }
    }
    private void Write<T>(string name, T data)
    {
        var bytes = JsonSerializer.SerializeToUtf8Bytes(data);
        var path = Path.Combine(root, name + ".bin");
        try { File.WriteAllBytes(path + ".tmp", ProtectedData.Protect(bytes, null, DataProtectionScope.CurrentUser)); File.Move(path + ".tmp", path, true); }
        finally { CryptographicOperations.ZeroMemory(bytes); }
    }
    private sealed record StoredDevice(Device Device, string TokenHash);
}
