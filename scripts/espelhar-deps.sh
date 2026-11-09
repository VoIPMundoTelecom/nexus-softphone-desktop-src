#!/usr/bin/env bash
# Espelha no GitHub (VoIPMundoTelecom/lp-<nome>) os submódulos que vêm do gitlab.linphone.org, para a CI não
# depender dele (fora do ar de 23/10 em diante). Percorre, nos commits fixados da HEAD (ou da ref dada):
# os submódulos do .gitmodules do Nexus e, recursivamente, os de cada um (os 29 do linphone-sdk, inclusive os
# relativos ../bcg729.git e ../bcmatroska2.git). Só espelha o que está em https://gitlab.linphone.org/BC/public/;
# o resto (github.com, googlesource) a CI busca na origem e este script só confere.
#
#   scripts/espelhar-deps.sh [--mirrors <dir>] [--visibilidade private|public] [--ref <ref>] [--trabalho <dir>]
#       envia ao GitHub; cria o repo lp-<nome> se faltar (gh; privado por padrão). Idempotente. Sem token aqui:
#       usa a credencial do git/gh da máquina. Origem: com --mirrors, os clones --mirror já feitos em <dir>
#       (<nome>.git ou <nome>, em qualquer subpasta; não são alterados); sem, git clone --mirror do gitlab
#       (o gitlab.linphone.org bloqueia IP de datacenter: rodar da rede do escritório).
#       Roda no bash 3.2 do macOS.
#   scripts/espelhar-deps.sh --teste [--mirrors <dir>] [--ref <ref>]
#       como o espelhar, mas não cria repo nem envia nada: só clona/atualiza e lista o que seria enviado.
#   scripts/espelhar-deps.sh --conferir [--ref <ref>]
#       só confere que o commit fixado de cada submódulo existe no espelho (CI: falha se faltar).
#       LP_DESTINO=<prefixo> troca o espelho conferido (teste: https://github.com/BelledonneCommunications/).
#
# ATENÇÃO: a origem tem de ter o histórico INTEIRO. Clone raso (--depth, como o .git/modules da CI e dos clones
# de desenvolvimento) não serve: o GitHub recusa o push ("index-pack failed", faltam os pais do commit).
#
# Mapa (o mesmo do insteadOf da CI; nome = último componente do caminho, sem .git):
#   https://gitlab.linphone.org/BC/public/external/<nome>.git -> https://github.com/VoIPMundoTelecom/lp-<nome>.git
#   https://gitlab.linphone.org/BC/public/<nome>.git          -> https://github.com/VoIPMundoTelecom/lp-<nome>.git
# Envia refs/heads/* e refs/tags/* (não refs/merge-requests/* nem refs/keep-around/* do GitLab) e uma tag
# nexus-pin/<commit> para cada commit fixado, para ele continuar alcançável mesmo se o upstream reescrever a branch.
# Termina listando o que faltou (código 1 se faltou algo).
set -uo pipefail

ORG="VoIPMundoTelecom"
ORIGEM="https://gitlab.linphone.org/BC/public/"
DESTINO="${LP_DESTINO:-https://github.com/$ORG/lp-}"
MODO=espelhar
VISIB=private
MIRRORS=""
REF=HEAD
TRAB=""
while [ $# -gt 0 ]; do
	case "$1" in
	--conferir) MODO=conferir; shift ;;
	--teste) MODO=teste; shift ;;
	--visibilidade) VISIB="${2:?--visibilidade private|public}"; shift 2 ;;
	--ref) REF="${2:?--ref <ref>}"; shift 2 ;;
	--trabalho) TRAB="${2:?--trabalho <dir>}"; shift 2 ;;
	--mirrors) MIRRORS="$(cd "${2:?--mirrors <dir>}" && pwd)" || exit 1; shift 2 ;;
	*) echo "opção desconhecida: $1" >&2; exit 1 ;;
	esac
done
if [ "$MODO" = espelhar ]; then
	case "$VISIB" in private | public) ;; *) echo "--visibilidade private|public" >&2; exit 1 ;; esac
fi
RAIZ="$(git rev-parse --show-toplevel)"
TRAB="${TRAB:-$RAIZ/../espelho-deps}"
mkdir -p "$TRAB"
TRAB="$(cd "$TRAB" && pwd)"

FALTOU=()
VISTO=$'\n' # "url@commit" já percorridos, um por linha (sem array associativo: bash 3.2 do macOS)
NVISTO=0

nome_de() { local n="${1%/}"; n="${n##*/}"; echo "${n%.git}"; }
espelhavel() { [[ "$1" == "$ORIGEM"* ]]; }
destino_de() { echo "$DESTINO$(nome_de "$1").git"; }

