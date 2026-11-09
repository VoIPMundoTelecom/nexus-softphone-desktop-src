#!/usr/bin/env bash
# Gera o espelho PÚBLICO do fonte (GPLv3) do Nexus desktop a partir de UMA tag de release.
# Não publica nada: a saída fica em <saida>/<tag>/ para revisão e publicação pelo responsável do release
# (docs/release-gpl.md).
#
#   scripts/espelho-fonte.sh [--com-submodulos] [--qt-fonte <qt-everywhere-src-X.tar.xz>] [--diff-upstream <ref>]
#                            [--teste] <tag> [saida]                                   (saida: ../espelho-fonte)
#
# - Exporta só a árvore da tag (git archive): sem histórico privado, sem branches, sem .git.
# - Tira os caminhos de scripts/espelho-fonte.excluir; troca o cabeçalho do README (links para docs internos).
# - SUBMODULOS.md (caminho, commit, URL); --com-submodulos gera <submódulo>-<commit>.tar.gz de cada um (recursivo,
#   nos commits da tag) para o Release; o repo do espelho fica leve. Exige os objetos no clone
#   (git submodule update --init --recursive). Obrigatório para publicar.
# - --qt-fonte: copia o fonte do Qt (LGPLv3) da versão do build para o Release (nome tem de ter a QT_VERSION da tag).
#   Obrigatório para publicar.
# - --sem-terceiros: só o fonte do app (job fonte-gpl do CI, que não alcança o gitlab.linphone.org nem baixa a Qt):
#   gitleaks obrigatório, sem os .tar.gz dos submódulos e da Qt, que o responsável do release anexa depois.
# - --teste: confere sem publicar (sem gitleaks/submódulos/Qt obrigatórios), pasta <tag>-TESTE-NAO-PUBLICAR.
# - BUILD-NEXUS.md com as opções -D e os comandos cmake da tag (GPLv3 §1: scripts que controlam a compilação).
# - Preenche NOTICE.md (tag, data, commit, SDK); --diff-upstream gera CHANGES.diff contra o upstream (ex.: 6.2.3).
# - Falha: link para arquivo excluído, valor secreto/referência interna (espelho-fonte.proibidos, com exceções
#   em espelho-fonte.permitidos), achado do gitleaks ou gitleaks ausente (obrigatório fora do --teste).
# - Cria <saida>/<tag>/repo (git com 1 commit + tag; publicar SÓ a tag), .tar.gz e SHA256SUMS.
set -Eeuo pipefail

COM_SUB=0
UPSTREAM=""
QT_FONTE=""
SEM_TERCEIROS=0
TESTE=0
while [ $# -gt 0 ]; do
  case "$1" in
    --com-submodulos) COM_SUB=1; shift ;;
    --diff-upstream) UPSTREAM="${2:?--diff-upstream <ref>}"; shift 2 ;;
    --qt-fonte) QT_FONTE="$(realpath "${2:?--qt-fonte <arquivo>}")"; shift 2 ;;
    --sem-terceiros) SEM_TERCEIROS=1; shift ;;
    --teste) TESTE=1; shift ;;
    --*) echo "opção desconhecida: $1" >&2; exit 1 ;;
    *) break ;;
  esac
done
TAG="${1:?uso: scripts/espelho-fonte.sh [--com-submodulos] [--qt-fonte <arquivo>] [--diff-upstream <ref>] [--sem-terceiros] [--teste] <tag> [saida]}"
if [ "$SEM_TERCEIROS" = 1 ] && { [ "$COM_SUB" = 1 ] || [ -n "$QT_FONTE" ]; }; then
  echo "--sem-terceiros não combina com --com-submodulos/--qt-fonte" >&2; exit 1
fi
RAIZ="$(git rev-parse --show-toplevel)"
SAIDA="${2:-$RAIZ/../espelho-fonte}"
cd "$RAIZ"

