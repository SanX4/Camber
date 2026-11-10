# Local session protocol v1

Signaling: TLS on TCP 47771, JSON UTF-8, maximum 65,536 bytes per message.
Media: local WebRTC DTLS-SRTP, empty ICE server lists. No public STUN/TURN.
PC certificate: self-signed RSA-3072, SHA-256 pin obtained through QR.

## Enrollment

The current-user-only Windows pipe issues a QR payload containing `version`,
`pcId`, `name`, `endpoint`, `fingerprint`, `secret`, and `expiresAt`.
`secret` is 32 random bytes encoded as 64 hexadecimal characters. Only its
SHA-256 digest remains in the pairing service. Generating another code replaces
the previous code. Attempts are rate limited to five failures per minute.

`POST /pair`: `version`, `secret`, `name` (1-80 characters), `platform`
(`android` or `ios`). Success returns a generated `device`, 256-bit `token`, and
protocol `version`. Only a hash of that token is saved on Windows; the protected
phone store retains the token. Pairing does not grant camera-sharing consent.

## Session

Open `wss://<pc>:47771/session` with `X-Camber-Device` and
`Authorization: Bearer <token>`. There is one active phone socket at a time.
Credentials are headers, never query strings. A remembered phone receives
`state`, containing a state snapshot and current `generation`.

Phone messages:

| Type | Payload | Effect |
|---|---|---|
| `approve` | `generation` | Explicit user consent tied to the latest received state. Stale requests are rejected. |
| `offer` | `sdp`, `generation` | Offer from the approved native phone. |
| `candidate` | `candidate`, `sdpMid`, `sdpMLineIndex`, `generation` | Trickle ICE. |
| `ack` | `ack` | Configuration result, including request ID, generation, actual applied configuration. |
| `pause` | none | Stop local capture and disable native output. |
| `stop` | none | End consent; future frames cannot revive the session. |
| `ping` | none | Receive `pong` and current session state. |

Host messages: `state`, `approved`, `answer`, `candidate`, `configure`, `pause`,
`stop`, `resumeRequested`, `pong`, `error`. A resume request cannot itself start
a phone camera. Only a new phone Start action sends approval.

The generation increments across privacy boundaries. An old generation's
frames, candidates, and configuration acknowledgments cannot update the active
session. Reconnection must preserve the selected device and a valid consent
window; Stop and Windows lock always invalidate consent.

## Local process boundaries

The UI uses newline-delimited JSON over `Camber.Control.<Windows user SID>`.
`PipeOptions.CurrentUserOnly` restricts access. It supports status, pairing,
configuration requests, pause, stop, resume request, revocation, and shutdown.

The host launches the native media worker with redirected standard streams.
Local status includes `video.stage`, `offersSent`, `candidatesSent`,
`framesPublished` and `error` for troubleshooting. These are session-stage
diagnostics, not measured total latency or certified frame-rate statistics.

### Optional reaction extension (0.2, protocol v1)

The phone may send `capabilities` with `gestureReactions: true`. Until that
handshake, automatic gestures are unavailable. The PC sends `effects` with
`enabled` and the current `generation`; this starts/stops phone analysis only.
The phone sends `reaction` with a camelCase reaction name and the generation of
the analyzed frame. No images or landmarks are included. The host validates the
reaction enum, current consent, generation, effects/gesture switches and a
four-second rate limit before forwarding a numeric reaction ID to the worker.
The native worker independently requires matching generation and live permission.
Stop clears active animations. Older companion apps may ignore `effects`; manual
PC reactions remain available.

Local `visuals` commands contain typed `settings` (outputMirror, previewMirror,
reactionsEnabled, gesturesEnabled, reducedMotion). Local `reaction` commands
contain a reaction name; responses include `ok` and an actionable message.
Commands: `permit`, `offer`, `candidate`, `configure`, `stop`. Worker events:
`answer`, `candidate`, `frame`, `error`. Native pixels never travel over JSON.

The camera source validates its owner/mapping attributes during discovery, then
creates `Global\Camber.<SID>` and a companion mutex when the Frame Server requests
samples. Discovery never requires the client's Global-object privilege. ACLs are
for the owning SID, SYSTEM and LOCAL SERVICE. Explicit owner/mapping attributes
are passed during virtual-camera registration. The reader rejects absent or
mismatched attributes. Mapping dimensions and format are bounded before copies.
Output is neutral when disabled or when the last frame is older than one second.

## Compatibility limits

v1 rejects unsupported pairing versions. Current application builds use v1.
Local discovery verification, a full device-mode capability envelope and future
protocol-version negotiation remain release work. The negotiated video extension
below is implemented without changing signaling version 1.

## Optional video-quality extension (0.9, signaling v1)

`capabilities` adds boolean `videoQuality`, `enhancementControls` and
`automaticOrientation`. Missing flags disable that phone's optional control flow.
Old clients may ignore the extension. Neither capability exchange nor settings
selection grants camera consent. New Android clients always use physical orientation;
there is no rotation lock, switch or manual angle preference.

| Direction/type | Fields | Meaning |
|---|---|---|
| Phone `videoStats` | generation; optional bounded numeric captureFps, encodedFps, encodeMs, bitrateBps, lossRatio, networkRttMs, encodedWidth/Height, width/height, sourceRotation, rotation, maxBitrateBps; thermalLimited | One sample per second. Counters use deltas; unavailable metrics stay unknown. Raw RTC reports, IDs, addresses and credentials are excluded. |
| Host `quality` | id, generation, maxBitrateBps, capture | Requested encoder ceiling 2–12 Mbps and 720p30/1080p30 capture demand. WebRTC may transmit below the ceiling. |
| Phone `qualityResult` | id, generation, ok, maxBitrateBps | Confirms sender configuration. Subsequent frames/statistics establish actual dimensions; no invented measured resolution. |
| Phone `selectQuality` | generation, height (720 or 1080) | Requests a new maximum through the existing configure/ack path. One configuration change is pending at a time across both interfaces. |
| Phone `setEnhancement` | generation, mode (original/natural/lowLight) | Requests a persisted PC-owned preference. Does not start capture. |
| Host `videoSettings` | generation, requested, applied, reason, requestedHeight, captureInfo | Synchronizes requested/applied processing and actual capture/encoded dimensions. |

`approve` may include the explicitly selected camera (`front`/`back`) so a lens
chosen before Start is honored atomically with fresh consent. Omission retains
legacy behavior. Stale approval cannot change the lens or grant capture.

Native command `enhance` carries numeric mode 0/1/2, id and generation. Native
`enhancementState` returns the same id/generation with requested/applied mode,
bounded reason and processingMs. Host ignores stale IDs, older generations and
mismatched modes. `decoderStats` supplies optional measured decodeMs and decoded
frame dimensions/rotation, scoped to the active generation. No raw report is exported.

Native image commands use a latest-settings handoff. Graphics work stays on the
compositor thread; settings and Stop do not wait for a blocked GPU operation.
Output enable/proof revocation still precedes clearing pending processing.

The per-session private owner proof and **frame ABI 2** remain mandatory. Frame
layout and mapped byte capacity are unchanged. Landscape and portrait buffers
must be even, have each dimension at most 1920 and total pixels at most 1920×1080.
This accepts rotated storage without allowing a larger allocation. Rotation is
validated cardinal frame metadata, used once for effects and final framing.

Visual preference writes and native enhancement dispatch are serialized. Legacy bulk visual updates preserve the separately selected image preset. Status snapshots suppress old-session measurements/processing confirmations, and native decoder dimensions remain unknown until a valid frame arrives. Enhancement state includes a fallback flag for explicit retry UI.
