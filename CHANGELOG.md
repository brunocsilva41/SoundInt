# Changelog

Todas as mudancas notaveis do SoundInt sao documentadas neste arquivo.
Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e versionado por [Semantic Versioning](https://semver.org/lang/pt-BR/).

## [0.1.0] - Não lançado

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
