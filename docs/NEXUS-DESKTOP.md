# Nexus desktop: fork do Linphone Desktop (P5 fase 3)

Softphone do Grupo IVM / Voipmundo para **Windows, macOS e Linux**, substituto do Zoiper. Base: `linphone-desktop`
(Belledonne Communications, GPLv3), branch **`release/6.2`** (última estável; tag 6.2.3 + correções). O CI gera os
três instaladores; a assinatura/notarização Apple só liga quando os secrets `APPLE_*` existirem.

Repositório **privado** `VoIPMundoTelecom/nexus-softphone-desktop`. O `LICENSE.txt` do upstream não foi alterado.

## Remotes, branches e submódulos

- `origin` = `VoIPMundoTelecom/nexus-softphone-desktop`, `upstream` = `BelledonneCommunications/linphone-desktop`.
- `main` = `upstream/release/6.2` **sem mudanças** (tags 6.2.x também foram enviadas, porque o build usa `git describe`).
  O trabalho do Nexus fica na branch `p5-fase3-desktop`, que vai para a `main` via PR.
- Para atualizar: `git fetch upstream && git checkout main && git merge upstream/release/6.2 && git push`,
  depois `git merge main` na branch do Nexus.
- **Submódulos** (`.gitmodules` sem mudança): `external/linphone-sdk` e `external/google/{gn,crashpad,chromium-depot-tools}`
  continuam apontando para o **gitlab.linphone.org público** (os submódulos internos do SDK também). Não há espelho
  nosso. Clonar com `git clone --recursive` ou `git submodule update --init --recursive`. Se um dia o gitlab da
  Belledonne ficar fora do ar ou fechar, é preciso espelhar esses repositórios e trocar as URLs no `.gitmodules`.

## O que muda em relação ao Linphone

| Item | Onde |
|---|---|
| Nome do app **Nexus**, executável **`nexus`** (`nexus.exe`), instalador `Nexus-<versão>-win64.exe` / `Nexus-<versão>-x86_64.AppImage` | `CMakeLists.txt` (`LINPHONEAPP_APPLICATION_NAME`/`LINPHONEAPP_EXECUTABLE_NAME`) |
| Identificadores próprios: `APPLICATION_ID=br.com.grupoivm.nexus` (o mesmo do Android), fornecedor "Grupo IVM", pastas de dados `.../nexus` (QStandardPaths usa o nome do executável), chaves de registro `HKLM\SOFTWARE\Grupo IVM\Nexus`, esquemas `nexus-config:`, `sip-nexus:`, `nexus-sip:`. **Pode ser instalado ao lado do Linphone**, sem dividir configuração nem contas | `Linphone/application_info.cmake`, `cmake/install/windows/*.nsi.in` (parametrizados) |
| Ícones: hexagrama Nexus (de `LogoIvmNexus.tsx`) branco sobre azul IVM `#1E2C5C`, nó e centro laranja `#E8672A`, igual ao ícone Android. `.ico` (Windows/instalador), PNGs hicolor 16 a 1024 (Linux/AppImage), `logo.svg` (ícone da janela/bandeja), splash e logo das boas-vindas. Gerados por `nexus/gerar_icones.py` (Pillow) | `Linphone/data/icon.ico`, `Linphone/data/icon/hicolor/`, `Linphone/data/image/{logo,logo_margins,splashscreen-logo,linphone}.svg` |
| Cor principal = laranja Nexus `#E8672A` (tema padrão "orange" do QML, tons 100 a 700 derivados) e ilustrações recoloridas de `#FE5E00` para `#E8672A` | `Linphone/view/Style/Themes.qml`, `Linphone/data/image/*.svg` |
| **Português (Brasil)** habilitado (o upstream tem `pt_BR.ts` completo, mas só compilava en/fr/de); textos de marca "Linphone" → "Nexus" em pt_BR/en | `Linphone/CMakeLists.txt`, `Linphone/data/languages/{pt_BR,en}.ts` |
| Botão **"Configurar pelo link do Nexus"** no topo do assistente | `Linphone/view/Page/Form/Login/LoginPage.qml` |
| Sem criação de conta sip.linphone.org no assistente | `Linphone/data/config/linphonerc-factory` (`[ui] assistant_hide_create_account=1`) |
| Sem verificação de atualização em download.linphone.org (`ENABLE_UPDATE_CHECK=OFF` por padrão) | `CMakeLists.txt` |
| Sem servidores da Belledonne: upload de traces e transferência de arquivos `*.linphone.org` apagados a cada start (o upstream gravava `files.linphone.org`), sem e-mail da Belledonne para traces | `Linphone/model/core/CoreModel.cpp`, `Linphone/core/setting/SettingsCore.cpp`, `Linphone/tool/Constants.hpp` |
| Assistente sem login de conta sip.linphone.org (formulário oculto) e sem a tela de aviso "conta Linphone / contato comercial" antes do formulário de conta SIP | `Linphone/view/Page/Form/Login/{LoginPage,SIPLoginPage}.qml` |
| Sem FlexiAPI da Belledonne: `[account_creator] url` vazio (era `subscribe.linphone.org`); a lista "Vos appareils" da conta não faz requisição sem URL (antes mandava o endereço da conta para a Belledonne ao abrir as configurações da conta). `version_check_url_root`/`account_creator url` `*.linphone.org` gravados por builds anteriores são apagados a cada start | `Linphone/data/config/linphonerc-factory`, `Linphone/core/account/AccountDeviceList.cpp`, `Linphone/model/core/CoreModel.cpp` |
| Links para o usuário: privacidade e contato = site da Voipmundo (`voipmundo.com.br/politica-de-privacidade/`, `/contato/`, páginas que existem no `voipmundo-site`); termos, tradução (weblate), recuperação de senha, download e verificação de versão = vazios (o item some). Botão "verificar atualização" da Ajuda some sem `ENABLE_UPDATE_CHECK` | `Linphone/tool/Constants.hpp`, `Linphone/view/Page/Main/Help/HelpPage.qml` |
| **GPLv3**: item **"Código-fonte"** (Ajuda e "Sobre" do assistente) com o link do repositório (`Constants::SourceCodeUrl`); licença e crédito "Baseado no Linphone © Belledonne Communications" mantidos | `Linphone/tool/Constants.hpp`, `HelpPage.qml`, `Linphone/view/Page/Layout/Login/LoginLayout.qml` |
| CI que gera os instaladores de teste | `.github/workflows/instaladores-teste.yml` |

