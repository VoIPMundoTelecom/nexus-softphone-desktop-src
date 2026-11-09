# NOTICE — Nexus desktop

**Nexus desktop** é um fork do **Linphone desktop** (https://github.com/BelledonneCommunications/linphone-desktop),
© 2010-2026 **Belledonne Communications SARL**, distribuído sob a **GNU General Public License versão 3**
(`LICENSE.txt`). As modificações — © 2026 IVM Telecomunicação (CNPJ 10.285.037/0001-67) — também são GPLv3.

## Componentes e licenças

| Componente | Versão usada | Licença | Fonte correspondente |
|---|---|---|---|
| Linphone desktop (base do app) | tag/commit do upstream na história deste fork | GPLv3 | https://github.com/BelledonneCommunications/linphone-desktop |
| linphone-sdk (`external/linphone-sdk`: liblinphone, mediastreamer2, belle-sip, oRTP, bctoolbox, bzrtp, lime… e as bibliotecas externas do SDK), compilado junto com o app | commit `@SDK_COMMIT@` | **AGPLv3** (LICENSE do repositório linphone-sdk nesse commit) | `SUBMODULOS.md` e https://github.com/BelledonneCommunications/linphone-sdk/tree/@SDK_COMMIT@ (espelho de https://gitlab.linphone.org/BC/public/linphone-sdk) |
| crashpad, gn, depot_tools (`external/google`) | ver `SUBMODULOS.md` | BSD | não entram nos instaladores do Nexus (crash handler desligado) |
| Qt | a do instalador (`QT_VERSION` do build) | LGPLv3 | https://download.qt.io/ |
| Bibliotecas de terceiros do SDK | — | ver Ajuda → licenças e os arquivos de licença de cada componente | dentro do linphone-sdk |

O SDK é **AGPLv3**: o app (GPLv3) é combinado com ele conforme a seção 13 da GPLv3. Não modificamos o SDK; ele é
compilado sem alteração a partir do commit acima. @SUBMODULOS_INCLUIDOS@

## Mudanças da IVM Telecomunicação (até @DATA@, tag `@TAG@`)

| Desde | Mudança |
|---|---|
| 2026-10 | Marca **Nexus** (nome, ícones, `APPLICATION_ID br.com.grupoivm.nexus`, esquemas `nexus-config:`/`sip-nexus:`), instaladores AppImage/NSIS/MSI/DMG |
| 2026-10 | Provisionamento remoto **só do givm** (`https://api.givm.com.br/api/prov/<token>`), sempre com confirmação, token mascarado nos logs |
| 2026-10 | Sem servidores da Belledonne (login linphone.org, FlexiAPI, envio de logs, arquivos, checagem de versão) |
| 2026-10 | Item "Código-fonte" da Ajuda aponta para este espelho na versão instalada |

@DIFF@

## Marcas

Os nomes e logotipos **Ondafone**, **Voipmundo**, **i9c** e **Nexus** (nome anterior do app), marcas da IVM Telecomunicação (CNPJ 10.285.037/0001-67), **não** são licenciados pela GPL: veja `TRADEMARKS.md`.

## Esta versão

Gerado a partir da tag `@TAG@` (commit `@COMMIT@`).
