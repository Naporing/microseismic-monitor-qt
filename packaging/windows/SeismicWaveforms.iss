#ifndef AppVersion
  #error AppVersion must be supplied by package-windows.ps1
#endif
#ifndef SourceDir
  #error SourceDir must be supplied by package-windows.ps1
#endif
#ifndef OutputDir
  #error OutputDir must be supplied by package-windows.ps1
#endif

[Setup]
AppId={{DDA39D2A-DFCE-48A9-A769-B683EF01F46A}
AppName=Microseismic Monitor
AppVersion={#AppVersion}
AppPublisher=Naporing
AppPublisherURL=https://github.com/Naporing/microseismic-monitor-qt
AppUpdatesURL=https://github.com/Naporing/microseismic-monitor-qt/releases
DefaultDirName={localappdata}\Programs\SeismicWaveformsDemo
DefaultGroupName=Microseismic Monitor
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\bin\Seismic_Waveforms_demo.exe
OutputDir={#OutputDir}
OutputBaseFilename=SeismicWaveforms-Setup-v{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
VersionInfoVersion={#AppVersion}

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Microseismic Monitor"; Filename: "{app}\bin\Seismic_Waveforms_demo.exe"; WorkingDir: "{app}\bin"

[Run]
Filename: "{app}\bin\Seismic_Waveforms_demo.exe"; Description: "Launch Microseismic Monitor"; WorkingDir: "{app}\bin"; Flags: nowait postinstall skipifsilent
Filename: "{app}\bin\Seismic_Waveforms_demo.exe"; WorkingDir: "{app}\bin"; Flags: nowait; Check: IsUpdate

[Code]
function IsUpdate: Boolean;
begin
  Result := WizardSilent and (ExpandConstant('{param:UPDATE|0}') = '1');
end;
