#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — essai E2 : « la couche plateforme POSIX de WAMR compile-t-elle pour QNX ? »
#
# Compile, avec le vrai compilateur QNX (qcc), chacun des fichiers de la couche d'abstraction
# POSIX de WAMR, pour x86_64 puis aarch64. Un verdict par fichier, jamais un verdict global
# inventé : E2 est « OUI » si tout compile, « PARTIEL » si certains fichiers passent, « NON »
# sinon. Les erreurs sont conservées dans build/e2/ pour itération.
#
# Prérequis : un SDP QNX 8.0 installé et QNX_HOST / QNX_TARGET posés
#             (source qnxsdp-env.sh, ou tools/setup-sdp.sh <chemin-du-sdp>).
# Usage : tools/e2-probe.sh [x86_64|aarch64|les-deux]
set -uo pipefail

RACINE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WAMR="$RACINE/.deps/wasm-micro-runtime"
PLAT="$WAMR/core/shared/platform"
OUT="$RACINE/build/e2"
mkdir -p "$OUT"

if [ -z "${QNX_HOST:-}" ] || [ -z "${QNX_TARGET:-}" ]; then
  echo "KO : QNX_HOST / QNX_TARGET absents." >&2
  echo "     Source le SDP :  source <sdp>/qnxsdp-env.sh   (ou tools/setup-sdp.sh)" >&2
  exit 2
fi
QCC="$QNX_HOST/usr/bin/qcc"
[ -x "$QCC" ] || { echo "KO : qcc introuvable en $QCC" >&2; exit 2; }

# -D_QNX_SOURCE : l'analogue QNX de _GNU_SOURCE (le cmake amont pose _GNU_SOURCE pour Linux).
# -DWASM_HAVE_MREMAP=0 : QNX n'a pas mremap(2) ; le cmake amont ne le pose que sous Linux.
# Chemins d'en-têtes : ceux du cmake amont (platform/linux + platform/include) plus les
# en-têtes internes que ce même cmake ajoute via platform_api_posix.cmake et utils.
UTILS="$WAMR/core/shared/utils"
COMMON="$WAMR/core/shared/platform/common"
CFLAGS_QNX=(-c -D_QNX_SOURCE -DWASM_HAVE_MREMAP=0
            -I"$PLAT/linux" -I"$PLAT/include"
            -I"$COMMON/posix" -I"$COMMON/libc-util"
            -I"$UTILS" -I"$WAMR/core/iwasm/include" -I"$WAMR/core/iwasm/common")

# Jeu de fichiers EXACTEMENT celui que le cmake amont retient pour NOTRE configuration
# (INTERP=1, FAST_INTERP=1, AOT=0, LIBC_WASI=0, DEBUG_INTERP=0, mremap absent).
# Lecture de common/posix/platform_api_posix.cmake :
#   - si LIBC_WASI != 1 → posix_file.c et posix_clock.c sont RETIRÉS du build
#   - si LIBC_WASI != 1 ET DEBUG_INTERP != 1 → posix_socket.c est RETIRÉ, et libc-util n'est
#     PAS inclus (donc libc_errno.c hors build)
#   - si mremap absent (WASM_HAVE_MREMAP=0) → common/memory/ est AJOUTÉ au build
# common/math/math.c n'est référencé par aucun de ces cmake : il ne fait pas partie du build.
# Tester autre chose que ce jeu serait un verdict hors sujet.
FICHIERS=(
  "$PLAT/common/posix/posix_time.c"
  "$PLAT/common/posix/posix_thread.c"
  "$PLAT/common/posix/posix_sleep.c"
  "$PLAT/common/posix/posix_blocking_op.c"
  "$PLAT/common/posix/posix_malloc.c"
  "$PLAT/common/posix/posix_memmap.c"
  "$PLAT/common/memory/mremap.c"
  "$PLAT/linux/platform_init.c"
)

# --- Correctifs amont optionnels -------------------------------------------------
# --patched applique upstream/wamr/patches/ au clone amont, lance la sonde, puis restaure
# l'arbre (le clone reste vierge : on ne soumet jamais un fork, seulement des patches).
APPLIQUE_PATCH=0
MODE="les-deux"
for arg in "$@"; do
  case "$arg" in
    --patched) APPLIQUE_PATCH=1 ;;
    x86_64|aarch64|les-deux) MODE="$arg" ;;
    *) echo "argument inconnu : $arg (attendu : --patched | x86_64 | aarch64)" >&2; exit 2 ;;
  esac
done

RESTAURER=""
# La sonde doit être déterministe : on part toujours d'un clone amont vierge, que les correctifs
# aient déjà été appliqués par tools/build-qnx.sh ou non. Sans ça, le verdict dépendrait de
# l'état du dépôt au lieu de dépendre du drapeau --patched.
if ! git -C "$WAMR" diff --quiet 2>/dev/null; then
  echo "== clone amont modifié : restauration à l'état vierge =="
  git -C "$WAMR" checkout -- . 2>/dev/null
fi

if [ "$APPLIQUE_PATCH" -eq 1 ]; then
  for p in "$RACINE"/upstream/wamr/patches/*.patch; do
    [ -e "$p" ] || continue
    echo "== application de $(basename "$p")"
    git -C "$WAMR" apply -p1 "$p" || { echo "KO : le patch ne s'applique pas" >&2; exit 2; }
  done
  RESTAURER=1
  trap 'git -C "$WAMR" checkout -- . 2>/dev/null' EXIT
fi

CIBLES=()
case "$MODE" in
  x86_64)   CIBLES=(x86_64) ;;
  aarch64)  CIBLES=(aarch64le) ;;
  *)        CIBLES=(x86_64 aarch64le) ;;
esac

echo "qcc      : $("$QCC" --version 2>/dev/null | head -1)"
echo "QNX_HOST : $QNX_HOST"
echo "QNX_TARGET: $QNX_TARGET"
echo

total=0; ok=0
for cible in "${CIBLES[@]}"; do
  echo "=== cible nto : $cible ==="
  for f in "${FICHIERS[@]}"; do
    nom="$(basename "$f" .c)"
    log="$OUT/${cible}-${nom}.log"
    total=$((total + 1))
    if "$QCC" "-Vgcc_nto${cible}" "${CFLAGS_QNX[@]}" "$f" -o "$OUT/${cible}-${nom}.o" \
         >"$log" 2>&1; then
      ok=$((ok + 1))
      printf '  ok      %-22s\n' "$nom"
    else
      printf '  ECHEC   %-22s  (voir %s)\n' "$nom" "$log"
      grep -m3 -E "error|Error" "$log" | sed 's/^/            /'
    fi
  done
  echo
done

echo "E2 : $ok / $total fichiers de la couche POSIX compilent avec qcc"
[ "$APPLIQUE_PATCH" -eq 1 ] && echo "(sonde lancée avec les correctifs de upstream/wamr/patches/ appliqués)"
if [ "$ok" -eq "$total" ]; then
  echo "VERDICT E2 : OUI — la couche POSIX de WAMR compile pour QNX"
  [ "$APPLIQUE_PATCH" -eq 0 ] && echo "               (telle quelle, sans aucun correctif)"
  exit 0
elif [ "$ok" -gt 0 ]; then
  echo "VERDICT E2 : PARTIEL — $((total - ok)) écart(s) POSIX à résoudre (voir build/e2/)"
  echo "               relance avec --patched pour voir l'effet des correctifs proposés"
  exit 1
else
  echo "VERDICT E2 : NON — rien ne compile, hypothèse de portage à revoir"
  exit 1
fi