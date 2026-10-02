# Processo de release do SoundInt

Este documento descreve as pipelines, os canais de distribuição, os segredos,
e como cortar/revogar uma release.

## Visão geral das pipelines

| Workflow | Gatilho | Objetivo |
| --- | --- | --- |
| `.github/workflows/ci.yml` | push em `main` + PRs | Build Debug x64 (1 job) + testes por parte em matriz paralela; sem segredos |
| `.github/workflows/nightly.yml` | cron 03:00 UTC + `workflow_dispatch` | Build Release → zip portátil → **prerelease** `nightly-AAAAMMDD-HHMM` |
| `.github/workflows/release.yml` | tag `v*` ou `workflow_dispatch` | Release estável validada → instalador → assinatura → attestation → draft → publicação |

### CI (`ci.yml`) — build único + testes por parte

1. **`build`** (windows-latest): `cmake -B build -A x64` →
   `cmake --build build --config Debug` → `ctest` → publica os `.exe` das
   suítes como artefato `test-exes`; em falha, sobe artefatos de log.
2. **`test`** (matriz `core/audio/ui/update/app/full`, `fail-fast: false`):
   cada parte roda a própria suíte (`soundint_tests_<track>.exe`, ou o
   `soundint_tests.exe` cheio no caso `full`) num job independente, baixando
   o artefato do build. Uma parte vermelha não esconde as demais e
   **Re-run failed jobs** refaz somente a parte que falhou — o build não
   repete.

### Nightly (`nightly.yml`) — etapas com `needs:`

1. **`build`** — metadados da versão (`0.1.0-nightly.<run_number>`, tag
   `nightly-AAAAMMDD-HHMM`), Release x64, testes, artefato `nightly-build`.
2. **`package`** — zip `SoundInt-<versão>-portable.zip` (exe + LICENSE +
   README), `SHA256SUMS.txt` e `update-manifest.json` com
   `prerelease: true`, `sha256`/`sizeBytes` do zip.
3. **`publish`** — `gh release create --prerelease` com `GITHUB_TOKEN`;
   idempotente (re-execução reenvia os assets).

### Release (`release.yml`) — etapas com `needs:`

1. **`validate`** — a tag precisa ser exatamente `v` + `project(SoundInt
   VERSION ...)` do `CMakeLists.txt`; o `CHANGELOG.md` precisa conter a
   entrada `## [x.y.z]`. Também detecta se os segredos de assinatura existem
   (output `can_sign`).
2. **`build-test`** — Release x64 + suítes + `ctest`; artefato
   `release-build`.
3. **`package`** — instala Inno Setup via choco, compila
   `installer/SoundInt.iss` com `/DAppVersion=<sem v>` e
   `/DSoundIntExe=<exe>`, gera o zip portátil, `SHA256SUMS.txt` e o
   `update-manifest.json` **estável** apontando para o `.exe` do instalador;
   artefato `release-package`.
4. **`sign`** — roda **somente** se `CERT_BASE64` e `CERT_PASSWORD` existirem
   (senão o `if:` é falso e o job é pulado). Decodifica o PFX, assina
   `SoundInt.exe` e o instalador com `signtool` (SHA256 + carimbo de tempo) e
   **regenera** `SHA256SUMS.txt`/`update-manifest.json` (a assinatura muda os
   bytes).
5. **`draft`** — **attestation de proveniência** (`actions/attest-build-provenance`,
   assinatura OIDC dos assets finais) e cria (ou atualiza) a release como
   **rascunho** com todos os assets via `gh`. Roda mesmo com `sign` pulado.
6. **`publish`** — `environment: production` (aprovador humano) e
   `gh release edit --draft=false`.

Os artefatos transitam entre jobs com `actions/upload-artifact` /
`actions/download-artifact` (`release-build`, `release-package`).

## Canais

