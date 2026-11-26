; SPDX-License-Identifier: GPL-3.0-or-later
; The Forge3D Scenery installer (Inno Setup 6). The Windows build runs it after packaging:
;   iscc /DAppVersion=0.1.0 /DAppLabel=Beta /DSourceDir=..\dist\Scenery /DOutputDir=..\dist installer\Scenery.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef AppLabel
  #define AppLabel ""
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\Scenery"
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
; Never change AppId: Windows uses it to find an installed Forge3D Scenery to update or
; remove. (Forge3D has its own; the two install, update and uninstall separately.)
AppId={{88A09C39-BC59-402D-820F-34A233F25161}
AppName=Forge3D Scenery
AppVersion={#AppVersion}
AppVerName=Forge3D Scenery {#AppFullVersion}
AppPublisher=The Forge3D contributors
AppPublisherURL=https://github.com/mitchellmarkw/Forge3D
AppSupportURL=https://github.com/mitchellmarkw/Forge3D/issues
AppUpdatesURL=https://github.com/mitchellmarkw/Forge3D/releases
DefaultDirName={autopf}\Forge3D Scenery
DefaultGroupName=Forge3D Scenery
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=Forge3D-Scenery-{#AppVersion}-Setup
SetupIconFile=..\src\scenery\appicon\scenery.ico
UninstallDisplayIcon={app}\Forge3D Scenery.exe
UninstallDisplayName=Forge3D Scenery {#AppFullVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "test-sheets\*,*.pdb"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Forge3D Scenery"; Filename: "{app}\Forge3D Scenery.exe"
Name: "{autodesktop}\Forge3D Scenery"; Filename: "{app}\Forge3D Scenery.exe"; Tasks: desktopicon

[Registry]
; .f3d files open in Forge3D (if it's installed); Scenery is offered under "Open with".
Root: HKA; Subkey: "Software\Classes\Applications\Forge3D Scenery.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Forge3D Scenery.exe"" ""%1"""; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\Forge3D Scenery.exe\SupportedTypes"; ValueType: string; ValueName: ".f3d"; ValueData: ""; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\.f3d\OpenWithList\Forge3D Scenery.exe"; ValueType: string; ValueName: ""; ValueData: ""; Flags: uninsdeletekey

[Run]
Filename: "{app}\Forge3D Scenery.exe"; Description: "{cm:LaunchProgram,Forge3D Scenery}"; Flags: nowait postinstall skipifsilent
