using System.IO.Pipes;
using System.Net;
using System.Net.Http.Json;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Net.WebSockets;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;
using System.Text.Json;
using Camber.Core;

var passed = 0;
void Assert(bool value, string label) { if (!value) throw new Exception(label); passed++; Console.WriteLine("PASS " + label); }
async Task<JsonElement> ReceiveType(ClientWebSocket socket, string type, CancellationToken token)
{
    var bytes = new byte[Protocol.MaxMessageBytes];
    while (true)
    {
        int count = 0; WebSocketReceiveResult received;
        do { received = await socket.ReceiveAsync(new ArraySegment<byte>(bytes, count, bytes.Length - count), token); count += received.Count;
            if (received.MessageType == WebSocketMessageType.Close || count == bytes.Length && !received.EndOfMessage) throw new IOException("Unexpected socket response.");
        } while (!received.EndOfMessage);
        using var doc = JsonDocument.Parse(bytes.AsMemory(0, count));
        if (doc.RootElement.GetProperty("type").GetString() == type) return doc.RootElement.Clone();
    }
}
async Task<JsonElement> Control(object message)
{
    using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(5));
    await using var pipe = new NamedPipeClientStream(".", "Camber.Control." + WindowsIdentity.GetCurrent().User!.Value, PipeDirection.InOut, PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
    await pipe.ConnectAsync(timeout.Token);
    using var reader = new StreamReader(pipe, Encoding.UTF8, false, 4096, true);
    await using var writer = new StreamWriter(pipe, new UTF8Encoding(false), 4096, true) { AutoFlush = true };
    await writer.WriteLineAsync(JsonSerializer.Serialize(message));
    using var document = JsonDocument.Parse((await reader.ReadLineAsync(timeout.Token))!);
    return document.RootElement.Clone();
}
var before = await Control(new { type = "status" });
if (before.GetProperty("state").GetProperty("approved").GetBoolean()) throw new InvalidOperationException("Stop the live phone session before running integration tests.");
var address = NetworkInterface.GetAllNetworkInterfaces().Where(n => n.OperationalStatus == OperationalStatus.Up)
    .SelectMany(n => n.GetIPProperties().UnicastAddresses).Select(a => a.Address)
    .First(a => a.AddressFamily == AddressFamily.InterNetwork && !IPAddress.IsLoopback(a));
