using System.Diagnostics;
using System.Security.Principal;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Channels;

namespace Camber.Host;

public sealed class MediaWorker : IAsyncDisposable
{
    private Process? process;
    private int ready;
    private long offersSent, candidatesSent, framesPublished;
    private string effectsBackend = "not-initialized";
    private string videoStage = "waiting", lastVideoError = "";
    public object Diagnostics => new { stage = videoStage, offersSent = Interlocked.Read(ref offersSent),
        candidatesSent = Interlocked.Read(ref candidatesSent), framesPublished = Interlocked.Read(ref framesPublished), error = lastVideoError, effectsBackend };
    private readonly SemaphoreSlim writeGate = new(1);
    private static readonly JsonSerializerOptions wireJson = CreateJson();
    private static JsonSerializerOptions CreateJson()
    {
        var options = new JsonSerializerOptions(JsonSerializerDefaults.Web);
        options.Converters.Add(new JsonStringEnumConverter(JsonNamingPolicy.CamelCase));
        return options;
    }
    public Channel<JsonElement> Events { get; } = Channel.CreateBounded<JsonElement>(new BoundedChannelOptions(64) { FullMode = BoundedChannelFullMode.DropOldest });
    public bool Available => Volatile.Read(ref ready) == 1 && process is { HasExited: false };
    public string State { get; private set; } = "missing";
    public string Status { get; private set; } = "Native media engine is not installed. Build the native targets to enable video.";
    public async Task Start()
    {
        var path = Path.Combine(AppContext.BaseDirectory, "Camber.Media.exe");
        if (!File.Exists(path)) return;
        var sid = WindowsIdentity.GetCurrent().User!.Value;
        process = Process.Start(new ProcessStartInfo(path, sid) { UseShellExecute = false, CreateNoWindow = true,
            RedirectStandardInput = true, RedirectStandardOutput = true, RedirectStandardError = true });
        if (process is null) return;
        State = "starting";
        Status = "Starting the Windows camera engine…";
        _ = Task.Run(async () =>
        {
            try
            {
                while (await process.StandardOutput.ReadLineAsync() is { } line)
                {
                    if (line.Length > 65_536) continue;
                    using var doc = JsonDocument.Parse(line);
                    var root = doc.RootElement;
                    var type = root.GetProperty("type").GetString();
                    if (type == "effectsStatus") effectsBackend = root.GetProperty("backend").GetString() ?? "unknown";
                    if (type == "videoStatus") videoStage = root.GetProperty("stage").GetString() ?? "unknown";
                    if (type == "frame") { Interlocked.Increment(ref framesPublished); videoStage = "delivering"; }
                    if (type == "error") lastVideoError = root.TryGetProperty("message", out var videoError) ? videoError.GetString() ?? "" : "Video failed.";
                    if (type == "ready")
                    {
                        Volatile.Write(ref ready, 1); State = "ready";
                        Status = "Camera engine ready. Start sharing on your phone.";
                    }
                    else if (type == "error" && Volatile.Read(ref ready) == 0)
                    {
                        State = "faulted";
                        var message = root.TryGetProperty("message", out var detail) ? detail.GetString() : null;
                        Status = string.IsNullOrWhiteSpace(message) ? "The camera engine could not start." : message[..Math.Min(message.Length, 512)];
                    }
                    await Events.Writer.WriteAsync(doc.RootElement.Clone());
                }
            }
            catch (Exception e) when (e is IOException or JsonException) { }
            finally
            {
                Volatile.Write(ref ready, 0);
                if (State != "faulted") { State = "stopped"; Status = "The camera engine stopped. Restart Camber to reconnect."; }
                Events.Writer.TryComplete();
            }
        });
        _ = Task.Run(async () => { while (await process.StandardError.ReadLineAsync() is not null) { } });
        await Task.CompletedTask;
    }
    public async Task Send(object message)
    {
        await writeGate.WaitAsync();
        try
        {
            if (process is not { HasExited: false }) throw new InvalidOperationException(Status);
            var command = JsonSerializer.SerializeToElement(message, wireJson);
            var type = command.GetProperty("type").GetString();
            if (type == "permit") { videoStage = "approved"; lastVideoError = ""; }
            if (type == "offer") { Interlocked.Increment(ref offersSent); videoStage = "offer-sent"; }
            if (type == "candidate") Interlocked.Increment(ref candidatesSent);
            await process!.StandardInput.WriteLineAsync(command.GetRawText());
            await process.StandardInput.FlushAsync();
        }
        finally { writeGate.Release(); }
    }
    public async Task Disable(long generation)
    {
        if (process is { HasExited: false }) await Send(new { type = "stop", generation });
    }
    public async ValueTask DisposeAsync()
    {
        if (process is { HasExited: false })
        {
            process!.StandardInput.Close();
            using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(2));
            try { await process.WaitForExitAsync(timeout.Token); } catch (OperationCanceledException) { process.Kill(true); }
        }
        process?.Dispose();
    }
}
