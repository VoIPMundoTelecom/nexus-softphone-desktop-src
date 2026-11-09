#!/usr/bin/env bash
# Nexus (revisao 27/10): `nexus --fetch-config <url do givm>` nao pode deixar o token no arquivo de log nem no
# linphonerc (misc/config-uri). Roda o app (AppImage ou binario) num HOME descartavel, sem tela (offscreen), com
# --verbose (logs do liblinphone/belle-sip tambem na saida), e procura o token inteiro e o token em base64 nos logs,
# na saida e no linphonerc. O token e falso: o givm responde 404.
# Uso: tests/nexus/fetch-config-sem-token-no-log.sh <AppImage-ou-binario> [segundos]
set -uo pipefail
APP="${1:?uso: $0 <AppImage-ou-binario> [segundos]}"
ESPERA="${2:-40}"
TOKEN="FakeTok3n-sem-valor_0123456789abcdefXYZ" # gitleaks:allow (falso, so para o teste)
URL="https://api.givm.com.br/api/prov/$TOKEN"

TMPHOME="$(mktemp -d)"
trap 'rm -rf "$TMPHOME"' EXIT
export HOME="$TMPHOME" XDG_DATA_HOME="$TMPHOME/.local/share" XDG_CONFIG_HOME="$TMPHOME/.config"
export QT_QPA_PLATFORM=offscreen APPIMAGE_EXTRACT_AND_RUN=1

timeout --kill-after=10 "$ESPERA" "$APP" --verbose --fetch-config "$URL" >"$TMPHOME/saida.txt" 2>&1
echo "app encerrado (codigo $?; 124 = timeout esperado)"

LOGS="$(find "$TMPHOME" -path '*/logs/*' -type f 2>/dev/null)"
if [ -z "$LOGS" ]; then
	echo "INCONCLUSIVO: nenhum arquivo de log em $TMPHOME (o app nao chegou a escrever log)"
	sed -n '1,40p' "$TMPHOME/saida.txt"
	exit 2
fi
echo "logs: $(echo "$LOGS" | wc -l) arquivo(s)"
# o token cru e a URL inteira em base64 (como chega por sip-nexus:?nexus-fetch-config=<b64>)
# (trecho do base64 que so cobre bytes do token: comeca no 1o grupo de 3 bytes inteiro dentro dele)
PREFIXO="${URL%"$TOKEN"}"
INICIO=$(( (${#PREFIXO} + 2) / 3 * 4 + 1 ))
B64="$(printf '%s' "$URL" | base64 -w0 | cut -c"$INICIO-$((INICIO + 31))")"
B64URL="$(printf '%s' "$B64" | tr '+/' '-_')"
falhou=0
procura() { # $1 = arquivo, $2 = onde
	local p
	for p in "$TOKEN" "$B64" $( [ "$B64URL" = "$B64" ] || printf '%s' "$B64URL"); do
		if grep -qF -- "$p" "$1"; then
			echo "FALHOU: token ($( [ "$p" = "$TOKEN" ] && echo cru || echo base64)) em $2:"
			grep -nF -- "$p" "$1" | sed "s|$p|<TOKEN>|g" | head -5
			falhou=1
		fi
	done
}
while IFS= read -r f; do procura "$f" "log $f"; done <<<"$LOGS"
procura "$TMPHOME/saida.txt" "saida do processo"

# linphonerc: misc/config-uri nao pode guardar o link com o token (o core leria de novo a cada inicio)
RCS="$(find "$TMPHOME" -name linphonerc -type f 2>/dev/null)"
[ -n "$RCS" ] || echo "aviso: nenhum linphonerc em $TMPHOME"
while IFS= read -r rc; do
	[ -n "$rc" ] || continue
	uri="$(awk -F= '/^\[/{s=$0} s=="[misc]" && $1=="config-uri"{sub(/^[^=]*=/,""); print}' "$rc")"
	if printf '%s' "$uri" | grep -qF "$TOKEN"; then
		echo "FALHOU: misc/config-uri com o token em $rc"
		falhou=1
	fi
	procura "$rc" "$rc"
done <<<"$RCS"

# o --verbose tem de ter trazido os logs do SDK; sem eles a checagem do liblinphone/belle-sip nao valeu
sdk="$(cat "$TMPHOME/saida.txt" $LOGS 2>/dev/null | grep -ciE 'liblinphone|belle-sip|bctbx|mediastreamer' || true)"
echo "linhas do liblinphone/belle-sip conferidas: $sdk"
[ "$sdk" -gt 0 ] || echo "aviso: nenhuma linha do liblinphone/belle-sip (conferir o formato do log)"

grep -rlF --exclude=saida.txt "$TOKEN" "$TMPHOME" 2>/dev/null | sed 's/^/aviso: token gravado em /'
[ "$falhou" = 0 ] && echo "OK: token ausente dos logs, da saida e do misc/config-uri"
exit "$falhou"
