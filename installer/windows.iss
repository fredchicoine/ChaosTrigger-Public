; Chaos Trigger Windows installer (Inno Setup 6)
#define AppVer GetEnv("CT_VERSION")
#if AppVer == ""
  #define AppVer "0.1.0"
#endif

[Setup]
AppId={{6C1F3B0E-6A66-4E11-9C66-1C4A05C0FFEE}
AppName=Chaos Trigger
AppVersion={#AppVer}
AppPublisher=iNi
DefaultDirName={commonpf64}\iNi\Chaos Trigger
DefaultGroupName=iNi
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\installer-out
OutputBaseFilename=ChaosTrigger-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Chaos Trigger

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone application"; Types: full

[Files]
Source: "..\build\ChaosTrigger_artefacts\Release\VST3\Chaos Trigger.vst3\*"; DestDir: "{commoncf64}\VST3\Chaos Trigger.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\ChaosTrigger_artefacts\Release\Standalone\Chaos Trigger.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Chaos Trigger"; Filename: "{app}\Chaos Trigger.exe"; Components: standalone
Name: "{group}\Uninstall Chaos Trigger"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Chaos Trigger.vst3"
