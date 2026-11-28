#ifndef AppVersion
  #error AppVersion must be supplied by the build command.
#endif

#ifndef SourceDir
  #error SourceDir must point to the staged NOTE application directory.
#endif

#ifndef IconFile
  #error IconFile must point to packaging/note.ico.
#endif

#ifndef OutputDir
  #error OutputDir must point to the installer output directory.
#endif

#define AppName "NOTE"

[Setup]
AppId={{8A9F2E71-6C43-4D18-A5B2-7F0E91C6D340}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=NOTE
DefaultDirName={autopf}\NOTE
DefaultGroupName=NOTE
DisableProgramGroupPage=yes

; Allow the user to choose current-user or all-users installation.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

UninstallDisplayName=NOTE
UninstallDisplayIcon={app}\note.exe

SetupIconFile={#IconFile}
WizardStyle=modern


CloseApplications=yes
RestartApplications=no

OutputDir={#OutputDir}
OutputBaseFilename=Note-{#AppVersion}-windows-x64-Setup

Compression=lzma2
SolidCompression=yes

; Keep the installer independent of the source build directory.
DisableDirPage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\NOTE"; Filename: "{app}\note.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\NOTE"; Filename: "{app}\note.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\note.exe"; Description: "Launch NOTE"; Flags: postinstall nowait skipifsilent
