# ============================================================================
# annotate_all.ps1 — gera as imagens do README: copia a galeria e produz as
# versoes ANOTADAS (docs/images/como-usar/) com badges numerados, setas e
# callouts curtos apontando os botoes de cada passo.
#
# Coordenadas derivadas de: probe.json (deteccao por cor), mapas ASCII e das
# constantes de layout do codigo (kNavW/margens/alturas — escala 2.0).
# ============================================================================
param(
    [string]$Raw = "docs\images\raw",
    [string]$Annotator = "tools\annotate_screens.ps1"
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = $PWD

if (-not (Test-Path "$Raw\probe.json")) {
    throw "Rode tools\probe_layout.ps1 antes (probe.json ausente)."
}
$probe = Get-Content "$Raw\probe.json" -Raw | ConvertFrom-Json

function Save-Annotation([string]$source, [string]$target, [array]$ann, [float]$scale) {
    & "$Annotator" -Source $source -Target $target -Annotations $ann -Scale $scale
}

# --- 0) Galeria (prints crus com nome final) -----------------------------------
New-Item -ItemType Directory -Force -Path docs\images | Out-Null
foreach ($name in @('mixer-apps-abertos', 'tray-menu', 'modal-novo-dispositivo',
                    'settings-geral', 'settings-regras', 'settings-perfis',
                    'settings-atalhos', 'settings-sobre')) {
    Copy-Item "$Raw\$name.png" "docs\images\$name.png" -Force
    Write-Host "galeria: docs\images\$name.png"
}

New-Item -ItemType Directory -Force -Path docs\images\como-usar | Out-Null

# --- 1) Passo 1 — bandeja (canvas estendido p/ os callouts) --------------------
# Base: tray-overflow.png colada em (32,32) de um canvas 640x400 escuro.
$src = [System.Drawing.Image]::FromFile("$Raw\tray-overflow.png")
$canvas = New-Object System.Drawing.Bitmap 640, 400
$g = [System.Drawing.Graphics]::FromImage($canvas)
$g.Clear([System.Drawing.Color]::FromArgb(255, 32, 32, 32))
$g.DrawImage($src, 32, 32, $src.Width, $src.Height)
$g.Dispose()
$temp = "$env:TEMP\opencode\bandeja-base.png"
New-Item -ItemType Directory -Force -Path "$env:TEMP\opencode" | Out-Null
$canvas.Save($temp, [System.Drawing.Imaging.ImageFormat]::Png)
$canvas.Dispose(); $src.Dispose()
# Rects do tray-rects.json (relativos a regiao) + offset da colagem (32).
$icon = $probe.tray.icon; $chev = $probe.tray.chevron
$iconCx = 32 + $icon.x + $icon.w / 2
$iconCy = 32 + $icon.y + $icon.h / 2
$chevCx = 32 + $chev.x + $chev.w / 2
$chevCy = 32 + $chev.y + $chev.h / 2
Save-Annotation $temp 'docs\images\como-usar\01-bandeja.png' @(
    @{ Type = 'Badge';   XY = "$iconCx,$iconCy"; Text = '1' },
    @{ Type = 'Callout'; XY = "$($iconCx + 16),$iconCy"; Text = 'SoundInt mora aqui'; Anchor = 'left' },
    @{ Type = 'Badge';   XY = "$chevCx,$chevCy"; Text = '2' },
    @{ Type = 'Callout'; XY = "$($chevCx + 16),$chevCy"; Text = 'divisa: abre os icones ocultos'; Anchor = 'left' }
) 2.0

# --- 2) Passo 2 — mixer (secoes + gear) -----------------------------------------
Save-Annotation "$Raw\mixer-apps-abertos.png" 'docs\images\como-usar\02-mixer.png' @(
    @{ Type = 'Badge';   XY = '48,132'; Text = '1' },                 # pill "Saidas"
    @{ Type = 'Badge';   XY = '150,200'; Text = '2' },                 # 1a saida (padrao)
    @{ Type = 'Callout'; XY = '90,320'; Text = 'marcada = padrao; clique noutra p/ trocar'; Anchor = 'left' },
    @{ Type = 'Arrow';   From = '170,300'; To = '170,215' },
    @{ Type = 'Badge';   XY = '48,628'; Text = '3' },                  # pill "Tocando agora"
    @{ Type = 'Badge';   XY = '684,56'; Text = '4' },                   # engrenagem
    @{ Type = 'Callout'; XY = '668,180'; Text = 'abre as Configuracoes'; Anchor = 'right' },
    @{ Type = 'Arrow';   From = '684,150'; To = '684,86' }
) 2.0

# --- 3) Passo 3 — rotear um app (chevron) ----------------------------------------
Save-Annotation "$Raw\mixer-apps-abertos.png" 'docs\images\como-usar\03-rotear-app.png' @(
    @{ Type = 'Badge';   XY = '648,698'; Text = '1' },                 # chevron do app
    @{ Type = 'Callout'; XY = '250,660'; Text = 'clique no chevron do app'; Anchor = 'left' },
    @{ Type = 'Arrow';   From = '530,680'; To = '636,698' },
    @{ Type = 'Badge';   XY = '48,628'; Text = '2' },                  # "Tocando agora"
    @{ Type = 'Callout'; XY = '80,560'; Text = 'o app tocando aparece aqui'; Anchor = 'left' },
    @{ Type = 'Arrow';   From = '150,600'; To = '180,685' },
    @{ Type = 'Callout'; XY = '350,430'; Text = 'a escolha salva uma regra (vale na proxima)'; Anchor = 'left' },
    @{ Type = 'Arrow';   From = '600,470'; To = '640,660' }
) 2.0

# --- 4) Passo 4 — modal de novo app ------------------------------------------------
Save-Annotation "$Raw\modal-novo-app.png" 'docs\images\como-usar\04-modal-novo-app.png' @(
    @{ Type = 'Badge';   XY = '357,198'; Text = '1' },                 # dropdown de saida
    @{ Type = 'Callout'; XY = '100,140'; Text = 'escolha a saida'; Anchor = 'left' },
    @{ Type = 'Arrow';   From = '300,150'; To = '340,190' },
    @{ Type = 'Badge';   XY = '45,279'; Text = '2' },                  # checkbox lembrar
    @{ Type = 'Callout'; XY = '64,279'; Text = 'lembrar = cria regra'; Anchor = 'left' },
    @{ Type = 'Badge';   XY = '512,361'; Text = '3' }                  # botao Aplicar
) 2.0

# --- 5) Passo 5 — configuracoes / regras ---------------------------------------------
Save-Annotation "$Raw\settings-regras.png" 'docs\images\como-usar\05-config-regras.png' @(
    @{ Type = 'Badge';   XY = '200,338'; Text = '1' },                 # nav selecionada
    @{ Type = 'Callout'; XY = '214,338'; Text = 'navegacao em 3 blocos'; Anchor = 'left' },
    @{ Type = 'Badge';   XY = '560,250'; Text = '2' },                 # campo processo
    @{ Type = 'Callout'; XY = '700,335'; Text = 'digite o processo (ex.: spotify.exe)'; Anchor = 'left' },
    @{ Type = 'Arrow';   From = '770,300'; To = '654,258' },
    @{ Type = 'Badge';   XY = '1072,250'; Text = '3' },                # dropdown saida
    @{ Type = 'Callout'; XY = '1010,430'; Text = 'escolha a saida'; Anchor = 'left' },
    @{ Type = 'Arrow';   From = '1080,395'; To = '1072,280' },
    @{ Type = 'Badge';   XY = '1378,250'; Text = '4' },                # botao Adicionar
    @{ Type = 'Badge';   XY = '1344,400'; Text = '5' },                # toggle da regra
    @{ Type = 'Badge';   XY = '1448,392'; Text = '6' },                # X remover
    @{ Type = 'Callout'; XY = '1390,520'; Text = 'X remove (com Desfazer)'; Anchor = 'right' },
    @{ Type = 'Arrow';   From = '1400,485'; To = '1448,392' }
) 3.0

# --- 6) Passo 6 — atualizacoes ---------------------------------------------------------
Save-Annotation "$Raw\settings-atualizacoes.png" 'docs\images\como-usar\06-config-atualizacoes.png' @(
    @{ Type = 'Badge';   XY = '572,384'; Text = '1' },                 # Verificar agora
    @{ Type = 'Callout'; XY = '600,384'; Text = 'verificar agora'; Anchor = 'left' },
    @{ Type = 'Badge';   XY = '1440,304'; Text = '2' },                # toggle canal beta
    @{ Type = 'Callout'; XY = '1420,304'; Text = 'canal beta (nightly)'; Anchor = 'right' },
    @{ Type = 'Badge';   XY = '1440,224'; Text = '3' },                 # autoCheck toggle
    @{ Type = 'Callout'; XY = '1440,160'; Text = 'checar ao iniciar'; Anchor = 'right' },
    @{ Type = 'Arrow';   From = '1440,190'; To = '1440,222' }
) 3.0

Write-Host ''
Write-Host 'Imagens geradas:'
Get-ChildItem docs\images\como-usar\*.png | ForEach-Object {
    $i = [System.Drawing.Image]::FromFile($_.FullName)
    '  {0,-28} {1}x{2}' -f $_.Name, $i.Width, $i.Height
    $i.Dispose()
}
