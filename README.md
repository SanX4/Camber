# Camber Camera

Windows 11 and Android development source. iOS source is preserved and deferred.
Current development version: **0.9.0-dev.1**, Android version code **9**.

The application pairs locally, uses encrypted camera transport, and exposes the
phone camera through a Windows virtual camera. This build adds automatic phone
rotation, synchronized quality settings, local image presets and smaller packages.
No comparative quality or end-to-end latency advantage has been certified.

**Development source; production distribution is not certified.** The owner has
reported successful meeting-app, installer and cross-account checks and authorized
uploading this source. Independent security review, production signing and the
remaining certification/evidence checks are still required. Read the
[current status](docs/STATUS.md) and [checklist](docs/CHECKLIST-0.9.md).

## Find the application

For the full installation and pairing walkthrough, read
**[Connect your Android phone to Windows](docs/CONNECTING.md)**. It includes
installer choices, APK selection, Wi-Fi/QR setup, Discord/Meet camera selection,
quality controls, gestures, privacy and troubleshooting. Candidate changes are
listed in the [development release notes](docs/RELEASE-NOTES-0.9.0-dev.1.md).

All new deliverables use one layout:

```text
artifacts/
  latest-build.json
  <version>/
    build-manifest.json
    android/             # four internal APKs, universal fallback, complete AAB
    windows/
      Camber.Desktop.exe # small launcher; keeps the existing executable path
      app/               # application, runtimes and native camera implementation
      licenses/
    windows-installer/   # unsigned internal test setup, or explicitly signed release
    reports/             # measurements and verification
archive/                 # private, verified preservation backups; never publish
```

Open the root **Camber.Desktop.exe** only in a suitable test installation. The
complete **app** folder must accompany it. Camera registration belongs in Program
Files, not a writable development checkout. The separate legacy `F:\Camber` and
installed copy were left unchanged. See the [user guide](docs/USER-GUIDE.md).

## Build

Prerequisites: pinned .NET SDK from `global.json`, MSVC/Windows SDK/CMake, JDK 21,
Gradle 9.3.1 and Android SDK 36/build tools 36.0.0. Dependency versions and checksums
are pinned in the project lock files and `tools/dependencies.json`.

```powershell
./tools/bootstrap.ps1
./tools/build.ps1 -Native -Installer
# For the existing portable local compiler instead of Visual Studio:
./tools/build.ps1 -Native -LocalCompiler -Installer
./tools/build-android.ps1 -Gradle 'path/to/gradle.bat' -Bundle
```

Android builds default to minified, non-debuggable **Internal**, retaining the
existing debug signing identity for local upgrade continuity. Choose ARM64 for
most current phones; universal is the engineering fallback. `-Configuration Debug`
retains debugging support. `-Rebuild` forces every Android build task to rerun.
The AAB uses a separate build directory to avoid mixing APK split resources.

`Directory.Build.props` is the version source for application metadata, Android
code/name, package paths and installer names. Component SDK references replace the
unused Windows AI/ML/widgets metapackage; .NET, WinUI, tray, localization and
accessibility dependencies remain. No aggressive managed trimming is enabled.

`-ReleasePackage` requires Windows signing credentials and evidence gates.
`tools/build-android-release.ps1` requires Android release signing credentials.
Neither build script installs, uploads or pushes the application.

## Source and verification

- `src`: consent/quality policy, protected signaling service, WinUI interface and tray.
- `native`: WebRTC receiver, image processing, reactions and owner-proof camera source.
- `mobile/android`: Compose, Camera2, physical orientation and numeric video statistics.
- `mobile/ios`: preserved future source; excluded from active builds.
- `tools`, `installer`, `.github/workflows`: reproducible builds and packaging.
- `docs`: current guides and specifications; superseded reports live in `docs/history`.

```powershell
dotnet run --project tests/Camber.Core.Tests -c Release
./tools/build-native-local.ps1
python tools/test_beta_evidence.py
```

The native and policy tests use generated video and synthetic metrics. They do
not certify a real Windows meeting app, phone camera, network or two-user setup.
Full integration procedures are in [BETA-VALIDATION](docs/BETA-VALIDATION.md).

## Privacy and attribution

No account, recording, microphone, cloud media relay, public STUN/TURN or automatic
crash upload. Phone camera access requires explicit consent. Pairing secrets and
keys retain their protected platform storage. New diagnostics export only bounded
numeric measurements; frames and raw WebRTC reports are excluded.

Read [privacy](docs/PRIVACY.md), [protocol](docs/PROTOCOL.md),
[Windows owner proof](docs/WINDOWS-OWNER-PROOF.md) and
[third-party notices](third_party/NOTICE.md). Permitted MyCam media-source reuse
retains its original attribution. Private historical archives may contain emulator
state and logs; keep them outside GitHub and distributed packages.
