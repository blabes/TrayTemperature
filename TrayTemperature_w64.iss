; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "TrayTemperature"
#define MyAppVersion GetFileVersion('C:\Users\blabe\Documents\build-TrayTemperature-Desktop_Qt_5_12_5_MinGW_64_bit-Release\release\TrayTemperature.exe')
#define MyAppPublisher "Doug Bloebaum"
#define MyAppURL "https://github.com/blabes/TrayTemperature"
#define MyAppExeName "TrayTemperature.exe"

[Setup]
; NOTE: The value of AppId uniquely identifies this application. Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{1B040282-B83E-492C-89C7-4C383D2C3D56}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
; Remove the following line to run in administrative install mode (install for all users.)
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputBaseFilename=TrayTemperature-{#MyAppVersion}-install-64
SetupIconFile=C:\Users\blabe\Downloads\c-weather-cloudy-with-sun.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern
InfoAfterFile=post-install-notes.rtf

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Note that the Inno wizard doesn't handle grabbing whole directories very well.
; These directories: platforms, bearer, styles, iconengines, imageformats, and translations
; need to exist in the installation directory; their contents can't just be flattened
; out as files - blabes
Source: "C:\Users\blabe\Documents\build-TrayTemperature-Desktop_Qt_5_12_5_MinGW_64_bit-Release\release\TrayTemperature.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\blabe\Documents\build-TrayTemperature-Desktop_Qt_5_12_5_MinGW_64_bit-Release\release\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\blabe\Documents\build-TrayTemperature-Desktop_Qt_5_12_5_MinGW_64_bit-Release\release\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "C:\Users\blabe\Documents\build-TrayTemperature-Desktop_Qt_5_12_5_MinGW_64_bit-Release\release\bearer\*"; DestDir: "{app}\bearer"; Flags: ignoreversion
Source: "C:\Users\blabe\Documents\build-TrayTemperature-Desktop_Qt_5_12_5_MinGW_64_bit-Release\release\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion
Source: "C:\Users\blabe\Documents\build-TrayTemperature-Desktop_Qt_5_12_5_MinGW_64_bit-Release\release\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion
Source: "C:\Users\blabe\Documents\build-TrayTemperature-Desktop_Qt_5_12_5_MinGW_64_bit-Release\release\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "C:\Users\blabe\Documents\build-TrayTemperature-Desktop_Qt_5_12_5_MinGW_64_bit-Release\release\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Registry]
; Clean up this program's registy entries during uninstall - blabes
Root: HKA; Subkey: "Software\{#MyAppPublisher}\{#MyAppName}"; Flags: uninsdeletekey

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
