using System;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Security.Principal;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Camber.Desktop;
internal sealed class HostClient
{
    private readonly SemaphoreSlim gate = new(1);
    public async Task<JsonElement> Request(object command)
    {
        await gate.WaitAsync();
        try
        {
            using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(3));
            await using var pipe = new NamedPipeClientStream(".", "Camber.Control." + WindowsIdentity.GetCurrent().User!.Value,
                PipeDirection.InOut, PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
            await pipe.ConnectAsync(timeout.Token);
            await using var writer = new StreamWriter(pipe, new UTF8Encoding(false), 4096, true) { AutoFlush = true };
            using var reader = new StreamReader(pipe, Encoding.UTF8, false, 4096, true);
            await writer.WriteLineAsync(JsonSerializer.Serialize(command));
            var line = await reader.ReadLineAsync(timeout.Token) ?? throw new IOException("Camera service disconnected.");
            using var doc = JsonDocument.Parse(line);
            return doc.RootElement.Clone();
        }
        finally { gate.Release(); }
    }
    public void Launch()
    {
        var path = Path.Combine(AppContext.BaseDirectory, "Camber.Host.exe");
        if (!File.Exists(path)) throw new FileNotFoundException("Camera service is missing. Build or repair the complete Camber package.");
        Process.Start(new ProcessStartInfo(path) { UseShellExecute = false, CreateNoWindow = true });
    }
}
