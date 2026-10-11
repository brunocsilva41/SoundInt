# ============================================================================
# annotate_screens.ps1 — desenha setas, badges numerados e callouts sobre os
# prints do SoundInt (docs/images). Gera as versoes anotadas do passo a passo.
#
# Uso:
#   $ann = @(
#     @{ Type='Arrow';  From='618,236'; To='540,214' },
#     @{ Type='Badge';  XY='628,236';   Text='1' },
#     @{ Type='Callout';XY='640,240';   Text='Clique no chevron para trocar a saida'; Anchor='left' }
#   )
#   .\tools\annotate_screens.ps1 -Source docs/images/mixer-apps-abertos.png `
#       -Target docs/images/annotated/mixer-apps-abertos.png -Annotations $ann
#
# Coordenadas em pixels da IMAGEM (nao da tela). Anchor: 'left' (texto a
# direita do ponto) ou 'right' (texto a esquerda; seta aponta para ele).
# ============================================================================
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Target,
    [Parameter(Mandatory = $true)][array]$Annotations,
    [float]$Scale = 1.0    # multiplicador de espessuras/fontes (2.0 p/ capturas HiDPI)
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# --- Paleta (accent do tema DARK do app — os prints sao dark) ------------------
$Accent   = [System.Drawing.Color]::FromArgb(255, 76, 160, 255)
$White    = [System.Drawing.Color]::White
$Ink      = [System.Drawing.Color]::FromArgb(235, 26, 26, 26)     # chip de texto
$ChipFg   = [System.Drawing.Color]::FromArgb(255, 243, 243, 243)

# --- Helpers de geometria ----------------------------------------------------
function Get-Point([string]$s)
{
    $p = $s -split ','
    @{ X = [int]$p[0]; Y = [int]$p[1] }
}

function New-Pen([System.Drawing.Color]$c, [float]$w)
{
    $p = New-Object System.Drawing.Pen($c, $w)
    $p.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $p.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round
    $p.Alignment = [System.Drawing.Drawing2D.PenAlignment]::Center
    $p
}

function Draw-Arrow($g, $from, $to, [float]$stroke)
{
    # haste
    $pen = New-Pen $Accent $stroke
    $g.DrawLine($pen, $from.X, $from.Y, $to.X, $to.Y)
    $pen.Dispose()

    # ponta (triangulo preenchido orientado pela direcao)
    $dx = $to.X - $from.X
    $dy = $to.Y - $from.Y
    $len = [Math]::Sqrt($dx * $dx + $dy * $dy)
    if ($len -lt 1) { return }
    $ux = $dx / $len; $uy = $dy / $len
    $head = 3.0 * $stroke
    $bx = $to.X - $ux * $head
    $by = $to.Y - $uy * $head
    $px = -$uy; $py = $ux
    $pts = @(
        (New-Object System.Drawing.PointF([float]$to.X, [float]$to.Y)),
        (New-Object System.Drawing.PointF([float]($bx + $px * $head * 0.55), [float]($by + $py * $head * 0.55))),
        (New-Object System.Drawing.PointF([float]($bx - $px * $head * 0.55), [float]($by - $py * $head * 0.55)))
    )
    $brush = New-Object System.Drawing.SolidBrush($Accent)
    $g.FillPolygon($brush, $pts)
    $brush.Dispose()
}

