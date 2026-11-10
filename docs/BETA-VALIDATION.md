# Candidate validation procedure

Scope: Windows 11 x64 and Android 10+. iOS is deferred. This procedure is a test
plan, not a record of passed physical tests. Record the exact source commit,
candidate hashes, OS/build, GPU/driver, phone/model, network and consuming app
version with each report. Do not collect private video or credentials by default.

## Automated candidate checks

1. Build with the pinned SDK/toolchains, restored locked NuGet dependencies and
   strict Gradle verification. Review checksum changes against trusted origins.
2. Run the core checks, all native CTest suites, Android debug/release unit tests,
   release shrinking/lint, and `python tools/test_beta_evidence.py`.
3. Run `python tools/beta-readiness.py --apk PATH --check-apk-only` on the packaged
   release APK. Also generate/install AAB splits on a 16KB device; APK ELF/ZIP
   checks alone do not establish AAB installation or runtime compatibility.
4. Run `Camber.ReactionTests.exe --benchmark PATH.json` from the native build to
   measure isolated effects cost. Warmup is excluded. This unpaced synthetic test
   includes rendering/readback/blending and excludes capture/network/decode/display.
5. Run `python tools/dependency-inventory.py`. Review the inventory against actual
   packaged binaries, native transitive dependencies, notices and model license.
   The generated inventory does not claim a complete dependency graph or audit.
6. Query NuGet and Maven/native advisories. A failed lookup is blocked, never pass.
   A database with no matches is evidence of that query, not a security guarantee.

## Gesture accuracy and real output timing

Use Pixel 6, Galaxy A54 and Android 10 compatibility coverage. Beta invitations
should use security-supported devices. Separate calibration participants from a
held-out validation group. Include both cameras, portrait/landscape, varied skin
tones, hand sizes, lighting, distances and backgrounds. Do not tune on validation
clips and then reuse them as an independent accuracy result.

For each of the eight documented gestures, record deliberate eligible poses,
correct output effects, missed effects, wrong effects, duplicate effects and
output onset latency. Eligible means the intended pose is established and visible;
the 600ms hold is included in onset latency. A wrong effect counts as both a miss
of the intended gesture and a false positive. Report per-gesture recall/precision
and sample counts, not only an aggregate. Targets: recall >=95%, precision >=99%,
output onset p95 <=1s. If no effects are detected, precision is undefined and the
run cannot pass. Retain misses in the recall denominator.

Run two hours of ordinary conversation/typing/waving without deliberate poses;
target <=1 unwanted effect. Test holding a pose for >10s (one effect), a tracking
gap, a changed pose, a neutral second hand, brief uncertain poses, thermal pressure
and reaction cooldown. Check local practice creates no peer connection or media
traffic and that leaving practice releases capture. Measure effect onset in the
remote consumer output; a phone message saying accepted is insufficient.

## Media and effect performance

Primary LAN: dedicated 5GHz AP, Ethernet PC, then both devices wireless. Record a
physical timer/flashing target and the displayed consumer output in the same
high-speed external recording. Report p50/p95/p99, unique frames, repeats and
freeze durations with measurement uncertainty. Network RTT is not total latency.
Targets: >=29 unique fps at 1080p30 over ten minutes; median <=120ms, p95 <=180ms;
no unexplained >250ms freeze in 30 minutes. Measure each effect against the same
scene without effects: compositor p95 <=8ms and <=1 frame extra output latency.

Run two-hour calls, minimized/tray preview states, front/rear switching, both
output formats, rotation, Fit/Fill and natural/mirrored output. Test network loss,
congestion, decoder/GPU failure and CPU fallback. Track CPU/GPU, memory, thermal
state and queue age. Synthetic GPU timings do not certify sustained calls.

## UI and accessibility

Android: capture Perfetto FrameTimeline and main-thread scheduling during camera
switch, start, pause, focus loss, practice, reconnect and rapid options changes.
Use `dumpsys gfxinfo camera.camber framestats` as a supplementary view; identify
startup/tracing artifacts separately. Do not record screen contents by default.
Windows: capture WPR UI/composition traces and inspect UI-thread stalls and
present deadlines in WPA. Test on Intel integrated and NVIDIA graphics; include
AMD in compatibility certification. Targets: >=99% frames at the tested refresh
deadline, no >100ms UI-thread stall. Record refresh rate and trace overhead.

Verify Android system animation scale/reduced motion, Windows animations-off,
rapid interrupted navigation, TalkBack/Narrator, keyboard/focus, high contrast,
100–250% Windows scaling, enlarged text and narrow windows. Pause/Stop must remain
reachable. A screenshot or code inspection cannot establish smooth animation.

## Privacy and trust-boundary tests

Verify pairing expiry/replay/rate limit, wrong pin, revoked device, altered SDP,
malformed/oversized messages, stale/duplicate reaction IDs, bounded ICE and state
generation changes. On focus loss, lock, close, Stop or crash, inspect outgoing
consumer video and camera indicators. Re-entry must require fresh Start.
Targets: Windows local Pause/Stop neutral output <=250ms; stale substitution one
second; missing peer/control heartbeat expires at six seconds plus scheduling.
Report event-to-output timing separately from camera hardware release timing.

**Blocking unresolved boundary:** caller-supplied camera owner/mapping attributes
are not trusted registration identity. Before certification, bind the media
source to authoritative Windows registration identity and independently prove it
with two accounts. Account A attempts forged owner, registration and mapping
attributes pointing at B, object precreation, broad/null DACLs and fast switching.
No B pixels may be exposed. ACL validation alone cannot pass this gate. Run these
tests in disposable VMs, not by capturing another person's live camera.

## Signing, installation and rollout

Configure protected Windows signing credentials and Google Play upload/app keys.
Build the non-debuggable signed AAB with `tools/build-android-release.ps1`; preserve
R8 mapping/symbols privately. Sign/timestamp and independently verify all first-
party Windows EXE/DLL, the upgrade probe and installer. No keys belong in Git.

Test clean install, standard-user launch, UAC with another admin, upgrade from
0.7, repair, signed rollback and uninstall across accounts. New Setup refuses to
proceed while any Camber app/host/media process runs; test refusal while another
account is sharing. Verify registration and private/LAN firewall cleanup and
that unrelated applications/firewall rules survive. Existing unsigned/.NET8
packages are not an approved rollback baseline.

Commit reviewed source and run hosted CI. Produce final signed candidate files;
then copy beta-evidence.template.json to artifacts/beta/evidence.json. Every check
needs status, report path/hash, sourceCommit and both exact candidateHashes.
Top-level artifacts records identify the signed Windows installer and Android
bundle. Any binary rebuild invalidates the approval evidence. The readiness gate
also rejects a dirty/uncommitted tree. Review report contents and signatures;
the gate checks consistency and cannot validate a person's assertion of pass.

Run internal tests before inviting approximately 20 closed-beta testers. Obtain
owner approval for these exact signed hashes before any upload/distribution.
Publish known limitations, support/privacy information and certified results.
No public deployment or automatic update service is configured by these steps.
