; ============================================================================
;  AX25Chat - Inno Setup script (bilingual English / French)
;
;  Saved as UTF-8 with a byte-order mark: without it Inno Setup may read the
;  file in the system code page, and the French accents come out garbled.
;
;  Built by build_all.bat, which stages everything in dist\ first:
;      ISCC.exe /DAppVersion=x.y.z AX25Chat.iss
;
;  The version is passed by build_all.bat (/DAppVersion=x.y.z), which reads
;  it from CMakeLists.txt. The value below only serves when this file is
;  compiled by hand, and is then wrong: go through build_all.bat.
; ============================================================================

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#define AppName "AX25Chat"
#define AppPublisher "AX25Chat Project"
#define AppExeName "ax25chat.exe"

[Setup]
AppId={{9C8B7E52-3D3A-4E0B-9F6F-2A7E8C1D5B40}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}
OutputDir=output
OutputBaseFilename={#AppName}-{#AppVersion}-setup
SetupIconFile=..\assets\ax25chat.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
LicenseFile=..\LICENSE.txt

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french";  MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Everything windeployqt and build_all.bat gathered: the program, the Qt
; runtime, the data files and the optional symbol artwork.
Source: "dist\*"; DestDir: "{app}"; Excludes: "vc_redist.x64.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
; The Visual C++ runtime, unpacked to a temporary folder, run, then deleted.
#ifexist "dist\vc_redist.x64.exe"
Source: "dist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
#endif

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
; A runtime already present, of the same or a newer version, makes it return
; at once with a non-zero code, which Inno Setup ignores.
#ifexist "dist\vc_redist.x64.exe"
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
    StatusMsg: "{cm:InstallingRuntime}"; Flags: waituntilterminated
#endif
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[CustomMessages]
english.InstallingRuntime=Installing the Microsoft Visual C++ runtime...
french.InstallingRuntime=Installation du runtime Microsoft Visual C++...

[Code]
// The user's settings (config.json, direwolf.conf) live in %APPDATA%\AX25Chat
// and are left alone by the uninstaller, so a reinstall keeps them.
