namespace Camber.Core;

/// <summary>Serialized authoritative consent state. Generation fences reject late media/acks.</summary>
public sealed class SessionController(TimeProvider? clock = null)
{
    private readonly object gate = new();
    private readonly TimeProvider time = clock ?? TimeProvider.System;
    private DateTimeOffset? disconnectedAt;
    private DateTimeOffset? idleAt;
    private SessionSnapshot state = new(SessionPhase.Unpaired, null, false, false, 0,
        "Connect your phone to get started.", new(CameraFacing.Back, VideoMode.FullHD, VideoMode.FullHD));
    public SessionSnapshot Snapshot { get { lock (gate) return state; } }

    public SessionSnapshot Pair(string deviceId)
    {
        lock (gate)
        {
            if (state.DeviceId is not null && state.DeviceId != deviceId && state.Approved)
                throw new InvalidOperationException("Stop the active phone before selecting another.");
            state = state with { DeviceId = deviceId, Phase = SessionPhase.AwaitingApproval,
                Approved = false, PrivacyBlocked = false, Generation = state.Generation + 1,
                Message = "Tap Start on your phone to approve camera sharing." };
            disconnectedAt = idleAt = null;
            return state;
        }
    }

    public SessionSnapshot Approve(string deviceId)
    {
        lock (gate)
        {
            RequireDevice(deviceId);
            if (state.PrivacyBlocked || state.Phase != SessionPhase.AwaitingApproval)
                throw new InvalidOperationException("Request a new session before approving.");
            state = state with { Phase = SessionPhase.Ready, Approved = true, Message = "Phone approved. Connecting video…" };
            return state;
        }
    }

    public SessionSnapshot ApproveRequest(string deviceId, long expectedGeneration,CameraFacing? requestedCamera=null)
    {
        lock (gate)
        {
            RequireDevice(deviceId);
            if (state.Generation != expectedGeneration) throw new InvalidOperationException("This Start request expired. Tap Start again.");
            if(requestedCamera is {} camera && !Enum.IsDefined(camera))throw new ArgumentException("Unknown camera.");
            if (state.Phase == SessionPhase.Paused) ResumeFromPhone(deviceId);
            else {
                if (state.Phase is SessionPhase.Stopped or SessionPhase.Interrupted) Pair(deviceId);
                Approve(deviceId);
            }
            if(requestedCamera is {} selected)state=state with {Configuration=state.Configuration with {Camera=selected}};
            return state;
        }
    }

    public bool AcceptFrame(long generation)
    {
        lock (gate)
        {
            if (!state.Approved || state.PrivacyBlocked || generation != state.Generation ||
                state.Phase is not (SessionPhase.Ready or SessionPhase.Streaming or SessionPhase.Reconnecting)) return false;
            state = state with { Phase = SessionPhase.Streaming, Message = "Camera is sharing with this PC." };
            disconnectedAt = null;
            return true;
        }
    }

    public SessionSnapshot Pause(string reason = "Camera paused. No live video is being shared.")
    {
        lock (gate)
        {
            if (!state.Approved) return state;
            state = state with { Phase = SessionPhase.Paused, PrivacyBlocked = true,
                Generation = state.Generation + 1, Message = reason };
            return state;
        }
    }

    public SessionSnapshot ResumeFromPhone(string deviceId)
    {
        lock (gate)
        {
            RequireDevice(deviceId);
            if (state.Phase != SessionPhase.Paused) throw new InvalidOperationException("Start a new session on your phone.");
            state = state with { Phase = SessionPhase.Ready, Approved = true, PrivacyBlocked = false,
                Generation = state.Generation + 1, Message = "Resuming camera…" };
            return state;
        }
    }

    public SessionSnapshot Stop(string reason = "Camera stopped. Start again on your phone.")
    {
        lock (gate)
        {
            state = state with { Phase = SessionPhase.Stopped, Approved = false, PrivacyBlocked = true,
                Generation = state.Generation + 1, Message = reason };
            disconnectedAt = idleAt = null;
            return state;
        }
    }

    public SessionSnapshot Disconnect()
    {
        lock (gate)
        {
            if (state.Approved && !state.PrivacyBlocked)
                return Stop("Connection closed. Start again on your phone.");
            return state;
        }
    }

    public SessionSnapshot Tick(bool externalConsumer, bool previewVisible)
    {
        lock (gate)
        {
            var now = time.GetUtcNow();
            if (disconnectedAt is { } lost && now - lost >= TimeSpan.FromSeconds(30))
                return Stop("Connection expired. Start again on your phone.");
            if (!externalConsumer && !previewVisible && state.Approved && !state.PrivacyBlocked)
            {
                idleAt ??= now;
                if (now - idleAt >= TimeSpan.FromSeconds(5)) return Pause();
            }
            else idleAt = null;
            return state;
        }
    }

    public SessionSnapshot ApplyAcknowledgment(Acknowledgment ack)
    {
        lock (gate)
        {
            if (!ack.Ok || ack.Generation != state.Generation || ack.Applied is null || state.PrivacyBlocked) return state;
            var config = ack.Applied;
            if (!config.Capture.IsLaunchMode || !Enum.IsDefined(config.Camera) || !Enum.IsDefined(config.Framing) || config.Output != state.Configuration.Output ||
                config.Rotation is not (0 or 90 or 180 or 270)) throw new ArgumentException("Invalid applied configuration.");
            state = state with { Configuration = config };
            return state;
        }
    }

    public void SetOutputMirror(bool mirror)
    {
        lock (gate) state = state with { Configuration = state.Configuration with { OutputMirror = mirror } };
    }

    private void RequireDevice(string id)
    {
        if (state.DeviceId != id) throw new UnauthorizedAccessException("This phone is not selected.");
    }
}
