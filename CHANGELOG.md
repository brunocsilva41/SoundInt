# Changelog

Todas as mudancas notaveis do SoundInt sao documentadas neste arquivo.
Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e versionado por [Semantic Versioning](https://semver.org/lang/pt-BR/).

## [0.2.0] - 2026-10-11

### Added

- Passo a passo "Como usar" no README, com prints reais anotados (setas e
  badges apontando cada botão) e galeria de telas — gerados pelo novo modo
  de captura `SoundInt.exe --capture` + ferramentas em `tools/`.
- Botão **"Baixar atualização"** quando há atualização disponível, com
  estados reais (verificando/erro/sucesso) na página de Atualizações.
- **Desfazer** (5 s) ao remover regra, perfil ou rota; remoção nunca mais
  é irreversível.
- Dropdowns de saída com **nome amigável** nas Regras e nos Perfis (fim do
  deviceId cru); "Padrão do sistema" vira opção do seletor.
- Aviso de **conflito de atalho** na página de Atalhos quando o
  `RegisterHotKey` falha; resultado do re-registro chega à página.
- Cursor de mão/IBeam em todo o app (`WM_SETCURSOR` na base) e fechamento
  de modais ao perder o foco (fila de modais nunca mais trava).
- Ícone da bandeja é **re-adicionado sozinho** quando o Explorer reinicia
  (`TaskbarCreated`) — o app não fica mais "zumbi" sem menu Sair.
- Encerramento limpo no fim de sessão do Windows (`WM_ENDSESSION`) e log
  "SoundInt encerrando" (diagnóstico de ciclo de vida).

### Fixed

- Updater de produção: os workflows publicavam `sizeBytes` e o parser
  exige `size` — o auto-check falhava em 100% dos boots; corrigido nos
  dois workflows e aceito o formato legado.
- Bug Tema × Idioma nas Configurações (clique no popup do Tema caia no
  campo Idioma).
- Volume "possuído" no mixer (slider continuava aplicando valor no
  hover após clique direito durante o drag) e wheel de trackpad rolando
  a página durante o clique.
- UAF na página de Atalhos (segurar GRAVAR + apertar a tecla da
  combinação destruía o botão sob o mouse).
- Duplo clique na bandeja "piscava" o mixer (esconde e reabre no mesmo
  clique) — debounce de 250 ms.
- 5 controles das páginas Regras/Perfis que nunca recebiam `setBounds`
  (X e toggles invisíveis); 11 valores fora do grid de 4 px; DPI faltando
  em 8 controles; link de ícone quebrado no README; typo "Perfícil".

### Changed

- Configurações: navegação em 3 blocos (Comportamento / Roteamento /
  Sistema), Geral seccionada, scrollbar visível, navegação por Tab/foco,
  vocabulário único (Regra/Rota) e remoção com confirmação visual.
- Descrições canônicas do produto em tooltip da bandeja, `app.rc`
  bilíngue (en-US/pt-BR), instalador e rodapé das release notes.

## [0.1.0] - 2026-10-02

### Added

- App de tray em C++17/Win32 + Direct2D, leve, para Windows 10 1903+ x64.
- Vigilancia de dispositivos de audio via `IMMNotificationClient`
  (adicao, remocao, mudanca de estado e de default).
- Vigilancia de sessoes de audio por app via `IAudioSessionNotification`
  (criacao, remocao, volume e estado).
- Roteamento de saida por app (per-app routing) via
  `Windows.Media.Internal.AudioPolicyConfig`.
- Modais de escolha de dispositivo/saida (novo app, novo dispositivo).
- Mixer flyout com volume/mudo por sessao e menu de bandeja.
- Regras por processo (`AppRule`), regras de chegada de dispositivo
  (`ArrivalRule`) e perfis (`Profile`).
- Hotkeys globais (`RegisterHotKey`) configuraveis.
- Armazenamento de configuracoes e logs rotativos em
  `%LOCALAPPDATA%\SoundInt`.
- Atualizador proprio via GitHub Releases com manifest
  `update-manifest.json` (canais stable/beta, SHA256).
- Instalador per-user (Inno Setup) e distribuicao portatil (zip).
- Pipeline CI (build + testes), nightly e release no GitHub Actions.
- Testes unitarios com doctest (`soundint_tests` + suites por track).

### Changed

- Nada ainda — primeira versao.
