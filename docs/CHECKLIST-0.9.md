# Camber 0.9 development checklist

Current decision: **NOT APPROVED FOR DISTRIBUTION OR GITHUB PUSH**.
This supplements the [existing security/release gates](BETA-CHECKLIST.md).
Remaining checks need evidence for the exact source and signed artifact hashes.
Earlier-version results cannot certify this build. iOS remains deferred.

## Implemented and locally checked

- [x] Original source: 240 files snapshotted, restored and hash checked.
- [x] Historical builds and stopped emulator state: 3,425 files archived, restored and hash checked before duplicate originals were removed.
- [x] Source/assets, unknown files, toolchains, signing material and protected user data preserved. Separate F:\Camber and installed application unchanged.
- [x] Physical orientation independent of system screen rotation; cardinal sensor/facing, stability, hysteresis and unknown-angle policy tests.
- [x] Normalized metadata reaches WebRTC and gesture analysis; manual Windows offset removed.
- [x] Landscape/portrait buffers share unchanged ABI 2 memory capacity; overflow and oversized frames rejected.
- [x] Original default; optional Natural/Low light frame-local GPU processing before reactions; noise, edges, chroma, SDR endpoints and resize tests.
- [x] Graphics failure and sustained-budget fallback; explicit retry and new-session retry paths.
- [x] Deliberately blocked compositor does not delay settings or Stop; generated-video cutoff regression passes under 100 ms.
- [x] Actual sender ceilings/capture demand, numeric counter telemetry, nullable stages, downgrade/recovery hysteresis and requested maximum preservation.
- [x] Both apps show requested/applied settings and capture/encoded dimensions; stale IDs/generations and overlapping settings requests checked.
- [x] Four minified non-debuggable internal APKs, universal fallback and complete AAB; existing development certificate retained.
- [x] All native-library/model hashes preserved; 64-bit ELF and ZIP 16 KB checks pass.
- [x] Windows SDK component references, stable launcher, app subfolder, offline temporary prerequisite extraction and stronger lossless setup compression.
- [x] Launcher and upgrade checker work before C++ runtime installation; startup resources and package layout checked.
- [x] UTF-8 source/document validation; current guides consolidated and superseded reports preserved under history.
- [x] Directory.Build.props drives version 0.9.0-dev.1/code 9; versioned artifacts and latest-build index agree.
- [x] ARM64 APK ≤40 MiB and Windows payload reduction ≥15% measured and passed.

## Physical phones and controlled Windows installation

- [ ] Front/rear cameras at 0/90/180/270 degrees with Android auto-rotate on/off: readable text, correct proportions, upright effects, stable Fit/Fill, correction ≤500 ms.
- [ ] Rotation keeps approval; actual app switching/focus loss, lock, Pause, Stop, kill and close retain cutoff on both apps.
- [ ] Live two-app settings, rejection, missing capabilities, reconnect, delayed acknowledgments, same-preset retry and graphics reset/fallback.
- [ ] Real-camera detail, faces/skin tones, shadows, low light, clipping, halos and SDR charts. Synthetic variance is insufficient for quality certification.
- [ ] Sustained 1080p30 unique frames, startup/switch/recovery times, glass-to-glass latency, network/thermal adaptation and two-hour endurance.
- [ ] Hardware encoder/decoder use and stage timing on reference devices; unknown measurements remain unknown.
- [ ] Install/run every APK architecture, upgrade prior development builds without losing pairing/settings, and test a 16 KB device and store-generated splits.
- [ ] Windows clean install/upgrade/repair/uninstall and failed-update recovery: launcher, tray/privacy, registration, firewall and temporary prerequisite cleanup.
- [ ] Actual Windows camera-service owner-proof routing; two-account/fast-user-switch isolation and adversarial owner/resource cases.
- [ ] Teams/Zoom/Discord/OBS/Windows Camera/Meet with a remote participant; concurrent consumers where supported.
- [ ] Narrator/TalkBack, keyboard, high contrast, text/display scaling, reflow, reduced motion and measured animation smoothness.

## Before deployment or GitHub push

- [ ] Review/commit currently untracked source, scan secrets and run hosted CI against exact locks/artifacts.
- [ ] Independent security review and full dependency/license/native provenance/vulnerability review. Inventory alone is insufficient.
- [ ] Production signing/verification, update recovery, developer accounts and distribution setup.
- [ ] Evidence gate passes for the exact clean source commit and report/artifact hashes.
- [ ] Owner approves the exact signed artifacts for deployment or publishing.

No installer was run, camera registration changed, account created or GitHub push performed during this implementation.
