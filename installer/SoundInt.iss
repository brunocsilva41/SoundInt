; ============================================================================
; SoundInt — instalador per-user (Inno Setup 6.3+).
;
; Compilacao local:
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\SoundInt.iss
; Compilacao na CI (release.yml, etapa 3/6):
;   ISCC.exe /DAppVersion=0.1.0 /DSoundIntExe=<abs>\build\Release\SoundInt.exe ^
;            installer\SoundInt.iss
;
; - PrivilegesRequired=lowest: instala por usuario, sem UAC.
; - AppMutex/CloseApplications: fecha o app antes de atualizar/desinstalar.
; - Dados do app (%LOCALAPPDATA%\SoundInt) sao preservados na desinstalacao.
; ============================================================================

#define AppName "SoundInt"
#define AppPublisher "Bruno Silva"
#define AppExeName "SoundInt.exe"

; Versao vinda da linha `project(SoundInt VERSION x.y.z)` do CMakeLists.txt.
#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif

; Caminho do executavel buildado (relativo ao diretorio deste script).
#ifndef SoundIntExe
  #define SoundIntExe "..\build\Release\SoundInt.exe"
#endif

[Setup]
; GUID fixo — NUNCA altere (o Windows usa o AppId para rastrear a instalacao).
AppId={{F99A61FE-B4D4-4C3B-BFE1-105536BD6F73}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; Instalacao por usuario (sem elevation).
PrivilegesRequired=lowest
; x64 e ARM64 (requer Inno Setup 6.3+).
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Windows 10 1903 (build 18362) ou superior, 64 bits.
MinVersion=10.0.18362
; UI.
WizardStyle=modern
WizardImageFile=images\wizard_large.png
WizardSmallImageFile=images\wizard_small.png
SetupIconFile=images\soundint.ico
AllowNoIcons=yes
; Atualizacao/desinstalacao seguras.
AppMutex=SoundInt.SingleInstance
CloseApplications=yes
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}
; Saida.
OutputDir=Output
OutputBaseFilename=SoundInt-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
SetupLogging=yes

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
; ATENCAO: a ordem define os indices em [Code] (autostart = 0).
Name: "autostart"; Description: "Iniciar com o Windows"; Flags: unchecked
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SoundIntExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"; \
    Comment: "Roteador de saida de audio por aplicativo"; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; \
    Tasks: desktopicon; WorkingDir: "{app}"

[Registry]
; "Iniciar com Windows" via HKCU Run (removido na desinstalacao).
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
    ValueName: "{#AppName}"; ValueData: """{app}\{#AppExeName}"""; \
    Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; \
    Flags: nowait postinstall skipifsilent

; Dados em %LOCALAPPDATA%\SoundInt (configuracoes e logs) sao propositalmente
; preservados na desinstalacao; {app} e removido se ficar vazio.

[Code]
// Devolve True se o app ja esta registrado para iniciar com o Windows.
function AutostartEnabled: Boolean;
var
  Value: String;
begin
  Result := RegQueryStringValue(HKCU,
    'Software\Microsoft\Windows\CurrentVersion\Run', '{#AppName}', Value)
    and (Value <> '');
end;

// Pre-marca o checkbox "Iniciar com o Windows" conforme o estado real do
// registro quando a pagina de tarefas e exibida (indice 0 = autostart).
procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpSelectTasks then
    WizardForm.TasksList.Checked[0] := AutostartEnabled;
end;
