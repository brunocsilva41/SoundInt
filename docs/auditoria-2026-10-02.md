# Auditoria técnica de código — 2026-10-02

Auditoria de produção crítica conduzida por 4 agentes em paralelo, somente
leitura, varrendo todos os 52 `.cpp` de `src/` + headers + `tests/` onde
relevante. Cada achado foi **validado por leitura do código real** antes de
reportar; falsos positivos foram descartados com justificativa (seção final).

- Agente A — vazamento de memória e recursos do SO
- Agente B — concorrência, threads e sincronização
- Agente C — buffer/UB/ponteiros e segurança de dados externos
- Agente D — CPU/desempenho, estabilidade, erros e init/shutdown

## Resumo executivo

| Severidade | Quantidade | Situação |
| --- | --- | --- |
| CRÍTICA | 0 | — |
| ALTA | 2 | pendentes (Fase 0) |
| MÉDIA | ~16 | 1 corrigido nesta sessão, demais pendentes |
| BAIXA | ~20 | backlog |

Nenhum vazamento de handle Win32/COM sistemático, nenhum deadlock
demonstrável, nenhum overflow de buffer clássico (`strcpy`/`sprintf` não
existem no código), nenhuma recursão descontrolada e nenhum polling no áudio
(confere com o AGENTS.md). Os riscos reais estão em: 1 use-after-free de UI,
exceção que derruba o processo na thread de update, violação de contrato de
callback do WASAPI, e CPU gasta rebuildando o mixer mesmo com ele oculto.

## Lista priorizada — o que corrigir primeiro

### Fase 0 — antes de considerar seguro/estável para produção

| # | Sev. | Arquivo | Problema |
| --- | --- | --- | --- |
| 1 | ALTA | `ui/windows/settings/settings_window.cpp` | **UAF**: `capture_` aponta para controles destruídos por `refreshAll()` (2ª instância → `WM_APP_SHOW_REQUEST` → rebuild durante botão pressionado) |
| 2 | ALTA | `ui/windows/mixer.cpp` | **CPU 24/7**: rebuild completo (enum COM + árvore) a cada `AppEvent`, mesmo com o mixer oculto, sem coalescência |
| 3 | ALTA* | `audio/session_watcher.cpp:854,893` | **Contrato MSDN**: `Register/UnregisterAudioSessionNotification` dentro do próprio `OnSessionCreated` (comportamento indefinido no WASAPI) |
| 4 | MÉDIA | `update/http_win.cpp` + `app/main.cpp` | Corpo HTTP sem limite + `std::thread(...).detach()` sem `try/catch` → `bad_alloc` = `std::terminate` (app morre no auto-check do boot) |
| 5 | MÉDIA | `update/manifest.cpp` + `update_service.cpp` | `manifest.version` não sanitizado no destino de download (**path traversal**, dormente) e `http://` aceito |
| 6 | MÉDIA | `audio/session_watcher.cpp` | Sessões de apps encerrados **nunca saem** de `m_endpoints` (crescimento 24/7: mapa + COM refs) |

\* item 3 é ALTA mas exige refactor com fila de pendentes — ver Fase 1.

### Fase 1 — estabilidade e concorrência

