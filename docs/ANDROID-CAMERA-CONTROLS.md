# Android camera controls - 0.6.0

The mobile app now has Rear camera and Front camera tiles. Choose before starting, or switch during an approved live session. The last successful selection is saved on the phone. Unsupported lenses are disabled. Selecting a lens while stopped never opens the camera or requests consent.

## Visual direction

The supplied dashboard reference informs rounded control tiles, recessed graphite backgrounds, subtle metallic edge highlights, and generous spacing. The palette is always dark: background #0B0E13, surface #171C24, raised surface #242C38, accent #80B8FF, selected surface #183552. Preserve the approved Open C mark. No decorative account, device tiles, or nonfunctional switches are added.

Use native Compose color transitions and pressed feedback, respecting system animation scaling. Scrolling, safe-area insets, 48dp minimum interactive controls, radio selection semantics and polite status announcements remain available. Graphite theme also applies when Android uses light appearance.

## Command and privacy contract

A live phone sends selectCamera with front/back and its current session generation. The authenticated host validates consent, privacy state and generation, then issues the existing configure command with a tracked request ID. The phone targets a specific Camera2 camera ID, waits for the WebRTC switch completion callback and acknowledges the applied selection. Windows reflects acknowledged configuration changes. No media source or consumer camera identity changes.

Switch requests are serialized on the phone. Callbacks are fenced by capture epoch, operation ID and session generation; late callbacks after Pause, Stop or focus loss cannot change state. A five-second switch timeout stops sharing. Callback completion is posted off the camera thread to avoid waiting on the camera thread during privacy shutdown. Camera selection never starts or resumes capture by itself.

Both Android and Windows 0.6.0 are required for phone-initiated live switching. Existing DTLS-SRTP transport, pinned signaling, active-session leases and focus-loss shutdown are preserved.

## Verification

- Windows build and package validation; 29 core checks pass.
- Android unit tests and debug APK build pass.
- 15 real-server integration checks pass, including waiting for acknowledgment before changing the Windows selection and applying an acknowledged phone request.
- Emulator at 375dp: both lenses selectable; front preference survives application restart.
- Landscape at 200% font size with animations disabled: controls and primary action remain reachable by scrolling.
- Text contrast: primary/surface 15.5:1 approximately; actual computed ratios recorded during implementation, all tested text pairs exceed 4.5:1.

Physical-device camera switching, call continuity and latency remain unverified in this environment. Integration tests emulate protocol acknowledgments; they do not certify phone hardware. Installation packages are development builds.
