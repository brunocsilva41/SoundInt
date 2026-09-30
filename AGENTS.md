# AGENTS.md — regras para agentes de codigo do SoundInt

Leia **antes** de qualquer edicao: `docs/OWNERSHIP.md` e os headers de contrato.

## O que e o projeto

App de tray em C++17/Win32 + Direct2D, extremamente leve, que vigia
dispositivos/sessoes de audio e roteia saidas por app no Windows 10/11 x64.

## Regras inegociaveis

1. **Dominio**: so edite arquivos do seu track (OWNERSHIP.md). Fora dele =
   reportar, nao editar.
2. **Contratos congelados**: nao altere headers de contrato da Wave 0
   (lista no OWNERSHIP.md). Precisa de mudanca? Descreva no relatorio.
3. **Sem git**: nao rode `git add/commit/push`. O integrador commita por wave.
4. **Build proprio**: configure e constroia no seu dir `out/<wave>-<track>`:
   ```
   cmake -B out/w1-x -DCMAKE_BUILD_TYPE=Debug
   cmake --build out/w1-x --config Debug --target <alvo>
   ```
   Alvos: `core` `audio` `ui` `update` `SoundInt` `soundint_tests`.
5. **Warnings = erro**: mantenha `/W4` limpo nos seus arquivos.
6. **Estilo**: `.clang-format` (Allman, 4 espacos, UTF-8, linhas <=100).
   Nomes: `CamelCase` tipos, `camelCase` locais, `kConstant`, `m_` membros.
   Arquivos `.h` com `#pragma once`. Comentarios em portugues.
7. **Unicode**: `std::wstring`/`wchar_t` para texto do usuario; API Win32 `W`.
8. **Sem dependencias novas** sem reportar (so libs do Windows SDK + third_party
   ja presente: doctest, JSON do Track C).
9. **Nada de polling** no audio: use as notificacoes dos contratos.

## Comandos de verificação

```
cmake -B out/<track> -DCMAKE_BUILD_TYPE=Debug
cmake --build out/<track> --config Debug --target <alvo> 2>&1 | Select-String "error"
out\<track>\Debug\soundint_tests.exe                      # testes
```

## Relatorio obrigatório (final do agente)

```
## <Track> — concluido
- Arquivos criados: ...
- Arquivos modificados (fora do dominio): NENHUM | justificativa
- Build: OK/falha (trecho do erro)
- Testes: N pass / N fail
- Premissas tomadas: ...
- Pedidos de mudanca de contrato: ...
- Follow-ups para integracao: ...
```

## Contexto util

- Per-app routing: `Windows.Media.Internal.AudioPolicyConfig` via
  `RoGetActivationFactory` (2 vtables: >= 21H2 e downlevel), deviceId =
  `\\?\SWD#MMDEVAPI#{<id>}#{e6327cad-dcec-4949-ae8a-991e976a79d2}` (render).
- Referencias: EarTrumpet (File-New-Project/EarTrumpet), AppAudioRouter.
- Sessoes so existem enquanto o app toca som.
- Eventos: `IMMNotificationClient` (dispositivos) e
  `IAudioSessionNotification` (sessoes) — chegam em threads de sistema;
  use `EventBus::publish` (thread-safe) e deixe a main dar `pump()`.