function Draw-Badge($g, $xy, [string]$text, [float]$r)
{
    $brush = New-Object System.Drawing.SolidBrush($Accent)
    $outline = New-Object System.Drawing.Pen($White, [float](2.5 * $Scale))
    $g.FillEllipse($brush, ($xy.X - $r), ($xy.Y - $r), (2 * $r), (2 * $r))
    $g.DrawEllipse($outline, ($xy.X - $r), ($xy.Y - $r), (2 * $r), (2 * $r))
    $brush.Dispose(); $outline.Dispose()

    $font = New-Object System.Drawing.Font('Segoe UI', [float](14 * $Scale),
        [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment = [System.Drawing.StringAlignment]::Center
    $fmt.LineAlignment = [System.Drawing.StringAlignment]::Center
    $tb = New-Object System.Drawing.SolidBrush($White)
    $rect = New-Object System.Drawing.RectangleF(($xy.X - $r), ($xy.Y - $r), (2 * $r), (2 * $r))
    $g.DrawString($text, $font, $tb, $rect, $fmt)
    $font.Dispose(); $tb.Dispose(); $fmt.Dispose()
}

function Draw-Callout($g, $xy, [string]$text, [string]$anchor, [float]$stroke)
{
    $font = New-Object System.Drawing.Font('Segoe UI', [float](13 * $Scale),
        [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
    $size = $g.MeasureString($text, $font)
    $pad = 6 * $Scale
    $chipH = $size.Height + 2 * $pad
    $chipW = $size.Width + 2 * $pad

    if ($anchor -eq 'right') {
        $chipX = $xy.X - $chipW - 12 * $Scale
    } else {
        $chipX = $xy.X + 12 * $Scale
    }
    $chipY = $xy.Y - $chipH / 2

    # chip arredondado
    $chip = New-Object System.Drawing.SolidBrush($Ink)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $rad = 6 * $Scale
    $path.AddArc($chipX, $chipY, $rad, $rad, 180, 90)
    $path.AddArc(($chipX + $chipW - $rad), $chipY, $rad, $rad, 270, 90)
    $path.AddArc(($chipX + $chipW - $rad), ($chipY + $chipH - $rad), $rad, $rad, 0, 90)
    $path.AddArc($chipX, ($chipY + $chipH - $rad), $rad, $rad, 90, 90)
    $path.CloseFigure()
    $g.FillPath($chip, $path)
    $chip.Dispose(); $path.Dispose()

    # tracinho ligando o chip ao ponto
    $pen = New-Pen $Accent ($stroke * 0.75)
    if ($anchor -eq 'right') {
        $g.DrawLine($pen, ($chipX + $chipW), ($chipY + $chipH / 2), $xy.X, $xy.Y)
    } else {
        $g.DrawLine($pen, $chipX, ($chipY + $chipH / 2), $xy.X, $xy.Y)
    }
    $pen.Dispose()

    $tb = New-Object System.Drawing.SolidBrush($ChipFg)
    $rect = New-Object System.Drawing.RectangleF(($chipX + $pad), ($chipY + $pad), $size.Width, $size.Height)
    $g.DrawString($text, $font, $tb, $rect)
    $font.Dispose(); $tb.Dispose()
}

# --- Execucao ----------------------------------------------------------------
$inPath = Resolve-Path $Source
$src = [System.Drawing.Image]::FromFile($inPath)
$outDir = Split-Path $Target -Parent
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

$bmp = New-Object System.Drawing.Bitmap($src.Width, $src.Height)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::ClearTypeGridFit
$g.DrawImage($src, 0, 0, $src.Width, $src.Height)

$stroke = [Math]::Max(2.0, 2.5 * $Scale)
$badgeR = [Math]::Max(11.0, 13 * $Scale)

foreach ($a in $Annotations) {
    switch ($a.Type) {
        'Arrow'   { Draw-Arrow   $g (Get-Point $a.From) (Get-Point $a.To) $stroke }
        'Badge'   { Draw-Badge   $g (Get-Point $a.XY) ([string]$a.Text) $badgeR }
        'Callout' { Draw-Callout $g (Get-Point $a.XY) $a.Text ([string]$a.Anchor) $stroke }
        default   { Write-Warning "Tipo desconhecido: $($a.Type)" }
    }
}

$bmp.Save($Target, [System.Drawing.Imaging.ImageFormat]::Png)
$w = $src.Width; $h = $src.Height
$g.Dispose(); $bmp.Dispose(); $src.Dispose()
Write-Host "OK: $Target (${w}x${h})"
