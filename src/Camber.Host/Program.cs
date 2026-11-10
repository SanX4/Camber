using System.IO.Pipes;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.WebSockets;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using Camber.Core;
using Camber.Host;

var userSid = WindowsIdentity.GetCurrent().User!.Value;
if (args.Contains("--can-upgrade"))
{
    if (!Mutex.TryOpenExisting("Local\\Camber.Host."+userSid,out var existing)) return;
    existing.Dispose();
    try {
        await using var pipe=new NamedPipeClientStream(".","Camber.Control."+userSid,PipeDirection.InOut,PipeOptions.Asynchronous|PipeOptions.CurrentUserOnly);
        using var deadline=new CancellationTokenSource(TimeSpan.FromSeconds(3));await pipe.ConnectAsync(deadline.Token);
        using var reader=new StreamReader(pipe,Encoding.UTF8,false,4096,true);
        await using var writer=new StreamWriter(pipe,new UTF8Encoding(false),4096,true){AutoFlush=true};
        await writer.WriteLineAsync("{\"type\":\"status\"}");
        using var document=JsonDocument.Parse((await reader.ReadLineAsync(deadline.Token))!);
        Environment.ExitCode=document.RootElement.GetProperty("state").GetProperty("approved").GetBoolean()?20:0;
    } catch {Environment.ExitCode=21;}
    return;
}
if (args.Contains("--shutdown"))
{
    await using var control = new NamedPipeClientStream(".", "Camber.Control." + userSid, PipeDirection.InOut, PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
    using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(5));
    try
    {
        await control.ConnectAsync(timeout.Token);
        await using var writer = new StreamWriter(control, new UTF8Encoding(false), 4096, true) { AutoFlush = true };
        await writer.WriteLineAsync("{\"type\":\"shutdown\"}");
        using var reader = new StreamReader(control, Encoding.UTF8, false, 4096, true);
        await reader.ReadLineAsync(timeout.Token);
    }
    catch (Exception e) when (e is IOException or OperationCanceledException) { }
    return;
}
using var singleton = new Mutex(false, "Local\\Camber.Host." + userSid, out var firstInstance);
if (!firstInstance) return;
var json = new JsonSerializerOptions(JsonSerializerDefaults.Web);
json.Converters.Add(new JsonStringEnumConverter(JsonNamingPolicy.CamelCase));
var store = new SecureStore();
using var cert = store.Certificate();
var pairing = new PairingService();
var session = new SessionController();
var visuals = VisualSettingsStore.Load();
session.SetOutputMirror(visuals.OutputMirror);
var reactions = new ReactionGate();
var reactionRequests = new ReactionRequests();
var videoControl = new VideoControl();
var visualSettingsGate = new SemaphoreSlim(1);
var enhancementGate = new SemaphoreSlim(1);
var gestureAvailable = false;
var sessionLeaseAvailable = false;
long lastPhoneBeat = Environment.TickCount64, lastUiBeat = Environment.TickCount64;
string? lastReaction = null;
var pendingCommands = new PendingCommands();
await using var media = new MediaWorker();
await media.Start();
var socketGate = new SemaphoreSlim(1);
var sendGate = new SemaphoreSlim(1);
WebSocket? phoneSocket = null;
string? activeDevice = null;
var builder = WebApplication.CreateSlimBuilder(args);
builder.Logging.ClearProviders(); // Never log pairing bodies, credentials, SDP, or LAN addresses.
builder.WebHost.ConfigureKestrel(k =>
{
    k.Limits.MaxRequestBodySize = Protocol.MaxMessageBytes;
    k.ListenAnyIP(Protocol.Port, endpoint => endpoint.UseHttps(cert));
});
var app = builder.Build();
app.UseWebSockets(new() { KeepAliveInterval = TimeSpan.FromSeconds(5) });

