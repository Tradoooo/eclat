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
CFLAGS_QNX=(-c -D_QNX_SOURCE -DWASM_HAVE_MREMAP=0
            -I"$PLAT/linux" -I"$PLAT/include")

FICHIERS=(
  "$PLAT/common/posix/posix_clock.c"
  "$PLAT/common/posix/posix_time.c"
  "$PLAT/common/posix/posix_thread.c"
  "$PLAT/common/posix/posix_sleep.c"
  "$PLAT/common/posix/posix_blocking_op.c"
  "$PLAT/common/posix/posix_malloc.c"
  "$PLAT/common/posix/posix_memmap.c"
  "$PLAT/common/posix/posix_file.c"
  "$PLAT/common/posix/posix_socket.c"
  "$PLAT/common/math/math.c"
  "$PLAT/linux/platform_init.c"
)

MODE="${1:-les-deux}"
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
if [ "$ok" -eq "$total" ]; then
  echo "VERDICT E2 : OUI — la couche POSIX de WAMR compile pour QNX telle quelle"
  exit 0
elif [ "$ok" -gt 0 ]; then
  echo "VERDICT E2 : PARTIEL — $((total - ok)) écart(s) POSIX à résoudre (voir build/e2/)"
  exit 1
else
  echo "VERDICT E2 : NON — rien ne compile, hypothèse de portage à revoir"
  exit 1
fi