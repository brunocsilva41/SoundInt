# ============================================================================
# capture_screens.ps1 — captura janelas do SoundInt em PNG nativo (HiDPI).
#
# Usa PrintWindow(PW_RENDERFULLCONTENT) — captura conteudo Direct2D mesmo
# se a janela estiver parcialmente oculta — com fallback BitBlt de tela.
#
# Uso:
#   .\tools\capture_screens.ps1 -TitleMatch "Configura" -OutName settings-geral
#   .\tools\capture_screens.ps1 -List   # lista janelas visiveis do SoundInt
# ============================================================================
param(
    [string]$TitleMatch = '',
    [string]$OutName = 'capture',
    [string]$OutDir = 'docs\images\raw',
    [switch]$List
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class WinCap
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lParam);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, StringBuilder sb, int max);
    [DllImport("user32.dll")] public static extern int GetClassName(IntPtr hWnd, StringBuilder sb, int max);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();

    public static List<IntPtr> Find(uint targetPid, string titleContains)
    {
        var result = new List<IntPtr>();
        EnumWindows((h, l) =>
        {
            uint pid; GetWindowThreadProcessId(h, out pid);
            if (pid == targetPid && IsWindowVisible(h))
            {
                var sb = new StringBuilder(256);
                GetWindowText(h, sb, 256);
                var cls = new StringBuilder(256);
                GetClassName(h, cls, 256);
                var t = sb.ToString();
                if (titleContains.Length == 0 ||
                    (t.IndexOf(titleContains, StringComparison.OrdinalIgnoreCase) >= 0) ||
                    cls.ToString().IndexOf(titleContains, StringComparison.OrdinalIgnoreCase) >= 0)
                {
                    if (t.Length > 0 || cls.ToString() == "#32768")
                        result.Add(h);
                }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
"@

[WinCap]::SetProcessDPIAware() | Out-Null

# --- Localiza o processo SoundInt -------------------------------------------
$proc = Get-Process SoundInt -ErrorAction SilentlyContinue |
    Where-Object { $_.MainWindowHandle -ne 0 -or $true } | Select-Object -First 1
if (-not $proc) { Write-Error "SoundInt nao esta rodando. Abra o app (ou use --capture) antes."; }

$wins = [WinCap]::Find($proc.Id, $TitleMatch)
if ($List) {
    foreach ($w in $wins) {
        $sb = New-Object System.Text.StringBuilder 256
        [WinCap]::GetWindowText($w, $sb, 256) | Out-Null
        $cls = New-Object System.Text.StringBuilder 256
        [WinCap]::GetClassName($w, $cls, 256) | Out-Null
        "{0}  titulo='{1}'  classe='{2}'" -f $w, $sb, $cls
    }
    return
}
if ($wins.Count -eq 0) { Write-Error "Nenhuma janela matched '$TitleMatch'."; }

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

$i = 0
foreach ($w in $wins) {
    $r = New-Object WinCap+RECT
    [WinCap]::GetWindowRect($w, $r) | Out-Null
    $wpx = $r.Right - $r.Left
    $hpx = $r.Bottom - $r.Top
    if ($wpx -le 0 -or $hpx -le 0) { continue }

    $bmp = New-Object System.Drawing.Bitmap $wpx, $hpx
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    $ok = [WinCap]::PrintWindow($w, $hdc, 2)   # PW_RENDERFULLCONTENT (D2D/DWM)
    $g.ReleaseHdc($hdc); $g.Dispose()

    if (-not $ok) {
        # Fallback: BitBlt da regiao de tela.
        $bg = [System.Drawing.Graphics]::FromImage($bmp)
        $bg.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
        $bg.Dispose()
    }

    $name = if ($wins.Count -gt 1) { "{0}-{1}" -f $OutName, $i } else { $OutName }
    $path = Join-Path $OutDir "$name.png"
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "OK: $path (${wpx}x${hpx}) (PrintWindow=$ok)"
    $i++
}