async Task SendPhone(object data)
{
    await sendGate.WaitAsync();
    try
    {
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(3));
        if (phoneSocket?.State == WebSocketState.Open)
            await phoneSocket.SendAsync(JsonSerializer.SerializeToUtf8Bytes(data, json), WebSocketMessageType.Text, true, timeout.Token);
    }
    catch (Exception e) when (e is WebSocketException or OperationCanceledException) { phoneSocket?.Abort(); }
    finally { sendGate.Release(); }
}
async Task PrivacyStop(string reason)
{
    session.Stop(reason);
    reactions.Reset(); lastReaction = null;
    pendingCommands.Clear();
    await media.Disable(session.Snapshot.Generation);
    await SendPhone(new { type = "stop", state = session.Snapshot });
    await SendVideoSettings();
}
object Status() {
    var snapshot=session.Snapshot;var settings=visuals;
    return new { state=snapshot,devices=store.Devices,mediaAvailable=media.Available,mediaStatus=media.Status,mediaState=media.State,
        video=media.Diagnostics,quality=videoControl.StatusFor(snapshot,settings.Enhancement),visuals=settings,
        effects=new {gestureAvailable,lastReaction},version=Protocol.Version,output="Camber Camera",latency=(double?)null };
}
async Task SendEffects() => await SendPhone(new { type = "effects", enabled = visuals.ReactionsEnabled && visuals.GesturesEnabled && gestureAvailable,
    generation = session.Snapshot.Generation, reactionResults = true });
string enhancementSignature="";
async Task ConfigureEnhancement(bool force=false) {
    await enhancementGate.WaitAsync();
    try {
        var snapshot=session.Snapshot;var requested=visuals.Enhancement;
        var signature=$"{snapshot.Generation}:{requested}";
        if(!media.Available || (!force && enhancementSignature==signature))return;
        var id=Guid.NewGuid().ToString("N");
        videoControl.ExpectEnhancement(id,snapshot.Generation,requested);
        await media.Send(new {type="enhance",mode=(int)requested,id,generation=snapshot.Generation});
        enhancementSignature=signature;
    }finally {enhancementGate.Release();}
}
async Task SendVideoSettings() {
    var snapshot=session.Snapshot;var requested=visuals.Enhancement;
    var enhancement=videoControl.Enhancement;
    var live=snapshot.Approved && !snapshot.PrivacyBlocked;
    var confirmed=enhancement.Generation==snapshot.Generation && enhancement.Requested==requested;
    var actual=videoControl.Actual;
    var encodedInfo=live && actual.EncodedWidth is not null && actual.EncodedHeight is not null ? $"Encoded video: {actual.EncodedWidth} × {actual.EncodedHeight}. " : "";
    var captureInfo=$"Requested maximum: {snapshot.Configuration.Capture.Height}p at 30 fps. "+(live && actual.Width is not null ? $"Actual capture: {actual.Width} × {actual.Height}. {encodedInfo}{actual.Reason}" : "Starts only when you share.");
    if(videoControl.EnhancementControls)await SendPhone(new { type="videoSettings",generation=snapshot.Generation,
        requested,applied=live && confirmed?enhancement.Applied:EnhancementMode.Original,
        reason=!live?"Selected; applies when sharing.":confirmed?enhancement.Reason:"Waiting for the video engine to confirm.",captureInfo,requestedHeight=snapshot.Configuration.Capture.Height });
}
async Task SetEnhancement(EnhancementMode mode) {
    if(!Enum.IsDefined(mode))throw new ArgumentException("Unknown video preset.");
    await visualSettingsGate.WaitAsync();
    try {
        var settings=visuals with {Enhancement=mode};VisualSettingsStore.Save(settings);visuals=settings;
        await ConfigureEnhancement(true);
    }finally {visualSettingsGate.Release();}
    await SendVideoSettings();
}
async Task ApplyVisuals()
{
    session.SetOutputMirror(visuals.OutputMirror);
    if (media.Available)
    {
        await media.Send(new { type = "configure", configuration = session.Snapshot.Configuration });
        await media.Send(new { type = "effects", enabled = visuals.ReactionsEnabled, reducedMotion = visuals.ReducedMotion });
        await ConfigureEnhancement();
    }
    await SendEffects();
}
async Task<bool> TriggerReaction(string? name, long generation, bool gesture)
{
    if (!Enum.TryParse<ReactionKind>(name, true, out var kind) ||
        !reactions.TryTrigger(session.Snapshot, generation, kind, visuals.ReactionsEnabled && (!gesture || (visuals.GesturesEnabled && gestureAvailable)))) return false;
    await media.Send(new { type = "reaction", reaction = (int)kind, generation });
    lastReaction = kind.ToString();
    return true;
}
using var privacy = new Camber.Tray.PrivacyMonitor(() => _ = Task.Run(() => PrivacyStop("Windows locked or suspended. Start again on your phone.")));

