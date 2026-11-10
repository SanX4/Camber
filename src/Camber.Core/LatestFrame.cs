namespace Camber.Core;

/// <summary>A bounded decoded-frame mailbox. Never use it for compressed dependency frames.</summary>
public sealed class LatestFrame<T> where T : class
{
    private T? frame;
    private long overwritten;
    public long Overwritten => Interlocked.Read(ref overwritten);
    public void Publish(T next)
    {
        var previous = Interlocked.Exchange(ref frame, next);
        if (previous is not null) { if (previous is IDisposable disposable) disposable.Dispose(); Interlocked.Increment(ref overwritten); }
    }
    public T? Take() => Interlocked.Exchange(ref frame, null);
    public void Clear() { if (Take() is IDisposable previous) previous.Dispose(); }
}
