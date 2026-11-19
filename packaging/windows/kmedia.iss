; Native Windows installer definition for KMedia (Inno Setup 6).
; Default parameters allow standalone compilation with ISCC.exe after staging dist/windows.

#ifndef AppVersion
  #define AppVersion "0.1.1"
#endif

#ifndef NumericVersion
  #define NumericVersion "0.1.1.0"
#endif

#ifndef SourceDir
  #define SourceDir "..\.."
#endif

#ifndef DistDir
  #define DistDir "..\..\dist\windows"
#endif

#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif

#ifndef OutputBaseFilename
  #define OutputBaseFilename "kmedia-windows-x64-setup"
#endif

[Setup]
; Double opening brace escapes the Inno Setup constant prefix while keeping the registry GUID valid.
AppId={{8E3C6B74-4F9A-4D2B-9A61-7C3F8E2D1A50}
AppName=KMedia
AppVerName=KMedia {#AppVersion}
AppVersion={#AppVersion}
AppPublisher=KMedia Contributors
AppPublisherURL=https://github.com/matheuscardosos/kmedia
AppSupportURL=https://github.com/matheuscardosos/kmedia/issues
AppUpdatesURL=https://github.com/matheuscardosos/kmedia/releases
DefaultDirName={autopf}\KMedia
DefaultGroupName=KMedia
AllowNoIcons=yes
LicenseFile={#SourceDir}\LICENSE
SetupIconFile={#SourceDir}\resources\icons\icon.ico
UninstallDisplayIcon={app}\kmedia.exe
UninstallDisplayName=KMedia
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#NumericVersion}
VersionInfoProductName=KMedia
VersionInfoDescription=KMedia Installer

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\KMedia"; Filename: "{app}\kmedia.exe"
Name: "{autodesktop}\KMedia"; Filename: "{app}\kmedia.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\kmedia.exe"; Description: "{cm:LaunchProgram,KMedia}"; Flags: nowait postinstall skipifsilent
