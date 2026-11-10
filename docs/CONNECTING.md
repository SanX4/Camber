# Connect your Android phone to Windows

Camber is currently a development build. Android and Windows are supported;
iPhone support is deferred. See [release status](STATUS.md) before distributing
it. Download packages only from the project's maintainer and verify the published
checksums. A successful build is not a security certification.

## 1. Install on Windows

1. Use a supported Windows 11 x64 PC. Close Camber on all Windows accounts before
   installing or updating; Setup refuses to interrupt an active session.
2. Run `Camber-Setup-<version>.exe`. Windows asks for administrator access because
   Setup installs the virtual-camera component and private-network firewall rules.
   Normal use does not require running Camber as administrator.
3. Read the welcome page, then choose the installation folder. The default is
   `C:\Program Files\Camber`. Keep the application's files in a protected location.
4. On the optional-tasks page, choose a desktop shortcut and/or startup at
   Windows sign-in. Both are optional. They apply to all users on this PC.
   Startup opens Camber in the tray; it never starts sharing your phone camera.
5. Review your selections, install, then choose **Open Camber**. You can also
   open **Camber Camera** from Start or run `Camber.Desktop.exe` in its installed
   root folder. Keep the `app` subfolder with that launcher.

During an update, keep the existing folder unless following a tested migration
procedure. Pairing and preferences are stored separately per Windows user.
Unchecking a shortcut task during an update removes that Camber shortcut.

## 2. Install on Android

1. Use Android 10 or later and a phone with a usable camera and H.264 encoder.
2. Install the matching-version APK. Most current phones, including recent
   Samsung Galaxy and Pixel devices, use **arm64-v8a**. Other packages are
   **armeabi-v7a**, **x86**, **x86_64** and the larger **universal** fallback.
3. Open the APK on the phone. If Android asks, allow installation from the
   trusted app you used to open this specific package. Development APKs are not
   Google Play releases; never install a package from an unknown sender.
4. Open **Camber**. Allow camera access when you want to scan a QR code or use
   the camera. Camber does not need microphone access.

Upgrade over the existing app using the same signing identity to retain pairing.
Do not uninstall or clear app storage as an ordinary update step.

## 3. Put both devices on the same local network

1. Connect the phone to your trusted Wi-Fi. The PC can use that Wi-Fi or an
   Ethernet cable connected to the same router.
2. On a trusted home/work network, set the Windows network profile to **Private**
   so Camber's private-network firewall rules can apply. Do this only for a
   network you trust.
3. Avoid guest Wi-Fi or access points with client/AP isolation: they may prevent
   devices from communicating even when the network names look the same.
4. If a VPN is running, ensure its local-network option permits communication.
   Camber does not require a public server, public STUN server or TURN relay.

Never disable the Windows firewall or expose Camber ports on your router to make
pairing work. Signaling uses local TCP port 47771; media uses the application's
private-network UDP rule. Setup creates rules scoped to the app and local subnet.

## 4. Pair once

1. In Windows Camber, open **Connect**.
2. Select the PC's address on the phone's Wi-Fi network. Skip disconnected,
   virtual-machine and unrelated VPN adapters. For example, a phone on
   `192.168.1.x` will usually reach the PC's `192.168.1.x` address; subnet rules
   still depend on your router.
3. Create a fresh QR code. It expires after two minutes and can be used once.
4. In Android Camber, choose **Scan QR code** and scan the Windows screen.
5. Check that the PC identity is the one you intend to use. Wait for the phone
   to show that it is paired/connected. Pairing alone does not start video.

Treat the pairing QR code as private. Do not post it in screenshots or issue
reports. The phone pins the PC certificate during pairing and authenticated
signaling binds the encrypted WebRTC media session to the paired devices.

## 5. Start video and choose your camera

1. Keep Camber visible on the phone. Select **Rear camera** or **Front camera**.
2. Tap **Start sharing** on the phone. This is the approval to open and transmit
   the camera for the current session.
3. Wait for live video in Windows. Preview activity and a phone sharing message
   do not, by themselves, prove that a meeting participant receives the video.
4. In your meeting or recording app, select **Camber Camera** as the camera.
   Keep your normal PC/headset microphone selected; Camber transmits video only.

