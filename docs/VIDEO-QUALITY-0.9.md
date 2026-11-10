# Video quality and automatic rotation implementation

This is a development build. See [remaining checks](CHECKLIST-0.9.md),
[protocol](PROTOCOL.md), [traceability](requirements.csv) and the versioned reports.
iOS source remains preserved and deferred.

## Orientation and synchronized settings

FrameOrientation wraps Android's existing capture observer, obtains the active
camera's sensor orientation/facing and normalizes physical orientation. Its policy
uses 200 ms stability and 55-degree hysteresis; unknown angles retain the previous
orientation. The foreground orientation listener is independent of system screen
auto-rotate. Configuration changes preserve capture; genuine focus/background/lock
events retain privacy handling. There is no rotation switch, lock or manual angle.

WebRTC and gesture analysis receive the normalized metadata. Windows uses the same
decoded angle for effects and final framing, without another manual offset.
Landscape and portrait NV12 storage share the existing 1080p pixel budget. Owner
proof, frame ABI 2 layout and mapped byte capacity remain unchanged. Fit keeps the
whole portrait image; consumer output negotiation remains stable.

Both interfaces offer Original, Natural and Low light plus the requested 720p or
1080p maximum. PC preferences are authoritative; phone sender/capture results and
native processing acknowledgments establish applied state. A lens selected before
Start is included with fresh approval. Settings alone never start capture.

## Compression and measurements

RtpSender receives an initial 8 Mbps ceiling, adapting within 2–12 Mbps. WebRTC may
transmit below it. Network congestion lowers the ceiling first; capture, encoder,
decoder and thermal limits reduce capture demand. Three poor samples downgrade;
thirty measured healthy samples allow recovery. Missing recovery-stage metrics
cannot imply health. The user's maximum is preserved, and acknowledged closest
capture sizes do not repeatedly restart capture.

Only bounded numeric statistics are exported: delta bitrate, encoded/capture frame
rates, encode/decode time where available, loss, RTT, capture/encoded/decoded
dimensions, rotation, sender ceiling and thermal state. Initial/missing/reset rates
remain unknown. Raw RTC reports, identifiers, addresses, credentials and frames are
excluded. These measurements are never presented as total camera latency.

## Local image presets

Original bypasses additional image filters and remains the default/fallback.
Natural uses gentle edge-aware spatial noise reduction, bounded sharpening and
restrained contrast. Low light increases spatial denoising and bounded shadow
brightening. Embedded D3D11 shaders process NV12 before reactions, using reusable
resources. There are no additional models, future-frame buffers or deliberate
frame delay. Preview and camera consumers read the same processed stream.

Graphics failure retains the original CPU frame and reports a fallback reason.
Three complete 30-sample enhancement windows above 4 ms p95 latch fallback. A new
session or explicit preset selection retries; Windows exposes Try preset again.
Settings use a bounded latest-settings handoff. A generated-video test blocks the
compositor and confirms that setting commands and Stop do not wait for graphics.
Output/proof revocation precedes clearing pending processing. Decoded frames still
use latest-frame-wins; compressed dependency handling remains WebRTC's job.

## Packages and preservation

Directory.Build.props defines 0.9.0-dev.1/Android code 9. Gradle builds four internal
architecture APKs plus a universal fallback with release optimization and the
existing development signing identity. A separate bundle directory avoids AGP
split-resource conflicts and retains the complete AAB. R8 mappings stay in reports.
Static checks verify unchanged native-library/model hashes and signing continuity;
runtime feature parity still requires device execution.

Windows references supported SDK components rather than the AI/ML/widgets
metapackage. Implementation files stay under app, with a small static-runtime root
launcher preserving Camber.Desktop.exe. Registration/firewall/uninstall paths use
app. The offline C++ prerequisite extracts temporarily; stronger lossless setup
compression reduces downloads. Known legacy files are removed only when their
inventory hash matches. Modified and unknown files remain untouched. Installation
lifecycle validation is still pending.

Original source and historical packages/emulator files have private verified
archives with hashes/timestamps/restoration metadata. All 240 source and 3,425
historical files were restored and hash checked before duplicate historical copies
and restoration scratch files were removed. Archives remain local. Earlier status
and implementation reports are under history/pre-0.9. Toolchains, signing material,
protected user data, iOS source, F:\Camber and the installed app were preserved.

## Evidence and limits

Reports contain policy/native/Android test logs, synthetic GPU timings, APK
alignment/signature checks, restoration/cleanup results, component inventory,
package sizes and artifact hashes. Synthetic timings include upload, shader and
readback plus optional reaction composition. They exclude capture/network/decode,
consumer and display, and do not certify glass-to-glass latency.

The ARM64 internal APK is about 33.8 MiB. Windows payload is about 252.5 MiB versus
the matching 327.3 MiB baseline, about 23% smaller. Setup size is separate. The new
installed size remains unmeasured because no installation was performed. The older
installed copy is not the comparison baseline.

Real-phone rotation/privacy/quality, Windows service routing and two-account
isolation, installation, accessibility/motion, compatibility, signing and exact
source/artifact approval remain mandatory. This build is not deployment approved.

References: [Android orientation](https://developer.android.com/media/camera/camerax/orientation-rotation),
[device-specific delivery](https://developer.android.com/topic/performance/reduce-apk-size),
[Windows SDK components](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/release-notes/windows-app-sdk-1-8?pivots=stable).