var qr = await Control(new { type = "pairing", address = address.ToString() });
var fingerprint = qr.GetProperty("fingerprint").GetString();
using var handler = new HttpClientHandler { UseProxy = false, ServerCertificateCustomValidationCallback = (_, certificate, _, _) => certificate is not null && Convert.ToHexString(SHA256.HashData(certificate.RawData)) == fingerprint };
using var client = new HttpClient(handler) { BaseAddress = new Uri(qr.GetProperty("endpoint").GetString()!), Timeout = TimeSpan.FromSeconds(5) };
var health = await client.GetFromJsonAsync<JsonElement>("/health");
Assert(health.GetProperty("version").GetInt32() == Protocol.Version, "pinned TLS reaches real server");
using (var wrongHandler = new HttpClientHandler { UseProxy = false, ServerCertificateCustomValidationCallback = (_, _, _, _) => false })
using (var wrongClient = new HttpClient(wrongHandler) { Timeout = TimeSpan.FromSeconds(3) })
{
    bool rejected = false;
    try { await wrongClient.GetAsync(client.BaseAddress); } catch (HttpRequestException) { rejected = true; }
    Assert(rejected, "wrong PC certificate is rejected without insecure fallback");
}
var body = new { version = 1, secret = qr.GetProperty("secret").GetString(), name = "Camber integration test", platform = "android" };
using var bad = await client.PostAsJsonAsync("/pair", body with { secret = new string('0', 64) });
Assert(bad.StatusCode == HttpStatusCode.Unauthorized, "wrong pairing secret is rejected");
using var result = await client.PostAsJsonAsync("/pair", body);
Assert(result.IsSuccessStatusCode, "valid single-use pairing succeeds");
var enrollment = await result.Content.ReadFromJsonAsync<JsonElement>();
var id = enrollment.GetProperty("device").GetProperty("id").GetString()!;
try
{
    using var replay = await client.PostAsJsonAsync("/pair", body);
    Assert(replay.StatusCode == HttpStatusCode.Unauthorized, "pairing replay is rejected by real server");
    using var socket = new ClientWebSocket();
    socket.Options.SetRequestHeader("X-Camber-Device", id);
    socket.Options.SetRequestHeader("Authorization", "Bearer " + enrollment.GetProperty("token").GetString());
    socket.Options.RemoteCertificateValidationCallback = (_, certificate, _, _) => certificate is not null && Convert.ToHexString(SHA256.HashData(certificate.GetRawCertData())) == fingerprint;
    using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(5));
    await socket.ConnectAsync(new Uri(client.BaseAddress!.ToString().Replace("https://", "wss://").TrimEnd('/') + "/session"), timeout.Token);
    var state = await ReceiveType(socket, "state", timeout.Token);
    await socket.SendAsync(JsonSerializer.SerializeToUtf8Bytes(new { type = "capabilities", sessionLease = true,enhancementControls=true,videoQuality=true }), WebSocketMessageType.Text, true, timeout.Token);
    var initialGeneration = state.GetProperty("state").GetProperty("generation").GetInt64();
    Assert(state.GetProperty("state").GetProperty("approved").GetBoolean() == false, "remembered pairing does not authorize video");
    var reaction = await Control(new { type = "reaction", reaction = "hearts" });
    Assert(!reaction.GetProperty("ok").GetBoolean(), "reactions cannot run without camera approval");
    var status = await Control(new { type = "status" });
    if (!status.GetProperty("mediaAvailable").GetBoolean())
    {
        await socket.SendAsync(JsonSerializer.SerializeToUtf8Bytes(new { type = "approve", generation = initialGeneration }), WebSocketMessageType.Text, true, timeout.Token);
        var error = await ReceiveType(socket, "error", timeout.Token);
        Assert(error.GetProperty("type").GetString() == "error", "missing native engine fails closed");
        status = await Control(new { type = "status" });
        Assert(!status.GetProperty("state").GetProperty("approved").GetBoolean(), "unavailable video is never shown as authorized");
    }
    else
    {
        await socket.SendAsync(JsonSerializer.SerializeToUtf8Bytes(new { type = "approve", generation = initialGeneration }), WebSocketMessageType.Text, true, timeout.Token);
        var approved = await ReceiveType(socket, "approved", timeout.Token);
        var cameraGeneration = approved.GetProperty("state").GetProperty("generation").GetInt64();
        var originalCamera = approved.GetProperty("state").GetProperty("configuration").GetProperty("camera").GetString();
        var desiredCamera = originalCamera == "front" ? "back" : "front";
        await socket.SendAsync(JsonSerializer.SerializeToUtf8Bytes(new { type = "selectCamera", camera = desiredCamera, generation = cameraGeneration }), WebSocketMessageType.Text, true, timeout.Token);
        var cameraCommand = (await ReceiveType(socket, "configure", timeout.Token)).GetProperty("command");
        status = await Control(new { type = "status" });
        Assert(status.GetProperty("state").GetProperty("configuration").GetProperty("camera").GetString() == originalCamera,
            "phone lens request waits for hardware acknowledgment");
        await socket.SendAsync(JsonSerializer.SerializeToUtf8Bytes(new { type = "ack", ack = new {
            id = cameraCommand.GetProperty("id").GetString(), ok = true, generation = cameraGeneration,
            applied = cameraCommand.GetProperty("payload") } }), WebSocketMessageType.Text, true, timeout.Token);
        await socket.SendAsync(JsonSerializer.SerializeToUtf8Bytes(new { type = "ping" }), WebSocketMessageType.Text, true, timeout.Token);
        await ReceiveType(socket, "pong", timeout.Token);
        status = await Control(new { type = "status" });
        Assert(status.GetProperty("state").GetProperty("configuration").GetProperty("camera").GetString() == desiredCamera,
            "acknowledged phone camera selection reaches Windows");
        reaction = await Control(new { type = "reaction", reaction = "hearts" });
        Assert(reaction.GetProperty("ok").GetBoolean(), "approved current session accepts a reaction");
        reaction = await Control(new { type = "reaction", reaction = "fireworks" });
        Assert(!reaction.GetProperty("ok").GetBoolean(), "real host rate-limits overlapping reactions");
    }
    await socket.SendAsync(JsonSerializer.SerializeToUtf8Bytes(new {type="setEnhancement",generation=initialGeneration,mode="natural"}),WebSocketMessageType.Text,true,timeout.Token);
    JsonElement video;
    do {video=await ReceiveType(socket,"videoSettings",timeout.Token);}while(video.GetProperty("requested").GetString()!="natural");
    Assert(video.GetProperty("applied").GetString()=="original","unapproved session cannot apply an image preset to live video");
    status=await Control(new {type="status"});
    Assert(status.GetProperty("visuals").GetProperty("enhancement").GetString()=="natural","phone preset reaches authoritative Windows preferences");
    await Control(new {type="visuals",settings=new {outputMirror=false,previewMirror=false,reactionsEnabled=true,gesturesEnabled=true,reducedMotion=true}});
    status=await Control(new {type="status"});
    Assert(status.GetProperty("visuals").GetProperty("enhancement").GetString()=="natural","legacy visual update preserves separately selected preset");
    await Control(new {type="enhancement",mode="lowLight"});
    do {video=await ReceiveType(socket,"videoSettings",timeout.Token);}while(video.GetProperty("requested").GetString()!="lowLight");
    Assert(video.GetProperty("applied").GetString()=="original","Windows preset synchronizes to phone without authorizing capture");
    await Control(new { type = "stop" });
    status = await Control(new { type = "status" });
    Assert(status.GetProperty("state").GetProperty("privacyBlocked").GetBoolean(), "PC Stop blocks live output");
    // Consume the Stop event, then replay a Start from before the privacy boundary.
    await ReceiveType(socket, "stop", timeout.Token);
    await socket.SendAsync(JsonSerializer.SerializeToUtf8Bytes(new { type = "approve", generation = initialGeneration }), WebSocketMessageType.Text, true, timeout.Token);
    var stale = await ReceiveType(socket, "error", timeout.Token);
    Assert(stale.GetProperty("type").GetString() == "error", "delayed Start from before PC Stop is rejected");
    reaction = await Control(new { type = "reaction", reaction = "hearts" });
    Assert(!reaction.GetProperty("ok").GetBoolean(), "Stop blocks late reactions as well as video");
    var current = (await Control(new { type = "status" })).GetProperty("state").GetProperty("generation").GetInt64();
    await socket.SendAsync(JsonSerializer.SerializeToUtf8Bytes(new { type = "approve", generation = current }), WebSocketMessageType.Text, true, timeout.Token);
    if(!status.GetProperty("mediaAvailable").GetBoolean()) {
        await ReceiveType(socket,"error",timeout.Token);
        Assert(!(await Control(new {type="status"})).GetProperty("state").GetProperty("approved").GetBoolean(),"fresh Start still fails closed without the native engine");
    } else {
    await ReceiveType(socket, "approved", timeout.Token);
    for (int i=0;i<8;i++) { await Task.Delay(1000); status=await Control(new { type = "status" }); }
    Assert(status.GetProperty("state").GetProperty("privacyBlocked").GetBoolean(), "missing phone heartbeat expires camera consent");
    }
    socket.Abort();
}
finally { await Control(new { type = "revoke", deviceId = id }); }
Console.WriteLine($"{passed} real-server integration checks passed.");