| Canal | Release | Quem consome | Manifest |
| --- | --- | --- | --- |
| **stable** | tags `vX.Y.Z` (não-prerelease) | usuários finais (padrão) | `update-manifest.json` aponta para o instalador, `prerelease: false` |
| **nightly/beta** | tags `nightly-*` marcadas como prerelease | quem ativa "canal beta" no app | mesmo nome de asset, `prerelease: true`, aponta para o zip |

Como as nightlies são *prerelease*, elas **nunca** aparecem em
`releases/latest` — ou seja, `releases/latest/download/update-manifest.json`
sempre entrega o manifest estável. O updater (`src/update/update_service.h`)
baixa esse arquivo do canal escolhido e compara com a versão instalada.

## Environments e aprovação

- O job `publish` usa `environment: production`.
- Configuração (uma única vez): **Settings → Environments → New environment**
  → nome `production` → **Required reviewers** → adicionar o mantenedor.
  Sem isso, o job publica sem pedir confirmação.
- Recomendado: restringir o environment a tags (`main`), embora aqui o job
  só rode na pipeline de release.

## Segredos

| Segredo | Obrigatório? | Onde é usado | Conteúdo |
| --- | --- | --- | --- |
| `GITHUB_TOKEN` | automático | `nightly`/`release` (`gh release ...`) | fornecido pela Actions |
| `CERT_BASE64` | opcional | job `sign` | certificado `.pfx` em Base64 (`[Convert]::ToBase64String([IO.File]::ReadAllBytes('cert.pfx'))`) |
| `CERT_PASSWORD` | opcional | job `sign` | senha do PFX |

Sem `CERT_BASE64`/`CERT_PASSWORD` a release é publicada **sem assinatura de
código** — funciona, mas o SmartScreen mostra aviso. Para assinar, adicione
os dois secrets em **Settings → Secrets and variables → Actions**.

Nenhum workflow usa outros segredos; a CI roda apenas com `permissions:
contents: read` (as pipelines de release usam `contents: write` para criar
releases).

## Como cortar uma release (estável)

1. Garanta que `main` está verde (CI) e que as mudanças estão no `CHANGELOG.md`.
2. Edite o `CHANGELOG.md`: troque `## [0.1.0] - Não lançado` pela data de
   hoje e, se for o caso, acrescente uma seção `## [Unreleased]`.
3. Confirme que o `project(SoundInt VERSION x.y.z)` do `CMakeLists.txt`
   bate com a tag (a etapa `validate` falha se não bater).
4. Crie e envie a tag:

   ```powershell
   git tag -a v0.1.0 -m "SoundInt 0.1.0"
   git push origin v0.1.0
   ```

   Alternativa: **Actions → Release → Run workflow** informando a tag
   (útil para re-executar).
5. Acompanhe as 6 etapas; em **`6/6 publish`** aprove a environment
   `production` quando solicitado.
6. Confira a release publicada: instalador, zip, `SHA256SUMS.txt` e
   `update-manifest.json` presentes; `releases/latest/download/update-manifest.json`
   resolvendo para a nova versão.

## Rollback

- **Antes do `publish`**: cancele a execução e apague a tag se necessário
  (`git push origin :refs/tags/v0.1.0`); a release fica em draft e pode ser
  descartada (`gh release delete vX.Y.Z --yes`).
- **Depois de publicada**:
  1. `gh release edit vX.Y.Z --draft=true` (ou `--prerelease=true`) para
     tirá-la do canal imediatamente — o `latest` volta a apontar para a
     versão anterior assim que a anterior voltar a ser a mais recente;
  2. publique um hotfix `vX.Y.Z+1` com a correção;
  3. assets antigos nunca são apagados: quem baixou continua podendo baixar.
- **Usuários já atualizados**: o updater **não faz downgrade**. A correção é
  sempre uma versão nova. Para reverter localmente, o usuário reinstala o
  `.exe` da release anterior.

## Re-execução (idempotência)

Todas as etapas de publicação são seguras para re-executar: `gh release
upload --clobber` substitui assets e `gh release create` é precedido por um
`gh release view` — se a tag já existe, apenas os assets são atualizados.
