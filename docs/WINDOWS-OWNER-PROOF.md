# Windows service-side owner proof

Status: implemented and locally regression-tested; Windows routing, two-account
isolation and compatibility certification remain blocked. This is development
source, not a signed/installed release. No system camera registration, account,
installed application or GitHub repository was changed during these tests.

## Authorization boundary

The source no longer treats a supplied owner SID, mapping name or a Local Service
execution identity as permission to return live pixels. They still locate and
validate the protected frame resource. A separate private grant is mandatory for
all live output, including service-hosted sources.

For each approved camera session, the native publisher generates a new 256-bit
random bearer proof with Windows CNG. The publisher already verifies that its
owner SID matches its Windows process token. Only SHA-256 of the proof is written
to the user-restricted frame mapping. The proof itself remains private; it is not
in camera activation attributes, the mapping, logs, UI diagnostics, phone traffic
or persistent settings.

The publisher delivers the proof using IMFVirtualCamera::SendCameraProperty on
its own MFVirtualCameraAccess_CurrentUser camera object. The source accepts a
fixed-size SET-only private IKsControl property and stores a digest of that grant
in its private instance. Support queries return only the SET access flag; GET
cannot retrieve a proof or digest. A valid-size SET acknowledgment means the
grant was stored, not that a frame has yet been authorized.

Before reporting new-frame availability, claiming consumer demand or copying any
live pixels, the source compares its private grant with the current protected
mapping verifier. No grant, a random grant, a copied public verifier, revoked
permission, or an old session grant produces live output. Neutral NV12 is written
instead. Cached private pixels are erased when the grant is cleared or no longer
matches. Owner/mapping configuration cannot change after initialization.

The owner proof rotates at fresh approval. Stop disables output immediately,
erases the publisher's private proof and clears the mapped verifier when it can
take the mapping lock. Even if that lock is contended, disabled output and the
existing one-second stale-frame cutoff remain in force. Source Stop/Shutdown
clear their grant. Approved-session lease commands renew the grant through the
camera object so a recreated source can receive it. Failed camera-property
delivery stops native publication and tells the host to end the corresponding
session, rather than falling back to SID-only access.

## Why a copied SID is insufficient

A different user can supply the victim's public owner/mapping attributes, but
cannot satisfy the victim publisher's fresh random proof by supplying their own
proof or by copying its hash. The verifier is not accepted as the bearer value.
Producing frames into that mapping also requires Windows access to the protected
objects, and the supplied producer SID must match the producer's process token.

The intended ownership binding is Windows' per-user camera object plus the
private proof routed through that object. Microsoft documents CurrentUser
cameras as distinct for each account, and SendCameraProperty as a wrapper around
the underlying IKsControl operation. Our inference is that this route can grant
the legitimate source instance without granting an attacker's cloned camera.
That routing assumption still requires actual Windows integration tests; these
local tests do not prove it. A failure to reach the intended instance will leave
video neutral or stop sharing, not automatically bypass the check.

Trust assumptions include an uncompromised Windows kernel/Frame Server and
protected installed Camber binaries. This does not protect against an
administrator, compromised Local Service execution, or arbitrary malware already
running with the camera owner's full Windows permissions.

## Verification completed

- All eight native CTest suites pass.
- Real FrameReader code, with a uniquely named Local test resource and its normal
  Windows ACL checks, renders generated live pixels only with the correct proof.
  Another reader with copied attributes stays neutral and cannot claim consumer
  demand. No security predicate is bypassed in that fixture.
- Missing, malformed, zero, wrong, hash-as-bearer, rotated, cleared and revoked
  proofs, and legacy mapping versions produce neutral output.
- Own-user metadata activation and direct foreign-identity denial still pass.
- The actual COM source's IKsControl rejects GET, malformed requests and invalid
  proof lengths, reports only SET support, and rejects SET after shutdown.
- Generated H.264 transport verifies a live producer verifier exists and Stop
  clears both output and verifier. This transport test is separate from the
  FrameReader proof test; neither substitutes for OS camera routing tests.
- Host build/publish completes; 32 core checks pass. Host handles a current-
  generation authorization failure by stopping the session. Full host/native/
  Windows-service failure delivery still needs integration testing.

The test fixtures use generated pixels, never physical cameras. They neither
install a COM server nor modify the live Camber mapping. Source-level resource
prefix injection is a C++ constructor dependency only; activation attributes
cannot change the production Global resource prefix.

## Required integration validation

1. In an isolated Windows setup, install the complete matching new native DLL,
   media worker and host. Confirm the registered DLL path and restrictive file
   permissions. Do not mix these components with older packages.
2. Under account B, approve generated video and verify the OS camera delivers it
   to a real consumer after the owner property is sent. Inspect shared-client
   routing, startup, Stop/Start, reconnect and source recreation.
3. Under account A, register an independent CurrentUser camera using copied B
   owner/mapping attributes. Exercise no proof, A's own proof, the public hash,
   and stale proof. Observe only neutral output; verify B's valid consumer still
   works. Do not use another person's live camera for this test.
4. Verify concurrent consumers, user switching, lock/sleep, native termination,
   missing/unsupported control properties and mixed-version rejection. Measure
   cutoff times in actual consumer output and confirm no preview-only bypass.
5. Complete independent security review and all existing beta release gates.

The private native frame ABI is now version 2. Android signaling remains version
1 and no Android changes are needed for this internal PC proof. Older Windows
packages are superseded and unapproved. New development binaries are staged in
artifacts/ownership-proof, separately from the installed app. No installer is
being distributed and no GitHub push has been performed.

Primary references:
- [CurrentUser camera scoping](https://learn.microsoft.com/en-us/windows/win32/api/mfvirtualcamera/nf-mfvirtualcamera-mfcreatevirtualcamera)
- [SendCameraProperty](https://learn.microsoft.com/en-us/windows/win32/api/mfvirtualcamera/nf-mfvirtualcamera-imfvirtualcamera-sendcameraproperty)
- [Shared-client GetMediaSource](https://learn.microsoft.com/en-us/windows/win32/api/mfvirtualcamera/nf-mfvirtualcamera-imfvirtualcamera-getmediasource)
