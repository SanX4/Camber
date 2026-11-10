# Current development status

Current version is defined in `Directory.Build.props`: **0.9.0-dev.1**, Android code **9**.

The implementation adds automatic physical orientation, negotiated encoder limits,
optional local Original/Natural/Low light presets, synchronized quality controls,
per-architecture Android APKs and a compact Windows application layout.

See [0.9 implementation and validation](VIDEO-QUALITY-0.9.md),
[developer build guide](../README.md), [user guide](USER-GUIDE.md),
[release checklist](CHECKLIST-0.9.md) and the generated versioned reports.

**Distribution remains blocked** by physical-phone certification, actual Windows
camera service property routing, two-account isolation, installation lifecycle,
independent security review, signing and exact-artifact release approval.

## November 10 follow-up

The registered Windows camera delivered live Android video in a real source-reader
consumer on the connected Galaxy S24+ and this Windows account. This confirms the
normal single-account property route; cloned registration, two-account and fast
user switching remain unverified. Front/rear requests and all three processing
presets were acknowledged in a live session. Repeated privacy checks measured
neutral consumer output after PC Pause (16-31 ms) and phone focus loss
(234-250 ms). Phone process termination measured 203 ms before the Android
dependency refresh; its final-candidate repeat was interrupted by the USB test
connection and remains incomplete. Hardware camera release followed within
roughly 0.6-0.9 seconds in the completed observations. Returning to
the phone app did not restart capture, and its Out of focus explanation was
verified. These are short checks, not endurance/latency certification.

The Windows wizard now has branded modern light/dark presentation, an always
visible installation-folder page, optional shared-desktop and shared-startup
shortcuts, and a completion-page launch as the original user. Startup opens the
tray with no camera consent. All four shortcut selections, custom paths with
spaces, deselection on upgrade and removal on uninstall pass an unprivileged,
redirected-folder fixture. Actual elevation, machine-wide installation lifecycle
and visual accessibility checks still need their controlled-environment runs.

The Android runtime dependency review replaced transitive Guava 27.0.1-android
with 33.7.2-android and Protobuf Lite 4.26.1 with 4.28.2, addressing identified
version advisories. New artifact checksums were cross-checked against canonical
HTTPS downloads and existing verification hashes were retained. A successful OSV
query of the locked graph found no app-runtime advisory matches after the change;
build/test-tool findings remain separately documented for review. This does not
establish exploitability of the previous versions or constitute a security audit.

The [connection guide](CONNECTING.md) and [release notes](RELEASE-NOTES-0.9.0-dev.1.md)
are prepared for the requested SanX4/Camber repository. No source, packages or
release have been uploaded. GitHub authentication and the remaining exact-candidate
checks are still required. Per-run evidence is private under versioned artifacts.

Previous status and implementation notes are preserved under
[history/pre-0.9](history/pre-0.9/STATUS.md). No old result certifies this build.
