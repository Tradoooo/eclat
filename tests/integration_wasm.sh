#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — test d'intégration du runtime WAMR sur Linux.
#   ECL-04 : un module .wasm s'exécute sous la couche POSIX et appelle les imports
#   ECL-05 : le module hostile reçoit des refus, et chaque refus est journalisé
#   ECL-06 : le module gourmand est tué par le budget CPU
#   ECL-07 : un octet modifié dans le module refuse le chargement
# Aucune cible QNX, aucun SDP : tout se joue sur la couche platform/posix.
set -uo pipefail

RACINE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$RACINE"

RUN=build/bin/eclat-run
WASM=build/wasm
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

passes=0
echecs=0

verifier() {  # verifier <nom> <condition:0=ok>
  if [ "$2" -eq 0 ]; then
    passes=$((passes + 1)); printf '  ok    %s\n' "$1"
  else
    echecs=$((echecs + 1)); printf '  ECHEC %s\n' "$1"
  fi
}

politique() {  # politique <fichier> <nom> <sha256> <cpu_ms> <calls>
  cat > "$1" <<EOF
[module]
name = "$2"
sha256 = "$3"
cpu_budget_ms_per_s = $4
calls_per_s = $5

[[allow]]
cap = "read_sensor"
ids = [1]

[[allow]]
cap = "write_actuator"
ids = [3]
value_milli_range = [-1000, 1000]
EOF
}

[ -x "$RUN" ] || { echo "KO : $RUN absent (make runtime)"; exit 1; }

for m in honnete gourmand hostile; do
  src="$WASM/$(echo "$m" | sed 's/honnete/honneste/')*.wasm"
  f=$(ls $src 2>/dev/null | head -1)
  [ -n "$f" ] || { echo "KO : module $m absent (make wasm)"; exit 1; }
done

echo "== ECL-04 : module honnête =="
f="$WASM/honneste.wasm"
politique "$TMP/honneste.toml" "honneste" "$(sha256sum "$f" | cut -d' ' -f1)" 0 0
"$RUN" "$TMP/honneste.toml" "$f" > "$TMP/hon.out" 2> "$TMP/hon.err"
rc=$?
verifier "le module s'exécute (code 0)" $rc
grep -q "appels au courtier : [1-9]" "$TMP/hon.out"; verifier "il appelle bien le courtier" $?
grep -q "refus              : 0" "$TMP/hon.out"; verifier "aucun refus pour un module honnête" $?
grep -q "code du module     : 0" "$TMP/hon.out"; verifier "run() retourne 0" $?

echo "== ECL-05 : module hostile =="
f="$WASM/hostile.wasm"
politique "$TMP/hostile.toml" "hostile" "$(sha256sum "$f" | cut -d' ' -f1)" 0 0
"$RUN" "$TMP/hostile.toml" "$f" > "$TMP/hos.out" 2> "$TMP/hos.err"
rc=$?
verifier "le module hostile survit à ses refus (code 0)" $rc
grep -q "CAP_DENIED_POLICY" "$TMP/hos.err"; verifier "accès non autorisé → CAP_DENIED_POLICY" $?
grep -q "CAP_DENIED_RANGE" "$TMP/hos.err"; verifier "valeur hors plage → CAP_DENIED_RANGE" $?
n=$(grep -c "CAP_DENIED" "$TMP/hos.err")
[ "$n" -ge 4 ]; verifier "au moins 4 refus journalisés ($n)" $?

echo "== ECL-06 : module gourmand, budget CPU de 200 ms =="
f="$WASM/gourmand.wasm"
politique "$TMP/gourmand.toml" "gourmand" "$(sha256sum "$f" | cut -d' ' -f1)" 200 0
debut=$(date +%s%N)
"$RUN" "$TMP/gourmand.toml" "$f" > "$TMP/gou.out" 2> "$TMP/gou.err"
fin=$(date +%s%N)
duree_ms=$(( (fin - debut) / 1000000 ))
grep -q "tué par le budget  : OUI" "$TMP/gou.out"
verifier "la boucle sans fin est tuée par le budget CPU" $?
[ "$duree_ms" -lt 5000 ]; verifier "tuée en moins de 5 s (mesuré : ${duree_ms} ms)" $?

echo "== ECL-07 : module falsifié =="
cp "$WASM/honneste.wasm" "$TMP/falsifie.wasm"
printf '\xff' | dd of="$TMP/falsifie.wasm" bs=1 seek=200 conv=notrunc status=none
politique "$TMP/falsifie.toml" "honneste" "$(sha256sum "$WASM/honneste.wasm" | cut -d' ' -f1)" 0 0
"$RUN" "$TMP/falsifie.toml" "$TMP/falsifie.wasm" > "$TMP/fal.out" 2> "$TMP/fal.err"
rc=$?
[ "$rc" -ne 0 ]; verifier "un octet modifié refuse le chargement" $?
grep -qi "empreinte" "$TMP/fal.err"; verifier "le motif cite l'empreinte" $?

echo
echo "intégration wasm : $passes ok, $echecs échec(s)"
[ "$echecs" -eq 0 ] || exit 1
echo "OK $passes vérifications"