# Sanity do toggle do mixer (validacao pos-integracao AG7/AG8).
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class San {
    public delegate bool EP(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EP cb, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int max);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    public static IntPtr ByClass(uint pid, string cls) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((h, l) => {
            uint q; GetWindowThreadProcessId(h, out q);
            if (q == pid) {
                var c = new StringBuilder(256); GetClassName(h, c, 256);
                if (c.ToString() == cls) { found = h; }
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
"@

Get-Process SoundInt -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
$p = Start-Process "$PSScriptRoot\..\out\ux-int\Debug\SoundInt.exe" -PassThru
Start-Sleep -Seconds 4
$main = [San]::ByClass([uint32]$p.Id, 'SoundIntMainWindow')
"main hwnd: $main"
if ($main -eq [IntPtr]::Zero) { throw 'janela principal nao encontrada' }

$WM_APP_SHOW_REQUEST = 0x8000 + 2   # WM_APP + 2 (messages.h)
$KSHOW_MIXER = [IntPtr]3            # kShowRequestMixer

function MixerVisible { $m = [San]::ByClass([uint32]$p.Id, 'SoundIntMixerFlyout'); ($m -ne [IntPtr]::Zero) -and [San]::IsWindowVisible($m) }

# 1) Abre
[San]::PostMessage($main, $WM_APP_SHOW_REQUEST, $KSHOW_MIXER, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 900
$abriu = MixerVisible
"T1 abre pelo clique da tray: $(if($abriu){'PASS'}else{'FAIL'})"

# 2) Fecha (toggle)
[San]::PostMessage($main, $WM_APP_SHOW_REQUEST, $KSHOW_MIXER, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 900
$fechou = -not (MixerVisible)
"T2 toggle fecha: $(if($fechou){'PASS'}else{'FAIL'})"

# 3) Reabre apos intervalo (> debounce de 250ms)
Start-Sleep -Milliseconds 600
[San]::PostMessage($main, $WM_APP_SHOW_REQUEST, $KSHOW_MIXER, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 900
$reabriu = MixerVisible
"T3 reabre apos espera: $(if($reabriu){'PASS'}else{'FAIL'})"

# 4) Fecha de novo e sai limpo
[San]::PostMessage($main, $WM_APP_SHOW_REQUEST, $KSHOW_MIXER, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 900
[San]::PostMessage($main, 0x0010, [IntPtr]1, [IntPtr]::Zero) | Out-Null   # WM_CLOSE wParam=1 (Sair)
Start-Sleep -Seconds 3
$saiu = $p.HasExited
"T4 Sair encerra: $(if($saiu){'PASS'}else{'FAIL'})"
$tail = Get-Content "$env:LOCALAPPDATA\SoundInt\logs\SoundInt.log" -Tail 3
"T4 log: $($tail -match 'encerrando' ? 'PASS' : 'FAIL')"
$tail | Select-Object -Last 2