| # | Sev. | Arquivo | Problema |
| --- | --- | --- | --- |
| 7 | MÉDIA | `audio/session_watcher.cpp:868-877` | `buildSessionInfo` (OpenProcess + SHLoadIndirectString) roda **sob `m_mutex`** — trava UI e callbacks do sistema |
| 8 | MÉDIA | `audio/audio_service.cpp:278-287` | `sync()` completo em todo `DeviceEvent`, inclusive `DefaultChanged` (que não muda sessões) |
| 9 | MÉDIA | `audio/session_watcher.cpp:850-903` | QI + `new SessionEvents` + Register/Unregister **por sessão a cada sync** para sessão já conhecida (churn COM + eventos duplicados na janela Register→Unregister) |
| 10 | MÉDIA | `app/main.cpp:443` | Data race: `betaChannel` lido na thread de update sem snapshot (`store().settings()` escapa do lock por contrato) |
| 11 | MÉDIA | `ui/renderer_d2d.cpp:79` | Estática local destruída antes das janelas na saída anormal → `detach()` em objeto morto (UB) — mesmo problema que `log.cpp:26` resolve vazando de propósito |
| 12 | MÉDIA | `core/log.cpp:187` | `log::init()` falha em silêncio total (sem disco/ACL) e o app roda sem diagnóstico nenhum |
| 13 | MÉDIA | `app/router.cpp` | Reenumera **todos** os dispositivos COM por regra/perfil aplicado (perfis grandes ficam lentos) |
| 14 | MÉDIA | `core/store.cpp:119-123` | 4× `FlushFileBuffers` + 4× `MOVEFILE_WRITE_THROUGH` na UI thread por mutação (trava perceptível em disco lento) |
| 15 | MÉDIA | `ui/ui_runtime.cpp` + `window_base.cpp` | `Control::invalidate()` repinta **todas** as janelas visíveis (hover no mixer repinta Configurações) |
| 16 | MÉDIA | `settings/settings_window.cpp:310` | `layout()` completo (com `GetDpiForWindow`) a cada `WM_NCHITTEST`/mousemove |
| 17 | MÉDIA | `update/http_win.cpp:262-317` | `downloadFile` nunca valida `done` vs `total` (aceita download maior/menor que o esperado) |

### Fase 2 — robustez e desempenho (BAIXA)

`CoTaskMemFree` só no ramo de sucesso (4 pontos) · cooldowns do Router sem
podas · cache de ícone de exe sem teto · fila do `EventBus` sem teto ·
`g_shell = nullptr` antes de `DestroyWindow` pula cleanup raro · "Verificar
agora" sem guarda de reentrada (N threads + N WinHTTP) · instalador deixado
em `%TEMP%` (dormente) · `checkForUpdates` mantém `manifest.valid=true` com
versão inválida · TOCTOU hash→spawn (dormente) · HWND destruído antes de
`g_hwnd.store(nullptr)` · `cycleOutput` ignora falha do role Console e loga
sucesso · `CoCreateInstance` por chamada com factory cacheada disponível ·
`flush()` + `OutputDebugStringW` por linha de log · argumento `/p,"dir"` sem
rejeição de aspas vinda de env · `swprintf_s(buffer[192])` em
`debugFailure` (hoje seguro, frágil).

## Achados por categoria

### Vulnerabilidades de segurança

1. **Path traversal no instalador (MÉDIA, dormente)** —
   `update_service.cpp:163-164` monta `%TEMP%\SoundInt-Setup-<manifest.version>.exe`
   com `version` vinda do JSON remoto sem sanitização; `parseManifest` aceita
   qualquer string não-vazia e **`http://`**. Hoje inalcançável porque
   `download()`/`launchInstaller()` não têm chamadores. Correção: exigir
   `https://` no manifesto, validar `Version::parse` + whitelist
   `[0-9A-Za-z.-]` no `version` antes de montar caminho. SHA256 do
   manifesto é auto-referente (mesmo JSON que traz a URL) — a confiança real
   é o TLS ao GitHub; attestation de proveniência (agora na pipeline)
   adiciona verificação de origem.
2. **Resposta HTTP ilimitada → OOM/terminate (MÉDIA)** —
   `http_win.cpp:193-215`: `std::vector<char>(available)` com `available`
   do servidor + `body.append` sem teto; redirect `ALWAYS`. Com exceção
   escapando de `std::thread.detach()` sem handler → `std::terminate` no
   processo inteiro. Correção: teto (1 MiB para API/manifesto) + `try/catch`
   no corpo das duas threads.
3. **UAF de input de UI (MÉDIA)** — `SettingsWindow::capture_` cru,
   destruído por `rows_.clear()` durante `refreshAll()` no caminho
   "segunda instância → show → rebuild" (detalhado em Estabilidade).
