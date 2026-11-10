# November 10 validation and publication decision

## Owner follow-up: source upload authorized

On November 10, the owner reported that meeting-app, installer and cross-account
checks are OK and explicitly requested pushing the project to SanX4/Camber.
These are owner-reported results; detailed independent test evidence was not
provided. The owner authorizes this development source upload. The remaining
production-signing, certification and distribution limitations below still apply;
this update does not mark the automated release evidence gate as passed.

The earlier no-push decision below records the state before this follow-up.

**Decision: do not push or publish this candidate yet.** The user requested
publication to a new `SanX4/Camber` repository with releases only if every
required test passes. Several mandatory checks remain incomplete. No repository
or release has been created and no packages have been uploaded.

## Completed work

The modern Windows wizard reuses the approved offset-C logo, follows Windows
light/dark appearance and has a welcome page, visible installation-folder page,
optional desktop shortcut and optional startup in the tray. The checkbox page
explains the machine-wide shortcut scope. Options start unchecked and retain
upgrade selections; deselection removes the corresponding Camber link. Startup
does not approve sharing. The completion page launches as the original user.

The [complete connection guide](CONNECTING.md) covers installation, APK choice,
QR pairing, local-network troubleshooting, camera selection in other apps,
quality, rotation, reactions and privacy. It is included in the Windows package
with the documentation. [Release notes](RELEASE-NOTES-0.9.0-dev.1.md) and a local
six-file release/checksum inventory are prepared.

## Observed results

| Check | Result and limits |
| --- | --- |
| Core policies | 46 checks pass. |
| Native components | All nine suites pass, including proof/ACL, generated H.264, effects and enhancement. |
| Android units | 20 Debug and 20 Internal JVM tests pass. |
| Android lint | Completes with no errors; informational/upgrade and other warnings remain. |
| Signaling integration | 17 isolated real-server checks pass, including pinned TLS, wrong certificate rejection, pairing replay rejection and privacy precedence. The missing-engine path fails closed. |
| Installer options | 12 fixture checks pass: four checkbox combinations, paths with spaces, targets/arguments, upgrade deselection and uninstall cleanup. The fixture redirects shell folders and omits privileged operations. |
| Windows startup | Updated app runs hidden at `--startup`, with the engine ready and approval false. Normal launch creates the native window. |
| Windows app termination | Terminating the test UI caused its host/media workers to exit after approximately 7.1 seconds. This checks the UI lease, not an active-call consumer cutoff. |
| Actual Windows camera | The installed source delivered approved phone pixels through Windows Frame Server. A 90-frame observation contained 90 non-neutral frames and 77 distinct sampled signatures; a longer observation also delivered live frames. This is one user/device, not a 29-fps certification. No frames or hashes were saved. |
| Live controls | Front/rear hardware acknowledgments completed in roughly 1.4-1.6 seconds. Original, Natural and Low light were applied. Both requested effects were accepted. |
| Final Android APK privacy | PC Pause produced neutral consumer output after 16 ms; phone focus loss after 250 ms. Camera release was observed within about 0.6-0.7 seconds. Capture did not restart on return and the Out of focus explanation was present. Gesture inference ran without a logged runtime failure. |
| Earlier candidate privacy | PC Pause 31 ms, focus loss 234-235 ms and phone process termination 203 ms. The final APK process-termination repeat was interrupted by the USB test connection, so the earlier result does not certify the final APK. |
| Android payload | Five optimized APKs and complete AAB retain native-library/model hashes and the development signing identity; 16 KB checks pass. Updated ARM64 APK installed in place without clearing app data. |
| Dependency review | Guava 27.0.1-android and Protobuf Lite 4.26.1 had advisory version matches. They were replaced by 33.7.2-android and 4.28.2. Sixteen new artifact hashes were cross-checked against canonical HTTPS downloads; old verification hashes were unchanged. |
| Advisory queries | Resolved desktop/host NuGet queries returned no listed vulnerable packages. A successful OSV query after the Android change found zero app-runtime matches and 18 build/test-tool matches. Native/transitive provenance and reachability review remain separate. |
| Source preparation | Credential-pattern scan found no token/private-key patterns in candidate source. Synthetic foreign-SID fixtures are test values. Archives, toolchains, local data, keys and private reports remain outside Git. This is not a complete secret/security audit. |

Timing uses a local monotonic command marker and the real consumer's transition
to neutral pixels. Phone-command measurements include ADB dispatch and observation
overhead. These short runs are not external glass-to-glass measurements or
population-level latency percentiles. The first privacy harness run acted before
some consumers had actually delivered live frames; those cases were discarded.
The confirmation harness waits for two distinct live frames and fails closed
when readiness/cutoff cannot be established.

## Incomplete or blocked checks

- Two-account, cloned-registration, forged-owner and fast-user-switch isolation.
  Single-account success does not prove this boundary.
- Independent security review and the remaining build/test-tool advisory,
  native dependency, transitive license and provenance review.
- Final-APK termination repeat after the USB interruption; physical lock/sleep,
  adverse-network, thermal, reference-device and full rotation matrices.
- Two-hour endurance, controlled unique-frame rate, external latency and
  held-out gesture recall/precision. Previous usability feedback is not a
  >=95%/99% recognition certification.
- Actual Discord/Meet/OBS/Teams/Zoom remote participant and concurrent-consumer
  compatibility. The source-reader test does not replace those applications.
- Measured Windows/Android animation deadlines, Narrator/TalkBack, high contrast
  and large-text/display-scale matrices. Supplemental mixed Android gfx samples
  included missed frames and do not establish the 99% deadline target. The
  Windows UI automation helper failed before initialization, so no visual
  wizard/desktop certification is claimed.
- Full elevated clean-install/upgrade/repair/relocation/uninstall/rollback and
  standard-user/other-admin/multi-account paths. Shortcut fixtures do not certify
  actual camera registration, firewall lifecycle or UAC behavior.
- Production Windows/Android signing, independently verified signatures,
  exact-source evidence, hosted CI and owner approval of signed candidates.
- GitHub is not authenticated on the PC. The requested destination is
  `SanX4/Camber`; release uploads remain conditional on the required checks.

Run `python tools/beta-readiness.py --evidence artifacts/beta/evidence.json` to
evaluate the exact-candidate evidence. It currently fails closed. Never replace
missing reports with asserted pass values.

Private raw results and build logs are kept under
`artifacts/0.9.0-dev.1/reports/release-check-20261110`. Only sanitized summaries
belong in source. No private camera images, pairing secrets or keys were saved
or uploaded by these tests.
