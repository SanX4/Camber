> Latest service-side work: private owner-proof enforcement is implemented and
> locally tested. OS property routing and two-account validation are still pending.
> See [Windows owner proof](WINDOWS-OWNER-PROOF.md). Older notes below are historical.

# Windows ownership follow-up

Status: partial hardening verified; full account isolation remains BLOCKED.
No installed application, camera registration, Windows account or GitHub repository
was changed. All exercised media is generated; no physical camera was opened.

## Changed boundary

The media source previously accepted a different user's SID in direct in-process
activation. Configure and mapping access now compare the requested owner with the
effective Windows thread identity when impersonation exists. Without a thread
token, ordinary processes must match their process identity. Failure to query a
thread token, including anonymous impersonation, is denied rather than falling
back to the process account. Mapping access rechecks even if the view is cached.

Unimpersonated Local Service/System metadata activation remains compatible. This
exception is NOT authorization of a camera registration owner. Frame Server runs
under a service identity; correlating that activation with the actual requesting
account is still unresolved. This change must not be presented as a complete fix
for the service confused-deputy concern. The old metadata behavior alone is not
proof that another account's pixels were successfully exposed.

## Verification

- The updated activation test, run against the previous candidate DLL in an
  isolated folder, exits 7: it detects acceptance of a foreign-owner fixture.
- The patched DLL rejects that fixture and permits own-user metadata activation.
- Same-user impersonation succeeds, foreign identity and anonymous impersonation
  fail. Existing restricted/world-accessible object ACL checks still pass.
- All seven native CTest suites pass, including generated H.264 transport.
- The test only checks whether a mapping handle already exists; it never reads
  existing live pixels and does not register/unregister a camera.

Latest native components and the before/after result are under
artifacts/security-followup. Earlier artifacts/beta packages predate this patch
and are marked superseded. They have not been repackaged for installation.

## Remaining work

Use an isolated Windows test setup with two test users and generated video to
inspect the service activation identity/context and implement a trusted binding.
Do not substitute a caller-provided symbolic link, SID, process ID or secret in
public camera attributes for operating-system authorization. Exercise forged
registrations, impersonation failures, user switching and legitimate consumers.
Independent security review, physical tests and signing remain mandatory.

References: [OpenThreadToken](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-openthreadtoken)
and [Frame Server security](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/frame-server-custom-media-source#security).
