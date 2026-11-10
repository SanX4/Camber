# Camber Windows Setup

The production script uses the existing machine-wide installation identity and
requires administrator access for its protected files, camera registration and
private-network firewall rules. The modern wizard follows the Windows light/dark
appearance and keeps standard keyboard and accessibility controls.

The folder page is always available, including during upgrades. Both optional
tasks start unchecked on a fresh installation and retain previous selections
on upgrade:

- **Desktop shortcut:** creates Camber's link on the shared desktop.
- **Start at sign-in:** creates Camber's link in the shared Startup folder,
  passing `--startup`. The application starts in the tray; it never approves a
  phone, starts capture or restores a previous consent session.

The wizard explicitly explains that these shortcuts apply to all Windows users.
Shared folders avoid accidentally configuring only an elevation account when a
standard user supplies another administrator's credentials. Deselecting a task
during upgrade removes only its Camber shortcut. Uninstall removes both links.
Pairing and preferences remain in their existing protected per-user locations.

Setup retains the active-session guard. It never closes active calls to proceed.
The completion page offers to open Camber as the original, unelevated user.
Silent installation does not launch the application. Silent task selection uses
the native `/TASKS="desktopicon,startup"` or `/TASKS=""` arguments.

Rebuild the artwork with `python tools/generate-installer-art.py`; it reuses the
approved offset-C mark and requires Pillow only on the developer's machine.
Build the complete offline package with
`./tools/build.ps1 -Native -LocalCompiler -Installer`.

Full installation, repair, relocation, UAC, multi-account and uninstall tests
must use a controlled Windows environment and preserve real user pairing data.