git rev-parse -q --verify "refs/tags/$TAG^{commit}" >/dev/null || { echo "tag $TAG não existe" >&2; exit 1; }
COMMIT="$(git rev-parse "$TAG^{commit}")"
DATA="$(git log -1 --format=%cs "$COMMIT")"
DEST="$SAIDA/$TAG"
[ "$TESTE" = 1 ] && DEST="$SAIDA/$TAG-TESTE-NAO-PUBLICAR"
if [ "$TESTE" = 0 ]; then
  command -v gitleaks >/dev/null || { echo "gitleaks não instalado: obrigatório para publicar (use --teste para conferir)" >&2; exit 1; }
fi
if [ "$TESTE" = 0 ] && [ "$SEM_TERCEIROS" = 0 ]; then
  [ "$COM_SUB" = 1 ] || { echo "falta --com-submodulos (o Release leva o fonte do linphone-sdk; use --teste para conferir)" >&2; exit 1; }
  [ -n "$QT_FONTE" ] || { echo "falta --qt-fonte (o Release leva o fonte da Qt, LGPLv3; use --teste para conferir)" >&2; exit 1; }
fi
[ -z "$QT_FONTE" ] || [ -f "$QT_FONTE" ] || { echo "--qt-fonte: $QT_FONTE não existe" >&2; exit 1; }
[ -e "$DEST" ] && { echo "$DEST já existe (apague antes de gerar de novo)" >&2; exit 1; }
mkdir -p "$DEST/repo"
# erro no meio: não deixa espelho pela metade
# apaga a saída parcial em qualquer saída antes do fim (inclusive os exit 1 explícitos, que não disparam ERR)
ok=0
trap '[ "$ok" = 1 ] || rm -rf "$DEST"' EXIT
ARVORE="$DEST/repo"

lista() { git show "$TAG:scripts/$1" 2>/dev/null || cat "scripts/$1"; }
EXCLUIR="$(lista espelho-fonte.excluir)"
PROIBIDOS="$(lista espelho-fonte.proibidos)"
PERMITIDOS="$(lista espelho-fonte.permitidos 2>/dev/null || true)"
WORKFLOW="$(git show "$TAG:.github/workflows/instaladores-teste.yml")"

git archive --format=tar "$TAG" | tar -x -C "$ARVORE"

# submódulos da tag: caminho, commit e URL (.gitmodules da própria tag)
SUBS="$(git ls-tree -r "$TAG" | awk '$2=="commit"{print $4, $3}')"
url_do() { git config -f <(git show "$TAG:.gitmodules") --get-regexp '^submodule\..*\.path$' |
  awk -v p="$1" '$2==p{sub(/\.path$/,"",$1); print $1}' | while read -r k; do
    git config -f <(git show "$TAG:.gitmodules") --get "$k.url"; done; }
{
  echo "# Submódulos da tag $TAG"
  echo
  echo "| Caminho | Commit | Origem |"
  echo "|---|---|---|"
  while read -r caminho sha; do echo "| \`$caminho\` | \`$sha\` | $(url_do "$caminho") |"; done <<< "$SUBS"
  echo
  echo "O linphone-sdk também está espelhado pela Belledonne em"
  echo "https://github.com/BelledonneCommunications/linphone-sdk (mesmo commit)."
} > "$ARVORE/SUBMODULOS.md"
SDK_COMMIT="$(git ls-tree "$TAG" external/linphone-sdk | awk '{print $3}')"
[ -n "$SDK_COMMIT" ] || { echo "external/linphone-sdk não é submódulo na tag $TAG" >&2; exit 1; }

