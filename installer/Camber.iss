#define AppName "Camber Camera"
#ifndef AppVersion
  #error Pass AppVersion from tools/build.ps1; Directory.Build.props is the version source.
#endif
#ifndef PublishDirectory
  #define PublishDirectory "..\artifacts\invalid-build-path"
#endif
#if !FileExists(PublishDirectory + "\app\Camber.Desktop.pri")
  #error The Camber interface resources are missing. Publish the desktop project before building Setup.
#endif
[Setup]
AppId={{CC6E4C7C-9D73-44C2-B7F1-B64162890CF6}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Camber
AppComments=Your phone. A better camera.
DefaultDirName={autopf}\Camber
DefaultGroupName=Camber
DisableProgramGroupPage=yes
DisableWelcomePage=no
DisableDirPage=no
UsePreviousAppDir=yes
UsePreviousTasks=yes
WizardStyle=modern dynamic
WizardSizePercent=110
WizardImageFile=assets\wizard-large.bmp
WizardSmallImageFile=assets\wizard-small.bmp
SetupIconFile=..\src\Camber.Desktop\Assets\Camber.ico
ShowLanguageDialog=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
PrivilegesRequired=admin
OutputDir=..\artifacts\installer
OutputBaseFilename=Camber-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
LZMANumBlockThreads=2
UninstallDisplayIcon={app}\Camber.Desktop.exe
CloseApplications=no
[Messages]
WelcomeLabel1=Welcome to Camber
WelcomeLabel2=Your phone. A better camera.%n%nSet up your Android phone as a camera for Discord, Google Meet, Teams, Zoom and more.%n%nSetup installs Camber for all Windows users. You choose the location and optional shortcuts on the next pages.%n%nYour camera stays off until you approve sharing on your phone. Your existing microphone stays selected.
SelectDirDesc=Where should Camber be installed?
SelectDirLabel3=Choose a folder for Camber. Your settings and paired devices are stored separately and are preserved when updating.
SelectTasksDesc=Make Camber work your way
SelectTasksLabel2=Choose the optional shortcuts below. These choices apply to all Windows users on this PC. Startup opens Camber in the tray; it never starts camera sharing.
ReadyLabel1=Ready to set up your camera
FinishedHeadingLabel=Camber is ready
FinishedLabel=Open Camber, install the Android companion, then scan the QR code to pair.%n%nKeep both devices on the same trusted Wi-Fi network. In your meeting app, select Camber Camera and keep your existing microphone.
[Files]
Source: "{#PublishDirectory}\app\Camber.UpgradeCheck.exe"; DestDir: "{tmp}"; Flags: dontcopy
Source: "{#PublishDirectory}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\.deps\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
#include "options.iss"
[Icons]
Name: "{group}\Camber Camera"; Filename: "{app}\Camber.Desktop.exe"
[Run]
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""Camber local signaling"""; Flags: runhidden waituntilterminated
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""Camber local video"""; Flags: runhidden waituntilterminated
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; Flags: runhidden waituntilterminated
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\app\Camber.Camera.dll"""; Flags: runhidden waituntilterminated
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""Camber local signaling"" dir=in action=allow program=""{app}\app\Camber.Host.exe"" protocol=TCP localport=47771 profile=private remoteip=localsubnet"; Flags: runhidden waituntilterminated
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""Camber local video"" dir=in action=allow program=""{app}\app\Camber.Media.exe"" protocol=UDP profile=private remoteip=localsubnet"; Flags: runhidden waituntilterminated
Filename: "{app}\Camber.Desktop.exe"; Description: "Open Camber"; WorkingDir: "{app}"; Flags: postinstall nowait skipifsilent runasoriginaluser
[UninstallRun]
Filename: "{app}\app\Camber.Host.exe"; Parameters: "--shutdown"; Flags: runhidden waituntilterminated; RunOnceId: "StopHost"
Filename: "{app}\app\Camber.Media.exe"; Parameters: "--remove-current"; Flags: runhidden waituntilterminated; RunOnceId: "RemoveCamera"
Filename: "{sys}\regsvr32.exe"; Parameters: "/s /u ""{app}\app\Camber.Camera.dll"""; Flags: runhidden waituntilterminated; RunOnceId: "UnregisterSource"
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""Camber local signaling"""; Flags: runhidden waituntilterminated; RunOnceId: "RemoveSignalingRule"
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""Camber local video"""; Flags: runhidden waituntilterminated; RunOnceId: "RemoveVideoRule"
[Code]
#include "legacy-cleanup.iss"
function PrepareToInstall(var NeedsRestart: Boolean): String;
var Code: Integer;
begin
  Result := '';
  ExtractTemporaryFile('Camber.UpgradeCheck.exe');
  if (not Exec(ExpandConstant('{tmp}\Camber.UpgradeCheck.exe'), '', '', SW_HIDE, ewWaitUntilTerminated, Code)) or (Code <> 0) then
    Result := 'Close Camber on all Windows accounts before installing. Active camera sessions will not be interrupted by Setup.';
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then RemoveKnownLegacyFiles;
end;

function InitializeUninstall(): Boolean;
var Code: Integer;
begin
  Result := False;
  if not FileExists(ExpandConstant('{app}\app\Camber.UpgradeCheck.exe')) then begin
    MsgBox('Camber cannot verify running sessions. Close Camber on all accounts and repair this installation before uninstalling.', mbError, MB_OK);
    exit;
  end;
  if (not Exec(ExpandConstant('{app}\app\Camber.UpgradeCheck.exe'), '', '', SW_HIDE, ewWaitUntilTerminated, Code)) or (Code <> 0) then begin
    MsgBox('Close Camber on all Windows accounts before uninstalling.', mbInformation, MB_OK);
    exit;
  end;
  Result := True;
end;
