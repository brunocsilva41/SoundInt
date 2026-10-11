# ============================================================================
# probe_layout.ps1 — detecta por cor os elementos de cada captura (botoes
# accent, toggles, faixas de secao do mixer, campos, chevrons, gear) e escreve
# docs/images/raw/probe.json com os rects para o passo de anotacao.
#
# Usa LockBits (buffer direto) — GetPixel por pixel e' ordens de magnitude
# mais lento. Cores do tema dark: accent=(76,160,255), danger=(255,99,71),
# surfaceAlt=(56,56,56), surface=(44,44,44), border=(74,74,74).
# ============================================================================
param(
    [string]$Dir = "docs\images\raw"
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

# --- Buffer de pixels via LockBits -------------------------------------------
function Get-PixelBuffer($path) {
    $img = [System.Drawing.Image]::FromFile($path)
    $bmp = New-Object System.Drawing.Bitmap $img
    $rect = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
    $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                          [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $size = [Math]::Abs($data.Stride) * $data.Height
    $bytes = New-Object byte[] $size
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $size)
    $buf = @{
        w = $bmp.Width; h = $bmp.Height; stride = [Math]::Abs($data.Stride)
        b = $bytes
        # BGRA: pixel(x,y) -> b[idx+0]=B, [idx+1]=G, [idx+2]=R
        at = { param($x, $y)
            $i = $y * $this.stride + $x * 4
            , @($this.b[$i + 2], $this.b[$i + 1], $this.b[$i])
        }
    }
    $bmp.UnlockBits($data)
    $bmp.Dispose()
    $img.Dispose()
    return $buf
}

function Test-Px($buf, [int]$x, [int]$y, [int[]]$rgb, [int]$tol) {
    $i = $y * $buf.stride + $x * 4
    $b = $buf.b
    [Math]::Abs($b[$i + 2] - $rgb[0]) -le $tol -and
    [Math]::Abs($b[$i + 1] - $rgb[1]) -le $tol -and
    [Math]::Abs($b[$i] - $rgb[2]) -le $tol
}

# Blocos solidos (botao/toggle/campo): runs horizontais >= minW agrupados.
function Find-ColorBlocks($buf, [int[]]$rgb, [int]$tol, [int]$minW, [int]$minH,
                          [int]$step = 2, [int]$x0 = 0, [int]$x1 = -1, [int]$y0 = 0, [int]$y1 = -1) {
    if ($x1 -lt 0) { $x1 = $buf.w }
    if ($y1 -lt 0) { $y1 = $buf.h }
    $runs = @()   # runs horizontais da cor, >= minW
    for ($y = $y0; $y -lt $y1; $y += $step) {
        $runStart = -1
        for ($x = $x0; $x -lt $x1; $x += $step) {
            $match = Test-Px $buf $x $y $rgb $tol
            if ($match -and $runStart -lt 0) { $runStart = $x }
            if (-not $match) {
                $width = $x - $runStart
                if ($runStart -ge 0 -and $width -ge $minW) {
                    $runs += [pscustomobject]@{ y = $y; x0 = $runStart; x1 = $x - $step }
                }
                $runStart = -1
            }
        }
        if ($runStart -ge 0) {
            $width = $x1 - $runStart
            if ($width -ge $minW) {
                $runs += [pscustomobject]@{ y = $y; x0 = $runStart; x1 = $x1 - $step }
            }
        }
    }
    # Agrupa runs em blocos: mesma coluna de inicio (+-24) em linhas vizinhas.
    $blocks = @()
    foreach ($run in ($runs | Sort-Object y)) {
        $placed = $false
        foreach ($b in $blocks) {
            if ($run.y -le ($b.y + $b.h + $step) -and [Math]::Abs($run.x0 - $b.x) -le 24) {
                if ($run.x0 -lt $b.x) { $b.x = $run.x0 }
                if ($run.x1 -gt ($b.x + $b.w)) { $b.w = $run.x1 - $b.x }
                $b.h = $run.y - $b.y + $step
                $placed = $true
                break
            }
        }
        if (-not $placed) {
            $blocks += [pscustomobject]@{ x = $run.x0; y = $run.y; w = $run.x1 - $run.x0 + 1; h = $step }
        }
    }
    return @($blocks | Where-Object { $_.h -ge $minH })
}

# Faixas full-width (secoes do mixer).
function Find-FullBands($buf, [int[]]$rgb, [int]$tol, [int]$minPx, [int]$x0, [int]$x1) {
    $ys = @()
    for ($y = 0; $y -lt $buf.h; $y += 2) {
        $count = 0
        for ($x = $x0; $x -lt $x1; $x += 4) {
            if (Test-Px $buf $x $y $rgb $tol) { $count++ }
        }
        if ($count * 4 -ge $minPx) { $ys += $y }
    }
    $bands = @()
    foreach ($y in $ys) {
        $merged = $false
        foreach ($b in $bands) {
            if ($b.y + $b.h -ge $y - 4) { $b.h = $y - $b.y + 2; $merged = $true; break }
        }
        if (-not $merged) { $bands += [pscustomobject]@{ y = $y; h = 2 } }
    }
    return $bands
}

# Clusters de pixels de glifo (cinza claro) numa faixa de x.
function Find-GlyphClusters($buf, [int]$x0, [int]$x1, [int]$y0, [int]$y1,
                            [int]$brightMin = 140) {
    $b = $buf.b
    $ys = @()
    for ($y = $y0; $y -lt $y1; $y++) {
        $hit = $false
        for ($x = $x0; $x -lt $x1 -and -not $hit; $x++) {
            $i = $y * $buf.stride + $x * 4
            if ($b[$i + 2] -ge $brightMin -and [Math]::Abs($b[$i + 2] - $b[$i + 1]) -le 14 -and
                [Math]::Abs($b[$i + 1] - $b[$i]) -le 14) { $hit = $true }
        }
        if ($hit) { $ys += $y }
    }
    $clusters = @()
    foreach ($y in $ys) {
        $merged = $false
        foreach ($c in $clusters) {
            if ($c.y + $c.h -ge $y - 3) { $c.h = $y - $c.y + 1; $merged = $true; break }
        }
        if (-not $merged) { $clusters += [pscustomobject]@{ y = $y; h = 1 } }
    }
    return $clusters
}

$accent = @(76, 160, 255); $danger = @(255, 99, 71)
$surfaceAlt = @(56, 56, 56); $surface = @(44, 44, 44)

$result = [ordered]@{}

# --- M I X E R ---------------------------------------------------------------
$buf = Get-PixelBuffer "$Dir\mixer-apps-abertos.png"
$result.mixer = [ordered]@{
    bands    = @(Find-FullBands $buf $surfaceAlt 6 560 24 696)
    accent   = @(Find-ColorBlocks $buf $accent 14 6 6 2)
    chevrons = @(Find-GlyphClusters $buf 636 660 100 ($buf.h - 8) 150)
    gear     = @(Find-GlyphClusters $buf 624 704 16 100 100)
}

# --- S E T T I N G S -----------------------------------------------------------
foreach ($name in @('settings-regras', 'settings-atualizacoes', 'settings-geral', 'settings-perfis')) {
    $buf = Get-PixelBuffer "$Dir\$name.png"
    $result[$name] = [ordered]@{
        accent = @(Find-ColorBlocks $buf $accent 14 60 24 2)
        danger = @(Find-ColorBlocks $buf $danger 16 16 16 2)
        fields = @(Find-ColorBlocks $buf $surface 6 120 40 2)
    }
}

# --- M O D A I S ---------------------------------------------------------------
foreach ($name in @('modal-novo-app', 'modal-novo-dispositivo')) {
    $buf = Get-PixelBuffer "$Dir\$name.png"
    $result[$name] = [ordered]@{
        accent = @(Find-ColorBlocks $buf $accent 14 60 24 2)
        fields = @(Find-ColorBlocks $buf $surface 6 200 40 2)
    }
}

# --- T R A Y --------------------------------------------------------------------
$result.tray = (Get-Content "$Dir\tray-rects.json" -Raw | ConvertFrom-Json)

$json = $result | ConvertTo-Json -Depth 5
$json | Set-Content "$Dir\probe.json" -Encoding utf8
Write-Host "OK: $Dir\probe.json"

# Resumo p/ conferencia
$m = $result.mixer
"mixer bands:    " + (($m.bands | ForEach-Object { "y=$($_.y) h=$($_.h)" }) -join ' | ')
"mixer accent:  " + (($m.accent | ForEach-Object { "($($_.x),$($_.y) $($_.w)x$($_.h))" }) -join ' | ')
"mixer chevrons: " + (($m.chevrons | ForEach-Object { "y=$($_.y) h=$($_.h)" }) -join ' | ')
"mixer gear:     " + (($m.gear | ForEach-Object { "y=$($_.y) h=$($_.h)" }) -join ' | ')
foreach ($name in @('settings-regras', 'settings-atualizacoes', 'settings-geral', 'settings-perfis', 'modal-novo-app', 'modal-novo-dispositivo')) {
    $r = $result[$name]
    "$name accent: " + (($r.accent | ForEach-Object { "($($_.x),$($_.y) $($_.w)x$($_.h))" }) -join ' | ')
    if ($r.danger) { "$name danger: " + (($r.danger | ForEach-Object { "($($_.x),$($_.y) $($_.w)x$($_.h))" }) -join ' | ') }
    "$name fields: " + (($r.fields | ForEach-Object { "($($_.x),$($_.y) $($_.w)x$($_.h))" }) -join ' | ')
}