embutir() {   # $1 = repo (dir com .git), $2 = commit, $3 = destino
  local repo="$1" sha="$2" dest="$3"
  git -C "$repo" cat-file -e "$sha^{commit}" 2>/dev/null || {
    echo "objeto $sha ausente em $repo: rode 'git submodule update --init --recursive' (gitlab.linphone.org no ar)" >&2; return 1; }
  mkdir -p "$dest"
  git -C "$repo" archive --format=tar "$sha" | tar -x -C "$dest"
  git -C "$repo" ls-tree -r "$sha" | awk '$2=="commit"{print $4, $3}' | while read -r sub ssha; do
    embutir "$repo/$sub" "$ssha" "$dest/$sub"
  done
}
if [ "$COM_SUB" = 1 ]; then
  # confere antes (falha cedo); os .tar.gz saem no fim
  while read -r caminho sha; do
    git -C "$RAIZ/$caminho" cat-file -e "$sha^{commit}" 2>/dev/null || {
      echo "objeto $sha ausente em $caminho: rode 'git submodule update --init --recursive' (gitlab.linphone.org no ar)" >&2
      exit 1; }
  done <<< "$SUBS"
  INCL="O fonte de cada submódulo (inclusive o linphone-sdk, com os submódulos dele) está no **Release desta tag** como \`<submódulo>-<commit>.tar.gz\`, nos commits de \`SUBMODULOS.md\`."
else
  INCL="Os submódulos **não** acompanham esta cópia: os commits exatos estão em \`SUBMODULOS.md\`, com o fonte nos endereços indicados."
