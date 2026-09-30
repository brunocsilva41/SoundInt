# OWNERSHIP — mapa de propriedade de arquivos (execucao paralela)

Este documento e **lei** durante as waves. Cada agente so edita arquivos do
seu dominio. Precisa mexer fora dele? **Nao edite** — reporte ao integrador.

## Arquivos CONGELADOS (Wave 0, mudam apenas por decisao do integrador)

- `CMakeLists.txt`, `cmake/version.h.in`
- `src/core/types.h`, `src/core/event_bus.h/.cpp`, `src/core/semver.h`,
  `src/core/store.h`, `src/core/log.h`, `src/core/init.cpp`
- `src/audio/audio_service.h`
- `src/ui/renderer.h`, `src/ui/control.h`, `src/ui/design_tokens.h`
- `src/app/services.h`
- `src/update/update_service.h`
- `docs/OWNERSHIP.md`, `AGENTS.md`

## Tracks (Wave 1)

| Track | Domino (edicao exclusiva) | Alvo de build proprio |
|---|---|---|
| **A — audio-core** | `src/audio/device_watcher.*`, `src/audio/session_watcher.*`, `src/audio/volume.*`, `src/audio/audio_service.cpp` (fachada), `src/audio/init.cpp`, `tests/audio_core_test.cpp` | `out/w1-a` |
| **B — audio-routing** | `src/audio/policy_config.*` (cpp), `src/audio/endpoint_id.*`, `tests/audio_endpoint_test.cpp` | `out/w1-b` |
| **C — core-services** | `src/core/*.cpp` (store, store_codec, semver, log), `third_party/` (nlohmann ja presente), `tests/core_test.cpp` | `out/w1-c` |
| **D — ui-renderer** | `src/ui/renderer_d2d.*`, `src/ui/palette.*`, `tests/ui_test.cpp` | `out/w1-d` |
| **E — ui-controls** | `src/ui/controls/**`, `tests/ui_controls_test.cpp` | `out/w1-e` |
| **F — updater** | `src/update/*.cpp` (exceto header), `tests/update_test.cpp` | `out/w1-f` |
| **G — build/CI/gov** | `.github/**`, `installer/**`, `docs/**`, `LICENSE`, `CONTRIBUTING*`, `CODE_OF_CONDUCT*`, `SECURITY*`, `README*`, `CHANGELOG*`, `.clang-format`, `cliff.toml` | `out/w1-g` |
| **H — app-shell** | `src/app/**` (exceto `services.h`), `tests/app_policy_test.cpp` | `out/w1-h` |

\* `src/ui/ui_runtime.cpp` e `src/ui/i18n.cpp` sao substitutiveis: H pode
ajustar ui_runtime (loop/invalidate) e o agente de Settings da Wave 2
substitui i18n.cpp. `tests/ui_test.cpp` pertence a D/E.

**Testes**: cada track tem seu proprio arquivo `tests/<track>_test.cpp`
(nao ha arquivo de testes compartilhado — evita conflito entre agents).

## Waves 2-4

| Agente | Domino |
|---|---|
| **I — modal windows** | `src/ui/windows/modal.*` |
| **J — mixer flyout + menu tray** | `src/ui/windows/mixer.*`, `src/ui/windows/menu.*` |
| **K — settings UI + i18n** | `src/ui/windows/settings/**`, `src/ui/i18n.*`, `assets/i18n/**` |
| **L — QA/testes** | `tests/**` (novos), `docs/test-plan.md` |
| **M — integracao** | wiring em `src/app/**`, correcoes pontuais (unico que pode invadir dominios, na integracao final) |
| **N — docs/release** | `README*`, `CHANGELOG*`, `docs/**` |

## Regras de concorrência

1. **Nunca** rodar `git add/commit` (apenas o integrador, ao fim de cada wave).
2. Build **somente** no proprio dir `out/w1-<track>` (nunca `out/` compartilhado).
3. Contrato precisa mudar? **Reportar**, nao alterar.
4. CMake root e estatico (GLOB CONFIGURE_DEPENDS): basta adicionar `.cpp`
   dentro do proprio diretorio.
5. Ao terminar: rodar build proprio + testes e emitir o relatorio padrao
   (ver AGENTS.md).
