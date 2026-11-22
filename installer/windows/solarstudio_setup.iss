; SolarStudio DAW Windows x64 Installer (Inno Setup)
; Uses Assets\SolarStudio.ico for Setup Icon, Uninstall Icon, and App Shortcuts

#define MyAppName "SolarStudio"
#define MyAppVersion "2.1.0"
#define MyAppPublisher "SolarAudio"
#define MyAppURL "https://github.com/solarUpdateCode/SolarStudio"
#define MyAppExeName "SolarStudio.exe"

[Setup]
AppId={{8E2D4C9A-7B12-4F5A-9D08-5A1B7F3C2100}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\SolarAudio\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=SolarStudio-Setup-{#MyAppVersion}-Win64
SetupIconFile=..\..\Assets\SolarStudio.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardImageFile=..\..\Assets\installer_sidebar.bmp
WizardSmallImageFile=..\..\Assets\installer_small.bmp
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; Standalone SolarStudio DAW Application
Source: "..\..\build\SolarStudio_artefacts\Release\SolarStudio.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\Assets\SolarStudio.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\Backend\solar_ai_server.py"; DestDir: "{app}\Backend"; Flags: ignoreversion
; Companion SolarStudio Bridge VST3 Plugin
Source: "..\..\build\SolarStudioBridge_artefacts\Release\VST3\SolarStudio Bridge.vst3\*"; DestDir: "{commoncf}\VST3\SolarStudio Bridge.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\SolarStudio.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\SolarStudio.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
