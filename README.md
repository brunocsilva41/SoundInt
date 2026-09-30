# SoundInt

<!-- Substitua OWNER pelo usuario/organizacao do GitHub quando o repo for publicado. -->
[![CI](https://github.com/OWNER/SoundInt/actions/workflows/ci.yml/badge.svg)](https://github.com/OWNER/SoundInt/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/OWNER/SoundInt?label=release&color=0067C0)](https://github.com/OWNER/SoundInt/releases)
[![License: MIT](https://img.shields.io/badge/licença-MIT-0067C0.svg)](LICENSE)
![Platform](https://img.shields.io/badge/platform-Windows%2010%20%2B%20x64-202020)

App de bandeja (tray) ultraleve para Windows 10/11 que vigia seus dispositivos
e sessões de áudio e **roteia a saída de som por aplicativo** — o Spotify no
fone, o navegador nas caixas, o Discord no microfone — sem tocar no mixer do
sistema.

Feito em **C++17 + Win32 + Direct2D**, sem Electron, sem runtime, sem
serviço em segundo plano: um único exe com CRT estática que fica parado na
bandeja esperando eventos.

## Funcionalidades

- **Modais inteligentes** — aviso quando um novo app começa a tocar ou um
  novo dispositivo é conectado, com escolha de saída em um clique.
- **Mixer por app** — volume e mudo por sessão, agrupadas por processo, com
  fluxo instantâneo de eventos (sem polling).
- **Regras e perfis** — `AppRule` por processo ("Spotify sempre no fone"),
  `ArrivalRule` ("ao conectar o fone: vira default e aplica regras") e perfis
  comutáveis.
- **Hotkeys globais** — atalhos configuráveis (`RegisterHotKey`) para abrir o
  mixer, ciclar saídas ou trocar de perfil.
- **Troca automática de default** — detecção de dispositivos via
  `IMMNotificationClient` (conexão, desconexão, default do sistema).
- **Atualizador próprio** — checa o `update-manifest.json` das GitHub
  Releases, valida SHA256 e aplica a atualização (canais stable/beta).
- **Perfícil de viver junto** — inicia com o Windows, fecha para a bandeja,
  logs rotativos em `%LOCALAPPDATA%\SoundInt`.

## Requisitos

- Windows 10 versão 1903 (build 18362) ou superior, **x64**
  (Windows 11 também).
- Nada mais: a CRT é estática e o app não instala serviço.

## Instalação

1. Baixe o `SoundInt-Setup-x.y.z.exe` da
   [página de releases](https://github.com/OWNER/SoundInt/releases/latest);
2. Execute — instala **por usuário**, sem UAC, em
   `%LOCALAPPDATA%\Programs\SoundInt`;
3. Marque "Iniciar com o Windows" se quiser.

Alternativa **portátil**: baixe o `SoundInt-x.y.z-portable.zip`, extraia em
qualquer pasta e rode o `SoundInt.exe`.

Canal beta: as builds `nightly-*` são publicadas como *prerelease* e podem
ser habilitadas no app (Configurações → Atualizações → canal beta).

## Build a partir do código

Pré-requisitos: Visual Studio 2022 Build Tools (workload "C++ Windows") e
CMake 4.x.

```powershell
git clone https://github.com/OWNER/SoundInt.git
cd SoundInt

# configurar e compilar (Debug)
cmake -B out/dev -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build out/dev --config Debug

# testes
out\dev\Debug\soundint_tests.exe

# Release
cmake -B out/dev-rel -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build out/dev-rel --config Release
```

Alvos: `core`, `audio`, `ui`, `update`, `SoundInt` e `soundint_tests`.
Detalhes da arquitetura em [`docs/architecture.md`](docs/architecture.md) e
do fluxo de releases em [`docs/release-process.md`](docs/release-process.md).

> **Nota para agentes de IA**: este repositório usa `AGENTS.md` +
> `docs/OWNERSHIP.md` para execução paralela de agentes. Leia os dois antes
> de editar qualquer arquivo.

## Aviso: API não-documentada

O roteamento por aplicativo usa a interface **`Windows.Media.Internal.AudioPolicyConfig`**,
que é **interna e não-documentada** do Windows (é o que EarTrumpet e similares
usam). Ela:

- não possui garantia de estabilidade entre builds do Windows;
- pode mudar ou sumir em futuras atualizações do sistema;
- é acessada via `RoGetActivationFactory` com duas variantes de vtable
  (>= 21H2 e downlevel).

O SoundInt trata falhas graciosamente (a função deixa de funcionar, o resto do
app continua), mas **não há garantia** de compatibilidade com versões futuras
do Windows. Aproveite enquanto durar.

## Estrutura do repositório

```
src/core      tipos, EventBus, store, semver, log
src/audio     device/session watchers, volume, policy config (roteamento)
src/ui        renderer Direct2D, controles, janelas (modais/mixer/menu/settings)
src/update    atualizador via GitHub Releases
src/app       shell do tray, wiring e políticas
installer/    script Inno Setup (SoundInt.iss)
.github/      workflows de CI, nightly e release
docs/         OWNERSHIP, arquitetura, processo de release
tests/        suítes doctest por módulo
```

## Contribuindo

Leia [`CONTRIBUTING.md`](CONTRIBUTING.md) (setup, Conventional Commits, regras
de PR) e o [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md). Relatos de segurança:
[`SECURITY.md`](SECURITY.md).

## Licença

MIT © 2026 Bruno Silva — veja [`LICENSE`](LICENSE).
