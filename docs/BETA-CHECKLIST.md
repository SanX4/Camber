# Android / Windows beta deployment checklist

Status: BLOCKED. These are development candidates, not a certified or signed beta. Do not deploy them until every gate has evidence for the exact candidate commit and hashes. Never substitute cached or older-version evidence. iOS is excluded.

- [x] Direct user activation owner checks, effective-thread impersonation checks and anonymous-token denial; before/after regression verified. Service-side owner binding remains blocking.

- [x] Mandatory service-side owner proof: per-approval CNG random proof, private SET-only Windows camera grant, protected verifier, neutral ungranted output, revocation and native ABI v2. Nine native suites pass. OS property routing and two-account verification are NOT yet complete.
- [ ] Validate the owner-proof Windows property route against the actual registered camera, cloned registrations and two user accounts; see WINDOWS-OWNER-PROOF.md.

## Implemented foundation
- [x] .NET 10 SDK 10.0.403 migration and runtime target updates.
- [x] MediaPipe 0.10.35 upgrade; model checksum remains pinned.
- [x] 12 Hz recognition cap, thermal 8/6 Hz fallback, smoothing, 600ms single/800ms paired hold, 350ms rearm, tracking-gap reset, neutral second-hand handling and visible progress.
- [x] Local-only practice capture; it never requests PC streaming approval.
- [x] D3D11/DirectXTK layered reaction renderer with original lit sprites and CPU fallback.
- [x] Latest-frame compositor mailbox; privacy publication gate independent of expensive rendering.
- [x] Bounded reaction-ID replay rejection and request acknowledgment.
- [x] Restricted existing-object ACL checks with a negative world-accessible-object test. The generated H.264 test now exercises these production ACL checks without a bypass.
- [x] Windows installer/uninstaller refuses to proceed while Camber processes run in any account; no automatic shutdown of old hosts.
- [x] Exact CI action commits, NuGet/Gradle locks and SHA256 Gradle artifact verification. Android CI uses the verified Windows toolchain; CI itself still needs a hosted run.
- [x] CycloneDX component inventory generator (full license/native provenance review pending).
- [x] Camera failure callbacks and stale session callback rejection; async teardown waits outside the lifecycle lock.
- [x] Evidence gate binds reports to source commit and exact candidate hashes, rejects dirty source, missing/changed/outside evidence; seven negative/positive gate checks.
- [x] Non-debuggable R8 Android release configuration and signing-environment preflight.
- [x] iOS CI job removed. Existing source retained for future work.

## Mandatory release evidence
- [ ] Commit reviewed source baseline and scan for secrets; independently review bootstrapped dependency hashes and run hosted CI.
- [ ] Full SBOM/transitive license and vulnerability review, including runtime/native libraries/model.
- [ ] Verify packaged APK ELF and ZIP 16KB alignment, plus install/run AAB-generated splits on 16KB Android.
- [ ] Independent security review. Existing-object ACL checks are NOT a proof of camera-registration identity binding. Forged-owner, two-account and fast-user-switch tests remain blocking.
- [ ] Pairing expiry/replay/revocation, malformed traffic, stale/duplicated reaction IDs and protocol-version cases.
- [ ] Physical focus-loss, lock, close, Stop and crash cutoff timings. Re-entry requires fresh Start.
- [ ] Recognition per gesture >=95% recall, >=99% precision; eligible-pose-to-output p95 <=1s. Held poses never repeat. <=1 unwanted trigger/2h conversation.
- [ ] Reference devices: Pixel 6 / Galaxy A54 comparisons, Android 10 compatibility, current security-supported devices for invitations.
- [ ] Camera switching and local-only practice on real phones, including permission dialogs and interruptions.
- [ ] Camera/encoder teardown is asynchronous; prove UI feedback <=100ms during teardown.
- [ ] Cinematic effects in remote Discord/Meet video and OBS/Teams/Zoom/Windows Camera, rotation/framing/mirroring, GPU reset and CPU fallback.
- [ ] 1080p30 >=29 unique fps, median <=120ms / p95 <=180ms latency in reference LAN; effect overhead p95 <=8ms, <=1 frame added latency.
- [ ] UI frame deadline >=99% at tested refresh rate, no UI-thread stalls >100ms; resize/high contrast/large text/reduced motion/Narrator/TalkBack.
- [ ] Two-hour endurance without crash, progressive delay or unbounded memory growth.
- [ ] Clean install, upgrade, repair, rollback, uninstall; standard-user/UAC and cross-account cleanup; active calls must not be terminated by upgrades. New installer probe also guards upgrades from older hosts; full UAC/multi-account installation tests remain.
- [ ] Google Play account/testing setup and signing key; Windows signing credentials.
- [ ] Non-debuggable signed AAB; all first-party Windows EXE/DLL and installer signed/timestamped and signatures independently verified.
- [ ] Supported signed rollback baseline, not old unsigned .NET8 development packages.
- [ ] Privacy disclosures, support route, limitations and certified hardware list.
- [ ] Internal testing before external closed beta (~20 invited testers).
- [ ] Owner approves exact signed candidate before upload/distribution. Manual signed updates only for beta.

Record statuses, source commit, artifact hashes, evidence paths and SHA256 in docs/beta-evidence.template.json copied to artifacts/beta/evidence.json. Run python tools/beta-readiness.py --evidence artifacts/beta/evidence.json. Missing evidence fails closed. ELF/ZIP checks alone can be run with --apk PATH --check-apk-only; that does not approve a release.

## Known remaining engineering / verification
Camera-registration identity binding is not yet proven against two Windows users and is a release blocker. Horns now use 3D landmark angles, but held-out physical accuracy tuning remains. Full signing/rollback execution, hosted CI provenance and independent security signoff remain. Current rendering uses GPU sprites with CPU NV12 blending/readback, not a zero-copy video path. The isolated 1080p effects benchmark measured per-effect p95 between 0.90ms and 1.80ms on this PC; sustained-call timing targets remain unverified. Maven vulnerability lookup was unavailable due to a TLS connection error; no clean Maven security result is claimed. NuGet advisory queries returned no listed affected dependencies in the resolved desktop/host packages. No deployment has been performed.