fi
# Qt (LGPLv3, bibliotecas dinâmicas nos instaladores): versão do build da tag
QTV="$(printf '%s\n' "$WORKFLOW" | sed -n "s/^  QT_VERSION: *['\"]\{0,1\}\([0-9.]*\).*/\1/p" | head -1)"
[ -n "$QTV" ] || { echo "QT_VERSION não encontrada no workflow da tag" >&2; exit 1; }
if [ -n "$QT_FONTE" ]; then
  [[ "$(basename "$QT_FONTE")" == *"$QTV"* ]] || { echo "--qt-fonte não é da Qt $QTV: $(basename "$QT_FONTE")" >&2; exit 1; }
  INCL="$INCL O fonte da **Qt $QTV** (LGPLv3) usada nos instaladores está no Release desta tag (\`$(basename "$QT_FONTE")\`)."
else
  INCL="$INCL Qt $QTV (LGPLv3): fonte em https://download.qt.io/archive/qt/${QTV%.*}/$QTV/single/; cópia sob pedido em https://voipmundo.com.br/contato/."
fi

shopt -s nullglob dotglob globstar
EXCLUIDOS=()
while IFS= read -r padrao; do
  [[ -z "$padrao" || "$padrao" == \#* ]] && continue
  EXCLUIDOS+=("${padrao%/}")
  for alvo in "$ARVORE"/$padrao "$ARVORE"/**/$padrao; do [ -e "$alvo" ] && rm -rf -- "$alvo"; done
done <<< "$EXCLUIR"
shopt -u nullglob dotglob globstar

for f in LICENSE.txt NOTICE.md TRADEMARKS.md README.md; do
  [ -f "$ARVORE/$f" ] || { echo "falta $f na tag $TAG" >&2; exit 1; }
done

# README: o cabeçalho do fork (citação no topo) aponta para docs internos -> cabeçalho do espelho
python3 - "$ARVORE/README.md" "$TAG" <<'PY'
import sys
p, tag = sys.argv[1], sys.argv[2]
linhas = open(p, encoding="utf-8").read().split("\n")
i = 0
while i < len(linhas) and (linhas[i].startswith(">") or not linhas[i].strip()):
    i += 1
cab = [
    f"> **Nexus desktop {tag} — fonte correspondente (GNU GPLv3).** Fork do",
    "> [linphone-desktop](https://github.com/BelledonneCommunications/linphone-desktop) da Belledonne Communications.",
    "> Licenças e componentes: [`NOTICE.md`](NOTICE.md) · submódulos: [`SUBMODULOS.md`](SUBMODULOS.md) ·",
    "> marcas: [`TRADEMARKS.md`](TRADEMARKS.md) · como compilar esta versão: [`BUILD-NEXUS.md`](BUILD-NEXUS.md).",
    "> \"Linphone\" é marca da Belledonne Communications.",
    "",
]
open(p, "w", encoding="utf-8").write("\n".join(cab + linhas[i:]))
PY

# BUILD-NEXUS.md: opções e comandos cmake da tag (o workflow não vai para o espelho)
printf '%s\n' "$WORKFLOW" > "$DEST/workflow.tmp"
python3 - "$TAG" "$DEST/workflow.tmp" > "$ARVORE/BUILD-NEXUS.md" <<'PY'
import re, sys
tag = sys.argv[1]
linhas = open(sys.argv[2], encoding="utf-8").read().split("\n")
ci = lambda t: re.sub(r"\$\{\{[^}]*\}\}", "<valor do CI>", t)
out = [f"# Como compilar o Nexus desktop {tag}", "",
       "Gerado de `.github/workflows/instaladores-teste.yml` da tag (o CI não vai para o espelho).",
       "Clone com os submódulos (`git submodule update --init --recursive`, ou extraia os .tar.gz do Release da tag) e rode os",
       "comandos abaixo na raiz. Valores `<...>` vêm do CI.", "", "## Ambiente (env do workflow)", "", "```"]
i = 0
while i < len(linhas):                       # bloco env: de nível 0
    if linhas[i].startswith("env:"):
        i += 1
        while i < len(linhas) and (linhas[i].startswith("  ") or not linhas[i].strip()):
            if linhas[i].strip() and not linhas[i].strip().startswith("#"):
                out.append(ci(linhas[i][2:]))
            i += 1
        break
    i += 1
out += ["```", ""]
job = None
for n, l in enumerate(linhas):
    m = re.match(r"^  ([A-Za-z0-9_-]+):\s*$", l)
    if m:
        job = m.group(1)
    m = re.match(r"^(\s*)run:\s*(\|)?\s*(.*)$", l)
    if not m:
        continue
    ind = len(m.group(1))
    bloco = [m.group(3)] if m.group(3) else []
    if m.group(2):
        j = n + 1
        while j < len(linhas) and (not linhas[j].strip() or len(linhas[j]) - len(linhas[j].lstrip()) > ind):
            bloco.append(linhas[j][ind + 2:])
            j += 1
    texto = "\n".join(bloco).strip()
    if "cmake " in texto and ("-S ." in texto or "--build" in texto):
        nome = ""
        for k in range(n - 1, max(n - 6, 0), -1):
            mm = re.match(r"^\s*- name:\s*(.*)$", linhas[k])
            if mm:
                nome = mm.group(1); break
        out += [f"## {job} — {nome}", "", "```", ci(texto), "```", ""]
print("\n".join(out))
PY
rm -f "$DEST/workflow.tmp"

# NOTICE: tag, data, commit, SDK, submódulos, diff do upstream
if [ -n "$UPSTREAM" ]; then
  git rev-parse -q --verify "$UPSTREAM^{commit}" >/dev/null || { echo "ref do upstream $UPSTREAM não existe" >&2; exit 1; }
  ex=(":(exclude)scripts/espelho-fonte.proibidos" ":(exclude)scripts/espelho-fonte.permitidos")
  for e in "${EXCLUIDOS[@]}"; do ex+=(":(exclude,glob)$e" ":(exclude,glob)**/$e"); done
  git diff "$UPSTREAM" "$TAG" -- . "${ex[@]}" > "$ARVORE/CHANGES.diff"
  DIFF="O diff completo contra o upstream (\`$UPSTREAM\` = $(git rev-parse --short "$UPSTREAM")) está em \`CHANGES.diff\`."
else
  DIFF="Para o diff completo, compare esta árvore com o upstream (branch \`release/6.2\` do linphone-desktop)."
fi
sed -i -e "s|@TAG@|$TAG|g" -e "s|@COMMIT@|$COMMIT|g" -e "s|@DATA@|$DATA|g" -e "s|@SDK_COMMIT@|$SDK_COMMIT|g" \
  -e "s|@SUBMODULOS_INCLUIDOS@|$INCL|g" -e "s|@DIFF@|$DIFF|g" "$ARVORE/NOTICE.md"
grep -q '@[A-Z_]*@' "$ARVORE/NOTICE.md" && { echo "NOTICE.md com marcador não preenchido" >&2; exit 1; }

falhou=0
for e in "${EXCLUIDOS[@]}"; do
  [[ "$e" == *"*"* ]] && continue
  if grep -rInF --include='*.md' -e "($e)" -e "($e/" "$ARVORE" >/dev/null; then
    echo "LINK PARA ARQUIVO EXCLUÍDO ($e):" >&2; grep -rInF --include='*.md' -e "($e)" -e "($e/" "$ARVORE" | head -5 >&2; falhou=1
  fi
done

# varredura do código do Nexus (os submódulos de terceiros vão em .tar.gz separados, fora da árvore);
# exceções em espelho-fonte.permitidos
while IFS= read -r re; do
  [[ -z "$re" || "$re" == \#* ]] && continue
  achados="$(cd "$ARVORE" && grep -rIniE --exclude="*.svg" -- "$re" . | sed 's|^\./||' \
    | grep -v '^scripts/espelho-fonte\.\(proibidos\|permitidos\):' || true)"
  if [ -n "$achados" ] && [ -n "$PERMITIDOS" ]; then
    achados="$(printf '%s\n' "$achados" | grep -vE -f <(printf '%s\n' "$PERMITIDOS" | grep -v '^#' | grep -v '^$') || true)"
  fi
  if [ -n "$achados" ]; then echo "PROIBIDO ($re):" >&2; printf '%s\n' "$achados" | head -10 >&2; falhou=1; fi
done <<< "$PROIBIDOS"

if command -v gitleaks >/dev/null; then
  gitleaks dir --no-banner "$ARVORE" >&2 || { echo "gitleaks achou segredo" >&2; falhou=1; }
else
  echo "aviso (--teste): gitleaks não instalado; só a lista própria (espelho-fonte.proibidos) foi usada" >&2
fi
[ "$falhou" = 0 ] || { echo "espelho NÃO gerado: corrija na main e faça outra tag" >&2; exit 1; }

git -C "$ARVORE" init -q -b main
git -C "$ARVORE" add -A
git -C "$ARVORE" -c user.name="Grupo IVM" -c user.email="dev@voipmundo.com.br" \
  commit -q -m "Nexus desktop $TAG — fonte correspondente (GPLv3)"
git -C "$ARVORE" tag "$TAG"
tar -C "$DEST" --exclude=.git -czf "$DEST/nexus-softphone-desktop-src-$TAG.tar.gz" --transform "s|^repo|nexus-softphone-desktop-src-$TAG|" repo

# fonte dos submódulos (linphone-sdk e externos), um .tar.gz por submódulo, para o Release
if [ "$COM_SUB" = 1 ]; then
  while read -r caminho sha; do
    nome="$(basename "$caminho")-$sha"
    embutir "$RAIZ/$caminho" "$sha" "$DEST/sub.tmp/$nome"
    tar -C "$DEST/sub.tmp" -czf "$DEST/$nome.tar.gz" "$nome"
    rm -rf "$DEST/sub.tmp/$nome"
  done <<< "$SUBS"
  rmdir "$DEST/sub.tmp"
fi
[ -z "$QT_FONTE" ] || cp -- "$QT_FONTE" "$DEST/"
( cd "$DEST" && sha256sum ./*.tar.gz $( [ -n "$QT_FONTE" ] && basename "$QT_FONTE" ) > SHA256SUMS )
ok=1

echo "OK: $DEST ($( [ "$COM_SUB" = 1 ] && echo 'com' || echo 'sem') fonte dos submódulos)"
echo "  linphone-sdk $SDK_COMMIT -> https://github.com/BelledonneCommunications/linphone-sdk/tree/$SDK_COMMIT"
[ "$TESTE" = 1 ] && { echo "TESTE: não publicar"; exit 0; }
[ "$SEM_TERCEIROS" = 1 ] && echo "SEM o fonte dos submódulos e da Qt: antes de distribuir, gere com --com-submodulos --qt-fonte e anexe ao Release"
echo "Publicar SÓ a tag (nunca push/force na main do espelho):"
echo "  git -C '$ARVORE' push <url do nexus-softphone-desktop-src> refs/tags/$TAG"
echo "  Release da tag: anexar $(cd "$DEST" && ls | grep -v '^repo$' | tr '\n' ' ')"
