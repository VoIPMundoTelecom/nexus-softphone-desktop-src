#!/usr/bin/env bash
# Nexus: prepara o linphone-sdk PRE-COMPILADO para o build do app, sem gitlab.linphone.org.
#
#   scripts/sdk-precompilado.sh <windows|macos|linux> <destino-do-sdk> <destino-do-fonte>
#
# 1. Baixa o binario publico da Belledonne (download.linphone.org) do MESMO commit do submodulo
#    external/linphone-sdk (lista em scripts/sdk-precompilado.txt) e confere o SHA-256:
#    - windows/macos: zip do SDK (o nome traz "+<commit>");
#    - linux: a Belledonne nao publica SDK para Linux; usa as bibliotecas do AppImage de snapshot do
#      Linphone desktop compilado com esse commit do SDK (confere a versao "g<commit>" no liblinphone.so).
# 2. Baixa do linphone-sdk no GitHub da Belledonne, no mesmo commit (fetch raso + sparse), o fonte do
#    liblinphone (de onde sai o wrapper C++ liblinphone++, que nenhum dos binarios traz), os headers
#    (Linux) e os utilitarios de CMake. O passo seguinte do CI e
#      cmake -S cmake/sdk-precompilado ... --target install   (ver o workflow).
#
# O commit vem do gitlink do submodulo (git ls-tree), entao funciona sem inicializar submodulo nenhum.
set -euo pipefail

plataforma="${1:?uso: $0 <windows|macos|linux> <destino-do-sdk> <destino-do-fonte>}"
destino_sdk="${2:?destino do SDK}"
destino_fonte="${3:?destino do fonte}"
raiz="$(cd "$(dirname "$0")/.." && pwd)"
lista="$raiz/scripts/sdk-precompilado.txt"

sdk_commit="$(git -C "$raiz" ls-tree HEAD external/linphone-sdk | awk '{print $3}')"
[ -n "$sdk_commit" ] || { echo "::error::sem gitlink external/linphone-sdk"; exit 1; }
curto="${sdk_commit:0:10}"

linha="$(tr -d '\r' < "$lista" | awk -v c="$sdk_commit" -v p="$plataforma" '$1 !~ /^#/ && $1 == c && $2 == p')"
if [ "$(printf '%s\n' "$linha" | grep -c .)" -gt 1 ]; then
	echo "::error::mais de uma linha para $plataforma no commit $sdk_commit em scripts/sdk-precompilado.txt"
	exit 1
fi
if [ -z "$linha" ]; then
	echo "::error::nenhum SDK pre-compilado para $plataforma no commit $sdk_commit em scripts/sdk-precompilado.txt"
	exit 1
fi
url="$(awk '{print $3}' <<<"$linha")"
sha_esperado="$(awk '{print $4}' <<<"$linha")"
if [ "$plataforma" != linux ]; then
	# O nome do zip traz "+<10 primeiros do commit>": confere que a linha nao aponta para outro build.
	case "$url" in
		*"%2B${curto}"*|*"+${curto}"*) ;;
		*) echo "::error::a URL $url nao e do commit $curto"; exit 1 ;;
	esac
fi

sha256() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'; else shasum -a 256 "$1" | awk '{print $1}'; fi; }

tmp="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"
command -v cygpath >/dev/null 2>&1 && tmp="$(cygpath -u "$tmp")"   # Windows (Git Bash): D:\a\_temp -> /d/a/_temp
tmp="$tmp/nexus-sdk-precompilado"
rm -rf "$tmp"; mkdir -p "$tmp/x"
arquivo="$tmp/sdk.bin"
echo "SDK: $url"
curl -fL --retry 5 --retry-delay 10 --connect-timeout 30 --speed-limit 10000 --speed-time 60 -o "$arquivo" "$url"
sha="$(sha256 "$arquivo")"
if [ "$sha" != "$sha_esperado" ]; then
	echo "::error::SHA-256 do SDK nao confere: $sha (esperado $sha_esperado)"
	exit 1
fi
echo "SHA-256 OK: $sha"

# ---- Fonte no mesmo commit (GitHub da Belledonne; o linphone-sdk 5.5 e monorepo) ----
rm -rf "$destino_fonte"; mkdir -p "$destino_fonte"
git -C "$destino_fonte" init -q
git -C "$destino_fonte" remote add origin https://github.com/BelledonneCommunications/linphone-sdk.git
# Modo cone, sem "/" no inicio: no Git Bash do Windows o MSYS converteria "/liblinphone/" em caminho do Windows.
git -C "$destino_fonte" sparse-checkout set --cone liblinphone cmake bctoolbox/cmake bctoolbox/include \
	ortp/include mediastreamer2/include belle-sip/include belr/include lime/include bzrtp/include
for i in 1 2 3; do
	git -C "$destino_fonte" -c protocol.version=2 fetch -q --depth 1 --filter=blob:none origin "$sdk_commit" && break
	[ "$i" = 3 ] && { echo "::error::fetch do linphone-sdk $sdk_commit no GitHub falhou"; exit 1; }
	sleep 15
done
git -C "$destino_fonte" -c advice.detachedHead=false checkout -q FETCH_HEAD
[ "$(git -C "$destino_fonte" rev-parse HEAD)" = "$sdk_commit" ] || { echo "::error::commit do fonte nao confere"; exit 1; }
[ -f "$destino_fonte/liblinphone/wrappers/cpp/genwrapper.py" ] || { echo "::error::liblinphone/wrappers/cpp ausente"; exit 1; }
echo "Fonte do linphone-sdk ($sdk_commit) em $destino_fonte"

