namespace Camber.Core;

public enum ReactionKind { Hearts = 1, ThumbsUp, ThumbsDown, Balloons, Rain, Confetti, Lasers, Fireworks }
public sealed record VisualSettings(bool OutputMirror = false, bool PreviewMirror = false,
    bool ReactionsEnabled = true, bool GesturesEnabled = true, bool ReducedMotion = false, EnhancementMode Enhancement = EnhancementMode.Original);

/// <summary>Camera consent and generation fencing also apply to every reaction.</summary>
public sealed class ReactionGate(TimeProvider? clock = null)
{
    private readonly TimeProvider time = clock ?? TimeProvider.System;
    private readonly object gate = new();
    private DateTimeOffset? last;
    public bool TryTrigger(SessionSnapshot session, long generation, ReactionKind kind, bool enabled)
    {
        lock (gate)
        {
        if (!enabled || !session.Approved || session.PrivacyBlocked || generation != session.Generation ||
            session.Phase is not (SessionPhase.Ready or SessionPhase.Streaming) || !Enum.IsDefined(kind)) return false;
        var now = time.GetUtcNow();
        if (last is { } previous && now - previous < TimeSpan.FromSeconds(4)) return false;
        last = now;
        return true;
        }
    }
    public void Reset() { lock (gate) last = null; }
}

/// <summary>Bounded, generation-scoped replay protection for capability-negotiated reactions.</summary>
public sealed class ReactionRequests
{
    private readonly object gate = new();
    private readonly HashSet<Guid> seen = [];
    private long generation = -1;
    public bool Accept(string? id, long currentGeneration)
    {
        lock(gate)
        {
            if(!Guid.TryParseExact(id,"N",out var parsed)) return false;
            if(generation != currentGeneration) { seen.Clear();generation=currentGeneration; }
            // Fail closed when full rather than evicting IDs and admitting a replay.
            return seen.Count < 4096 && seen.Add(parsed);
        }
    }
}
