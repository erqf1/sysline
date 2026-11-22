; Sysline installer (Inno Setup 6). Build: iscc /DMyAppVersion=1.0.1 packaging\windows\sysline.iss
; Expects the portable build in dist\Sysline (see the release workflow).
#ifndef MyAppVersion
  #define MyAppVersion "1.0.1"
#endif

[Setup]
AppId={{B7E3C1A4-5D29-4F6B-9A80-3C2E7D41F5A9}
AppName=Sysline
AppVersion={#MyAppVersion}
AppPublisher=W Software
DefaultDirName={autopf}\Sysline
DefaultGroupName=Sysline
UninstallDisplayIcon={app}\Sysline.exe
SetupIconFile=..\..\assets\icon.ico
OutputDir=..\..\dist
OutputBaseFilename=Sysline-windows-x64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DisableProgramGroupPage=yes
DisableDirPage=no
UsePreviousAppDir=yes
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
; Desktop-Symbol nur bei der Erst-Installation anbieten (Updates fassen es nicht an)
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; Check: IsFirstInstall

[Files]
Source: "..\..\dist\Sysline\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{autoprograms}\Sysline"; Filename: "{app}\Sysline.exe"; AppUserModelID: "WSoftware.Sysline"
Name: "{autodesktop}\Sysline"; Filename: "{app}\Sysline.exe"; AppUserModelID: "WSoftware.Sysline"; Tasks: desktopicon; Check: IsFirstInstall

[Run]
Filename: "{app}\Sysline.exe"; Description: "{cm:LaunchProgram,Sysline}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\taskkill.exe"; Parameters: "/f /im Sysline.exe"; Flags: runhidden; RunOnceId: "StopSysline"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "Sysline"; Flags: uninsdeletevalue dontcreatekey

[Code]
var
  FirstInstall: Boolean;

// Desktop-Symbol nur bei der Erst-Installation: Inno Setup trägt sich unter HKLM (alle Benutzer) oder HKCU
// (nur ich) ein. Gibt es den Eintrag schon, ist es ein Update (auch still über den eingebauten Updater) -
// dann bleibt das Symbol, wie es ist: kein neues Anlegen, kein Springen, gelöscht bleibt gelöscht.
function InitializeSetup(): Boolean;
var
  Key: String;
begin
  Key := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{{B7E3C1A4-5D29-4F6B-9A80-3C2E7D41F5A9}}_is1';
  FirstInstall := not (RegKeyExists(HKLM64, Key) or RegKeyExists(HKLM32, Key) or RegKeyExists(HKCU, Key));
  Result := True;
end;

function IsFirstInstall(): Boolean;
begin
  Result := FirstInstall;
end;
