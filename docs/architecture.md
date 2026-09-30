# Arquitetura do SoundInt

Visão geral dos módulos, do fluxo de eventos e dos contratos que amarram as
peças. Este documento é o mapa — o detalhe legal está nos **headers de
contrato** (arquivos com `CONTRACT` no topo) e em
[`OWNERSHIP.md`](OWNERSHIP.md).

## Visão geral

```
+----------------------------------------------------------------------+
|                          SoundInt.exe (tray)                         |
|                                                                      |
|  +----------+   +-----------+   +---------+   +--------+   +--------+ |
|  |   app    |-->|   core    |<--|  audio  |   |   ui   |   | update | |
|  | shell/   |   | EventBus  |   | watchers|   | Direct2|   | GitHub | |
|  | wiring   |   | Store     |   | routing |   | D/Win32|   |Releases| |
|  +----------+   | SemVer    |   +---------+   +--------+   +--------+ |
|                 | Log       |                                        |
|                 +-----------+                                        |
+----------------------------------------------------------------------+
        ^                    ^                        ^
        | eventos            | eventos                | rede
 IMMNotificationClient  IAudioSessionNotification  WinHTTP (update)
 (threads de sistema)    (threads de sistema)
```

- **Um processo, sem serviço**: tudo roda no processo do tray.
- **Sem polling**: o audio notifica; a UI só redesenha quando algo muda.
- **CRT estática** (`MultiThreaded`): o instalador e o zip portátil não
  exigem VC++ Redistributable.

## Módulos

| Módulo (target) | Diretório | Responsabilidade |
| --- | --- | --- |
| `core` | `src/core/` | Tipos de domínio (`types.h`), `EventBus` thread-safe, `Store` (config em JSON), `SemVer`, log rotativo |
| `audio` | `src/audio/` | Watchers de dispositivo e sessão, volume/mudo, fachada `AudioService`, roteamento via `policy_config` |
| `ui` | `src/ui/` | Renderer Direct2D, design tokens/paleta, controles, runtime de janela, i18n |
| `update` | `src/update/` | Manifesto, download com SHA256 (WinHTTP/BCrypt), spawn do instalador |
| `SoundInt` (app) | `src/app/` | `wWinMain`, loop de mensagem, tray, wiring dos serviços e políticas puras |

Depências (definidas no `CMakeLists.txt`): `audio → core`,
`ui → core`, `update → core`, `app → todos`. `core` não depende de nenhum
outro módulo.

## Fluxo de eventos

```
thread de sistema          main thread (UI)                consequência
-----------------          -----------------               -------------
IMMNotificationClient ──┐
  OnDeviceStateChanged  │   EventBus::publish(event)   ->  fila interna
  OnDeviceAdded/Removed │   (thread-safe, bloqueio     ->  PostMessage
  OnDefaultDeviceChanged─┘    mínimo)                       (WM_APP_WAKEUP)
                                                              │
IAudioSessionNotification ─┐                              main loop pump()
  OnSessionCreated        │                              -> callbacks
  OnSessionDisconnected   ─┘                              -> UI invalidate
                                                          -> política decide
                                                             (modal? regra?)
```

Regras de ouro (ver `AGENTS.md`):

1. O callback de notificação **só publica** o evento — nunca redesenha, nunca
   chama COM pesado, nunca bloqueia.
2. `EventBus::publish` é thread-safe; o consumo acontece na main thread via
   `pump()` (loop de mensagens do tray).
3. UI é invalidate-driven: só redesenha quando um evento chega ou quando o
   usuário interage.

## Roteamento por aplicativo (per-app routing)

- API: `Windows.Media.Internal.AudioPolicyConfig` via `RoGetActivationFactory`
  (interface interna e **não documentada** — duas variantes de vtable:
  >= 21H2 e downlevel).
- `deviceId` usado no setter no formato de render:

  ```
  \\?\SWD#MMDEVAPI#{<id-do-endpoint>}#{e6327cad-dcec-4949-ae8a-991e976a79d2}
  ```

- Conversões de endpoint ficam em `src/audio/endpoint_id.*`; a lógica pura de
  "qual dispositivo escolher" vive em `policy_*` (testável sem Windows).
- Sessões de áudio **só existem enquanto o app toca som**: a UI precisa
  tolerar sumiço/retorno de sessões (eventos `Created`/`Removed`).

## Dados e persistência

| Onde | O quê |
| --- | --- |
| `%LOCALAPPDATA%\SoundInt\` | configurações (Store, JSON via nlohmann) e `logs\` rotativos |
| `%LOCALAPPDATA%\Programs\SoundInt\` | instalação (instalador per-user) |
| Registro `HKCU\...\Run\SoundInt` | "Iniciar com o Windows" (espelha `Settings::startWithWindows`) |

Nada é gravado em `Program Files` nem exige admin.

## Atualizador (`src/update/`)

Fonte de verdade: o asset `update-manifest.json` da release correspondente
(`releases/latest/download/update-manifest.json` — sem rate-limit de API).

```json
{
  "version": "0.1.0",
  "tag": "v0.1.0",
  "url": "https://github.com/<owner>/SoundInt/releases/download/v0.1.0/SoundInt-Setup-0.1.0.exe",
  "sha256": "hex minusculo",
  "sizeBytes": 12345678,
  "notes": "secao do CHANGELOG",
  "prerelease": false
}
```

Fluxo: `checkForUpdates(beta)` → compara `SemVer` → `download()` valida
SHA256 (BCrypt) → `launchInstaller()` em modo silencioso (o app sai antes).
Canais: stable = releases normais; beta = inclui nightlies (prerelease).

## UI (`src/ui/`)

- `renderer.h`/`renderer_d2d.*` — abstração de desenho + implementação
  Direct2D (retângulos arredondados, texto DWrite, alpha).
- `design_tokens.h` — paleta clara/escura (accent `#0067C0`), tipografia e
  métricas compartilhadas.
- `control.h` + `controls/` — controles reutilizáveis (botão, slider, lista).
- Janelas: modais (`modal.*`), mixer flyout (`mixer.*`), menu tray
  (`menu.*`), settings (`settings/**`) — tudo janela Win32 filha da do tray,
  com i18n em `i18n.*` (`pt-BR`, `en`, `auto`).

## Testes

- Framework: **doctest** single-header (`third_party/doctest`).
- Suites por módulo: `tests/core_test.cpp`, `audio_core_test.cpp`,
  `ui_test.cpp`, `update_test.cpp`, `app_policy_test.cpp` → alvos
  `soundint_tests_<track>`; a suite completa é `soundint_tests` (registrada
  no ctest como `unit` + `app_policy`).
- Lógica pura (sem Win32) primeiro — política de roteamento, semver, codecs,
  controles.

## Contratos congelados

Os headers marcados como `CONTRACT` na Wave 0 (`types.h`, `event_bus.h`,
`store.h`, `log.h`, `audio_service.h`, `renderer.h`, `control.h`,
`design_tokens.h`, `services.h`, `update_service.h`) **não mudam** sem
decisão do integrador — a lista está em [`OWNERSHIP.md`](OWNERSHIP.md).

## CI e distribuição

Build, testes, nightlies e releases são automatizados em
`.github/workflows/` — fluxo completo em
[`release-process.md`](release-process.md).
