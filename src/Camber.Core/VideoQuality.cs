namespace Camber.Core;

public enum EnhancementMode { Original = 0, Natural = 1, LowLight = 2 }

/// <summary>Only measured, local stage metrics. Missing values stay unknown.</summary>
public sealed record QualitySample(double? CaptureFps = null, double? EncodeMs = null,
    double? NetworkRttMs = null, double? DecodeMs = null, double? LossRatio = null,
    double? BitrateBps = null, double? EncodedFps = null, bool ThermalLimited = false);

public sealed record AppliedVideoQuality(int? Width = null, int? Height = null,
    double? CaptureFps = null, int? MaxBitrateBps = null, string Reason = "Starting camera…",
    int? EncodedWidth=null,int? EncodedHeight=null);

public sealed record EnhancementState(EnhancementMode Requested = EnhancementMode.Original,
    EnhancementMode Applied = EnhancementMode.Original, string Reason = "No additional image filters.",
    double? ProcessingMs = null, long Generation = 0,bool HasFallback=false);

public sealed record QualityRequest(string Id, long Generation, int MaxBitrateBps,
    VideoMode Capture, long IssuedTick);