app.MapGet("/health", () => Results.Json(new { product = "Camber", version = Protocol.Version }));
app.MapPost("/pair", async (HttpContext context) =>
{
    PairRequest? request;
    try { request = await context.Request.ReadFromJsonAsync<PairRequest>(json); } catch (JsonException) { return Results.BadRequest(); }
    if (request is null || string.IsNullOrWhiteSpace(request.Name) || request.Name.Length > 80 || request.Platform is not ("android" or "ios") || request.Secret is null)
        return Results.BadRequest();
    if (!pairing.Consume(request.Secret, request.Version)) return Results.Unauthorized();
    var result = store.Add(request.Name, request.Platform);
    return Results.Json(new { device = result.Device, token = result.Token, version = Protocol.Version }, json);
});
app.Map("/session", async (HttpContext context) =>
{
    var id = context.Request.Headers["X-Camber-Device"].ToString();
    var auth = context.Request.Headers.Authorization.ToString();
    if (!auth.StartsWith("Bearer ") || !store.Authenticate(id, auth[7..])) { context.Response.StatusCode = 401; return; }
    if (!context.WebSockets.IsWebSocketRequest) { context.Response.StatusCode = 400; return; }
    if (!await socketGate.WaitAsync(0)) { context.Response.StatusCode = 409; return; }
    try
    {
        using var socket = await context.WebSockets.AcceptWebSocketAsync();
        phoneSocket = socket; activeDevice = id;
        videoControl.Reset();
        gestureAvailable = false; sessionLeaseAvailable = false; Interlocked.Exchange(ref lastPhoneBeat, Environment.TickCount64);
        if (session.Snapshot.DeviceId != id || !session.Snapshot.Approved) session.Pair(id);
        await SendPhone(new { type = "state", state = session.Snapshot });
        await SendEffects();
        var buffer = new byte[Protocol.MaxMessageBytes];
        while (socket.State == WebSocketState.Open)
        {
            int count = 0;
            WebSocketReceiveResult result;
            do
            {
                if (count == buffer.Length) { await socket.CloseAsync(WebSocketCloseStatus.MessageTooBig, "Message too large", CancellationToken.None); return; }
                result = await socket.ReceiveAsync(new ArraySegment<byte>(buffer, count, buffer.Length - count), context.RequestAborted);
                if (result.MessageType == WebSocketMessageType.Close) return;
                if (result.MessageType != WebSocketMessageType.Text) throw new JsonException("Text required");
                count += result.Count;
            } while (!result.EndOfMessage);
            using var document = JsonDocument.Parse(buffer.AsMemory(0, count));
            var message = document.RootElement;
            Interlocked.Exchange(ref lastPhoneBeat, Environment.TickCount64);
            var type = message.GetProperty("type").GetString();
            switch (type)
            {
                case "videoStats":
                    var qualityRequest=videoControl.Observe(message,session.Snapshot,Environment.TickCount64,pendingCommands.IsPending(DateTimeOffset.UtcNow));
                    if(qualityRequest is not null)await SendPhone(new {type="quality",id=qualityRequest.Id,generation=qualityRequest.Generation,
                        maxBitrateBps=qualityRequest.MaxBitrateBps,capture=qualityRequest.Capture});
                    await SendVideoSettings();
                    break;
                case "qualityResult": videoControl.Acknowledge(message,session.Snapshot);break;
                case "setEnhancement":
                    if(message.GetProperty("generation").GetInt64()==session.Snapshot.Generation &&
                        Enum.TryParse<EnhancementMode>(message.GetProperty("mode").GetString(),true,out var mode))await SetEnhancement(mode);
                    break;
                case "capabilities":
                    videoControl.Capabilities(message);
                    if (message.TryGetProperty("sessionLease", out var lease)) sessionLeaseAvailable = lease.ValueKind == JsonValueKind.True;
                    gestureAvailable = message.TryGetProperty("gestureReactions", out var gestures) && gestures.ValueKind == JsonValueKind.True;
                    await SendEffects();
                    await SendVideoSettings();
                    break;
                case "reaction":
                    var reactionGeneration=message.GetProperty("generation").GetInt64();
                    var hasRequest=message.TryGetProperty("id",out var requestId);
                    var rid=hasRequest?requestId.GetString():null;
                    var current= reactionGeneration==session.Snapshot.Generation;
                    var fresh= current && (!hasRequest || reactionRequests.Accept(rid,reactionGeneration));
                    var accepted=fresh && await TriggerReaction(message.GetProperty("reaction").GetString(),reactionGeneration,true);
                    if(hasRequest) await SendPhone(new { type="reactionResult", id=rid, generation=reactionGeneration, accepted,
                        message=accepted?"Reaction accepted.":"Reaction not started. Check sharing, cooldown and hand-reaction settings." });
                    break;
                case "approve":
                    if (!message.TryGetProperty("generation", out var approvalGeneration) || approvalGeneration.GetInt64() != session.Snapshot.Generation)
                    {
                        await SendPhone(new { type = "error", message = "The session changed. Tap Start again on your phone.", state = session.Snapshot });
                        break;
                    }
                    if (!sessionLeaseAvailable) { await SendPhone(new { type = "error", message = "Update the phone app for secure session monitoring." }); break; }
                    if (!media.Available) { await SendPhone(new { type = "error", message = media.Status }); break; }
                    SessionSnapshot state;
                    CameraFacing? requestedCamera=null;
                    if(message.TryGetProperty("camera",out var selectedCamera)) {
                        if(selectedCamera.GetString() is not ("front" or "back"))throw new ArgumentException("Unknown camera.");
                        requestedCamera=selectedCamera.GetString()=="front"?CameraFacing.Front:CameraFacing.Back;
                    }
                    try { state = session.ApproveRequest(id, approvalGeneration.GetInt64(),requestedCamera); }
                    catch (InvalidOperationException) { await SendPhone(new { type = "error", message = "The session changed. Tap Start again.", state = session.Snapshot }); break; }
                    await media.Send(new { type = "permit", generation = state.Generation });
                    await ApplyVisuals();
                    await SendPhone(new { type = "approved", state });
                    break;
                case "offer":
                case "candidate":
                    if (!session.Snapshot.Approved || session.Snapshot.PrivacyBlocked ||
                        message.GetProperty("generation").GetInt64() != session.Snapshot.Generation) break;
                    await media.Send(message);
                    break;
                case "pause": session.Pause(message.TryGetProperty("reason", out var reason) && reason.GetString() == "outOfFocus" ? "Out of focus. Camera paused on your phone." : "Camera paused. No live video is being shared."); await media.Disable(session.Snapshot.Generation); await SendPhone(new { type = "pause", state = session.Snapshot }); await SendVideoSettings(); break;
                case "stop": await PrivacyStop("Stopped on your phone. Start again to share."); break;
                case "selectCamera":
                case "selectQuality":
                    if (!session.Snapshot.Approved || session.Snapshot.PrivacyBlocked ||
                        message.GetProperty("generation").GetInt64() != session.Snapshot.Generation) break;
                    var facing = type=="selectCamera"?message.GetProperty("camera").GetString():null;
                    if (type=="selectCamera" && facing is not ("front" or "back")) break;
                    var height=type=="selectQuality"?message.GetProperty("height").GetInt32():session.Snapshot.Configuration.Capture.Height;
                    if(height is not (720 or 1080))break;
                    var selectionId = Guid.NewGuid().ToString("N");
                    pendingCommands.AddExclusive(selectionId, session.Snapshot.Generation, DateTimeOffset.UtcNow);
                    var selection = session.Snapshot.Configuration with { Camera = facing is null?session.Snapshot.Configuration.Camera:facing == "front" ? CameraFacing.Front : CameraFacing.Back, Capture=height==720?VideoMode.HD:VideoMode.FullHD };
                    await SendPhone(new { type = "configure", command = new Command(selectionId, "configure", session.Snapshot.Generation, selection) });
                    break;
                case "ack":
                    var ack = message.GetProperty("ack").Deserialize<Acknowledgment>(json);
                    if (ack is not null && pendingCommands.Consume(ack, DateTimeOffset.UtcNow))
                    {
                        var applied = session.ApplyAcknowledgment(ack with { Applied = ack.Applied is null ? null : ack.Applied with { OutputMirror = visuals.OutputMirror } });
                        if (ack.Ok && ack.Generation == applied.Generation && !applied.PrivacyBlocked)
                            await media.Send(new { type = "configure", configuration = applied.Configuration });
                        await SendVideoSettings();
                    }
                    break;
                case "ping": await SendPhone(new { type = "pong", state = session.Snapshot }); break;
                default: await SendPhone(new { type = "error", message = "Unsupported command. Update both Camber apps." }); break;
            }
        }
    }
    catch (Exception e) when (e is WebSocketException or JsonException or InvalidOperationException or ArgumentException or KeyNotFoundException or OperationCanceledException) { }
    finally { session.Disconnect(); await media.Disable(session.Snapshot.Generation); phoneSocket = null; activeDevice = null; gestureAvailable = false; socketGate.Release(); }
});