Créditos, licença (GPLv3, © Belledonne), tela "Sobre"/licenças de terceiros e a ilustração de montanhas do rodapé
continuam do Linphone (a ilustração só foi recolorida).

## Configuração pelo link do givm (provisionamento remoto)

O givm (`app/routers/provisionamento.py` no backend) gera por ramal uma URL `<base>/api/prov/<token>` (ou
`PROV_BASE_URL/<token>`) e o QR dessa URL. `GET` nela devolve o XML lpconfig da conta SIP. **Uso único, expira**
(48 h); token inválido, usado ou expirado = 404. O app desktop **já aceita esse formato sem mudança no givm**: toda
entrada abaixo termina em `CoreModel::setFetchConfig()` → `linphone::Core::setProvisioningUri(url)` → reinício do
Core, que baixa e aplica o XML.

Caminhos, do mais simples ao mais técnico:

1. **Assistente (primeira abertura)**: *Configurar pelo link do Nexus* → colar o link
   `https://.../api/prov/<token>` → *Validar* → confirmar "Deseja baixar e aplicar o provisionamento remoto deste
   endereço?" → a conta entra e registra. (O botão antigo *Provisionamento remoto*, abaixo, faz o mesmo.)
2. **Já configurado**: *Configurações → Avançado → Provisionamento remoto* → colar o link → *Baixar e aplicar*.
3. **Linha de comando** (pede confirmação, como as outras entradas):
   - Windows: `"C:\Program Files\Nexus\bin\nexus.exe" --fetch-config=https://api.givm.com.br/api/prov/<token>`
   - Linux: `./Nexus-<versão>-x86_64.AppImage --fetch-config=https://api.givm.com.br/api/prov/<token>`
   Se o Nexus já estiver aberto, a segunda instância repassa o comando para a primeira.
4. **URL handler** `nexus-config:` (registrado pelo instalador Windows em `HKCR\nexus-config`; no Linux, pelo
   `.desktop` que o app grava em `~/.local/share/applications/nexus.desktop` na primeira execução):
   `nexus-config:https://api.givm.com.br/api/prov/<token>` (sem esquema, assume `https`). Pede confirmação.
   É o link que a página do givm para navegador (ver abaixo) oferece.

**Só o givm configura conta** (revisão 26/10): todas as entradas acima (assistente, Configurações, `--fetch-config`,
`nexus-config:`, `sip-nexus:?nexus-fetch-config=`) passam por `CoreModel::useFetchConfig`, que aceita só
`https://api.givm.com.br/api/prov/<token>` ou `https://app.givm.com.br/api/prov/<token>` (host exato, sem http, sem
usuário@, porta, query ou fragmento — `Linphone/tool/NexusProvisioning.hpp`, mesma regra do Android) e **sempre**
pede confirmação mostrando o endereço. Qualquer outro servidor, `http://` ou arquivo local é recusado com o aviso
"Só o link de configuração do givm (Voipmundo) pode configurar conta no Nexus". O token sai mascarado nos logs
(`/api/prov/q5mJ…`).

