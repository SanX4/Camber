> Latest service-side work: private owner-proof enforcement is implemented and
> locally tested. OS property routing and two-account validation are still pending.
> See [Windows owner proof](../../WINDOWS-OWNER-PROOF.md). Older notes below are historical.

> Ownership follow-up: direct user activation and impersonation checks are now
> patched and tested. Full Frame Server account isolation is still unresolved.
> See [follow-up](../../WINDOWS-ISOLATION-FOLLOWUP.md). Earlier packaged candidates predate this patch.

# Current candidate: 0.8.0-beta.1 — NOT READY TO DEPLOY

Windows/Android only; iOS is excluded from current CI and release requirements.
The installed 0.7 development app is unchanged. New candidates and verification
outputs live under artifacts/beta. See BETA-CHECKLIST.md and BETA-VALIDATION.md.

Implemented: temporal gesture smoothing and 3D horns, local-only practice,
progress feedback, thermal-aware inference, layered D3D11 effects, bounded decoded
compositor queue, privacy publication fencing, camera failure/stale-callback
handling, signed-release build preflights, process-safe installer guard, dependency
locks/inventory, and source/artifact-bound release evidence checks.

No phones are performance-certified. Cross-user camera-registration isolation,
independent security review, physical accuracy/latency/UI/endurance tests, signing
credentials and distribution accounts remain blocking. The historical records
below are not certification of this candidate.

---

# Implementation status

Verified locally on Windows, 9 November 2026. This is a development alpha.
The complete public-release roadmap is **not finished**.

## 0.3 Android interface and shared identity

The Android app now has a soft teal/sage light/dark theme, rounded controls,
one state-dependent primary action, a connection-options sheet, finite native
motion, accessible gesture switch and persisted phone-side gesture preference.
The selected Open C mark is included in Android adaptive/themed launcher icons
and the Windows executable, window title bar and interface header.
Android builds and gesture-policy tests pass. Emulator checks cover a 375 dp
phone, Options, dark mode, enlarged text, disabled animation scale and landscape
scrolling. Physical-phone animation/performance verification remains pending.

## 0.2 mirroring and reactions

- Natural output and preview are the defaults; outgoing mirroring and local-only
  preview mirroring are separate, immediate, persisted controls.
- Eight original animated reactions are composited into NV12 frames in the media
  worker, so the Windows preview and external camera consumers receive them.
  Manual controls, off switches and reduced-motion output are included.
- Android 0.2 includes a pinned, bundled MediaPipe gesture model. Analysis is
  local, downscaled to at most 480 pixels, rate limited, and single-flight.
  Camera textures are released before inference to avoid blocking the stream.
- Recognized poses require an 850 ms hold, hand release and cooldown. The host
  independently checks session consent/generation and rate limits all reactions.
  Pause/Stop clears the animation in the native worker.
- Core privacy/configuration checks, generated H.264 transport, all eight rendered
  effects with rotation/mirroring, and Android gesture-policy tests pass. Physical
  gesture recognition accuracy and performance on the user's phone still need
  verification after installing the new APK. iOS automatic gestures are not
  implemented; Windows manual reactions also work with the existing iOS source.

## Produced

- WinUI 3 desktop executable, connection host, and tray helper.
- C++ WebRTC receiver executable and Windows Media Foundation camera DLL.
- Android debug APK built from Kotlin/Compose and Camera2 sources.
- Native iPhone companion source and XcodeGen project specification.
- Unsigned Windows development installer with native registration, Microsoft
  C++ runtime prerequisite, private/local-network firewall rules and uninstall.
- Build scripts, CI definitions, protocol/design/release docs and a 101-item
  requirement/evidence tracker at `requirements.csv`.

## Verified

- Installed startup repair: the original publish omitted `Camber.Desktop.pri`,
  causing an immediate WinUI crash. Adding it restored launch in the installed
  folder. Publishing now explicitly includes the merged resource index, the
  installer rejects packages without it, and a clean published-folder launch
  smoke test passes. `tools/test-windows-package.ps1` guards this regression.

- Windows desktop and host builds: zero warnings/errors.
- Native MSVC production targets build and link.
- Installed camera-engine startup now succeeds without elevation. The source
  validates owner attributes during metadata activation and defers Global shared
  memory creation until Frame Server requests samples. The activation regression
  rejects absent/mismatched owner attributes and confirms discovery creates no
  privileged mapping. The installed Windows camera delivered a neutral 1080p
  NV12 sample (3,110,400 bytes) through normal Media Foundation enumeration.
- 26 consent, pairing, timing, request-fencing and bounded-buffer core checks.
- Nine real-server integration checks covering pinned TLS, wrong secrets,
  valid enrollment, replay rejection, WebSocket authentication, consent
  separation, missing-engine failure, PC Stop and delayed Start rejection.
- Native H.264 decoder capability probe passes against the pinned WebRTC DLL.
- Black-video regression: the standalone receiver did not call `WSAStartup`.
  TLS pairing in the .NET host worked, but the native video process could not
  establish its transport. The receiver now owns a balanced Winsock lifetime.
  A generated-pixel integration test exercises real H.264 encoding, ICE/DTLS,
  decoding, shared NV12 delivery and Stop clearing. It failed before the repair
  and passes with the production receiver fix. It opens no physical camera.
- Local control status exposes video negotiation stage, offer/candidate counts,
  published-frame count and native error text, without SDP, frames or secrets.
- Physical phone pairing and Windows preview: the user confirmed that video
  appears after the installed receiver repair on 9 November 2026. No camera
  images were saved. This is functional verification, not performance certification.
- Native image transforms pass under MSVC and MinGW.
- Windows UI rendered; labeled accessibility tree inspected.
- Android APK compiled successfully. No Android device was connected.
- Apple framework archive hash and used Objective-C API headers checked;
  an actual Xcode build has not run.

## Important limits

The user installed the development package in `F:\Camber`. This session runs
without elevation; source activation, engine readiness and neutral-frame delivery
were verified against that installation after the startup repair. Installer
upgrade, rollback and uninstall coverage remains pending. The camera DLL should be installed
in a protected Program Files location before Windows loads it in its camera
service. Do not register it from the writable source checkout.

Remote-participant video delivery has not been verified. Physical phone video
in the Windows preview was confirmed by the user after the networking repair.
No performance advantage, latency target, thermal target or certified device
claim is established. The current receiver provides a CPU-copy NV12 baseline;
D3D11/hardware decoding and shared-texture performance work remain.

Outstanding implementation includes discovery/comparison-code pairing,
automatic reconnection, complete capability negotiation and acknowledged actual
camera modes, advanced camera controls, live telemetry/adaptation wiring,
external-consumer demand integration, RGB32 output fallback, labeled output
placeholders, localization, authenticated updates/rollback, and full data
deletion/migration/repair journeys. These are development work, not merely
missing credentials. The detailed tracker marks each requirement separately.

Physical compatibility, multi-user, privacy timing, accessibility, endurance,
image quality and fault-recovery tests remain. Apple signing/build hardware,
store accounts, Authenticode credentials and release hosting are external
prerequisites. No public release or upload has been performed.

## Architectural implementation detail

Authoritative consent orchestration uses the tested C# core in the Windows host;
the dedicated C++ worker independently enforces permission and generation gates.
This avoids separate contradictory state machines. The worker owns video pixels;
the UI reads the camera through Windows media APIs.

## Next release gate

Install the development package on a Windows 11 test machine, use a physical
Android phone to verify camera output and remote meeting delivery, then repeat
with the iPhone build on a Mac. Complete the outstanding development items and
all gates in RELEASE.md before treating this as public v1.
