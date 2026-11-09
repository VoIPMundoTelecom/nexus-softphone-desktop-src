# Sessão da conta Ondafone no desktop (tokens do givm)

- O desktop não usa cofre do sistema hoje (`ENABLE_QT_KEYCHAIN` OFF, código de keychain do upstream comentado). Os tokens (access e refresh) ficam em `AppLocalDataLocation/ondafone/ondafone_sessao.ini`.
- **Permissão antes do dado:** o diretório `ondafone` é posto em 0700 e o `.ini` é criado **vazio** e posto em 0600 em `caminhoSessao()`, antes do primeiro `setValue`. Criar o arquivo pelo `QSettings` e dar `chmod` depois deixa uma janela com o refresh token legível no umask padrão (0644): foi o que a revisão de 05/11 apontou.
- O `QSettings` grava por `QSaveFile` (temporário no mesmo diretório e rename); o temporário herda a permissão do arquivo existente, e o diretório 0700 cobre o resto.
- `AppLocalDataLocation`, não `AppDataLocation`: no Windows o segundo é o perfil móvel (Roaming) e levaria a sessão para outra máquina. No Windows o `QFile::setPermissions` quase não tem efeito; a proteção ali é o perfil do usuário.
- Nunca no linphonerc (qualquer provisionamento escreve nele) nem no log (só rota e status HTTP).
- **Médio prazo:** QtKeychain (Keychain no macOS, Credential Manager no Windows, Secret Service no Linux) quando o keychain voltar ao build; então o `.ini` guarda só o e-mail exibido.
- O givm só aceita `dispositivo.plataforma` em `android|ios|web`: o desktop não manda `dispositivo` e, com `ONDAFONE_EXIGIR_APARELHO` ligado no givm, não cria conta nova (só entra em conta criada pelo celular). Proposta à S1: aceitar `desktop` no `DispositivoIn`.