rm -rf "$destino_sdk"; mkdir -p "$(dirname "$destino_sdk")"
if [ "$plataforma" = linux ]; then
	# ---- Linux: bibliotecas do AppImage, extraidas sem executar o AppImage ----
	# AppImage = runtime ELF + squashfs; o squashfs comeca no fim do ELF (e_shoff + e_shentsize * e_shnum).
	if command -v unsquashfs >/dev/null 2>&1; then
		offset="$(python3 -c 'import struct,sys
d=open(sys.argv[1],"rb").read(64)
assert d[:4]==b"\x7fELF" and d[4]==2
shoff,=struct.unpack_from("<Q",d,0x28); shentsize,shnum=struct.unpack_from("<HH",d,0x3A)
print(shoff+shentsize*shnum)' "$arquivo")"
		rm -rf "$tmp/x"
		unsquashfs -n -o "$offset" -d "$tmp/x" "$arquivo" usr/lib usr/share/belr usr/share/images usr/share/linphone usr/share/sounds >/dev/null
	else
		7z x -y -o"$tmp/x" "$arquivo" 'usr/lib/*' 'usr/share/belr/*' 'usr/share/images/*' 'usr/share/linphone/*' 'usr/share/sounds/*' >/dev/null
	fi
	ul="$tmp/x/usr/lib"
	if ! grep -aq "g${curto}" "$ul"/liblinphone.so.*; then
		echo "::error::o liblinphone do AppImage nao e do commit $curto"; exit 1
	fi
	mkdir -p "$destino_sdk/lib/mediastreamer/plugins" "$destino_sdk/share" "$destino_sdk/include"
	# So o SDK e as dependencias dele (o resto o linuxdeploy pega do sistema na hora do AppImage).
	# Sem liblinphone++ (gerado aqui no mesmo commit) e sem Qt.
	for padrao in libbctoolbox libbelcard libbelle-sip libbelr libbzrtp libbcg729 libbv16 libdecaf libgsm \
		libjsoncpp liblime liblinphone libmbedcrypto libmbedtls libmbedx509 libmediastreamer2 libopenh264 \
		libopus liboqs libortp libpostquantumcryptoengine libsoci_core libsoci_sqlite3 libspeex libspeexdsp \
		libsqlite3 libsrtp2 libturbojpeg libjpeg libvpx libxerces-c libxml2 libyuv libhidapi-hidraw libldap liblber; do
		found=0
		for f in "$ul/$padrao".so "$ul/$padrao".so.* "$ul/$padrao"-*.so; do
			[ -e "$f" ] || continue
			cp -a "$f" "$destino_sdk/lib/"; found=1
		done
		[ "$found" = 1 ] || echo "::warning::$padrao nao esta no AppImage"
	done
	# Plugins do mediastreamer2 (sem o msqogl: e do app, linkado com o Qt do AppImage).
	for f in "$ul"/mediastreamer/plugins/*.so; do
		case "$(basename "$f")" in libmsqogl*) continue ;; esac
		cp -a "$f" "$destino_sdk/lib/mediastreamer/plugins/"
	done
	for d in belr images linphone sounds; do
		if [ -d "$tmp/x/usr/share/$d" ]; then cp -a "$tmp/x/usr/share/$d" "$destino_sdk/share/"; fi
	done
	# Headers do fonte, no mesmo commit (nenhum deles e gerado no build).
	# So as pastas (bctoolbox/, linphone/...): os .h soltos na raiz do include do bzrtp sao internos.
	for m in bctoolbox ortp mediastreamer2 belle-sip belr lime bzrtp liblinphone; do
		for d in "$destino_fonte/$m/include"/*/; do
			cp -a "$d" "$destino_sdk/include/"
		done
	done
	find "$destino_sdk/include" -name CMakeLists.txt -delete
else
	# ---- Windows/macOS: zip do SDK (linphone-sdk/<win64|macos>/...) ----
	(cd "$tmp/x" && cmake -E tar xf "$arquivo")
	interno="$(find "$tmp/x/linphone-sdk" -mindepth 1 -maxdepth 1 -type d | head -1)"
	[ -d "$interno" ] || { echo "::error::layout inesperado no zip"; exit 1; }
	mv "$interno" "$destino_sdk"
fi
rm -rf "$tmp"

# Utilitarios de CMake que o app usa (bc_compute_full_version, linphone_sdk_convert_comma_separated_list_to_cmake_list):
# o zip do Windows ja traz o primeiro; o do macOS e o Linux nao.
if [ ! -f "$destino_sdk/share/BCToolbox/cmake/BCToolboxCMakeUtils.cmake" ]; then
	mkdir -p "$destino_sdk/share/BCToolbox/cmake"
	cp "$destino_fonte/bctoolbox/cmake/BCToolboxCMakeUtils.cmake" "$destino_sdk/share/BCToolbox/cmake/"
fi
mkdir -p "$destino_sdk/cmake"
cp "$destino_fonte/cmake/LinphoneSdkUtils.cmake" "$destino_sdk/cmake/"
echo "SDK pronto em $destino_sdk"
