#!/usr/bin/env bash
# CI (os 3 jobs de instaladores-teste.yml): baixa os submódulos do espelho privado VoIPMundoTelecom/lp-<nome>
# (scripts/espelhar-deps.sh) no lugar do gitlab.linphone.org, que bloqueia IP de datacenter (GitHub Actions e outros).
# O .gitmodules continua com as URLs originais: o insteadOf é global e só vale no runner, então nem o token nem
# o espelho entram no cache de .git/modules.
# LP_MIRROR_TOKEN (secret): token só de leitura, com escopo nos repos lp-*; mascarado no log.
set -uo pipefail

[ -n "${LP_MIRROR_TOKEN:-}" ] || { echo "::error::secret LP_MIRROR_TOKEN ausente (leitura nos repos VoIPMundoTelecom/lp-*)"; exit 1; }
echo "::add-mask::$LP_MIRROR_TOKEN"
git config --global protocol.version 2
# o mesmo destino para os dois prefixos; o git usa o insteadOf de prefixo mais longo, então
# .../BC/public/external/gn.git vira lp-gn.git (e não lp-external/gn.git)
ESPELHO="https://x-access-token:${LP_MIRROR_TOKEN}@github.com/VoIPMundoTelecom/lp-"
git config --global url."$ESPELHO".insteadOf "https://gitlab.linphone.org/BC/public/external/"
git config --global --add url."$ESPELHO".insteadOf "https://gitlab.linphone.org/BC/public/"

ok=0
for i in 1 2 3; do
	git submodule update --init --recursive --filter=blob:none --jobs 8 && { ok=1; break; }
	echo "tentativa $i falhou, repetindo em $((i * 30))s..."
	sleep $((i * 30))
done
# o token não fica no runner depois do passo
git config --global --remove-section url."$ESPELHO" 2>/dev/null || true
[ "$ok" = 1 ] || { echo "::error::submódulos indisponíveis no espelho lp-* (commit fixado faltando? workflow \"Espelho das dependências\")"; exit 1; }
git -C external/linphone-sdk describe || true
