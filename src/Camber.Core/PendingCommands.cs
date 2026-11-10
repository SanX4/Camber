namespace Camber.Core;

/// <summary>Only an outstanding request may update the applied configuration.</summary>
public sealed class PendingCommands
{
    private readonly object gate = new();
    private readonly Dictionary<string, (long Generation, DateTimeOffset Expires)> pending = [];
    public void Add(string id, long generation, DateTimeOffset now)
    {
        lock (gate)
        {
            foreach (var expired in pending.Where(x => x.Value.Expires <= now).Select(x => x.Key).ToArray()) pending.Remove(expired);
            if (pending.Count >= 16) throw new InvalidOperationException("Wait for your phone to apply the previous changes.");
            pending[id] = (generation, now.AddSeconds(10));
        }
    }
    public bool Consume(Acknowledgment ack, DateTimeOffset now)
    {
        lock (gate)
        {
            if (!pending.Remove(ack.Id, out var request)) return false;
            return request.Generation == ack.Generation && now < request.Expires;
        }
    }
    public void Clear() { lock (gate) pending.Clear(); }
    public bool IsPending(DateTimeOffset now) { lock(gate)return pending.Any(p=>p.Value.Expires>now); }
    public void AddExclusive(string id,long generation,DateTimeOffset now)
    {
        lock(gate)
        {
            if(IsPending(now))throw new InvalidOperationException("Wait for your phone to apply the previous change.");
            Add(id,generation,now);
        }
    }
}