4. **Injeção de argumento em `explorer.exe` (INFO)** —
   `settings_about.cpp:43-51` monta `/p,"<dir>"` com dir influenciável por
   env `SOUNDINT_DATA_DIR`; aspas não rejeitadas. Impacto baixo (explorer
   não executa binários arbitrários).
5. Confirmado **sem**: format-string, `strcpy`/`sprintf`/`wcscpy`/`memcpy`
   sem limite (grep: zero em `src/`), path traversal em extração (não há
   extração de zip), carregamento de DLL com path manipulável.

### Problemas de memória

1. **Crescimento 24/7 em `m_endpoints` (MÉDIA)** — a saída normal de um app
   dispara `OnStateChanged(Expired)` (só marca `active=false`); o único
   caminho de remoção é `OnSessionDisconnected` (que a MS só dispara para
   remoção de dispositivo/etc.) ou o diff de `syncEndpoint` (só em evento de
   dispositivo). Cada sessão morta mantém nó do map + 5-6 `wstring` + 3
   refs COM (`control`/`volume`/`events`). Efeito colateral: mixer lista
   apps que já saíram. Correção: prune por PID vivo em `sessions()`/`sync()`.
2. `CoTaskMemFree` só no ramo `SUCCEEDED` (4 pontos — Fase 2).
3. `CooldownTracker::last_` nunca podado; `Router::stop()` não zera
   (Fase 2). `exeIconAvailable` cache estático sem teto (Fase 2).
   `EventBus::queue_` sem teto; com `wakeup_` vazia no shutdown os eventos
   enfileirados nunca drenam (Fase 2).
4. **Sem vazamento** em: singletons `log`/`Store` (vazados de propósito),
   factory COM de processo, `DeviceWatcher`/`SessionEvents` (refcount com
   Release em todos os caminhos incl. erro), `HICON`/`HMENU`/handles de
   arquivo (RAII ou pares simétricos), clipboard, doctest/nlohmann.

### Problemas de CPU/GPU e desempenho

1. **Mixer rebuilda tudo a cada evento, mesmo oculto (ALTA)** — depois do
   primeiro toggle a janela existe para sempre (`hide()`), `valid()` é só
   `hwnd_ != nullptr`; cada `AppEvent` → `host_.outputs()` (EnumAudioEndpoints
   + OpenPropertyStore por device) + snapshot de sessões + `pool_.clear()` +
   `SetWindowPos`. `EventBus::pump()` despacha N eventos → N rebuilds por
   iteração, sem coalescência. Com slider de volume do Windows rolando: 30-60
   rebuilds/s com o mixer fechado. Correção: pular rebuild quando oculto
   (marcar sujo; `showFlyout()` já chama `refresh()`) + 1 flush por batch de
   `pump()`.
2. Cascata de enumerações redundantes (sync em `DefaultChanged`, churn
   QI/Register por sessão, Router re-enum por regra) — Fase 1 #8/#9/#13.
