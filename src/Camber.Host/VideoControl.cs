using System.Text.Json;
using Camber.Core;

namespace Camber.Host;

/// <summary>Authoritative requested/applied state; telemetry is bounded and optional.</summary>
internal sealed class VideoControl
{
    private readonly object gate = new();
    private AdaptiveQuality policy = new();
    private QualityRequest? pending;
    private long generation = -1, lastSample;
    private int preferredHeight = 1080;
    private bool offeredQuality, rejectedQuality;
    private AppliedVideoQuality actual = new();
    private QualitySample sample = new();
    private EnhancementState enhancement = new();
    private double? decodeMs;
    private VideoMode? confirmedCapture;
    private int policyHeight=1080;
    private string enhancementId="";
    private long enhancementGeneration=-1;
    private int? sourceRotation,captureRotation,decodedWidth,decodedHeight,decodedRotation;
    public bool EnhancementControls { get; private set; }
    public object Status { get { lock(gate) return new { actual, sample, enhancement, automaticOrientation = true,
        orientation=new {sourceRotation,captureRotation,decodedWidth,decodedHeight,decodedRotation} }; } }
    public object StatusFor(SessionSnapshot session,EnhancementMode requested)
    {
        lock(gate)
        {
            var live=session.Approved && !session.PrivacyBlocked;
            var current=live && generation==session.Generation;
            var confirmed=live && enhancement.Generation==session.Generation && enhancement.Requested==requested;
            return new {
                actual=current?actual:new AppliedVideoQuality(),sample=current?sample:new QualitySample(),
                enhancement=confirmed?enhancement:new EnhancementState(requested,EnhancementMode.Original,
                    live?"Waiting for the video engine to confirm.":"Selected; applies when sharing.",null,session.Generation),
                automaticOrientation=true,
                orientation=new {sourceRotation=current?sourceRotation:null,captureRotation=current?captureRotation:null,
                    decodedWidth=current?decodedWidth:null,decodedHeight=current?decodedHeight:null,decodedRotation=current?decodedRotation:null}
            };
        }
    }
    public AppliedVideoQuality Actual { get { lock(gate)return actual; } }
    public EnhancementState Enhancement { get { lock(gate)return enhancement; } }
    public void ExpectEnhancement(string id,long g,EnhancementMode requested)
    {
        lock(gate){enhancementId=id;enhancementGeneration=g;enhancement=new(requested,EnhancementMode.Original,"Waiting for the video engine to confirm.",null,g);}
    }
    public void Capabilities(JsonElement data)
    {
        lock(gate)
        {
            offeredQuality=data.TryGetProperty("videoQuality",out var q) && q.ValueKind==JsonValueKind.True;
            EnhancementControls=data.TryGetProperty("enhancementControls",out var e) && e.ValueKind==JsonValueKind.True;
        }
    }
    public void Reset()
    {
        lock(gate){pending=null;generation=-1;lastSample=0;confirmedCapture=null;rejectedQuality=false;offeredQuality=false;enhancementId="";enhancementGeneration=-1;
            EnhancementControls=false;decodeMs=null;actual=new();sample=new();enhancement=new();
            sourceRotation=captureRotation=decodedWidth=decodedHeight=decodedRotation=null;}
    }
    private static double? Number(JsonElement data,string key,double max=1_000_000_000)
    {
        if(!data.TryGetProperty(key,out var field) || field.ValueKind==JsonValueKind.Null)return null;
        if(!field.TryGetDouble(out var value) || !double.IsFinite(value) || value<0 || value>max)
            throw new ArgumentException("Invalid video statistics.");
        return value;
    }
    private static int? Rotation(JsonElement data,string key)
    {
        var value=Number(data,key,270);
        if(value is null)return null;
        if(value is not (0 or 90 or 180 or 270))throw new ArgumentException("Invalid frame orientation.");
        return (int)value;
    }
    public QualityRequest? Observe(JsonElement data,SessionSnapshot session,long now,bool controlsBlocked=false)
    {
        if(!session.Approved || session.PrivacyBlocked || data.GetProperty("generation").GetInt64()!=session.Generation)return null;
        lock(gate)
        {
            if(generation!=session.Generation || preferredHeight!=session.Configuration.Capture.Height)
            {
                generation=session.Generation;lastSample=0;policy=new();pending=null;rejectedQuality=false;
                preferredHeight=session.Configuration.Capture.Height;policy.Prefer(session.Configuration.Capture);confirmedCapture=null;policyHeight=preferredHeight;
            }
            if(lastSample!=0 && now-lastSample<800)return null;
            lastSample=now;
            var width=(int?)Number(data,"width",1920);var height=(int?)Number(data,"height",1920);
            if(width is null or <2 || height is null or <2 || ((width|height)&1)!=0)return null;
            if((long)width.Value*height.Value>1920*1080)throw new ArgumentException("Video dimensions exceed the frame budget.");
            var fps=Number(data,"captureFps",240);
            sourceRotation=Rotation(data,"sourceRotation");captureRotation=Rotation(data,"rotation");
            sample=new(fps,Number(data,"encodeMs",10_000),Number(data,"networkRttMs",60_000),decodeMs,
                Number(data,"lossRatio",1),Number(data,"bitrateBps"),Number(data,"encodedFps",240),
                data.TryGetProperty("thermalLimited",out var thermal) && thermal.ValueKind==JsonValueKind.True);
            var limit=(int?)Number(data,"maxBitrateBps",12_000_000);
            if(limit is not null && limit<2_000_000)limit=null;
            actual=actual with {Width=width,Height=height,CaptureFps=fps,MaxBitrateBps=limit,
                EncodedWidth=(int?)Number(data,"encodedWidth",1920),EncodedHeight=(int?)Number(data,"encodedHeight",1920)};
            if(fps is null && sample.EncodedFps is null)return null;
            var adaptation=policy.Observe(sample);
            if(policyHeight!=adaptation.Mode.Height){policyHeight=adaptation.Mode.Height;confirmedCapture=null;}
            var captureMatches=(width==adaptation.Mode.Width && height==adaptation.Mode.Height) || (height==adaptation.Mode.Width && width==adaptation.Mode.Height);
            actual=actual with {Reason=rejectedQuality?"Automatic codec controls are unavailable on this phone.":adaptation.Message};
            if(!offeredQuality || rejectedQuality || controlsBlocked)return null;
            if(pending is {} previous && now-previous.IssuedTick<4000)return null;
            if(limit==adaptation.Bitrate && (captureMatches || confirmedCapture==adaptation.Mode)) {
                if(!captureMatches)actual=actual with {Reason="The phone provides a different capture size. Your requested maximum is preserved."};
                pending=null;return null;
            }
            pending=new(Guid.NewGuid().ToString("N"),generation,adaptation.Bitrate,adaptation.Mode,now);
            return pending;
        }
    }
    public void Acknowledge(JsonElement data,SessionSnapshot session)
    {
        lock(gate)
        {
            if(pending is not {} command || !session.Approved || session.PrivacyBlocked ||
                command.Generation!=session.Generation || data.GetProperty("generation").GetInt64()!=command.Generation ||
                data.GetProperty("id").GetString()!=command.Id)return;
            if(data.GetProperty("ok").ValueKind!=JsonValueKind.True)rejectedQuality=true;
            else confirmedCapture=command.Capture;
            pending=null;
        }
    }
    public void DecoderStatistics(JsonElement data,SessionSnapshot session)
    {
        if(data.GetProperty("generation").GetInt64()!=session.Generation)return;
        lock(gate){decodeMs=Number(data,"decodeMs",10_000);decodedWidth=(int?)Number(data,"width",1920);
            decodedHeight=(int?)Number(data,"height",1920);decodedRotation=Rotation(data,"rotation");}
    }
    public bool AppliedEnhancement(JsonElement data,SessionSnapshot session,EnhancementMode requested)
    {
        if(data.GetProperty("generation").GetInt64()!=session.Generation)return false;
        lock(gate)
        {
            if(enhancementGeneration!=session.Generation || data.GetProperty("id").GetString()!=enhancementId)return false;
            var mode=data.GetProperty("applied").GetInt32();var selected=data.GetProperty("requested").GetInt32();
            if(!Enum.IsDefined((EnhancementMode)mode) || selected!=(int)requested)return false;
            var reason=data.GetProperty("reason").GetString()??"";
            if(reason.Length>256)return false;
            enhancement=new(requested,session.Approved && !session.PrivacyBlocked?(EnhancementMode)mode:EnhancementMode.Original,
                reason,Number(data,"processingMs",60_000),session.Generation,
                data.TryGetProperty("fallback",out var fallback) && fallback.ValueKind==JsonValueKind.True);
            return true;
        }
    }
}
