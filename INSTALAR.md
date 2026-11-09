# Nexus desktop — como instalar a versão de teste

Instaladores **de teste** para Windows, macOS e Linux, gerados pelo GitHub Actions. A assinatura é **opcional**: o CI
assina Windows e macOS só quando os certificados estão cadastrados como *secrets* (veja [Assinatura](#assinatura)).
O nome do arquivo diz qual é o caso: termina em **`-assinado`** ou **`-nao-assinado`**. Instalador não assinado mostra
um aviso na primeira abertura (passos abaixo). Não são para clientes.

## 1. Baixar

1. GitHub → **Actions** → **Instaladores de teste** → abrir o último run com ✅ → **Artifacts**:

   | Sistema | Artifact (zip) | Arquivo dentro |
   |---|---|---|
   | Windows 10/11 x64 | `Nexus-windows-x64-instalador-<status>` | `Nexus-<versão>-win64-<status>.exe` e `Nexus-<versão>-win64-<status>.msi` |
   | macOS Apple Silicon (M1 em diante) | `Nexus-macos-arm64-dmg-<status>` | `Nexus-<versão>-mac-arm64-<status>.dmg` |
   | macOS Intel | `Nexus-macos-x86_64-dmg-<status>` | `Nexus-<versão>-mac-x86_64-<status>.dmg` |
   | Linux x86_64 | `Nexus-linux-x86_64-AppImage` | `Nexus-<versão>-x86_64.AppImage` |

   `<status>` = `assinado` ou `nao-assinado`. O AppImage do Linux não tem assinatura (confira pelo `SHA256SUMS`).

   No Mac, para saber qual baixar: menu  → **Sobre este Mac**. "Chip Apple M…" = arm64; "Processador Intel" = x86_64.
2. Pela linha de comando:
   ```bash
   gh run list -R VoIPMundoTelecom/nexus-softphone-desktop -w "Instaladores de teste" -L 5
   gh run download <run-id> -R VoIPMundoTelecom/nexus-softphone-desktop -p 'Nexus-macos-arm64-dmg-*'
   ```
3. Os artifacts expiram em 14 dias. Para gerar de novo: **Actions → Instaladores de teste → Run workflow** (dá para
   escolher só um sistema). O workflow também roda a cada push na `main` que não seja só documentação. Uma tag `v*`
   (ex.: `v6.2.3-nexus.1`) gera os 5 instaladores (Linux, Windows `.exe` e `.msi`, 2 macOS) e os publica num **Release**
   da tag, junto com um `SHA256SUMS` (o Release só sai se os 5 forem gerados). Para conferir o download:
   `sha256sum -c --ignore-missing SHA256SUMS` (Linux), `shasum -a 256 -c --ignore-missing SHA256SUMS` (macOS) ou
   `Get-FileHash .\Nexus-*.exe` (PowerShell; comparar com a linha do arquivo no `SHA256SUMS`).
   Minutos do Actions são cobrados: Windows conta 2x e macOS 10x.

## 2. Windows 10/11 (x64)

1. Extrair o zip. Há dois instaladores com o mesmo conteúdo; usar **um só** (os dois instalam em `C:\Program Files\Nexus`):
   - `Nexus-<versão>-win64-<status>.exe`: o instalador normal (assistente com licença, fecha o Nexus aberto antes de instalar).
   - `Nexus-<versão>-win64-<status>.msi`: para TI instalar em massa (GPO, Intune, SCCM) ou sem interação:
     `msiexec /i Nexus-<versão>-win64-<status>.msi /qn` (desinstalar: `msiexec /x <o mesmo .msi> /qn`). Instala para
     todos os usuários (pede administrador). O `.msi` se recusa a instalar numa máquina que já tem o `.exe`:
     desinstalar o `.exe` antes. O `.exe` **não** confere se o `.msi` está instalado: numa máquina com o `.msi`,
     não usar o `.exe` (desinstalar o `.msi` antes).
2. **SmartScreen**:
   - **`-nao-assinado`**: aparece "O Windows protegeu o computador". Clicar em **Mais informações → Executar assim
     mesmo**. Se o navegador bloquear o download, escolher **Manter**.
   - **`-assinado`**: o Windows mostra "Editor verificado" com o nome da empresa do certificado. Com certificado
     OV novo, o SmartScreen **ainda pode avisar** nas primeiras semanas, até o certificado ganhar reputação
     (mesmo caminho: **Mais informações → Executar assim mesmo**, agora com o editor identificado).
3. Avançar até o fim, aceitando a pasta padrão `C:\Program Files\Nexus`. O Nexus instala **ao lado do Linphone**,
   sem misturar contas. Ele fica no menu Iniciar como **Nexus**.
4. Na primeira chamada, se o Firewall do Windows perguntar, **permitir** nas redes privada e pública (sem isso,
   registra mas fica sem áudio).

**Desinstalar**: **Configurações → Aplicativos → Aplicativos instalados → Nexus → Desinstalar**. Para apagar
também contas e histórico: remover a pasta `%LOCALAPPDATA%\nexus`.

## 3. macOS (12.3 Monterey ou mais novo)

1. Abrir o `.dmg` e **arrastar o Nexus para a pasta Aplicativos**. Ejetar o disco.
   - **`-assinado`** (Developer ID + notarizado pela Apple): abre com duplo clique; no máximo o macOS pergunta
     "baixado da Internet, deseja abrir?" → **Abrir**. Pule o passo 2.
2. **Gatekeeper** (**`-nao-assinado`**): o app não é assinado com Developer ID nem notarizado, então um duplo clique mostra
   "O Nexus não pode ser aberto porque a Apple não pode verificar…" (ou "está danificado"). Para liberar, uma vez:
   - **macOS 13/14**: em **Aplicativos**, clicar com o **botão direito** (ou Control+clique) no Nexus → **Abrir** →
     **Abrir** de novo no aviso.
   - **macOS 15 Sequoia ou mais novo** (o botão direito não libera mais): tentar abrir uma vez, fechar o aviso, ir em
     **Ajustes do Sistema → Privacidade e Segurança**, rolar até "O Nexus foi bloqueado…" → **Abrir Mesmo Assim** →
     confirmar com a senha.
   - **Pelo Terminal** (qualquer versão, inclusive quando aparece "está danificado"):
     ```bash
     xattr -dr com.apple.quarantine /Applications/Nexus.app
     ```
3. Na primeira chamada, permitir o **Microfone** (e a **Câmera**, se for usar vídeo). Se negou sem querer:
   **Ajustes do Sistema → Privacidade e Segurança → Microfone → Nexus**.

**Desinstalar**: arrastar `/Applications/Nexus.app` para o Lixo. Para apagar também contas e histórico:
`~/Library/Application Support/nexus` e `~/Library/Preferences/nexus`.

## 4. Linux (x86_64, Ubuntu 22.04+ / Debian 12+ ou equivalente)

```bash
sudo apt install libopengl0 libfuse2      # Ubuntu 24.04+: libfuse2t64 no lugar de libfuse2
chmod +x Nexus-*-x86_64.AppImage
./Nexus-*-x86_64.AppImage
```

- Sem FUSE 2 dá `AppImages require FUSE to run`: instale o pacote acima ou rode
  `./Nexus-*-x86_64.AppImage --appimage-extract-and-run`.
- Na primeira execução, o app grava `~/.local/share/applications/nexus.desktop`. A partir daí ele aparece no menu e
  os links `nexus-config:` abrem nele. Convém guardar o `.AppImage` num lugar fixo (ex.: `~/Aplicativos/`) antes,
  porque o atalho aponta para onde ele estava.

**Desinstalar**: apagar o `.AppImage` e `~/.local/share/applications/nexus.desktop`. Para apagar também contas e
histórico: `~/.local/share/nexus` e `~/.config/nexus`.

## 5. Primeiro uso: configurar o ramal pelo link do givm

O givm gera, por ramal, um **link de configuração** `https://api.givm.com.br/api/prov/<token>` e o **QR** desse
link. O link é de **uso único** e vale **48 h**.

1. No givm, gerar o link do ramal: na Área do Cliente (app.grupoivm.com.br → PABX virtual → ramal) ou no gestor
   (detalhe do cliente → ramal → QR do softphone). **Copie o link** (texto). O Nexus desktop **não lê QR pela
   câmera**: o QR é para o app de celular. Se só tiver o QR, leia com o celular e mande o link para o computador.
2. Abrir o Nexus → na tela de boas-vindas, **Configurar pelo link do Nexus** → colar o link → **Validar** →
   confirmar. A conta entra e registra (bolinha verde).
   - Já configurado? **Configurações → Avançado → Provisionamento remoto** → colar → **Baixar e aplicar**.
     Um segundo link **substitui** a conta anterior.
   - Pela linha de comando (também pede confirmação):
     - Windows: `"C:\Program Files\Nexus\bin\nexus.exe" --fetch-config=<link>`
     - macOS: `/Applications/Nexus.app/Contents/MacOS/nexus --fetch-config=<link>`
     - Linux: `./Nexus-*-x86_64.AppImage --fetch-config=<link>`
   - Link `nexus-config:<link>` (o botão "Abrir no app Nexus" da página do givm, quando disponível) abre direto no app
     instalado e pede confirmação.
   - Só links do givm (`https://api.givm.com.br/api/prov/...` ou `app.givm.com.br`) são aceitos; qualquer outro
     endereço é recusado com aviso.
3. **Não abra o link `https://` no navegador antes**: isso pode consumir o token. Se aparecer "link inválido ou
   expirado", gere outro no givm.

## 6. Logs (para o suporte)

1. No app: **Ajuda → Solucionando problemas** → ligar **Habilitar rastreamentos de depuração** → reproduzir o problema
   → **Compartilhar rastreamentos de depuração** (copia um link ou abre o e-mail).
2. Os arquivos ficam em:

   | Sistema | Pasta dos logs |
   |---|---|
   | Windows | `%LOCALAPPDATA%\nexus\logs\` |
   | macOS | `~/Library/Application Support/nexus/logs/` |
   | Linux | `~/.local/share/nexus/logs/` |

   A configuração (`linphonerc`) fica em `%LOCALAPPDATA%\nexus\` (Windows), `~/Library/Preferences/nexus/` (macOS) e
   `~/.config/nexus/` (Linux).

## Problemas comuns

| Sintoma | O que fazer |
|---|---|
| Windows: "O Windows protegeu o computador" | **Mais informações → Executar assim mesmo** (normal no `-nao-assinado`; no `-assinado`, só enquanto o certificado não tem reputação) |
| macOS: "não pode ser aberto" / "está danificado" | Seção 3, passo 2 (botão direito → Abrir, Privacidade e Segurança, ou `xattr`) |
| macOS: abre e fecha logo em seguida | Conferir se baixou a arquitetura certa (arm64 × x86_64) e rodar `/Applications/Nexus.app/Contents/MacOS/nexus` no Terminal para ver o erro |
| Linux: `error while loading shared libraries: libOpenGL.so.0` | `sudo apt install libopengl0` |
| Linux: `AppImages require FUSE to run` | `sudo apt install libfuse2` (ou `libfuse2t64`), ou `--appimage-extract-and-run` |
| "Link inválido ou expirado" | O token já foi usado ou passou de 48 h: gere outro no givm |
| Registra, mas sem áudio | Liberar o Nexus no firewall (Windows) / permitir o microfone (macOS) |

## Assinatura

O mesmo workflow gera os dois casos. **Sem secrets, nada falha**: o passo de assinatura é pulado e o arquivo sai
como `-nao-assinado`. Os secrets ficam em **Settings → Secrets and variables → Actions** do repositório (nunca no
código; o GitHub mascara o valor nos logs).

| Sistema | Sem secrets | Secrets para assinar | Resultado |
|---|---|---|---|
| Windows | Sem assinatura (`-nao-assinado`) | `WINDOWS_CERT_PFX` (o `.pfx` com certificado + chave, em base64) e `WINDOWS_CERT_PASSWORD` | `signtool sign /fd SHA256 /tr <carimbo de tempo>` nos instaladores `.exe` e `.msi` (`-assinado`) |
| macOS | Assinatura **ad-hoc** (`-nao-assinado`) | `APPLE_CERTIFICATE_P12` (certificado **Developer ID Application** `.p12` em base64), `APPLE_CERTIFICATE_PASSWORD`, `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD` (senha de app de appleid.apple.com). Opcional: `APPLE_SIGNING_IDENTITY` (sem ele, usa o "Developer ID Application" do `.p12`) | `.app` assinado (hardened runtime) + `.dmg` notarizado (`notarytool`) e grampeado (`stapler`) (`-assinado`) |
| Linux | AppImage sem assinatura | — | Conferir pelo `SHA256SUMS` do Release |

No macOS, `-assinado` exige **notarização**: só o certificado (sem `APPLE_ID`/`APPLE_TEAM_ID`/`APPLE_APP_PASSWORD`)
assina o app, mas o Gatekeeper continua bloqueando, então o arquivo sai como `-nao-assinado`.

Gerar os valores base64 (sem quebra de linha):

```bash
base64 -w0 certificado.pfx      # Linux  (macOS: base64 -i certificado.p12 | tr -d '\n')
gh secret set WINDOWS_CERT_PFX -R VoIPMundoTelecom/nexus-softphone-desktop < <(base64 -w0 certificado.pfx)
gh secret set WINDOWS_CERT_PASSWORD -R VoIPMundoTelecom/nexus-softphone-desktop   # pede o valor sem ecoar
```

**Windows: por que `.pfx` e não Azure Trusted Signing.** O `.pfx` é o caminho mais simples no CI (2 secrets, só
`signtool`, sem conta de nuvem). A ressalva: desde junho/2023 as autoridades certificadoras emitem certificados de
*code signing* OV/EV **só em token/HSM ou serviço de nuvem**, sem chave exportável para `.pfx`. Então o `.pfx` serve
para um certificado que já exista em arquivo. Se o certificado comprado vier em nuvem, o passo `Assinar instalador`
do workflow precisa trocar o `signtool /f` pela ferramenta do fornecedor (Azure Trusted Signing tem a action
`azure/trusted-signing-action`; DigiCert KeyLocker e SSL.com eSigner têm as suas). A lógica condicional e o
rótulo `-assinado`/`-nao-assinado` continuam iguais. Só os instaladores `.exe` e `.msi` são assinados (são eles que o
SmartScreen avalia no download); os `.exe`/`.dll` de dentro não.

Detalhes técnicos (build, o que mudou do Linphone, pendências) estão em [`docs/NEXUS-DESKTOP.md`](docs/NEXUS-DESKTOP.md).
