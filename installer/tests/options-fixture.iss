; Exercises the exact production task/link entries with no elevation, COM,
; firewall, real startup entries, user settings or camera activity.
#define CamberDesktopFolder "{app}\test-desktop"
#define CamberStartupFolder "{app}\test-startup"
[Setup]
AppId=Camber-Installer-Options-Fixture
AppName=Camber installer options fixture
AppVersion=1.0
DefaultDirName={localappdata}\Camber-Installer-Options-Fixture
PrivilegesRequired=lowest
CreateUninstallRegKey=no
DisableProgramGroupPage=yes
DisableDirPage=no
WizardStyle=modern dynamic
OutputBaseFilename=Camber-Installer-Options-Fixture
UsePreviousTasks=yes
Uninstallable=yes
CloseApplications=no
[Files]
Source: "..\..\native\build-msvc-local\Camber.Launcher.exe"; DestDir: "{app}"; DestName: "Camber.Desktop.exe"; Flags: ignoreversion
#include "..\options.iss"
