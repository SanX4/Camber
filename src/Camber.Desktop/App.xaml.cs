using Microsoft.UI.Xaml;
using System;
using System.Linq;
namespace Camber.Desktop;
public partial class App : Application
{
    private MainWindow? window;
    public App() { InitializeComponent(); }
    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        window = new MainWindow();
        // Sign-in only opens the tray utility. Capture still requires a fresh,
        // approved phone session; the installer never grants camera consent.
        if (Environment.GetCommandLineArgs().Contains("--startup", StringComparer.OrdinalIgnoreCase))
            window.AppWindow.Hide();
        else
            window.Activate();
    }
}
