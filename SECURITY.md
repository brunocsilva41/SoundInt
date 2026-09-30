# Segurança

## Versões suportadas

| Versão | Suportada |
| --- | --- |
| 0.1.x | :white_check_mark: |
| < 0.1 | :x: (ainda nao lancada) |

Somente as ultimas releases estaveis recebem correcoes de seguranca. Builds
`nightly-*` sao prereleases de desenvolvimento e nao recebem correcoes.

## Relatar uma vulnerabilidade

**Nao abra uma issue publica.** Reporte em privado por um dos canais:

1. **Recomendado**: [GitHub Security Advisories](https://github.com/OWNER/SoundInt/security/advisories/new)
   ("Report a vulnerability").
2. **Email**: ybrunooh@gmail.com — assunto `[SECURITY] SoundInt`.

Inclua, sempre que possivel:

- Versao do SoundInt e build do Windows;
- Passos para reproduzir ou a prova de conceito;
- Impacto estimado (ex.: execucao de codigo, vazamento de dados);
- Sugestao de correcao, se houver.

### O que esperar

- **Confirmacao de recebimento** em ate 5 dias uteis;
- **Avaliacao e plano de correcao** comunicado em ate 14 dias;
- **Divulgacao coordenada**: a vulnerabilidade so e publicada apos a
  correcao estar disponivel (ou combinado outro prazo com o reporter);
- Creditos do reporter na release/CVE, a menos que prefira anonimato.

## Escopo

Dentro do escopo: o codigo do SoundInt (app, instalador, atualizador e
workflows de CI/release), incluindo a verificacao de SHA256 do atualizador
e o transporte das releases via GitHub.

Fora do escopo: vulnerabilidades em dependencias de terceiros (doctest,
nlohmann/json — reporte-upstream), no Windows ou em outros apps alvos do
roteamento de audio; denial of service fisico/local sem bypass de sandbox
(no app nao ha sandbox).

## Preferencias de relato

- Fale em portugues ou ingles.
- Nao inclua dados reais de audio, logs pessoais ou chaves no relatorio.
- Se houver duvida entre issue e advisory: **advisory**.
