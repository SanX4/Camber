> Latest service-side work: private owner-proof enforcement is implemented and
> locally tested. OS property routing and two-account validation are still pending.
> See [Windows owner proof](../../WINDOWS-OWNER-PROOF.md). Older notes below are historical.

> Ownership follow-up: direct user activation and impersonation checks are now
> patched and tested. Full Frame Server account isolation is still unresolved.
> See [follow-up](../../WINDOWS-ISOLATION-FOLLOWUP.md). Earlier packaged candidates predate this patch.

# 0.8.0-beta.1 implementation and readiness

Development candidate only. No release was installed, uploaded or deployed.
The existing Program Files installation remains unchanged. Windows and Android
are in scope; iOS source is retained and excluded from current CI/release gates.

| Area | Implemented | Evidence and remaining work |
|---|---|---|
| Gesture response | Temporal category smoothing, palm tracking, 600ms hold, 350ms rearm, 4s cooldown, tracking-gap reset and neutral second-hand handling | Ten policy tests per Android build type; physical precision/recall and false-trigger targets unverified |
| Custom poses | Aspect-corrected geometry and world-landmark joint angles for horns | Rotation/invalid-pose policy checks; held-out real-device tuning still required |
| Feedback/practice | Hold progress, cooldown/release guidance, isolated local camera practice and front/rear switching | Both build types compile; practice has no peer/SDP/network video track; hardware behavior still needs testing |
| Effects | D3D11/DirectXTK lit sprites, depth/particle motion, continuous beams, streaks, original artwork; reduced motion and CPU fallback | Eight effects tested with rotation/mirroring/expiry/clear; remote output and GPU recovery need physical testing |
| Media scheduling | One-slot latest decoded frame mailbox, compositor thread, stale frame and generation fences | Generated H.264/ICE/DTLS/decoded-frame integration passes; sustained quality/latency targets unverified |
| Privacy/lifecycle | Publication disabled before rendering teardown, async phone cleanup, late callback fencing, bounded ICE, camera interruption handling | Native consent/Stop/lease checks pass; phone focus/lock/close and consumer cutoff timing need device tests |
| Local security | Existing frame-object owner/DACL checks, producer SID from process identity, GUID reaction replay rejection | Positive/negative ACL tests and production ACL integration pass; trusted camera-registration owner binding is still unresolved and blocking |
| Build/release | .NET 10 migration, MediaPipe 0.10.35, non-debuggable R8 Android release, pinned CI actions and dependency locks/checksums, signing preflights | Windows publish, release APK/AAB build and APK 16KB checks pass; no release signing credentials, AAB split/device test or hosted CI proof |
| Installation | New native probe blocks install/uninstall while any account runs Camber; no old-host auto-shutdown | Probe tests and unsigned Inno compilation pass; clean VM/UAC/repair/rollback matrix remains |
| Release evidence | Required checklist, source/candidate hash binding, dirty-tree rejection, changed/outside evidence rejection | Seven gate tests pass; current gate correctly reports NOT READY |

## Local verification

- 32/32 core checks.
- 7/7 native suites, including generated H.264 transport and production producer
  ACL validation; no physical camera opened.
- 10/10 Android debug and 10/10 release policy checks.
- Android release shrinking and release lint complete; release APK and AAB are
  unsigned. The development APK uses the Android debug key.
- Six arm64/x86_64 packaged native libraries pass ELF and ZIP 16KB alignment.
- Windows desktop/host publish and required startup resource packaging pass.
  This is not a newly installed-camera or UI launch certification.
- Synthetic 1080p effect compositor per-effect p95: approximately 0.90–1.80ms on
  this PC after warmup. Capture, network, decoding, display, GPU contention and
  sustained-call performance are excluded. This does not certify total latency.
- CycloneDX inventory generator lists 310 locked/build/native components.
  Full transitive native provenance, graph and license review remain incomplete.
- NuGet advisory queries returned no listed affected resolved desktop/host
  dependencies. Maven advisory lookup failed TLS negotiation and is blocked.

## Candidate outputs

`artifacts/beta/windows` contains the development desktop, host, native camera,
media worker, upgrade probe and bundled runtime. `artifacts/beta/android` contains
the debug APK and unsigned release APK/AAB. `artifacts/beta/installer` contains
an unsigned Windows development installer. Logs, preview artwork and isolated
compositor measurements are under `artifacts/beta`.

These files are for engineering validation, not external distribution. Exact
hashes are recorded in the candidate manifest; signing/rebuilding changes them.

## Next required gates

Resolve authoritative camera-registration identity binding and prove two-user
isolation in disposable Windows VMs. Obtain independent security review. Run the
physical recognition, privacy, UI, compatibility, latency and endurance matrix.
Set up protected signing credentials and distribution accounts, commit reviewed
source, execute hosted CI, and validate signed installation/rollback. Review
evidence and approve the exact signed candidate hashes before distribution.

Use BETA-CHECKLIST.md for status and BETA-VALIDATION.md for the test procedure.
No security guarantee, physical accuracy claim or perfect-animation claim is
established by passing compilation and synthetic tests.