| Application | Where to choose the camera |
| --- | --- |
| Discord | User Settings > Voice & Video > Camera |
| Google Meet | Settings > Video > Camera |
| Microsoft Teams | Settings > Devices > Camera, or meeting device settings |
| Zoom | Settings > Video > Camera, or the camera arrow during a call |
| OBS Studio | Add Source > Video Capture Device > Device |
| Windows Camera | Switch cameras until Camber Camera is selected |

Menu names can change between application versions. If Camber was installed
while a meeting app was open, close and reopen that app so it refreshes devices.
Check the remote participant's view when validating the connection.

Official selection help: [Discord](https://support.discord.com/hc/en-us/articles/33030151293079-Discord-Voice-Video-Streaming-Guide),
[Google Meet](https://support.google.com/meet/answer/9302964),
[Teams](https://support.microsoft.com/en-us/teams/notifications-settings/manage-your-device-settings-in-microsoft-teams),
and [OBS](https://obsproject.com/kb/video-capture-sources).

## 6. Quality, rotation and reactions

- Choose a requested maximum of 720p30 or 1080p30. Camber may lower actual capture
  demand to recover from network congestion, processing limits or heat. Requested
  and applied values are shown separately.
- **Original** adds no image filter. **Natural** applies gentle processing;
  **Low light** reduces more noise and brightens shadows. Processing happens on
  the PC, and settings show their applied state after acknowledgment.
- Rotate the phone naturally; orientation is automatic. **Fit** shows the whole
  portrait image, while **Fill** crops it to fill the output.
- Outgoing video defaults to a natural view with readable text. Meeting apps
  may mirror their own local preview; ask another participant to check text.
- Enable reactions/hand gestures on Windows. Hold a clear pose for about a
  second. Keep both hands visible for paired gestures and leave four seconds
  between effects. Lower your hands or change the pose to rearm recognition.

| Gesture | Effect |
| --- | --- |
| One thumb up / one thumb down | Thumbs up / thumbs down |
| One peace sign / two peace signs | Balloons / confetti |
| Two thumbs up / two thumbs down | Fireworks / rain |
| Two hands forming a heart | Hearts |
| Two horns signs: index/pinky raised, middle/ring folded | Lasers |

**Try gestures locally** opens the phone camera for practice without sending
video to the PC. More details and current limits are in [Reactions](REACTIONS.md).

## 7. Pause and stop safely

- Switching away from the phone app, losing focus or locking the phone pauses
  capture and shows an out-of-focus/interruption state. Return and explicitly
  start/resume as requested; returning to the app must not silently share.
- **Pause** releases capture and substitutes neutral output. **Stop** ends the
  approved session. A fresh phone action is needed to share again.
- Closing Windows Camber stops sharing. Minimizing or opening it in the tray
  keeps the app running; use **Stop** or **Quit** to end the session.
- If either app crashes or the connection disappears, heartbeat/stale-frame
  protection cuts off live output. Actual cutoff timing remains a release test.

Encryption is not protection against malware or an administrator controlling
either endpoint. Do not describe this development build as impossible to steal
or independently security-certified. Read [Privacy](PRIVACY.md).

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| QR connection times out | Generate a fresh QR; check the selected PC address, trusted Private profile, app firewall rules, VPN LAN access and guest/client isolation. |
| Phone is paired, but video is black | Keep the phone app foregrounded, grant camera permission, tap Start sharing, check the Windows engine status and install matching versions. Start with Original/720p while diagnosing. |
| Camber is missing from another app | Reopen that app, verify the complete Windows installation/registration, then use installer repair if needed. Do not manually register a DLL from a writable checkout. |
| A setting appears unchanged | Wait for the phone/engine acknowledgment and check the applied value and any fallback reason. |
| Video is sideways | Update both apps, keep Camber foregrounded, turn to a stable cardinal orientation and check Fit framing. Do not add manual rotation offsets. |
| Gestures are unavailable | Use the matching Android update, reopen local practice and read the error/status. Manual Windows reactions remain available. Do not clear pairing as the first fix. |
| Gesture fires only once | Lower/change the pose and wait for the four-second cooldown. |
| Setup asks to close Camber | Quit it on every Windows account; Setup deliberately refuses to terminate active calls. |

For a report, include app versions, phone model/Android version, Windows version,
which step failed and the exact non-secret error. Export diagnostics only when
you choose. Do not attach pairing QR codes, tokens, private camera frames or keys.
