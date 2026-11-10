namespace Camber.Core;

public sealed class AdaptiveQuality
{
    private int badSamples;
    private int goodSamples;
    private Bottleneck recoveryCause;
    private VideoMode preferred = VideoMode.FullHD;
    public void Prefer(VideoMode maximum) { preferred=maximum; if(Mode.Height>maximum.Height)Mode=maximum; }
    public VideoMode Mode { get; private set; } = VideoMode.FullHD;
    public int Bitrate { get; private set; } = 8_000_000;
    // Compatibility adapter for existing stage-policy callers/tests. Wire data
    // uses QualitySample and never fills missing stages with invented zeroes.
    public Adaptation Observe(StageMetrics m) {
        if(new[]{m.CaptureMs,m.EncodeMs,m.NetworkRttMs,m.DecodeMs,m.LossRatio,m.UniqueFps}
            .Any(v=>!double.IsFinite(v) || v<0) || m.LossRatio>1)
            throw new ArgumentException("Invalid metrics.");
        return Observe(new QualitySample(
        CaptureFps: m.CaptureMs > 65 ? 15 : 30, EncodeMs:m.EncodeMs,
        NetworkRttMs:m.NetworkRttMs, DecodeMs:m.DecodeMs, LossRatio:m.LossRatio,
        EncodedFps:m.UniqueFps, ThermalLimited:m.ThermalLimited));
    }
    public Adaptation Observe(QualitySample m)
    {
        var values=new[]{m.CaptureFps,m.EncodeMs,m.NetworkRttMs,m.DecodeMs,m.LossRatio,m.BitrateBps,m.EncodedFps};
        if(values.Any(v=>v is {} number && (!double.IsFinite(number) || number<0)) || m.LossRatio>1)
            throw new ArgumentException("Invalid metrics.");
        var known=values.Any(v=>v.HasValue);
        var cause=m.ThermalLimited?Bottleneck.Thermal:m.DecodeMs>28?Bottleneck.Decoder:
            m.EncodeMs>28?Bottleneck.Encoder:m.CaptureFps<27?Bottleneck.Capture:
            m.LossRatio>.03 || m.NetworkRttMs>80?Bottleneck.Network:Bottleneck.None;
        if(!known && !m.ThermalLimited)return new(Bottleneck.None,Mode,Bitrate,"Waiting for measured video statistics.");
        if (cause != Bottleneck.None)
        {
            recoveryCause=cause;
            goodSamples = 0;
            if (++badSamples >= 3)
            {
                if (cause == Bottleneck.Network) Bitrate = Math.Max(2_000_000, (int)(Bitrate * .8));
                else Mode = VideoMode.HD;
                if (cause == Bottleneck.Network && Bitrate <= 4_000_000) Mode = VideoMode.HD;
                badSamples = 0;
            }
        }
        else
        {
            badSamples = 0;
            var recoveryKnown=recoveryCause switch {
                Bottleneck.Network=>m.NetworkRttMs.HasValue && m.LossRatio.HasValue,
                Bottleneck.Capture=>m.CaptureFps.HasValue,
                Bottleneck.Encoder=>m.EncodeMs.HasValue,
                Bottleneck.Decoder=>m.DecodeMs.HasValue,
                _=>true
            };
            if(!recoveryKnown){goodSamples=0;return new(Bottleneck.None,Mode,Bitrate,"Waiting for recovery measurements. Keeping the current capture quality.");}
            if (++goodSamples >= 30)
            { Mode = preferred; Bitrate = Math.Min(12_000_000, Bitrate + 1_000_000); goodSamples = 0; }
        }
        var message = cause switch
        {
            Bottleneck.Capture => "Phone camera capture is slow.", Bottleneck.Encoder => "Phone video processing is slow.",
            Bottleneck.Decoder => "PC video processing is slow.", Bottleneck.Network => "Wi-Fi is congested.",
            Bottleneck.Thermal => "Phone is overheating. Using less demanding video.", _ => "Connection is healthy."
        };
        if(cause==Bottleneck.None && Mode.Height<preferred.Height)message="Using 720p while measured conditions stabilize.";
        return new(cause, Mode, Bitrate, message);
    }
}
