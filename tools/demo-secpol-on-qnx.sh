#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — construit et lance l'image QNX 8.0 qui prouve ECL-14 (politique de sécurité).
#
# L'image est une IFS QNX 8.0 x86_64 autonome : noyau, shell, outils de sécurité, les
# binaires Éclat, et la politique de sécurité COMPILEE à /proc/boot/secpol.bin. Au démarrage,
# `secpolpush` la pousse dans procnto, puis le scénario mesure ce que le noyau autorise et
# refuse à un processus typé. La console série est la preuve.
#
# Prérequis : SDP QNX 8.0 (qcc, mkifs, secpolcompile) et un qemu-system-x86_64.
#
# Usage : tools/demo-secpol-on-qnx.sh [x86_64]
set -euo pipefail

RACINE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$RACINE"

ARCH="${1:-x86_64}"
[ "$ARCH" = "x86_64" ] || { echo "KO : seule l'image x86_64 a été éprouvée" >&2; exit 2; }

IMG="$RACINE/.deps/secpol-img"
OUT="$RACINE/docs/preuves/qnx800"
SORTIE="$OUT/$(date +%Y-%m-%d)-secpol-ecl14.txt"

[ -n "${QNX_HOST:-}" ] && [ -n "${QNX_TARGET:-}" ] || {
  echo "KO : SDP absent — source tools/setup-sdp.sh" >&2; exit 2; }
command -v secpolcompile >/dev/null || {
  echo "KO : secpolcompile absent du PATH (il est dans \$QNX_HOST/usr/bin)" >&2; exit 2; }
# Le lanceur QEMU peut être désigné explicitement (QEMU=...) : la construction de l'image et
# son lancement n'ont pas besoin d'être sur la même machine, ni le même QEMU.
QEMU="${QEMU:-qemu-system-x86_64}"
command -v "$QEMU" >/dev/null || {
  echo "KO : $QEMU absent (QEMU=/chemin/vers/qemu-system-x86_64 pour le désigner)" >&2; exit 2; }

mkdir -p "$IMG" "$OUT"

echo "== 1/4 compilation de la politique (secpolcompile) =="
# La politique source est versionnée ; le binaire compilé est régénéré à chaque fois, pour
# qu'aucune dérive ne s'installe entre la source et ce qui est poussé dans le noyau.
secpolcompile -m -o "$IMG/secpol.bin" docs/secpol/eclat.secpol
cp docs/secpol/eclat.secpol "$IMG/politique.secpol"
cp docs/secpol/demo-secpol.sh "$IMG/demo-secpol.sh"
cp docs/secpol/pol-broker.toml "$IMG/pol-broker.toml"
echo "   secpol.bin : $(stat -c%s "$IMG/secpol.bin") octets"

echo "== 2/4 compilation des binaires QNX (qcc) =="
CFLAGS_QNX=(-std=c11 -O2 -Wall -Wextra -Werror -D_QNX_SOURCE
             -Icore/include -Iplatform/include -Iruntime -Ibroker/include)
mkdir -p "$IMG/obj"
CORE_SRCS=(core/src/decision.c core/src/policy.c core/src/toml.c core/src/names.c
           core/src/log.c core/src/budget.c core/src/sha256.c)
CORE_OBJS=()
for f in "${CORE_SRCS[@]}"; do
  o="$IMG/obj/$(basename "$f" .c).o"
  "$QNX_HOST/usr/bin/qcc" -Vgcc_ntox86_64 "${CFLAGS_QNX[@]}" -c "$f" -o "$o"
  CORE_OBJS+=("$o")
done
"$QNX_HOST/usr/bin/qcc" -Vgcc_ntox86_64 "${CFLAGS_QNX[@]}" \
  "${CORE_OBJS[@]}" broker/eclat_broker.c -lsocket -lm -o "$IMG/eclat-broker"
"$QNX_HOST/usr/bin/qcc" -Vgcc_ntox86_64 "${CFLAGS_QNX[@]}" \
  "${CORE_OBJS[@]}" broker/eclat_client.c -lsocket -lm -o "$IMG/eclat-client"
"$QNX_HOST/usr/bin/qcc" -Vgcc_ntox86_64 "${CFLAGS_QNX[@]}" \
  broker/sonde.c -lsocket -lm -o "$IMG/eclat-sonde"
echo "   4 binaires produits"

echo "== 3/4 construction de l'image IFS (mkifs) =="
# Le buildfile est versionné : il dit exactement ce qui est dans l'image.
sed "s|__QNX_TARGET__|$QNX_TARGET|g" tools/image-secpol.buildfile > "$IMG/buildfile"
( cd "$IMG" && "$QNX_HOST/usr/bin/mkifs" buildfile ifs.bin )
echo "   ifs.bin : $(stat -c%s "$IMG/ifs.bin") octets"

echo "== 4/4 démarrage de la cible et capture de la console série =="
CONSOLE="$(mktemp)"
# --cpu max est obligatoire : qemu64 ne démarre pas un invité QNX 8.0.
# -serial file : la console série est la preuve, brute et datée.
timeout 120 "$QEMU" -cpu max -smp 2 -m 1024 -kernel "$IMG/ifs.bin" \
  -display none -serial "file:$CONSOLE" >/dev/null 2>&1 || true

if [ ! -s "$CONSOLE" ]; then
  echo "KO : console série vide — l'invité n'a pas démarré" >&2
  rm -f "$CONSOLE"
  exit 1
fi
cp "$CONSOLE" "$SORTIE"
rm -f "$CONSOLE"

# Verdict : on ne déclare pas la preuve acquise sans la lire.
if grep -q "name_open(intrus    ) : REFUSÉ" "$SORTIE" &&
   grep -q "name_open(cap       ) : OUVERT" "$SORTIE" &&
   grep -q "procmgr_ability(DENY|LOCK|EOL) : APPLIQUÉ" "$SORTIE"; then
  echo "   VERDICT : le runtime joint le courtier et se voit refuser le temoin -> ECL-14 démontré"
else
  echo "   VERDICT : la preuve attendue n'est PAS dans la console — voir $SORTIE" >&2
  exit 1
fi
echo "   preuve enregistrée : $SORTIE"