# URL relativa (../bcg729.git) em relação à URL do repo pai
resolver() { # $1 = url do pai, $2 = url do .gitmodules
	local base="${1%/}" rel="$2"
	[[ "$rel" == ../* || "$rel" == ./* ]] || { echo "$rel"; return; }
	base="${base%.git}"
	while [[ "$rel" == ../* || "$rel" == ./* ]]; do
		if [[ "$rel" == ../* ]]; then base="${base%/*}"; rel="${rel#../}"; else rel="${rel#./}"; fi
	done
	echo "$base/$rel"
}

# repositório local (bare) com o commit: no modo espelhar, o clone --mirror do gitlab; no conferir, um fetch
# só do commit (--filter=blob:none, depth 1) a partir do espelho (ou da origem, se não for do gitlab)
repo_com_commit() { # $1 = url original, $2 = commit -> imprime o dir; retorna 1 se não achou
	local url="$1" sha="$2" dir
	dir="$TRAB/$(echo "$url" | sed 's|^https://||; s|[^A-Za-z0-9._-]|_|g')"
	if [ "$MODO" != conferir ] && espelhavel "$url" && [ -n "$MIRRORS" ]; then
		dir="$(find "$MIRRORS" -maxdepth 3 -type d \( -name "$(nome_de "$url").git" -o -name "$(nome_de "$url")" \) \
			-exec test -f '{}/HEAD' ';' -print 2>/dev/null | head -1)"
		[ -n "$dir" ] || { echo "sem clone de $(nome_de "$url") em $MIRRORS" >&2; return 1; }
	elif [ "$MODO" != conferir ] && espelhavel "$url"; then
		if [ -d "$dir" ]; then
			git -C "$dir" remote update --prune >&2 || return 1
		else
			git clone --mirror "$url" "$dir" >&2 || return 1
		fi
	else
		local de="$url"
		[ "$MODO" = conferir ] && espelhavel "$url" && de="$(destino_de "$url")"
		[ -d "$dir" ] || git init -q --bare "$dir"
		git -C "$dir" cat-file -e "$sha^{commit}" 2>/dev/null ||
			git -C "$dir" fetch -q --depth=1 --filter=blob:none "$de" "$sha" >&2 || return 1
		# o fetch parcial precisa saber de onde buscar os blobs (.gitmodules) depois
		git -C "$dir" config remote.origin.url "$de"
		git -C "$dir" config remote.origin.promisor true
		git -C "$dir" config remote.origin.partialclonefilter blob:none
	fi
	git -C "$dir" cat-file -e "$sha^{commit}" 2>/dev/null || return 1
	if [ "$MODO" != conferir ] && [ "$(git -C "$dir" rev-parse --is-shallow-repository)" = true ]; then
		echo "$dir é raso: sem o histórico inteiro o GitHub recusa o push" >&2
		return 1
	fi
	echo "$dir"
}

enviar() { # $1 = url original, $2 = dir do mirror, $3.. = commits fixados
	local url="$1" dir="$2" repo dest
	shift 2
	repo="$ORG/lp-$(nome_de "$url")"
	dest="$(destino_de "$url")"
	git ls-remote "$dest" HEAD >/dev/null 2>&1 || gh repo view "$repo" >/dev/null 2>&1 ||
		gh repo create "$repo" "--$VISIB" --description "Espelho de $url (dependência do build do Nexus)" >&2 || return 1
	git -C "$dir" push --prune "$dest" '+refs/heads/*:refs/heads/*' '+refs/tags/*:refs/tags/*' >&2 || return 1
	local s
	for s in "$@"; do
		git -C "$dir" push "$dest" "$s:refs/tags/nexus-pin/$s" >&2 || return 1
	done
}

visitar() { # $1 = url, $2 = commit, $3 = caminho (para o relatório)
	local url="$1" sha="$2" onde="$3" dir
	case "$VISTO" in *$'\n'"$url@$sha"$'\n'*) return 0 ;; esac
	VISTO="$VISTO$url@$sha"$'\n'
	NVISTO=$((NVISTO + 1))
	echo "== $onde  $sha  $url" >&2
	if ! dir="$(repo_com_commit "$url" "$sha")"; then
		FALTOU+=("$onde $sha $url$(espelhavel "$url" && [ "$MODO" = conferir ] && echo " (espelho: $(destino_de "$url"))")")
		return 0
	fi
	if [ "$MODO" = teste ] && espelhavel "$url"; then
		echo "   (teste) enviaria $(git -C "$dir" for-each-ref refs/heads refs/tags | wc -l) refs + nexus-pin/$sha -> $(destino_de "$url")" >&2
	elif [ "$MODO" = espelhar ] && espelhavel "$url"; then
		enviar "$url" "$dir" "$sha" || FALTOU+=("$onde $sha $url (envio para $(destino_de "$url") falhou)")
	fi
	local mods
	mods="$(git -C "$dir" show "$sha:.gitmodules" 2>/dev/null)" || return 0
	local caminho ssha surl
	while read -r caminho ssha; do
		surl="$(git config -f <(printf '%s\n' "$mods") --get-regexp '^submodule\..*\.path$' |
			awk -v p="$caminho" '$2==p{sub(/^submodule\./,"",$1); sub(/\.path$/,"",$1); print $1; exit}')"
		[ -n "$surl" ] || { FALTOU+=("$onde/$caminho $ssha (sem entrada no .gitmodules)"); continue; }
		surl="$(git config -f <(printf '%s\n' "$mods") --get "submodule.$surl.url" | sed 's/^ *//; s/ *$//')"
		visitar "$(resolver "$url" "$surl")" "$ssha" "$onde/$caminho"
	done < <(git -C "$dir" ls-tree -r "$sha" | awk '$2=="commit"{print $4, $3}')
}

cd "$RAIZ"
while read -r caminho sha; do
	k="$(git config -f .gitmodules --get-regexp '^submodule\..*\.path$' | awk -v p="$caminho" '$2==p{print $1; exit}')"
	url="$(git config -f .gitmodules --get "${k%.path}.url" | sed 's/^ *//; s/ *$//')"
	visitar "$url" "$sha" "$caminho"
done < <(git ls-tree -r "$REF" | awk '$2=="commit"{print $4, $3}')

echo
echo "$NVISTO submódulo(s) percorrido(s) ($MODO, $REF)"
if [ "${#FALTOU[@]}" -gt 0 ]; then
	echo "FALTOU (${#FALTOU[@]}):"
	printf '  %s\n' "${FALTOU[@]}"
	exit 1
fi
echo "OK: todos os commits fixados estão $([ "$MODO" = conferir ] && echo "no espelho/origem" || echo "espelhados")"
