namespace Camber.Core;

public static class Protocol
{
    public const int Version = 1;
    public const int MaxMessageBytes = 65_536;
    public const int Port = 47771;
}

public enum SessionPhase { Unpaired, AwaitingApproval, Ready, Streaming, Paused, Reconnecting, Interrupted, Stopped, Error }
public enum CameraFacing { Back, Front }
public enum Framing { Fit, Fill }
public sealed record VideoMode(int Width, int Height, int Fps)
{
    public static readonly VideoMode HD = new(1280, 720, 30);
    public static readonly VideoMode FullHD = new(1920, 1080, 30);
    public bool IsLaunchMode => this == HD || this == FullHD;
}
public sealed record CameraCapability(string Id, CameraFacing Facing, VideoMode[] Modes,
    bool Torch, bool FocusLock, bool ExposureLock, double MinZoom, double MaxZoom);
public sealed record Configuration(CameraFacing Camera, VideoMode Capture, VideoMode Output,
    Framing Framing = Framing.Fit, int Rotation = 0, bool OutputMirror = false);
public sealed record Device(string Id, string Name, string Platform, DateTimeOffset PairedAt);
public sealed record PairingPayload(int Version, string PcId, string Name, string Endpoint,
    string Fingerprint, string Secret, DateTimeOffset ExpiresAt);
public sealed record SessionSnapshot(SessionPhase Phase, string? DeviceId, bool Approved,
    bool PrivacyBlocked, long Generation, string Message, Configuration Configuration);
public sealed record StageMetrics(double CaptureMs, double EncodeMs, double NetworkRttMs,
    double DecodeMs, double LossRatio, double UniqueFps, bool ThermalLimited, bool BatterySaver);
public enum Bottleneck { None, Capture, Encoder, Network, Decoder, Thermal }
public sealed record Adaptation(Bottleneck Cause, VideoMode Mode, int Bitrate, string Message);
public sealed record Command(string Id, string Type, long Generation, object? Payload = null);
public sealed record Acknowledgment(string Id, bool Ok, long Generation, Configuration? Applied, string? Error);
