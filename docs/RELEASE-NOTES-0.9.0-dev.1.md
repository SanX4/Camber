# Camber 0.9.0-dev.1 - development candidate

**Not approved for public distribution.** This document prepares release notes;
it does not assert that a GitHub release has been published. Required security,
signing, installation and physical-device gates remain in the current checklist.

Android 10+ / Windows 11 x64. iOS is deferred.

## Changes

- Automatic physical phone rotation, independent of Android's screen-rotation
  setting; proportional portrait Fit framing and stable Windows output.
- Synchronized Original, Natural and Low light presets and actual encoder/capture
  adaptation with requested/applied settings and numeric diagnostics.
- Gesture-runtime ownership/shrinking fixes; paired-pose priority, more tolerant
  heart/horns geometry and persistent recognized-pose feedback.
- Smoother laser sweeps with layered glow and light glints. Rain adds depth,
  variable speed, wind, soft haze and small impact splashes.
- Modern Windows Setup with the approved Camber artwork, visible folder choice,
  optional desktop shortcut and optional startup in the tray. Startup never
  approves camera capture. Shortcut selections apply to all Windows users.
- Patched Android runtime Guava and Protobuf Lite versions with refreshed locks
  and checksum verification; build/test-tool advisory findings remain under review.
- Smaller processor-specific Android APKs, universal fallback and organized
  Windows implementation files beneath the stable root launcher.

## Planned release files after approval

- Windows offline Setup and checksums.
- Minified Android APKs for arm64-v8a, armeabi-v7a, x86_64 and x86, plus the
  universal fallback. Most current phones use arm64-v8a.
- A checksum manifest, exact source revision, verification summary, known limits,
  [connection guide](CONNECTING.md), privacy information and third-party notices.

The current APKs retain the development signing identity for upgrade continuity;
they are not Google Play production releases. Private signing material, mappings,
pairing records, diagnostic logs, emulator state and preservation archives must
not be uploaded. Publishing any release requires passing the exact-candidate
checks and authorized GitHub access.