Cuidados:

- **Uso único**: o token vale uma vez (48 h). Antes, abrir o link `https://` no **navegador** consumia o token e o
  app não conseguia mais usar. **Resolvido no givm** (back #93 + #106): só o `User-Agent` do app consome o token
  (regex `linphone|belle-sip|liblinphone|nexus-app`); navegador e prévias de link recebem uma página com o botão
  **"Abrir no app Nexus"** (`nexus-config:<link>`) sem gastar o token. O desktop é compatível: o handler `nexus-config:`
  está registrado (item 4; NSIS e MSI) e o `User-Agent` ao baixar a configuração é
  `Nexus-Desktop/<versão> (...) Qt/<versão> LinphoneSDK`, que bate em `linphone`. Não mudar esse formato sem ajustar o givm.
- O XML do givm escreve a conta em `proxy_0`: um segundo link sobrescreve a primeira conta.
- O XML do givm deixa `misc/config-uri` vazio, então o app não tenta baixar de novo a cada abertura (o token
  já estaria queimado).

## Build

### CI (é assim que os instaladores de teste saem)

`.github/workflows/instaladores-teste.yml`, disparado por push na `main`, por tag `v*` (que também cria um
Release em rascunho com os instaladores) ou manualmente (*Actions → Instaladores de teste → Run workflow*, com
escolha de plataforma). Linux/Windows sem segredos; macOS usa os `APPLE_*` opcionais.

- **Linux** (`ubuntu-22.04`): Qt 6.10.0 (aqtinstall), dependências apt, `cmake -G Ninja ... --target install`
  com `ENABLE_APP_PACKAGING=YES`, que roda o `linuxdeploy` + plugin Qt → artifact **`Nexus-linux-x86_64-AppImage`**.
- **Windows** (`windows-2022`): MSVC 2022 x64, Qt 6.10.0 msvc2022_64, ferramentas do MSYS2 (doxygen, yasm, nasm,
  perl, nsis...), mesmo cmake → CPack **NSIS** (`.exe`, o formato do upstream) e, com `-DENABLE_APP_MSI=YES`
  (só no CI), também CPack **WiX 3** (`.msi`, para GPO/Intune/`msiexec /qn`) → artifact
  **`Nexus-windows-x64-instalador`** com os dois. O MSI grava os mesmos registros do NSIS (esquemas `nexus-config:`,
  `sip:`... e `Capabilities`; `cmake/install/windows/wix_patch.xml.in`) e tem `UpgradeCode` fixo em
  `cmake/install/install.cmake` (**não mudar**: é o que faz um MSI novo atualizar o instalado).
  Revisão 27/10: instala para a máquina (`CPACK_WIX_INSTALL_SCOPE perMachine`, CMake ≥ 3.29); versão do MSI com o
  N da tag no 3º campo (`v6.2.3-nexus.2` → `6.2.302`; o Windows Installer ignora o 4º campo, então sem isso
  `nexus.1` → `nexus.2` não atualiza; N < 100; `cpack_config.cmake.in`, só o gerador WIX); `sip/sips/tel/callto`:
  a chave `HKCR\<esquema>` é permanente e não sobrescreve a de outro app (desinstalar o Nexus não quebra outro
  softphone), e o que é do Nexus (ProgID `Nexus.<esquema>` + `URLAssociations`) sai na desinstalação; o MSI se recusa
  a instalar se o `.exe` (NSIS) estiver instalado (chave `Uninstall\Nexus`).
- **macOS** (matriz): `arm64` no `macos-latest` (Apple Silicon) e `x86_64` no `macos-15-intel`, cada um nativo
  (`-DLINPHONEAPP_MACOS_ARCHS=<arch>`; universal num runner só compilaria o SDK duas vezes em série). Homebrew
  (ninja, ccache, yasm, nasm, doxygen), Qt 6.10.0 `clang_64`, mesmo cmake → `TasksMacos.cmake` +
  `GenerateAppMacos.cmake` → `Nexus.app` + `.dmg` (CPack DragNDrop) → artifacts **`Nexus-macos-arm64-dmg`** e
  **`Nexus-macos-x86_64-dmg`** (`Nexus-<versão>-mac-<arch>.dmg`; nomes de artifact e arquivo ganham o sufixo
  `-assinado`/`-nao-assinado`). Sem secrets o `.app` sai com assinatura ad-hoc
  (`codesign --sign -`); com `APPLE_CERTIFICATE_P12` (base64), `APPLE_CERTIFICATE_PASSWORD` e
  `APPLE_SIGNING_IDENTITY` (opcional; sem ele usa o "Developer ID Application" do `.p12`) ele é assinado com Developer ID (`LINPHONE_BUILDER_SIGNING_IDENTITY`, hardened runtime +
  `cmake/install/macos/entitlements.xml.in`); com `APPLE_ID`, `APPLE_TEAM_ID` e `APPLE_APP_PASSWORD` o `.dmg` também
  é notarizado (`notarytool --wait`) e grampeado (`stapler`).
- **SDK**: a Belledonne **não publica SDK pré-compilado com o wrapper C++** (`liblinphone++`, que o app usa). O zip
  público `linphone-sdk-win64-*.zip` traz só o wrapper C#, e não há zip Linux. Por isso o `linphone-sdk` é compilado
  do submódulo, junto com o app. **Cache**: ccache por SO (chave = `.gitmodules` + commit, com restore da última),
  cache do Qt e cache dos **submódulos** (`.git/modules`, um por SO, chave = commits fixados em `external/`): com ele o
  build não depende do `gitlab.linphone.org` (fora do ar por horas em 23–24/10). Só vai à rede quando o commit
  do `linphone-sdk` muda (ou o cache expira, 7 dias sem uso). A primeira compilação de cada SO leva de 1 a 2 h; as seguintes, bem menos.
- Opções desligadas para encurtar e para não falar com servidores da Belledonne: `ENABLE_CRASH_HANDLER=NO`
  (crashpad/gn + upload para o Bugsplat deles), `ENABLE_UPDATE_CHECK=NO`, `ENABLE_AV1=NO` (aom/dav1d), docs e
  testes. Vídeo (VP8), SRTP/ZRTP, LIME e LDAP seguem como no upstream.
- Repositório privado: os minutos do Actions são cobrados da conta (Windows conta 2x). Rodar só quando precisar.

### Local

Ver o README do upstream (abaixo do aviso de fork): Qt ≥ 6.10 com `qtnetworkauth qtshadertools`, dependências do
linphone-sdk (cmake ≥ 3.22, python3 + pystache + six, yasm, nasm, doxygen; no Windows, VS 2022 + MSYS2), e:

```bash
git clone --recursive https://github.com/VoIPMundoTelecom/nexus-softphone-desktop.git
cd nexus-softphone-desktop
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_APP_PACKAGING=YES \
  -DENABLE_CRASH_HANDLER=NO -DENABLE_AV1=NO
cmake --build build --target install --parallel
# instaladores em build/OUTPUT/Packages/
```

## Baixar e instalar os artefatos de teste

1. GitHub → *Actions* → *Instaladores de teste* → run verde → *Artifacts*, ou:
   `gh run download <run-id> -R VoIPMundoTelecom/nexus-softphone-desktop -n Nexus-linux-x86_64-AppImage`
   (ou `-n Nexus-windows-x64-instalador`). Cada artifact vem em `.zip`; os artifacts expiram em 14 dias.
   Se o `gh run download` recusar com "would result in path traversal", baixe pela página do run ou com
   `gh api repos/VoIPMundoTelecom/nexus-softphone-desktop/actions/artifacts/<artifact-id>/zip > nexus.zip`.
2. **Linux**: `chmod +x Nexus-*.AppImage && ./Nexus-*.AppImage`. Se o sistema não tiver FUSE 2 (`libfuse2`/
   `libfuse2t64`), instale-o ou rode com `--appimage-extract-and-run`. Requer glibc ≥ 2.35 (Ubuntu 22.04+,
   Debian 12+) e a **`libOpenGL.so.0`** do sistema (pacote `libopengl0`; o linuxdeploy não empacota bibliotecas
   de GL de propósito). Sem ela: "error while loading shared libraries: libOpenGL.so.0" → `sudo apt install libopengl0`.
3. **Windows**: executar `Nexus-*-win64.exe`. Como o instalador **não é assinado**, o SmartScreen avisa
   "O Windows protegeu o computador" → *Mais informações* → *Executar assim mesmo*. Requer Windows 10/11 x64.
4. **macOS**: abrir o `.dmg` da arquitetura certa, arrastar para Aplicativos. Sem notarização, o Gatekeeper bloqueia:
   botão direito → *Abrir* (macOS 13/14), *Ajustes → Privacidade e Segurança → Abrir Mesmo Assim* (15+) ou
   `xattr -dr com.apple.quarantine /Applications/Nexus.app`.
5. Abrir o Nexus → *Configurar pelo link do Nexus* → colar o link gerado no givm. Passo a passo completo, logs e
   desinstalação: [`INSTALAR.md`](../INSTALAR.md).

## Pendências

- 🔴 **Fabricio: assinatura Windows**. Sem ela, SmartScreen avisa em toda instalação. Precisa de um certificado
  de **code signing** (OV ou, melhor, EV, que tira o aviso logo de cara; hoje os dois são emitidos em token/HSM ou
  serviço de nuvem, como Azure Trusted Signing, DigiCert KeyLocker ou SSL.com eSigner). O build já tem os ganchos:
  `-DLINPHONE_WINDOWS_SIGN_TOOL=<signtool> -DLINPHONE_WINDOWS_SIGN_TIMESTAMP_URL=<url> -DLINPHONE_WINDOWS_SIGN_HASH=<sha1>`
  (ver `cmake/install/install.cmake` e `cmake/install/sign_package.bat`), ou um passo `signtool sign` depois do
  CPack no CI, com o certificado/credenciais em *secrets* do Actions. Decidir fornecedor e cadastrar os secrets.
  **CI pronto para `.pfx`**: com `WINDOWS_CERT_PFX` + `WINDOWS_CERT_PASSWORD` o job Windows roda `signtool sign`
  no instalador (ver `INSTALAR.md` → Assinatura); certificado só em nuvem/HSM exige trocar esse passo.
- 🔴 **macOS: assinatura/notarização Apple** (conta Apple Developer, US$ 99/ano). O CI já está pronto: basta
  cadastrar os secrets `APPLE_*` (ver Build → CI). Sem eles, o Gatekeeper avisa (ver `INSTALAR.md`).
- AppImage sem assinatura GPG (opcional; o upstream assina com `LINPHONE_BUILDER_SIGNING_IDENTITY`).
- **Páginas do Nexus**: privacidade e contato usam as páginas gerais do site da Voipmundo. Quando houver página
  própria do app (privacidade do Nexus, termos), trocar `PrivatePolicyUrl`/`CguUrl` em `Linphone/tool/Constants.hpp`.
- **Código-fonte (GPLv3)**: o item "Código-fonte" aponta para `github.com/VoIPMundoTelecom/nexus-softphone-desktop`,
  que é **privado** (quem não é da organização vê 404). Antes de distribuir fora do Grupo IVM: tornar o repositório
  público (ou um espelho público só do fonte) ou trocar `SourceCodeUrl` por uma página com a oferta escrita.
- Restos conhecidos de `linphone.org` que **não** são contatados no uso normal: modelos `data/assistant/*-app-sip-account.rc`
  (só para contas sip.linphone.org, cujo login está oculto); `RegisterPage.qml` (cadastro oculto); no Windows, o download
  opcional do codec H264 da Cisco baixa o `bzip2.exe` de `download.linphone.org` (`Constants::LinphoneBZip2_*`, ou
  `[misc] bzip_download_path` no config para outro servidor) — só acontece se o usuário pedir o H264; os comentários de
  licença dos fontes ("see https://www.linphone.org") ficam (créditos do upstream).
- Envio de traces (Configurações → Depuração → Partilhar): sem servidor próprio, mostra o erro com a pasta das traces
  para anexar à mão. Quando houver servidor/e-mail do suporte: `[misc] log_collection_upload_server_url` e
  `[ui] logs_email` no `linphonerc-factory`.
- Sem push: o desktop só recebe chamadas com o app aberto (ou minimizado na bandeja), o que é o normal no desktop.
- **GPLv3**: ao distribuir os instaladores fora do Grupo IVM, é obrigatório oferecer o código-fonte correspondente
  (repositório público ou oferta escrita), incluindo este fork.


## GPL — espelho público do fonte (26/10)

Cada versão distribuída tem o fonte publicado em `VoIPMundoTelecom/nexus-softphone-desktop-src`, gerado por
`scripts/espelho-fonte.sh [--com-submodulos] <tag>` (árvore da tag, sem histórico nem docs internos, com
`LICENSE.txt`, `NOTICE.md`, `TRADEMARKS.md`, `SUBMODULOS.md`). O item "Código-fonte" abre o espelho na tag instalada
(`-DNEXUS_SOURCE_TAG`). Licenças: app GPLv3, linphone-sdk **AGPLv3**. Checklist: `docs/release-gpl.md`.
`[bloqueado: Fabricio]` criar o repo público e publicar o primeiro espelho.
