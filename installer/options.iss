; Shared by production Setup and the unprivileged shortcut lifecycle fixture.
; Test builds redirect only the shell folders into their own disposable folder.
#ifndef CamberDesktopFolder
  #define CamberDesktopFolder "{commondesktop}"
#endif
#ifndef CamberStartupFolder
  #define CamberStartupFolder "{commonstartup}"
#endif
[Tasks]
Name: desktopicon; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts for all Windows users:"; Flags: unchecked
Name: startup; Description: "Start Camber in the tray at Windows &sign-in"; GroupDescription: "Shortcuts for all Windows users:"; Flags: unchecked
[Icons]
Name: "{#CamberDesktopFolder}\Camber Camera"; Filename: "{app}\Camber.Desktop.exe"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{#CamberStartupFolder}\Camber Camera"; Filename: "{app}\Camber.Desktop.exe"; Parameters: "--startup"; WorkingDir: "{app}"; Tasks: startup
[InstallDelete]
; Remove only Camber's optional links when an upgrade deselects a task.
Type: files; Name: "{#CamberDesktopFolder}\Camber Camera.lnk"; Tasks: not desktopicon
Type: files; Name: "{#CamberStartupFolder}\Camber Camera.lnk"; Tasks: not startup
