# Camber development build

This build is for controlled internal testing. Windows service routing,
two-account isolation, signing and physical phone certification remain release gates.

The complete setup and pairing walkthrough is [Connect your Android phone to
Windows](CONNECTING.md). Setup offers an installation-folder page, an optional
desktop shortcut and optional startup in the tray. Both shortcut choices apply
to all Windows users and never authorize camera sharing. Uncheck an option when
rerunning Setup to remove that shortcut.

Open **Camber.Desktop.exe** in the Windows package root. Its implementation files
are in **app**; keep that folder with the launcher. An installer registers the
virtual camera under Program Files. Do not register a camera DLL from a writable
development folder. Installation and updates stop when another Camber session is open.

Install the Android **internal** APK matching your phone's processor. Most current
phones use **arm64-v8a**. The universal APK contains every supported processor and
is an engineering fallback. Internal APKs use the existing development signing key;
they are not store releases. The AAB is retained for future signed store builds.

Connect both devices to the same trusted private network. In Windows, open
**Connect**, select the local network address and create a QR code. Scan it in
Camber on Android, then tap **Start sharing**. Select **Camber Camera** in your
meeting app and keep your existing microphone.

Both apps offer **Original**, **Natural** and **Low light** under video quality.
Original adds no image filter. Natural gently reduces noise and sharpens detail;
Low light reduces more noise and raises dark tones. Processing stays on the PC.
The applied preset and any fallback reason appear after the engine confirms it.

Choose 720p or 1080p as the requested maximum while sharing. Actual capture can
fall back when the network, phone, decoder or temperature cannot sustain it.
The meeting app's output size stays stable. A preset or quality selection never
starts the camera or gives consent to share.

Turn the phone naturally. Physical orientation is automatic even when Android's
screen auto-rotate is off. Fit shows the entire portrait image; Fill crops to the
output shape. Outgoing video defaults to natural, readable text. No manual
rotation setting is needed.

Keep the Android app open while sharing. App switching, focus loss, locking,
Pause, Stop and closing the app retain their privacy cutoff. Rotation alone must
keep the session running; physical-device validation remains required.

Closing Windows Camber stops sharing. Minimizing keeps it open. Never install
this development package over a production camera session for testing.
