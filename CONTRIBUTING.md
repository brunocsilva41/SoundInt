# Contribuindo com o SoundInt

Obrigado por querer contribuir! Este documento explica o setup local, o estilo
de commits e as regras de Pull Request. Para o fluxo de releases, veja
[`docs/release-process.md`](docs/release-process.md).

## Requisitos de desenvolvimento

- Windows 10 1903+ x64 (o projeto e Win32/Win64 nativo).
- Visual Studio 2022 **Build Tools** (ou VS Community) com o workload
  **"C++ Windows"** (MSVC v143 + Windows 10/11 SDK).
- CMake 4.x (`winget install Kitware.CMake`).
- Opcional: `clang-format` (versao 14+) e Inno Setup 6.3+ (para testar o
  instalador localmente).

## Build local

```powershell
cmake -B out/dev -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build out/dev --config Debug
```

Alvos principais: `core`, `audio`, `ui`, `update`, `SoundInt` e
`soundint_tests`. Testes ficam em `out\dev\Debug\`.

```powershell
# Suite completa
out\dev\Debug\soundint_tests.exe

# Suite especifica de um modulo (durante desenvolvimento paralelo)
out\dev\Debug\soundint_tests_core.exe

# Via ctest (registra unit + app_policy)
ctest --test-dir out/dev -C Debug --output-on-failure
```

Release:

```powershell
cmake -B out/dev-rel -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build out/dev-rel --config Release
```

## Estilo de codigo

- Formate com o `.clang-format` da raiz (Allman, 4 espacos, UTF-8, <=100
  colunas): `clang-format -i src\caminho\arquivo.cpp`.
- Nomes: `CamelCase` para tipos, `camelCase` para locais, `kConstant` para
  constantes, `m_` para membros.
- Headers com `#pragma once` e comentarios em portugues.
- Unicode: `std::wstring`/`wchar_t` para texto do usuario e API Win32 `W`.
- Warnings sao erros: mantenha `/W4` limpo nos arquivos que voce alterar.
- Nada de dependencias novas sem discutir no PR (so Windows SDK +
  `third_party/` ja presente).
- Nada de polling no audio — use as notificacoes dos contratos
  (`IMMNotificationClient`, `IAudioSessionNotification`).

## Testes

- Toda mudanca de comportamento **precisa** de teste em
  `tests/<seu>_test.cpp` (doctest: `TEST_CASE` + `CHECK`/`REQUIRE`).
- Rode a suite **antes** de abrir o PR; a CI roda de novo em `windows-latest`.
- Bug fix: adicione um teste que reproduza o bug (e que falhe sem o fix).

## Commits — Conventional Commits

```
<tipo>(<escopo>): <descricao curta em imperativo>

[corpo opcional: o por que, e nao o como]

[footer opcional: Closes #123]
```

Tipos: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`,
`ci`, `chore`, `revert`. Escopos uteis: `core`, `audio`, `ui`, `update`,
`app`, `installer`, `ci`, `docs`.

Exemplos:

```
feat(audio): roteia saida por app ao reconectar dispositivo
fix(ui): invalida o frame certo ao fechar o modal
docs(readme): adiciona secao de build from source
```

O CHANGELOG e gerado/auxiliado por [git-cliff](https://git-cliff.org/) a
partir desses commits (`cliff.toml`).

## Pull Requests

1. **PR pequeno**: um assunto por PR. Refatoracao em PR separado de feature.
2. Fork + branch `feat/...`, `fix/...` ou `docs/...` a partir de `main`.
3. Preencha o checklist do template de PR.
4. Descreva **como testou** (suites e testes manuais).
5. Nao altere arquivos congelados da Wave 0 nem o dominio de outro track
   (veja [`docs/OWNERSHIP.md`](docs/OWNERSHIP.md)); se precisar, abra uma
   discusso no PR.
6. CI (`CI` workflow) precisa passar antes do merge.
7. Revisao obrigatoria: pelo menos 1 aprovacao.

## Agentes de IA

Contribuicoes geradas por agentes seguem [`AGENTS.md`](AGENTS.md):
leia `docs/OWNERSHIP.md` e os headers de contrato antes de editar, edite
apenas o seu dominio, nao commite (`git add/commit`) e emita o relatorio
final no formato do AGENTS.md.

## Segurança

Vulnerabilidades nao devem ser reportadas em issues publicas — veja
[`SECURITY.md`](SECURITY.md).

## Conduta

Ao participar, voce concorda com o [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md).
