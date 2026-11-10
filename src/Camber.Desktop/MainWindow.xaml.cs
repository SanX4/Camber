using System;
using System.IO;
using System.Linq;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;
using Camber.Core;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using QRCoder;
using Windows.Devices.Enumeration;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.Core;
using Windows.Storage.Streams;
using Windows.System;

namespace Camber.Desktop;
public sealed partial class MainWindow : Window
{
    private readonly HostClient host = new();
    private readonly DispatcherTimer timer = new() { Interval = TimeSpan.FromSeconds(1) };
    private readonly Camber.Tray.CameraTray tray;
    private MediaCapture? capture;
    private bool polling, quitting, closingStarted;
    private string deviceSignature = "";
    private JsonElement lastStatus;
    private bool syncingVisuals = true, savingVisuals;
    private string visualSignature = "";
    private string configurationSignature = "";
    private readonly Windows.UI.ViewManagement.AccessibilitySettings accessibility = new();
    private DateTimeOffset warningUntil, applyDeadline;
    private Configuration? pendingConfiguration;
    private bool uiReady;
    private bool titleBarHighContrast;
    public MainWindow()
    {
        InitializeComponent();
        uiReady = true;
        AppWindow.Resize(new(1180, 860));
        UpdateTitleBar();
        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "Camber.ico");
        if (File.Exists(iconPath)) AppWindow.SetIcon(iconPath);
        tray = new(() => DispatcherQueue.TryEnqueue(() => { AppWindow.Show(); Activate(); }),
            () => DispatcherQueue.TryEnqueue(async () => await Request(new { type = "pause" })),
            () => DispatcherQueue.TryEnqueue(async () => await Request(new { type = "stop" })),
            () => DispatcherQueue.TryEnqueue(async () => await Quit()));
        AppWindow.Closing += async (_, e) => { if (quitting) return; e.Cancel = true; if (!closingStarted) { closingStarted = true; await Quit(); } };
        foreach (var address in NetworkInterface.GetAllNetworkInterfaces().Where(n => n.OperationalStatus == OperationalStatus.Up)
            .SelectMany(n => n.GetIPProperties().UnicastAddresses).Where(a => a.Address.AddressFamily == AddressFamily.InterNetwork && !System.Net.IPAddress.IsLoopback(a.Address)))
            NetworkChoice.Items.Add(address.Address.ToString());
        if (NetworkChoice.Items.Count > 0) NetworkChoice.SelectedIndex = 0;
        timer.Tick += async (_, _) => await Poll(); timer.Start();
        _ = InitializeHost();
    }
    private async Task InitializeHost()
    {
        try { await host.Request(new { type = "status" }); }
        catch { try { host.Launch(); await Task.Delay(1500); } catch (Exception e) { ShowError(e.Message); } }
        await Poll();
        if (!new Windows.UI.ViewManagement.UISettings().AnimationsEnabled) ReducedMotionChoice.IsOn = true;
    }
    private async Task<JsonElement?> Request(object command)
    {
        try
        {
            var result = await host.Request(command);
            if (result.TryGetProperty("error", out var error)) { ShowError(error.GetString() ?? "This action could not be completed."); return null; }
            return result;
        }
        catch (Exception e) { ShowError(e.Message); return null; }
    }
    private async Task Poll()
    {
        if (polling) return; polling = true;
        try
        {
            if (titleBarHighContrast != accessibility.HighContrast) UpdateTitleBar();
            lastStatus = await host.Request(new { type = "status" });
            var state = lastStatus.GetProperty("state");
            var appliedConfiguration = state.GetProperty("configuration");
            if (configurationSignature != appliedConfiguration.GetRawText())
            {
                configurationSignature = appliedConfiguration.GetRawText();
                CameraChoice.SelectedIndex = appliedConfiguration.GetProperty("camera").GetString() == "front" ? 1 : 0;
                QualityChoice.SelectedIndex = appliedConfiguration.GetProperty("capture").GetProperty("height").GetInt32() == 720 ? 1 : 0;
                FitChoice.SelectedIndex = appliedConfiguration.GetProperty("framing").GetString() == "fill" ? 1 : 0;
            }
            LiveStatus.Text = state.GetProperty("message").GetString();
            var available = lastStatus.GetProperty("mediaAvailable").GetBoolean();
            if (!available)
            {
                Notice.Title = "Camera engine needs attention"; Notice.Message = lastStatus.GetProperty("mediaStatus").GetString();
                Notice.Severity = InfoBarSeverity.Warning; Notice.IsOpen = true;
            }
            else if (DateTimeOffset.UtcNow >= warningUntil) Notice.IsOpen = false;
            var approved = state.GetProperty("approved").GetBoolean();
            var active = approved && !state.GetProperty("privacyBlocked").GetBoolean();
            var phase = state.GetProperty("phase").GetString();
            var paired = state.GetProperty("deviceId").ValueKind != JsonValueKind.Null;
            SessionStatus.Text = phase == "streaming" ? "CAMERA SHARING" : active ? "CONNECTING VIDEO" : phase == "paused" ? "CAMERA PAUSED" : paired ? "PHONE PAIRED" : "CAMERA OFF";
            SessionHeading.Text = phase == "streaming" ? "You're live." : active ? "Your phone is ready." : phase == "paused" ? "Take your time." : paired ? "Ready when you are." : "Your camera stays yours.";
            PreviewTitle.Text = active ? "Your camera is ready." : phase == "paused" ? "Your camera is paused." : paired ? "Start on your phone." : "A better view starts here.";
            PreviewHint.Text = active ? "Open your preview here, or select Camber Camera in your meeting app." : paired ? "Tap Start sharing in Camber on your phone. Keep the app open." : "Connect your phone, then choose Camber Camera in your meeting app.";
            EmptyAction.Content = active ? "Open preview" : paired ? "View connection" : "Connect a phone";
            StopButton.IsEnabled = approved; PauseButton.IsEnabled = active;
            var appliedCamera = appliedConfiguration.GetProperty("camera").GetString() == "front" ? "Front" : "Rear";
            AppliedCameraText.Text = active ? $"On phone: {appliedCamera} camera · {appliedConfiguration.GetProperty("capture").GetProperty("height").GetInt32()}p" : "Choose your view before your next call.";
            if (!active) pendingConfiguration = null;
            if (pendingConfiguration is { } pending)
            {
                var matches = appliedConfiguration.GetProperty("camera").GetString() == (pending.Camera == CameraFacing.Front ? "front" : "back") &&
                    appliedConfiguration.GetProperty("capture").GetProperty("height").GetInt32() == pending.Capture.Height &&
                    appliedConfiguration.GetProperty("framing").GetString() == (pending.Framing == Framing.Fill ? "fill" : "fit");
                if (matches) { pendingConfiguration = null; ApplyHint.Text = "Applied on your phone."; }
                else if (DateTimeOffset.UtcNow > applyDeadline) { pendingConfiguration = null; ApplyHint.Text = "Your phone did not confirm. Try applying again."; }
            }
            ApplyButton.IsEnabled = active && pendingConfiguration is null;
            ApplyButton.Content = pendingConfiguration is null ? "Apply on phone" : "Applying…";
            if (!active) ApplyHint.Text = "Start sharing on your phone to apply camera controls.";
            else if (pendingConfiguration is not null) ApplyHint.Text = "Waiting for your phone to confirm the change…";
            else if (ApplyHint.Text == "Start sharing on your phone to apply camera controls.") ApplyHint.Text = "Changes appear after your phone confirms them.";
            if (!savingVisuals && lastStatus.TryGetProperty("visuals", out var visuals))
            {
                var settingsSignature = visuals.GetRawText();
                if (settingsSignature != visualSignature)
                {
                    syncingVisuals = true;
                    EnhancementChoice.SelectedIndex = visuals.TryGetProperty("enhancement",out var preset)?preset.GetString() switch {"natural"=>1,"lowLight"=>2,_=>0}:0;
                    MirrorChoice.IsOn = visuals.GetProperty("outputMirror").GetBoolean();
                    PreviewMirrorChoice.IsOn = visuals.GetProperty("previewMirror").GetBoolean();
                    ReactionsChoice.IsOn = visuals.GetProperty("reactionsEnabled").GetBoolean();
                    GesturesChoice.IsOn = visuals.GetProperty("gesturesEnabled").GetBoolean();
                    ReducedMotionChoice.IsOn = visuals.GetProperty("reducedMotion").GetBoolean();
                    PreviewTransform.ScaleX = PreviewMirrorChoice.IsOn ? -1 : 1;
                    visualSignature = settingsSignature; syncingVisuals = false;
                }
                foreach (var button in ReactionButtons.Children.OfType<Button>()) button.IsEnabled = ReactionsChoice.IsOn && state.GetProperty("approved").GetBoolean() && !state.GetProperty("privacyBlocked").GetBoolean();
                ReactionStatus.Text = !active ? "Start sharing on your phone to use reactions." : !ReactionsChoice.IsOn ? "Reactions are off." : !GesturesChoice.IsOn ? "Use the buttons below to react." :
                    lastStatus.GetProperty("effects").GetProperty("gestureAvailable").GetBoolean() ? "Gestures are ready. Keep both hands visible and away from your face." :
                    "Hand gestures are unavailable on this phone. Use the buttons below to react.";
            }
            if(lastStatus.TryGetProperty("quality",out var quality)) {
                var actual=quality.GetProperty("actual");var enhancement=quality.GetProperty("enhancement");
                RetryEnhancementButton.Visibility=active && enhancement.GetProperty("hasFallback").GetBoolean()?Visibility.Visible:Visibility.Collapsed;
                var appliedName=enhancement.GetProperty("applied").GetString() switch {"natural"=>"Natural","lowLight"=>"Low light",_=>"Original"};
                EnhancementStatus.Text=active ? $"Using {appliedName}. {enhancement.GetProperty("reason").GetString()}" : "Selected preset applies when sharing.";
                ActualQualityStatus.Text=active && actual.GetProperty("height").ValueKind!=JsonValueKind.Null ?
                    $"Actual capture: {actual.GetProperty("width")} × {actual.GetProperty("height")}. "+
                    (actual.GetProperty("encodedWidth").ValueKind==JsonValueKind.Number && actual.GetProperty("encodedHeight").ValueKind==JsonValueKind.Number?$"Encoded video: {actual.GetProperty("encodedWidth")} × {actual.GetProperty("encodedHeight")}. ":"")+
                    actual.GetProperty("reason").GetString() : "Starts only when you share.";
            }
            string measured(JsonElement data,string key,string unit="",double scale=1) => data.TryGetProperty(key,out var v) && v.ValueKind==JsonValueKind.Number ? $"{v.GetDouble()*scale:0.##}{unit}" : "unknown";
            var q=lastStatus.GetProperty("quality");var samples=q.GetProperty("sample");var orientation=q.GetProperty("orientation");
            var qualityDetails=$"Capture: {measured(samples,"captureFps"," fps")} · encoded: {measured(samples,"encodedFps"," fps")}\n"+
                $"Bitrate: {measured(samples,"bitrateBps"," Mbps",.000001)} · RTT: {measured(samples,"networkRttMs"," ms")} · loss: {measured(samples,"lossRatio","%",100)}\n"+
                $"Encode: {measured(samples,"encodeMs"," ms")} · decode: {measured(samples,"decodeMs"," ms")}\n"+
                $"Rotation: Android source {measured(orientation,"sourceRotation","°")} → normalized {measured(orientation,"captureRotation","°")} → PC {measured(orientation,"decodedRotation","°")}\n";
            Diagnostics.Text = qualityDetails+$"Effects backend: {lastStatus.GetProperty("video").GetProperty("effectsBackend").GetString()}\nProtocol: 1\nMedia engine: {(available ? "Running" : "Unavailable")}\nSession: {state.GetProperty("phase").GetString()}\nGlass-to-glass latency: not measured\nNo camera frames or pairing credentials are included in this summary.";
            var devices = lastStatus.GetProperty("devices");
            var signature = devices.GetRawText();
            if (signature != deviceSignature)
            {
                deviceSignature = signature; DeviceList.Children.Clear();
                DevicesEmpty.Visibility = devices.GetArrayLength() == 0 ? Visibility.Visible : Visibility.Collapsed;
                foreach (var d in devices.EnumerateArray())
                {
                    var id = d.GetProperty("id").GetString();
                    var panel = new StackPanel { Spacing = 8 };
                    panel.Children.Add(new TextBlock { Text = d.GetProperty("name").GetString(), FontSize = 18 });
                    panel.Children.Add(new TextBlock { Text = d.GetProperty("platform").GetString() });
                    var revoke = new Button { Content = "Remove pairing" };
                    revoke.Click += async (_, _) => await Request(new { type = "revoke", deviceId = id });
                    panel.Children.Add(revoke); DeviceList.Children.Add(new Border { Style = (Style)Application.Current.Resources["CamberPanel"], Child = panel });
                }
            }
        }
        catch (Exception e) { ShowError("Camera service is unavailable. " + e.Message); }
        finally { polling = false; }
    }
    private void ShowError(string text) { warningUntil = DateTimeOffset.UtcNow.AddSeconds(8); Notice.Title = "Camber needs your attention"; Notice.Message = text; Notice.Severity = InfoBarSeverity.Warning; Notice.IsOpen = true; }
    private void Navigate(string tag)
    {
        CameraPage.Visibility = tag == "camera" ? Visibility.Visible : Visibility.Collapsed;
        ConnectPage.Visibility = tag == "connect" ? Visibility.Visible : Visibility.Collapsed;
        DevicesPage.Visibility = tag == "devices" ? Visibility.Visible : Visibility.Collapsed;
        SettingsPage.Visibility = tag == "settings" ? Visibility.Visible : Visibility.Collapsed;
        PageScroll.ChangeView(null, 0, null, true);
        var target = tag switch { "connect" => (FrameworkElement)ConnectPage, "devices" => DevicesPage, "settings" => SettingsPage, _ => CameraPage };
        if (new Windows.UI.ViewManagement.UISettings().AnimationsEnabled)
        {
            var visual = Microsoft.UI.Xaml.Hosting.ElementCompositionPreview.GetElementVisual(target);
            var fade = visual.Compositor.CreateScalarKeyFrameAnimation();
            var ease = visual.Compositor.CreateCubicBezierEasingFunction(new System.Numerics.Vector2(.16f, 1), new System.Numerics.Vector2(.3f, 1));
            fade.InsertKeyFrame(0, 0.35f); fade.InsertKeyFrame(1, 1, ease);
            fade.Duration = TimeSpan.FromMilliseconds(220);
            visual.StartAnimation("Opacity", fade);
        }
    }
    private void Navigation_SelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
    { if (CameraPage is not null && args.SelectedItem is NavigationViewItem item) Navigate((string)item.Tag); }
    private void Connect_Click(object sender, RoutedEventArgs e) { Navigation.SelectedItem = Navigation.MenuItems[1]; }
    private void EmptyAction_Click(object sender, RoutedEventArgs e)
    { if (EmptyAction.Content?.ToString() == "Open preview") Preview_Click(sender, e); else Connect_Click(sender, e); }
    private async void Stop_Click(object sender, RoutedEventArgs e) { await Request(new { type = "stop" }); }
    private async void Pause_Click(object sender, RoutedEventArgs e) { await Request(new { type = "pause" }); }
    private async void Pair_Click(object sender, RoutedEventArgs e)
    {
        if (NetworkChoice.SelectedItem is not string address) { ShowError("Connect this PC to your local network first."); return; }
        var payload = await Request(new { type = "pairing", address });
        if (payload is null) return;
        using var qr = QRCodeGenerator.GenerateQrCode(payload.Value.GetRawText(), QRCodeGenerator.ECCLevel.M);
        var png = new PngByteQRCode(qr).GetGraphic(8);
        using var stream = new InMemoryRandomAccessStream();
        using (var writer = new DataWriter(stream)) { writer.WriteBytes(png); await writer.StoreAsync(); writer.DetachStream(); }
        stream.Seek(0); var bitmap = new BitmapImage(); await bitmap.SetSourceAsync(stream); PairingQr.Source = bitmap; QrPanel.Visibility = Visibility.Visible;
        PairingExpiry.Text = "Scan in Camber on your phone. Expires at " + payload.Value.GetProperty("expiresAt").GetDateTimeOffset().ToLocalTime().ToString("HH:mm:ss") + ".";
    }
    private async void Apply_Click(object sender, RoutedEventArgs e)
    {
        var config = new Configuration(CameraChoice.SelectedIndex == 1 ? CameraFacing.Front : CameraFacing.Back,
            QualityChoice.SelectedIndex == 1 ? VideoMode.HD : VideoMode.FullHD, VideoMode.FullHD,
            FitChoice.SelectedIndex == 1 ? Framing.Fill : Framing.Fit, OutputMirror: MirrorChoice.IsOn);
        var options = new JsonSerializerOptions(JsonSerializerDefaults.Web); options.Converters.Add(new JsonStringEnumConverter(JsonNamingPolicy.CamelCase));
        pendingConfiguration = config; applyDeadline = DateTimeOffset.UtcNow.AddSeconds(10);
        ApplyButton.IsEnabled = false; ApplyButton.Content = "Applying…"; ApplyHint.Text = "Waiting for your phone to confirm the change…";
        if (await Request(new { type = "configure", configuration = JsonSerializer.SerializeToElement(config, options) }) is null)
        { pendingConfiguration = null; ApplyButton.Content = "Apply on phone"; ApplyHint.Text = "Camera controls could not be applied. Try again."; }
    }
    private async void Visuals_Changed(object sender, RoutedEventArgs e)
    {
        if (syncingVisuals || savingVisuals) return;
        savingVisuals = true;
        try
        {
            PreviewTransform.ScaleX = PreviewMirrorChoice.IsOn ? -1 : 1;
            var settings = new VisualSettings(MirrorChoice.IsOn, PreviewMirrorChoice.IsOn, ReactionsChoice.IsOn, GesturesChoice.IsOn,
                ReducedMotionChoice.IsOn || !new Windows.UI.ViewManagement.UISettings().AnimationsEnabled,
                (EnhancementMode)Math.Max(0,EnhancementChoice.SelectedIndex));
            await Request(new { type = "visuals", settings });
        }
        finally { savingVisuals = false; }
    }
    private async void RetryEnhancement_Click(object sender,RoutedEventArgs e) {
        RetryEnhancementButton.IsEnabled=false;
        try {await Request(new {type="enhancement",mode=(EnhancementMode)Math.Max(0,EnhancementChoice.SelectedIndex)});}
        finally {RetryEnhancementButton.IsEnabled=true;}
    }
    private async void Enhancement_Changed(object sender,SelectionChangedEventArgs e) {
        if(!uiReady || syncingVisuals)return;
        EnhancementStatus.Text="Applying preset…";
        await Request(new {type="enhancement",mode=(EnhancementMode)Math.Max(0,EnhancementChoice.SelectedIndex)});
        await Poll();
    }
    private async void Reaction_Click(object sender, RoutedEventArgs e)
    {
        var result = await Request(new { type = "reaction", reaction = ((Button)sender).Tag.ToString() });
        if (result is { } response) ReactionStatus.Text = response.GetProperty("message").GetString();
    }
    private async void Preview_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            var devices = await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture);
            var device = devices.FirstOrDefault(d => d.Name.StartsWith("Camber Camera", StringComparison.Ordinal));
            if (device is null) { ShowError("Camber Camera is not registered. Build and install the native camera component first."); return; }
            capture?.Dispose(); capture = new MediaCapture();
            await capture.InitializeAsync(new() { VideoDeviceId = device.Id, StreamingCaptureMode = StreamingCaptureMode.Video, SharingMode = MediaCaptureSharingMode.SharedReadOnly });
            var source = capture.FrameSources.Values.First(f => f.Info.SourceKind == MediaFrameSourceKind.Color);
            Preview.Source = MediaSource.CreateFromMediaFrameSource(source); PreviewEmpty.Visibility = Visibility.Collapsed;
            PreviewSurface.Height = Math.Clamp(PreviewPanel.ActualWidth * 9 / 16, 280, 480);
        }
        catch (Exception ex) { ShowError("Cannot open camera preview: " + ex.Message); }
    }
    private void CameraLayout_SizeChanged(object sender, SizeChangedEventArgs e)
    {
        bool narrow = e.NewSize.Width < 880;
        Grid.SetColumn(ControlsPanel, narrow ? 0 : 1); Grid.SetRow(ControlsPanel, narrow ? 1 : 0);
        ControlsColumn.Width = narrow ? new GridLength(0) : new GridLength(352);
        PreviewSurface.MinHeight = Math.Clamp((e.NewSize.Width - (narrow ? 0 : 376)) * 9 / 16, 280, 480);
        PreviewSurface.Height = Preview.Source is null ? double.NaN : PreviewSurface.MinHeight;
        Grid.SetRow(PreviewActions, narrow || PreviewPanel.ActualWidth < 580 ? 1 : 0);
        Grid.SetColumn(PreviewActions, narrow || PreviewPanel.ActualWidth < 580 ? 0 : 1);
        var compact = Root.ActualWidth < 800;
        HeaderPanel.Padding = new Thickness(compact ? 20 : 32, 20, compact ? 20 : 32, 20);
        PageContent.Padding = new Thickness(compact ? 20 : 32, 24, compact ? 20 : 32, 32);
        FooterPanel.Padding = new Thickness(compact ? 20 : 32, 14, compact ? 20 : 32, 14);
    }
    private void Theme_Changed(object sender, SelectionChangedEventArgs e)
    { if (Root is not null) Root.RequestedTheme = ((ComboBox)sender).SelectedIndex switch { 1 => ElementTheme.Light, 2 => ElementTheme.Default, _ => ElementTheme.Dark }; }
    private void Root_ActualThemeChanged(FrameworkElement sender, object args) { if (uiReady) UpdateTitleBar(); }
    private void UpdateTitleBar()
    {
        if (!Microsoft.UI.Windowing.AppWindowTitleBar.IsCustomizationSupported()) return;
        titleBarHighContrast = accessibility.HighContrast;
        if (accessibility.HighContrast) { AppWindow.TitleBar.ResetToDefault(); return; }
        var resources = (ResourceDictionary)Application.Current.Resources.ThemeDictionaries[Root.ActualTheme == ElementTheme.Dark ? "Dark" : "Light"];
        var background = ((SolidColorBrush)resources["CamberBackground"]).Color;
        var foreground = ((SolidColorBrush)resources["CamberInk"]).Color;
        var muted = ((SolidColorBrush)resources["CamberMuted"]).Color;
        var hover = ((SolidColorBrush)resources["CamberSoft"]).Color;
        var bar = AppWindow.TitleBar;
        bar.BackgroundColor = bar.InactiveBackgroundColor = bar.ButtonBackgroundColor = bar.ButtonInactiveBackgroundColor = background;
        bar.ForegroundColor = bar.ButtonForegroundColor = bar.ButtonHoverForegroundColor = bar.ButtonPressedForegroundColor = foreground;
        bar.InactiveForegroundColor = bar.ButtonInactiveForegroundColor = muted;
        bar.ButtonHoverBackgroundColor = bar.ButtonPressedBackgroundColor = hover;
    }
    private async void Permissions_Click(object sender, RoutedEventArgs e) { await Launcher.LaunchUriAsync(new Uri("ms-settings:privacy-webcam")); }
    private async void Export_Click(object sender, RoutedEventArgs e)
    {
        var picker = new Windows.Storage.Pickers.FileSavePicker(); WinRT.Interop.InitializeWithWindow.Initialize(picker, WinRT.Interop.WindowNative.GetWindowHandle(this));
        picker.SuggestedFileName = "Camber-diagnostics"; picker.FileTypeChoices.Add("Text file", new[] { ".txt" });
        var file = await picker.PickSaveFileAsync(); if (file is not null) await Windows.Storage.FileIO.WriteTextAsync(file, Diagnostics.Text);
    }
    private async Task Quit() { await Request(new { type = "shutdown" }); quitting = true; timer.Stop(); capture?.Dispose(); tray.Dispose(); Close(); }
    private async void Quit_Click(object sender, RoutedEventArgs e) { await Quit(); }
}