// Local UI control is protected by Windows' current-user-only pipe ACL.
var shutdown = new CancellationTokenSource();
_ = Task.Run(async () =>
{
    var sid = WindowsIdentity.GetCurrent().User!.Value;
    while (!shutdown.IsCancellationRequested)
    {
        await using var pipe = new NamedPipeServerStream("Camber.Control." + sid, PipeDirection.InOut, 1,
            PipeTransmissionMode.Byte, PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
        try
        {
            await pipe.WaitForConnectionAsync(shutdown.Token);
            using var reader = new StreamReader(pipe, Encoding.UTF8, false, 4096, true);
            await using var writer = new StreamWriter(pipe, new UTF8Encoding(false), 4096, true) { AutoFlush = true };
            while (pipe.IsConnected && await reader.ReadLineAsync(shutdown.Token) is { } line)
            {
                if (line.Length > Protocol.MaxMessageBytes) break;
                using var doc = JsonDocument.Parse(line);
                var message = doc.RootElement;
                Interlocked.Exchange(ref lastUiBeat, Environment.TickCount64);
                object response;
                switch (message.GetProperty("type").GetString())
                {
                    case "enhancement":
                        var chosen=message.GetProperty("mode").Deserialize<EnhancementMode>(json);
                        await SetEnhancement(chosen);response=Status();break;
                    case "visuals":
                        var settings = message.GetProperty("settings").Deserialize<VisualSettings>(json) ?? throw new ArgumentException("Visual settings are required.");
                        if(!Enum.IsDefined(settings.Enhancement))throw new ArgumentException("Unknown enhancement preset.");
                        await visualSettingsGate.WaitAsync();
                        try {
                            // Legacy/full visual updates must not overwrite a concurrently
                            // selected image preset. Presets use the typed enhancement command.
                            settings=settings with {Enhancement=visuals.Enhancement};
                            VisualSettingsStore.Save(settings);visuals=settings;
                            await ApplyVisuals();
                        }finally {visualSettingsGate.Release();}
                        response=Status();break;
                    case "reaction":
                        var triggered = await TriggerReaction(message.GetProperty("reaction").GetString(), session.Snapshot.Generation, false);
                        response = new { ok = triggered, message = triggered ? "Reaction started." : "Start sharing first, enable reactions, and allow four seconds between reactions." }; break;
                    case "pairing":
                        var address = message.GetProperty("address").GetString()!;
                        var local = NetworkInterface.GetAllNetworkInterfaces().SelectMany(n => n.GetIPProperties().UnicastAddresses).Select(a => a.Address);
                        if (!IPAddress.TryParse(address, out var ip) || IPAddress.IsLoopback(ip) || !local.Contains(ip)) throw new ArgumentException("Select a local network address.");
                        var host = ip.AddressFamily == System.Net.Sockets.AddressFamily.InterNetworkV6 ? $"[{ip}]" : ip.ToString();
                        response = pairing.Create(store.PcId, Environment.MachineName, $"https://{host}:{Protocol.Port}", Convert.ToHexString(SHA256.HashData(cert.RawData)));
                        break;
                    case "pause": session.Pause(); await media.Disable(session.Snapshot.Generation); await SendPhone(new { type = "pause", state = session.Snapshot }); await SendVideoSettings(); response = Status(); break;
                    case "resume": await SendPhone(new { type = "resumeRequested" }); response = Status(); break;
                    case "stop": await PrivacyStop("Stopped on this PC. Start again on your phone."); response = Status(); break;
                    case "revoke":
                        var revokeId = message.GetProperty("deviceId").GetString()!;
                        store.Revoke(revokeId);
                        if (activeDevice == revokeId) { await PrivacyStop("Pairing was removed on this PC."); phoneSocket?.Abort(); }
                        response = Status(); break;
                    case "configure":
                        if (!session.Snapshot.Approved || session.Snapshot.PrivacyBlocked) throw new InvalidOperationException("Start the camera on your phone first.");
                        var config = message.GetProperty("configuration").Deserialize<Configuration>(json)!;
                        if (!config.Capture.IsLaunchMode || config.Output != VideoMode.FullHD || config.Rotation != 0) throw new ArgumentException("Unsupported configuration.");
                        var commandId = Guid.NewGuid().ToString("N");
                        pendingCommands.AddExclusive(commandId, session.Snapshot.Generation, DateTimeOffset.UtcNow);
                        await SendPhone(new { type = "configure", command = new Command(commandId, "configure", session.Snapshot.Generation, config) });
                        response = Status(); break;
                    case "shutdown": await PrivacyStop("Camber closed."); response = Status(); app.Lifetime.StopApplication(); break;
                    default: response = Status(); break;
                }
                await writer.WriteLineAsync(JsonSerializer.Serialize(response, json));
            }
        }
        catch (Exception e) when (e is IOException or JsonException or ArgumentException or InvalidOperationException or OperationCanceledException) { }
    }
});
_ = Task.Run(async () =>
{
    await foreach (var evt in media.Events.Reader.ReadAllAsync())
    {
        var type = evt.GetProperty("type").GetString();
        if (type == "frame") session.AcceptFrame(evt.GetProperty("generation").GetInt64());
        else if(type=="decoderStats")videoControl.DecoderStatistics(evt,session.Snapshot);
        else if(type=="enhancementState") {
            if(videoControl.AppliedEnhancement(evt,session.Snapshot,visuals.Enhancement))await SendVideoSettings();
        }
        else if(type=="ownershipError") {
            if(evt.GetProperty("generation").GetInt64()==session.Snapshot.Generation)
                await PrivacyStop(evt.GetProperty("message").GetString() ?? "Windows camera authorization failed.");
        }
        else if (type is "answer" or "candidate" or "error") await SendPhone(evt);
    }
    await PrivacyStop("Media engine stopped. Restart Camber.");
});
_ = Task.Run(async () =>
{
    while (!shutdown.IsCancellationRequested)
    {
        await Task.Delay(1000);
        var now = Environment.TickCount64;
        if (now - Interlocked.Read(ref lastUiBeat) > 6000)
        {
            await PrivacyStop("Windows app closed. Sharing stopped.");
            phoneSocket?.Abort(); app.Lifetime.StopApplication(); break;
        }
        if (phoneSocket is not null && now - Interlocked.Read(ref lastPhoneBeat) > 6000)
        {
            await PrivacyStop("Phone connection expired. Sharing stopped."); phoneSocket?.Abort();
        }
        try { if (media.Available) await media.Send(new { type = "lease" }); } catch (Exception e) when (e is IOException or InvalidOperationException) { }
    }
});
app.Lifetime.ApplicationStopping.Register(() => shutdown.Cancel());
await app.RunAsync();
record PairRequest(int Version, string Secret, string Name, string Platform);
