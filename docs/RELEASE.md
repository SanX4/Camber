# Windows / Android closed-beta verification

No physical devices are currently certified. An application build is not a
performance certification. The approved targets remain 1080p30, at least 29
distinct frames/s over ten minutes, median glass-to-glass delay <=120 ms and
p95 <=180 ms on the reference environment.

## Physical test matrix

- Windows 11 x64 with Intel, AMD, and NVIDIA configurations.
- Pixel 6, Galaxy A54, another Android manufacturer; Android 10 minimum coverage.
- Dedicated 5 GHz access point, Ethernet PC, then both devices wireless.
- Congested Wi-Fi, 2.4 GHz, jitter, packet loss, bandwidth caps and outages.
- Teams, Zoom, Discord, OBS, Windows Camera, Meet in Chrome/Edge.
- Remote participant verification and concurrent consumer behavior.

Record exact OS, app, driver, device and competitor versions. Use an external
high-speed camera to film a physical timing target and PC display together.
Report p50/p95/p99 latency, distinct frame rate, repeats, freeze durations, CPU,
GPU, memory and thermal state. Do not use network RTT as total video latency.

## Required scenarios

1. Consent, pairing replay/revocation and cross-user frame isolation.
2. Front/rear switch <=2 s; startup <=3 s; reconnect <=5 s after network recovery.
3. Neutral output <=250 ms after local Pause/Stop; stale cutoff one second.
4. Minimized/hidden/tray operation without decoding or pacing regression.
5. No consumers for five seconds; local preview separate from external usage.
6. Two-hour endurance without crash, unbounded memory or progressive delay.
7. Phone lock, PC lock, sleep/wake, user switch, logout, camera contention/calls.
8. GPU reset, decoder reset, worker crash and phone thermal pressure.
9. Narrator/TalkBack, keyboard, high contrast, enlarged text and scaling.
10. Clean install, repair, upgrade, rollback and uninstall across user accounts.

## Distribution prerequisites

Windows: complete native build, independent security review, Authenticode
certificate, signed installer, application-scoped private/LAN firewall rules,
upgrade/rollback verification, and full transitive notices/SBOM.

Android: signed release AAB, Play developer account, Data Safety declaration,
permission and physical-device tests. The generated debug APK is for testing.

iOS is deferred. Existing source is retained, but no Apple build or distribution is required for this beta.

Beta updates use signed manual installers only. The installer refuses upgrades and uninstall while any Camber app/host/media process runs, including in other accounts. Validate upgrade and rollback in clean VMs. No automatic updater or public update endpoint is configured.

Use BETA-CHECKLIST.md and BETA-VALIDATION.md as the release gates. tools/beta-readiness.py checks recorded evidence against exact signed candidate hashes and the clean source commit; it does not upload anything.
