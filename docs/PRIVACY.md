# Local camera privacy

Camber uses pinned HTTPS/WSS signaling and local WebRTC DTLS-SRTP video between
the paired phone and Windows. It has no cloud media relay or recording feature.
Pairing does not grant permission to start video. Start requires explicit phone
approval, and stale requests cannot resume a stopped or paused session.

Pause, Stop, app closure, Android focus loss and lock retain existing cutoff
behavior. The Windows camera source still requires the private per-session
owner proof; this update does not relax that gate or change frame ABI version 2.
Windows service routing and two-account integration tests are still mandatory.
Encryption alone cannot protect a compromised phone or PC.

Image presets run locally, before gesture overlays. The bundled gesture model
is preserved. Camera frames are not saved. Numeric diagnostics allow frame sizes,
rotation, frame rates, bitrate, stage duration, packet loss, RTT and thermal state.
Missing measurements remain unknown. Raw WebRTC reports, addresses, secrets,
credentials and frames are not exported by the new quality diagnostics.

Existing pairing records and protected data locations remain unchanged. Private
preservation archives may contain historical logs and emulator state. Keep them
local and do not publish them as installers or upload them to GitHub.
