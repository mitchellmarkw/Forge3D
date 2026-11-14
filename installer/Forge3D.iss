; SPDX-License-Identifier: GPL-3.0-or-later
; The Forge3D installer (Inno Setup 6). The Windows build runs it after packaging:
;   iscc /DAppVersion=0.25.0 /DAppLabel=Beta /DSourceDir=..\dist\Forge3D /DOutputDir=..\dist installer\Forge3D.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef AppLabel
  #define AppLabel ""
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\Forge3D"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif
#if AppLabel == ""
  #define AppFullVersion AppVersion
#else
  #define AppFullVersion AppVersion + " " + AppLabel
#endif

[Setup]
; Never change AppId: Windows uses it to find an installed Forge3D to update or remove.
AppId={{7C1E3F2A-5B84-4D1E-9A36-F03D3D8C2B51}
AppName=Forge3D
AppVersion={#AppVersion}
AppVerName=Forge3D {#AppFullVersion}
AppPublisher=The Forge3D contributors
AppPublisherURL=https://github.com/mitchellmarkw/Forge3D
AppSupportURL=https://github.com/mitchellmarkw/Forge3D/issues
AppUpdatesURL=https://github.com/mitchellmarkw/Forge3D/releases
DefaultDirName={autopf}\Forge3D
DefaultGroupName=Forge3D
DisableProgramGroupPage=yes
; Install just for you (no administrator needed) or for everyone on the PC.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=Forge3D-{#AppVersion}-Setup
SetupIconFile=..\src\app\appicon\forge3d.ico
UninstallDisplayIcon={app}\Forge3D.exe
UninstallDisplayName=Forge3D {#AppFullVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "associate"; Description: "Open .f3d files with Forge3D"; GroupDescription: "Files:"

[Files]
; Everything in the package except the testing material and debug symbols.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "test-sheets\*,*.pdb"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Forge3D"; Filename: "{app}\Forge3D.exe"
Name: "{autodesktop}\Forge3D"; Filename: "{app}\Forge3D.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.f3d"; ValueType: string; ValueName: ""; ValueData: "Forge3D.Model"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Forge3D.Model"; ValueType: string; ValueName: ""; ValueData: "Forge3D model"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Forge3D.Model\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Forge3D.exe,0"; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Forge3D.Model\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Forge3D.exe"" ""%1"""; Tasks: associate

[Run]
Filename: "{app}\Forge3D.exe"; Description: "{cm:LaunchProgram,Forge3D}"; Flags: nowait postinstall skipifsilent
