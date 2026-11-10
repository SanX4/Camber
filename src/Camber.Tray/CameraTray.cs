using Microsoft.Win32;
namespace Camber.Tray;
public sealed class CameraTray : IDisposable
{
    private readonly System.Windows.Forms.NotifyIcon icon;
    private readonly System.Drawing.Icon? applicationIcon;
    private readonly Action privacyStop;
    public CameraTray(Action open, Action pause, Action stop, Action quit)
    {
        privacyStop = stop;
        var menu = new System.Windows.Forms.ContextMenuStrip();
        menu.Items.Add("Open Camber", null, (_, _) => open());
        menu.Items.Add("Pause camera", null, (_, _) => pause());
        menu.Items.Add("Stop sharing", null, (_, _) => stop());
        menu.Items.Add("Quit Camber", null, (_, _) => quit());
        var iconPath = System.IO.Path.Combine(AppContext.BaseDirectory, "Assets", "Camber.ico");
        if (System.IO.File.Exists(iconPath)) applicationIcon = new System.Drawing.Icon(iconPath);
        icon = new() { Icon = applicationIcon ?? System.Drawing.SystemIcons.Application, Text = "Camber Camera", Visible = true, ContextMenuStrip = menu };
        icon.DoubleClick += (_, _) => open();
        SystemEvents.SessionSwitch += OnSessionSwitch;
        SystemEvents.PowerModeChanged += OnPowerMode;
    }
    private void OnSessionSwitch(object sender, SessionSwitchEventArgs args) { if (args.Reason == SessionSwitchReason.SessionLock) privacyStop(); }
    private void OnPowerMode(object sender, PowerModeChangedEventArgs args) { if (args.Mode == PowerModes.Suspend) privacyStop(); }
    public void Dispose() { SystemEvents.SessionSwitch -= OnSessionSwitch; SystemEvents.PowerModeChanged -= OnPowerMode; icon.Dispose(); applicationIcon?.Dispose(); }
}
