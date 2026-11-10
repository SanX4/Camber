using Microsoft.Win32;
namespace Camber.Tray;
public sealed class PrivacyMonitor : IDisposable
{
    private readonly Action stop;
    public PrivacyMonitor(Action stop) { this.stop = stop; SystemEvents.SessionSwitch += Session; SystemEvents.PowerModeChanged += Power; }
    private void Session(object sender, SessionSwitchEventArgs e) { if (e.Reason is SessionSwitchReason.SessionLock or SessionSwitchReason.SessionLogoff or SessionSwitchReason.ConsoleDisconnect or SessionSwitchReason.RemoteDisconnect) stop(); }
    private void Power(object sender, PowerModeChangedEventArgs e) { if (e.Mode == PowerModes.Suspend) stop(); }
    public void Dispose() { SystemEvents.SessionSwitch -= Session; SystemEvents.PowerModeChanged -= Power; }
}
