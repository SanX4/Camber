> Latest service-side work: private owner-proof enforcement is implemented and
> locally tested. OS property routing and two-account validation are still pending.
> See [Windows owner proof](WINDOWS-OWNER-PROOF.md). Older notes below are historical.

> Ownership follow-up: direct user activation and impersonation checks are now
> patched and tested. Full Frame Server account isolation is still unresolved.
> See [follow-up](WINDOWS-ISOLATION-FOLLOWUP.md). Earlier packaged candidates predate this patch.

# Camber 0.8 beta security status — NOT APPROVED

The transport remains pinned HTTPS/WSS plus WebRTC DTLS-SRTP. This source review
does not establish that the entire app is secure. The candidate is blocked from
distribution pending camera-registration identity binding, two-account isolation
tests and independent review. A caller-supplied owner SID is not trusted evidence
of the Windows camera registration owner. Restricted mapping ACLs do not resolve
that confused-deputy risk by themselves.

New controls include production frame-object owner/DACL validation, producer SID
binding to its process token, bounded GUID reaction replay protection, late-callback
session fencing, camera interruption shutdown, bounded ICE queues, and a separate
publication lock that disables output before expensive rendering cleanup.
Generated H.264 transport tests exercise the actual producer ACL checks.

Phone camera teardown runs asynchronously. Focus loss first disables the track,
sends a privacy command and closes the signaling connection. Returning requires
a new Start. These implementation changes still need physical cutoff timing and
crash/lock tests; a shared-buffer test does not prove a remote app's cutoff time.

Release builds are non-debuggable and use R8. CI actions and dependency versions
are pinned, with NuGet locks and bootstrapped Gradle checksums. Independent origin
and native/transitive license review remain required. NuGet advisory queries found
no listed affected resolved dependencies; Maven lookup failed TLS negotiation and
remains unverified. No TLS checks were bypassed for that lookup.

See BETA-CHECKLIST.md for gates and BETA-VALIDATION.md for test procedure. The
following older review records historical fixes, not approval of this candidate.

---

# Camber 0.5 security review — 9 November 2026

Scope: phone-to-PC transport, pairing, local storage, Windows installation and
capture lifecycle. This is a source review with local regression tests, not an
independent penetration test or a guarantee against compromise.

## Transport and identity

Video is direct peer-to-peer WebRTC using DTLS-SRTP. The Windows receiver now
explicitly selects DTLS-SRTP; no public STUN, TURN, cloud processing or recording
is configured. Signaling is HTTPS/WSS. The Android app verifies the exact SHA-256
PC certificate fingerprint carried in the QR code and certificate validity.
Redirects and cleartext traffic are disabled. Hostname verification is replaced
by this explicit certificate-pin identity check for local self-signed PCs.
SDP fingerprints therefore travel over the authenticated pinned channel.

Pairing uses a 256-bit, single-use, two-minute secret and rate-limited attempts.
Stored phone tokens are hashed on Windows; Windows private enrollment/certificate
material uses DPAPI, and Android enrollment uses Keystore-backed AES-GCM.
Pairing alone never authorizes camera capture. Revocation remains available.

Reference: https://www.rfc-editor.org/rfc/rfc8827.html

## Findings repaired

1. Closing the Windows window previously hid it in the tray. Close now calls
   the same privacy stop and shutdown path as Quit.
2. Disconnect previously retained approval for a recovery window. It now revokes
   active consent and advances the generation; old frames/reactions/Start requests
   cannot restore it. An existing privacy pause remains paused.
3. Half-open/crashed sessions lacked application-level liveness enforcement.
   Phone/PC heartbeat monitoring now expires missing peers after six seconds
   plus scheduling time (normally within about seven seconds). The media worker
   independently stops publishing when host control traffic expires. Existing
   one-second stale-frame substitution remains in the camera source.
4. Android capture was released only in onStop. Loss of window focus and onPause
   now cancel pending approval, release capture, send an Out of focus pause and
   close signaling. Returning does not resume camera capture automatically.
5. F:\Camber inherited Modify access for Authenticated Users, unsuitable for the
   native camera DLL Windows loads in its service. The installed app and COM
   registration were moved through the installer to C:\Program Files\Camber.
   Its inherited ACL grants ordinary users read/execute, not modification.
   Old F:\Camber launchers were archived with .pre-security.bak suffixes and
   Open Camber.lnk points to the protected installation.

The host requires a companion advertising session-heartbeat support before it
approves capture. Older companion builds fail closed with an update message.
The current iPhone source has not implemented that extension and cannot stream
to this host until updated; Android 0.5 is the supported companion for this build.

## Verification

- 29 core tests, including immediate disconnect revocation and privacy fencing.
- Five native test suites, including generated H.264 transport, reaction gating,
  and expiry of the media worker's host heartbeat while video is arriving.
- 13 real-server checks: pinned TLS, rejected certificate, invalid/replayed pairing,
  authenticated session consent, Stop, stale commands and phone heartbeat expiry.
- Android APK builds. Emulator lifecycle test confirms the Out of focus message
  after Home/app switching and return. No physical phone focus-loss timing test
  has been completed for this release.
- Windows Close was exercised through the window close command; host and media
  process exit were verified. The protected installation's ACL was inspected.
- NuGet vulnerability check for the host reports no known vulnerable packages
  from the configured source at review time. This does not cover every Android,
  native/WebRTC or transitive dependency.

## Boundaries and remaining release work

Encryption protects the phone-to-PC link, not a compromised phone/PC, exposed
pairing QR code, administrator, debugger or malicious application running as
the same Windows user. Windows camera-authorized applications can consume the
virtual camera while it is sharing. A meeting service's handling/encryption after
it receives frames is separate from Camber's link.

No independent multi-user/frame-buffer isolation test, comprehensive dependency
and decoder security assessment, fuzzing or third-party penetration test has been
completed. The distributed APK is debuggable and Windows binaries are unsigned
development builds. Signed production distribution, full dependency review and
independent security testing remain release requirements. Do not describe this
alpha as independently certified or impossible to intercept/steal.
