# ============================================================================
# capture_all.ps1 — orquestra a captura de TODAS as telas do README.
#
# Fluxo: mata SoundInt -> sobe um loop de audio real (sessao "Tocando agora")
# -> para cada tela: abre SoundInt --capture <tela> com SOUNDINT_CAPTURE_SCALE
# -> fotografa via tools/capture_screens.ps1 -> mata a instancia.
#
# Uso:  .\tools\capture_all.ps1 [-Exe out\ux-int\Debug\SoundInt.exe]
# Saida: docs/images/raw/*.png (720px+ nativos, escala 2.0)
# ============================================================================
param(
    [string]$Exe = "out\ux-int\Debug\SoundInt.exe",
    [string]$OutDir = "docs\images\raw",
    [string]$Scale = "2.0"
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Exe)) { throw "Exe nao encontrado: $Exe (build antes)." }

# Escala de captura: renderiza como num monitor 200% (prints nitidos).
$env:SOUNDINT_CAPTURE_SCALE = $Scale

# 1) Limpeza de execucoes anteriores ------------------------------------------
Get-Process SoundInt -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

# 2) Sessao de audio real: loop de wav em um processo PowerShell (aparece no
#    mixer como sessao ativa; as regras reais do usuario completam a tela).
$audioArgs = @('-NoProfile', '-WindowStyle', 'Hidden', '-Command',
    "Add-Type -AssemblyName System.Media;" +
    "`$p = New-Object System.Media.SoundPlayer 'C:\Windows\Media\Alarm10.wav';" +
    "`$p.PlayLooping(); Start-Sleep -Seconds 300")
$audio = Start-Process powershell.exe -PassThru -ArgumentList $audioArgs
Write-Host "audio de demo: PID $($audio.Id)"

function Invoke-Capture([string]$target, [string]$match, [string]$name, [int]$waitSec = 3)
{
    Write-Host "==> $name ($target)"
    $proc = Start-Process $Exe -PassThru -ArgumentList '--capture', $target
    Start-Sleep -Seconds $waitSec
    & "$PSScriptRoot\capture_screens.ps1" -TitleMatch $match -OutName $name -OutDir $OutDir
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 900   # mutex de instancia unica
}

# 3) Telas de Configuracoes (uma por pagina) -----------------------------------
Invoke-Capture 'settings:general' 'SoundIntSettingsWindow' 'settings-geral'
Invoke-Capture 'settings:rules'    'SoundIntSettingsWindow' 'settings-regras'
Invoke-Capture 'settings:profiles'  'SoundIntSettingsWindow' 'settings-perfis'
Invoke-Capture 'settings:hotkeys'  'SoundIntSettingsWindow' 'settings-atalhos'
Invoke-Capture 'settings:updates'  'SoundIntSettingsWindow' 'settings-atualizacoes'
Invoke-Capture 'settings:about'    'SoundIntSettingsWindow' 'settings-sobre'

# 4) Mixer flyout (audio tocando + regras reais) --------------------------------
Invoke-Capture 'mixer' 'SoundIntMixerFlyout' 'mixer-apps-abertos' 5

# 5) Modais (dados de demonstracao; saidas reais no dropdown) -------------------
Invoke-Capture 'modal:newapp'    'SoundInt.ModalApp'    'modal-novo-app' 2
Invoke-Capture 'modal:newdevice' 'SoundInt.ModalDevice' 'modal-novo-dispositivo' 2

# 6) Menu da bandeja + contexto da barra de tarefas (regiao de tela) ------------
Write-Host '==> tray-menu (regiao com menu aberto)'
$proc = Start-Process $Exe -PassThru -ArgumentList '--capture', 'traymenu'
Start-Sleep -Seconds 3
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
$work = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
# Menu ancora: (work.right-260, work.bottom-8); regiao cobre menu + taskbar.
$rx = $work.Right - 320
$ry = $work.Bottom - 220
$bmp = New-Object System.Drawing.Bitmap 320, 220
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($rx, $ry, 0, 0, $bmp.Size)
$g.Dispose()
$bmp.Save((Join-Path $OutDir 'tray-menu.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "OK: $OutDir\tray-menu.png"
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

# 7) Fim ------------------------------------------------------------------------
Stop-Process -Id $audio.Id -Force -ErrorAction SilentlyContinue
$env:SOUNDINT_CAPTURE_SCALE = $null
Get-ChildItem $OutDir -Filter *.png | Sort-Object Name |
    ForEach-Object { '{0,-34} {1}x{2}' -f $_.Name,
        [System.Drawing.Image]::FromFile($_.FullName).Width,
        [System.Drawing.Image]::FromFile($_.FullName).Height }
Write-Host 'Captura concluida.'
