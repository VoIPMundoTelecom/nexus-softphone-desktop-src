# GPL do Nexus desktop — espelho do fonte (26/10)

- **Licenças conferidas**: app (fork do linphone-desktop) = **GPLv3** (`LICENSE.txt`; GitHub `GPL-3.0`).
  `external/linphone-sdk` no commit `9168d5041bc6817bb6c4bccf2993c79de8d4a28c` = **AGPLv3** (`LICENSE.txt` do
  repositório nesse commit; GitHub `linphone-sdk` e `liblinphone` = `AGPL-3.0`). O POM Maven do SDK Android ainda
  diz GPLv3 — não usar como fonte.
- GPLv3 §13 permite combinar com AGPLv3. O desktop **compila** o SDK a partir do submódulo, então o fonte
  correspondente inclui o SDK: `--com-submodulos` gera um `<submódulo>-<commit>.tar.gz` de cada submódulo
  (recursivo) para o **Release da tag** — o repo do espelho fica leve, sem os submódulos dentro; só o link para o
  upstream fica como provisório (o gitlab.linphone.org ficou fora do ar de 23/10 em diante — dependência externa).
- crashpad/gn/depot_tools (`external/google`) não entram nos instaladores (crash handler desligado no CI).
- O link "Código-fonte" vem de `NEXUS_SOURCE_TAG` (cache do CMake → `config.h` → `Constants::SourceCodeUrl`).
- Limpeza da saída parcial: `trap ... EXIT` com `ok=1` no fim; `trap ERR` não dispara nos `exit 1` explícitos. CI `espelho-teste.yml` roda `--teste` e confere que a pasta some na falha.
