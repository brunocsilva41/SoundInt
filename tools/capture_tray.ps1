# ============================================================================
# capture_tray.ps1 — captura a bandeja (passo 1 do README "Como usar").
#
# O Win10 oculta icones novos no overflow (atrás da divisa ^): a captura mostra
# o flyout aberto com o SoundInt dentro + a taskbar com a divisa.
#
# Passos: purga icones fantasmas (hover) -> 1 instancia -> abre overflow ->
# rects via UIAutomation -> captura regiao -> fecha -> mata a instancia.
# Escreve tray-rects.json (origem da regiao + rects p/ anotacao com setas).
# ============================================================================
param(
    [string]$Exe = "out\ux-int\Debug\SoundInt.exe",
    [string]$OutDir = "docs\images\raw",
    [string]$Scale = "2.0"
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName Microsoft.VisualBasic

$desktop = [System.Windows.Automation.AutomationElement]::RootElement
$btnCond = New-Object System.Windows.Automation.PropertyCondition(
    [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
    [System.Windows.Automation.ControlType]::Button)

function Get-Overflow {
    $desktop.FindFirst([System.Windows.Automation.TreeScope]::Children,
        (New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::ClassNameProperty,
            'NotifyIconOverflowWindow')))
}
function Open-Overflow {
    $taskbar = $desktop.FindFirst([System.Windows.Automation.TreeScope]::Children,
        (New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::ClassNameProperty, 'Shell_TrayWnd')))
    if (-not $taskbar) { throw 'taskbar nao encontrada' }
    $chevron = $taskbar.FindAll([System.Windows.Automation.TreeScope]::Descendants, $btnCond) |
        Where-Object { $_.Current.Name -like 'Divisa*' } | Select-Object -First 1
    if (-not $chevron) { throw 'divisa da bandeja nao encontrada' }
    $chevron.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
    Start-Sleep -Milliseconds 900
    return $chevron
}
function Get-SoundIntIcons {
    $ov = Get-Overflow
    if (-not $ov) { return @() }
    return @($ov.FindAll([System.Windows.Automation.TreeScope]::Descendants, $btnCond) |
        Where-Object { $_.Current.Name -like 'SoundInt*' })
}

# 1) Purga: mata SoundInt, varre o overflow com o cursor (Explorer remove
#    icones de processos mortos ao pingar a janela dona).
Get-Process SoundInt -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
[void](Open-Overflow)
$icons = Get-SoundIntIcons
if ($icons.Count -gt 0) {
    Write-Host "purgando $($icons.Count) icone(s) fantasma..."
    $ovr = (Get-Overflow).Current.BoundingRectangle
    $x0 = [int]$ovr.X + 12; $y0 = [int]$ovr.Y + 12
    for ($y = $y0; $y -lt ($y0 + [int]$ovr.Height - 8); $y += 20) {
        for ($x = $x0; $x -lt ($x0 + [int]$ovr.Width - 8); $x += 20) {
            [System.Windows.Forms.Cursor]::Position = New-Object System.Drawing.Point($x, $y)
            Start-Sleep -Milliseconds 120
        }
    }
    Start-Sleep -Milliseconds 600
    $rest = Get-SoundIntIcons
    Write-Host "restantes apos varredura: $($rest.Count)"
    # Fantasma que sobreviveu: um clique em cima o remove.
    foreach ($ic in $rest) {
        $r = $ic.Current.BoundingRectangle
        [System.Windows.Forms.Cursor]::Position = New-Object System.Drawing.Point(
            [int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2))
        Start-Sleep -Milliseconds 150
        Add-Type -AssemblyName System.Windows.Forms
        [System.Windows.Forms.SendKeys]::SendWait('{ESC}')
        [void](Open-Overflow)
    }
}
# Fecha o overflow antes de seguir (ESC com foco nele).
[System.Windows.Forms.SendKeys]::SendWait('{ESC}')
Start-Sleep -Milliseconds 500

# 2) Uma unica instancia viva.
$env:SOUNDINT_CAPTURE_SCALE = $Scale
$proc = Start-Process $Exe -PassThru
Start-Sleep -Seconds 3

# 3) Abre o overflow e pega os rects exatos (SoundInt + divisa).
$chevron = Open-Overflow
$icons = Get-SoundIntIcons
if ($icons.Count -lt 1) { throw 'SoundInt nao apareceu no overflow' }
$icon = $icons[-1]                       # o ultimo e' a instancia viva
$iconRect = $icon.Current.BoundingRectangle
$chevRect = $chevron.Current.BoundingRectangle
$ovr = (Get-Overflow).Current.BoundingRectangle

# 4) Regiao: flyout inteiro + faixa da taskbar (divisa e' re-aberta abaixo).
$rx = [int]([Math]::Min($ovr.X, $chevRect.X)) - 24
$ry = [int]$ovr.Y - 24
$rw = [int]([Math]::Max($ovr.Right, $chevRect.Right + 40)) - $rx + 24
$rh = [int]$chevRect.Y + 40 - $ry + 24
$bmp = New-Object System.Drawing.Bitmap $rw, $rh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($rx, $ry, 0, 0, $bmp.Size)
$g.Dispose()
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }
$bmp.Save((Join-Path $OutDir 'tray-overflow.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

# 5) Rects relativos a regiao -> JSON p/ tools/annotate_screens.ps1.
$rects = [ordered]@{
    region  = @{ x = $rx; y = $ry; w = $rw; h = $rh }
    icon    = @{ x = [int]$iconRect.X - $rx; y = [int]$iconRect.Y - $ry
                 w = [int]$iconRect.Width; h = [int]$iconRect.Height }
    chevron = @{ x = [int]$chevRect.X - $rx; y = [int]$chevRect.Y - $ry
                 w = [int]$chevRect.Width; h = [int]$chevRect.Height }
}
$rects | ConvertTo-Json | Set-Content (Join-Path $OutDir 'tray-rects.json') -Encoding utf8
Write-Host "OK: $OutDir\tray-overflow.png (${rw}x${rh}); icon rel=($($rects.icon.x),$($rects.icon.y)) chev rel=($($rects.chevron.x),$($rects.chevron.y))"

# 6) Fecha overflow e finaliza.
[System.Windows.Forms.SendKeys]::SendWait('{ESC}')
Start-Sleep -Milliseconds 400
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
$env:SOUNDINT_CAPTURE_SCALE = $null
Write-Host 'Bandeja capturada.'