3. `Store::save()` = 8 CreateFile + 4 fsync + 4 rename write-through por
   clique, na UI thread (Fase 1 #14). Repaint global de janelas (Fase 1
   #15). `layout()` por mousemove (Fase 1 #16). Log com flush por linha
   (Fase 2).
4. **Confirmado**: nenhum polling, todos os timers são one-shot com
   `KillTimer` correspondente, únicos `while(true)` têm parada garantida.

### Problemas de concorrência

1. **Registro de callback dentro de event callback (ALTA)** —
   `createSession` roda dentro de `OnSessionCreated` (thread de sistema) e
   chama `RegisterAudioSessionNotification` (e no caminho de dedup,
   `Unregister`) — MSDN: *"must not register or unregister notification
   callbacks during an event callback"*. Risco: UB/latência/perda de
   notificações no WASAPI. Ciclo de deadlock não demonstrado (todo espera de
   callback acontece com `m_mutex` livre — invariante documentada e
   conferida). Correção: enfileirar `PendingSession` no callback e registrar
   na main (`drainPending()` chamado de `sync`/`sessions`/`setVolume`).
2. **Callbacks esperam `m_mutex` (MÉDIA)** — literalmente contra MSDN
   ("must be nonblocking / never wait on a synchronization object"), mas
   sem ciclo de deadlock atual; a mitigação prática é o fix de I/O sob lock
   (Fase 1 #7) e a fila do item 1. Manter como invariante de review.
3. **Data race em `betaChannel` (MÉDIA)** — leitura cross-thread de campo
   não-atômico do Store (contrato `settings()` escapa do lock). + threads
   `.detach()` sem join no shutdown (janela curta de corrida com estáticos).
4. **TOCTOU benigno (BAIXA)** — `g_hwnd.store(nullptr)` depois de
   `window.destroy()` (só HWND, sem ponteiro de objeto).
5. Confirmados como seguros: `EventBus` (wakeup/handlers copiados e
   chamados fora do lock; handlers copiados no `pump` — unsub durante
   despacho não invalida), `SessionWatcher::stop()` (Unregister/Release
   fora do mutex), guards AddRef durante callback, `SendMessage`
   cross-thread inexistente (só `PostMessageW`), COM em MTA, atomics de
   i18n.

### Vazamento / uso indevido de recursos

- Fase 2: `g_shell = nullptr` antes de `DestroyWindow` pula
  `Shell_NotifyIcon(NIM_DELETE)`/`UnregisterHotKey`/`KillTimer` no caminho
  raro de saída sem `WM_DESTROY`; "Verificar agora" reentrante cria N
  threads (N stacks ~1 MB + N sessões WinHTTP); instalador em `%TEMP%` não
  removido (dormente); sem handle de arquivo/socket/thread vazando nos
  caminhos de erro (todos auditados com RAII).

### Problemas de estabilidade

- **`std::terminate` na thread de update** (Fase 0 #4) — pior caso: auto-check
  do boot contra corpo gigante/malformado → app some da bandeja sem log.
- **UAF em `SettingsWindow::capture_`** (Fase 0 #1) — cenário validado:
  pressionar toggle em Regras + lançar 2ª instância → `refreshAll()` →
  `rows_.clear()` destrói o `Toggle` → próximo `WM_MOUSEMOVE/UP` usa o
  ponteiro. Crash garantido.
- **UB na saída anormal** (Fase 1 #11) — estáticas de UI destruídas antes do
  `Renderer`; `WM_DESTROY` tardio chama `detach()` em objeto morto.
- Fase 2: `checkForUpdates` devolve `valid=true` com versão inválida;
  `cycleOutput` loga sucesso quando o role Console falhou.
- **Condições extremas conferidas como OK**: disco cheio (log/store tratam),
  0 dispositivos, DPI/monitor removido, wrap de `GetTickCount()`, settings
  corrompido/menor que 64 MB, `AudioService::stop()` idempotente, ordem de
  shutdown do caminho normal correta.

### Comportamentos indefinidos

1. UAF (Fase 0 #1).
2. Exceção fora de handler em thread → `terminate`.
3. `Register/Unregister` dentro de callback (Fase 0 #3) — UB de plataforma.
4. Data race formal em `betaChannel` (UB pelo padrão C++).
5. Destruição invertida de estáticas na saída anormal (Fase 1 #11).
6. Verificados e **seguros**: overflow assinado em `cooldownRemaining`
   (inalcançável — só constantes), `static_cast<size_t>` de float negativo
   (sempre precedido de guarda), `MultiByteToWideChar` com `int` (exigiria
   >2 GB), índices `vec[i]` (todos com guarda — lista conferida), parser
   nlohmann iterativo (sem stack overflow por aninhamento).

### Problemas arquiteturais relevantes

1. **Contrato "Store só na main"** (`store.h:29` — `settings()` sem lock)
   não está documentado nem imposto; a thread de update o violou. Se leitura
   cross-thread for necessária no futuro: snapshot travado
   (`Settings settingsSnapshot() const`).
2. **Invariantes não documentados** que a auditoria precisou reconstituir:
   "sob `m_mutex` não fazer I/O" (hoje violado por `buildSessionInfo`),
   "nunca esperar callback com lock" (correto por acidente dos pontos
   certos), "registro de callbacks só fora de callbacks" (violado no item 3).
3. **`Control::invalidate()` sem dono** (header congelado) força repaint
   global — se o contrato puder mudar, pointer janela→controle resolve.
4. **Updater sem caminho de `download`/`launch` ligado**: o fluxo de
   atualização real está dormente; segurança (traversal/TOCTOU) precisa ser
   resolvida **antes** de ligar (Fase 0 #5 e Fase 2).
5. **Duas formas de criar default endpoint** (`CoCreateInstance` direto em
   `audio_service` vs factory cacheada em `policy_config`) — convergir.

## Falsos positivos descartados (principais)

- Singletons `log`/`Store` com `new` sem `delete`: intencional e comentado
  (evita ordem de destruição de estáticos) — é exatamente o padrão
  recomendado pelo achado Fase 1 #11.
- `std::thread(...).detach()` como vazamento de heap: não é (a thread
  termina e libera o próprio heap); reportado só como recurso/estabilidade.
- `SetTimer(nullptr, ..., 1ms)` em `modal_common.cpp`: one-shot com
  auto-kill no callback e em `modal::shutdown()`.
- nlohmann: parser iterativo, sem recursão por aninhamento; `dump` com limite.
- `EventBus::pump` copia handlers sob lock e despacha fora — unsub durante
  despacho seguro.
- `tr()`/`palette()` por frame: hash em tabela estática/POD, sem alocação.
- `lstrcpynW` do tray: limitado por `ARRAYSIZE`; logs sem format-string.
- `exeIconAvailable` static map: só main thread (crescimento sim — Fase 2).
- Handles em `http_win`/`sha256`/`installer`/`store`: RAII ou pares em
  todos os ramos, inclusive erro.

## Conforme AGENTS.md

```
## Auditoria (read-only) — concluida
- Arquivos criados: docs/auditoria-2026-10-02.md
- Arquivos modificados (fora do dominio): NENHUM (agentes nao editaram)
- Build: N/A na auditoria (correcoes posteriores validadas com build+testes)
- Testes: N/A na auditoria
- Premissas: callbacks WASAPI/MMDevice chegam em threads de sistema;
  "event callback" = métodos de IMMNotificationClient/IAudioSessionEvents/
  IAudioSessionNotification; severidade ALTA = crash/UB/violação de
  contrato de plataforma/objetivo 24-7; MÉDIA = estável mas custoso ou
  frágil; BAIXA = robustez
- Pedidos de mudança de contrato: store.h (acesso cross-thread) se
  necessário; ui/control.h (invalidate com dono) se Fase 1 #15 for atacado
- Follow-ups: Fase 0 → Fase 1 → Fase 2 nesta ordem
```

## Addendum — Fase 0 e Fase 1 aplicadas (2026-10-02)

- **Fase 0** (commit `55add0c`): UAF `capture_`, tetos HTTP (1 MiB/512 MiB),
  `try/catch` nas threads de update, snapshot do `beta`, updater https-only +
  charset em `version`, reentry "Verificar agora", tetos EventBus/ícones,
  renderer leaky, `CoTaskMemFree` incondicional, ordem `g_shell`/`g_hwnd`.
- **Fase 1** (este commit): `RegisterAudioSessionNotification` fora do
  `OnSessionCreated` (fila `m_pendingRegistrations`, `registerNow=false` no
  callback, `processPendingRegistrations()` nos pontos de `drainDead`);
  `buildSessionInfo` fora do `m_mutex` (+ releitura barata de volume no
  insert); poda de zumbis sem eventos em `syncInternal`; mixer não
  reconstrói oculto; `Store::save()` com dedup por conteúdo (só regrava o
  arquivo que mudou).
- Build `/W4` limpo, suíte 119/119, CI com os 7 checks verdes.
