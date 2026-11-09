# Release com GPL — checklist (interno, não vai para o espelho)

Decisão de 26/10: Android e desktop cumprem a GPLv3 publicando o **fonte de cada versão distribuída** num
repositório público espelho (`VoIPMundoTelecom/nexus-softphone-desktop-src`). iOS em standby.

Ordem obrigatória — **nenhum instalador sai para usuário antes do espelho da mesma tag estar público**:

0. **Antes da tag**: os PRs que mexem no app entram primeiro (ordem do desktop: #10 → #8 → #11); a tag sai da
   `main` atualizada.
1. **Versão e SDK**: o submódulo `external/linphone-sdk` fixa o commit do SDK (fonte correspondente). Atualizar o
   submódulo = novo NOTICE/SUBMODULOS no próximo espelho (o script preenche sozinho).
2. **Tag**: `git tag v<versao>-nexus.<n> && git push origin v<versao>-nexus.<n>` → `instaladores-teste.yml` gera
   AppImage/NSIS/MSI/DMG com `-DNEXUS_SOURCE_TAG=<tag>` (link "Código-fonte" desta tag). Em paralelo, o job
   `fonte-gpl` anexa ao Release da tag (em rascunho até o job `release`) o fonte dela:
   `scripts/espelho-fonte.sh --sem-terceiros`, ou seja, `git archive` da tag sem os arquivos internos, com gitleaks e
   a varredura (`nexus-softphone-desktop-src-<tag>.tar.gz` + `SHA256SUMS-fonte.txt`); sem ele o `release` não publica.
   **Atenção:** este repositório é privado, então o Release dele **não** é visível para quem recebe o instalador;
   o anexo é o registro do fonte exato de cada build. Para quem recebe, valem os passos 3 e 4 (espelho público,
   que não torna este repositório público, com o fonte dos submódulos e da Qt).
3. **Espelho**: instalar o `gitleaks` (**obrigatório**); com os submódulos no clone (`git submodule update --init
   --recursive`, precisa do gitlab.linphone.org no ar) e o fonte da Qt da versão do build (`QT_VERSION` do workflow;
   `qt-everywhere-src-<versão>.tar.xz` de https://download.qt.io/archive/qt/), rodar
   `scripts/espelho-fonte.sh --com-submodulos --qt-fonte <qt-everywhere-src-...tar.xz> [--diff-upstream 6.2.3] v<versao>-nexus.<n>`
   → árvore da tag sem histórico e sem arquivos internos, com `LICENSE.txt`, `NOTICE.md`, `TRADEMARKS.md` e
   `SUBMODULOS.md`, mais um `<submódulo>-<commit>.tar.gz` por submódulo (linphone-sdk com os submódulos dele: o
   fonte correspondente do SDK, AGPLv3), a cópia do fonte da Qt (LGPLv3) e `SHA256SUMS`. O repo do espelho fica leve
   (o fonte de terceiros vai no Release). Para conferir sem publicar: `--teste` (pasta `<tag>-TESTE-NAO-PUBLICAR`).
4. **Publicar o espelho** (responsável do release): criar o repo público uma vez; a cada tag, publicar **só a tag**:
   `git -C ../espelho-fonte/<tag>/repo push <url-do-espelho> refs/tags/<tag>`. Nunca `push` nem `--force` na `main`
   do espelho. Criar o GitHub Release da tag no espelho e anexar todos os `.tar.gz`, o fonte da Qt e o `SHA256SUMS`.
   Conferir `https://github.com/VoIPMundoTelecom/nexus-softphone-desktop-src/tree/<tag>`.
5. **Só então distribuir**. O job `release` do workflow recusa publicar a tag se o espelho ainda não responde.

O script remove `scripts/espelho-fonte.excluir` (este arquivo, `docs/NEXUS-DESKTOP.md`, `INSTALAR.md`, `.github/`,
learnings e, em qualquer pasta, `*.pfx *.p12 *.pem *.key .env*`) e falha com as referências de `scripts/espelho-fonte.proibidos`.

O espelho ganha `BUILD-NEXUS.md` (env do workflow com as opções `-D…` e os comandos `cmake` de cada SO, extraídos
da tag — GPLv3 §1), `SUBMODULOS.md` e, com `--diff-upstream`, `CHANGES.diff`; o README troca o cabeçalho do fork
pelo do espelho. Falha também com link em `.md` para arquivo excluído, valor secreto (chave privada, chaves
Google/GitHub/Slack, `/home/`, `sip09`, `nextbilling`, e-mails pessoais), achado do `gitleaks` ou gitleaks ausente.
