# Espelho das dependências do desktop (gitlab.linphone.org)

- O gitlab.linphone.org bloqueia IP de datacenter (GitHub Actions, brs; 26/10); da rede do escritório funciona.
- A CI baixa os submódulos de `github.com/VoIPMundoTelecom/lp-<nome>` (privados), via `url.insteadOf` global no runner (`scripts/ci-submodulos.sh`); o `.gitmodules` continua com as URLs do gitlab.
- Nome no espelho = último componente da URL sem `.git`, com `lp-`: `BC/public/external/gn.git` → `lp-gn`, `BC/public/linphone-sdk.git` → `lp-linphone-sdk`, `../bcg729.git` → `lp-bcg729`.
- Os dois `insteadOf` (`.../BC/public/external/` e `.../BC/public/`) apontam para o mesmo `lp-`; o git usa o prefixo mais longo, então a ordem não importa.
- Secret `LP_MIRROR_TOKEN`: token só de leitura, com escopo nos `lp-*`; vai no `insteadOf` com `::add-mask::`, sai da config global no fim do passo e nunca entra no cache de `.git/modules` (que guarda a URL original).
- Subiu o linphone-sdk (ou gn/crashpad/depot-tools)? Antes do merge, da rede do escritório: `scripts/espelhar-deps.sh` (ou `--mirrors <dir>` com clones `--mirror` já feitos). O workflow "Espelho das dependências" falha se algum commit fixado faltar.
- A origem do espelho tem de ter o histórico inteiro: clone raso (`--depth`, como o `.git/modules` da CI e o clone do mkt) não serve — o GitHub recusa o push (`index-pack failed`).
- Sem o gitlab, só 8 dos 33 têm o commit fixado em outro lugar com histórico (linphone-sdk, bcg729, bcmatroska2, opencore-amr no GitHub da Belledonne; gn, crashpad, depot-tools, decaf no upstream). Os outros 25 externos são commits do fork da Belledonne.
- Envia `refs/heads/*`, `refs/tags/*` e uma tag `nexus-pin/<commit>` por commit fixado (não os `refs/merge-requests/*`/`keep-around` do GitLab).
- crashpad/gn/depot-tools não têm submódulos git; o crashpad puxa as dependências dele por `DEPS` (gclient, chromium.googlesource.com) só com `ENABLE_CRASH_HANDLER` no Windows — desligado na CI.
- O script roda no bash 3.2 do macOS (sem array associativo).
