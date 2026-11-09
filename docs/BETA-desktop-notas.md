# Notas do beta desktop do Ondafone (07/11/2026)

Instaladores **de teste** do beta fechado. Nada foi publicado: os arquivos ficam só nos artefatos do run do GitHub
Actions (precisa de acesso ao repositório) e são entregues aos testadores pela equipe. O roteiro de testes é o mesmo
do Android (`docs/BETA-ondafone.md` no repositório Android).

## Build

| | |
|---|---|
| Versão | `6.2.3-82-g32616c85` (main em `32616c85`, merge do #18) |
| Workflow | Instaladores (SDK pré-compilado), disparo manual com `ondafone` marcado (liga `ENABLE_ONDAFONE_MODOS` e `ENABLE_ONDAFONE_CONTA`) |
| Run | [42378517531](https://github.com/VoIPMundoTelecom/nexus-softphone-desktop/actions/runs/42378517531) (07/11) |
| Artefatos disponíveis até | 21/11/2026 (retenção do Actions) |
| Assinatura | **nenhuma** (sem certificados cadastrados) |

## Arquivos

| Plataforma | Artefato | Arquivo | Tamanho | SHA-256 |
|---|---|---|---|---|
| Windows x64 (instalador) | `Nexus-windows-x64-instalador-nao-assinado-ondafone` | `Nexus-6.2.3-82-g32616c85-win64-nao-assinado.exe` | 85 MB | `ed6ad4c8caf7230cf4726c10ca2c60c709ef8507c796ff1a0c4795424c60e81d` |
| Windows x64 (MSI, implantação em massa) | (mesmo artefato) | `Nexus-6.2.3-82-g32616c85-win64-nao-assinado.msi` | 108 MB | `3f5687cf66cec91b71a9aaf4a83485bdcaf53961d2d1687c6a6b7badd1e004d6` |
| macOS Apple Silicon (M1 ou mais novo) | `Nexus-macos-arm64-dmg-nao-assinado-ondafone` | `Nexus-6.2.3-82-g32616c85-mac-arm64-nao-assinado.dmg` | 185 MB | `aea7d3382515819a8f06200536372ff70834acbf27c1bea159ac65c7324948f9` |
| Linux x86_64 | `Nexus-linux-x86_64-AppImage-ondafone` | `Nexus-6.2.3-82-g32616c85-x86_64.AppImage` | 112 MB | `efecce5f2c93576fe07ad76fb3f34ad3c2c31ddb50407f1c24ec7f7f3690753a` |

Conferir antes de entregar: `sha256sum <arquivo>` (Linux), `shasum -a 256 <arquivo>` (macOS) ou
`Get-FileHash <arquivo>` (PowerShell).

## O que entra

- **Modo limitado e completo** (#15): qualquer provedor SIP no limitado (G.711/Opus, sem vídeo, presença
  nem push); completo nos proxies Voipmundo da lista assinada (cópia embutida até a chave pública nova). Estado do
  modo num arquivo privado do app, fora do linphonerc (#18).
- **Conta Ondafone** (#16): e-mail + código, sair, excluir (Configurações → Conta Ondafone). Sem login com Google no
  desktop.
- "TLS primeiro" **desligado**: o app não troca o transporte da conta.

## O que não entra

- Login com Google, Mensagens (SMS/WhatsApp/Telegram), compartilhar logs pelo app (só Android, #29 do Android, em revisão).
- SIP por TCP automático nos servidores Voipmundo (só Android, #27): no desktop, configure a conta Voipmundo em
  **TCP** à mão (transporte nas configurações da conta) para a chamada sainte não cair em 408.
- macOS Intel (só Apple Silicon neste workflow).
- Os LOWs do desktop #19 (CI e guard da migração do linphonerc), abertos depois deste build.

## Instalar (sem assinatura)

- **Windows:** o SmartScreen avisa → **Mais informações → Executar assim mesmo**. O `.msi` é para GPO/Intune.
- **macOS:** clique com o botão direito no app → **Abrir** → **Abrir** (Gatekeeper, assinatura ad-hoc).
- **Linux:** `chmod +x Nexus-*.AppImage` e rode o arquivo.

O nome do app nos arquivos e na janela ainda é **Nexus** (a troca de marca do desktop é outra entrega).

## Problemas conhecidos

- Instaladores sem assinatura: avisos do sistema na primeira abertura (acima).
- Desktop sem envio de log: se a equipe precisar, ela orienta caso a caso.
