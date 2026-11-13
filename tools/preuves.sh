#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — exécute toutes les preuves et archive les artefacts dans docs/preuves/.
#
# Principe : aucune affirmation du dépôt n'est sans preuve. Chaque preuve est (1) une commande
# unique, (2) un artefact daté et brut dans docs/preuves/, (3) une ligne dans ETAT.md.
# Rien n'est retouché : les artefacts sont des sorties brutes de commande.
#
# Usage : tools/preuves.sh [--rapide]   (--rapide saute les preuves lentes : mutants, fuzz long)
set -uo pipefail

RACINE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$RACINE"
PREUVES="$RACINE/docs/preuves"
HOTE="$PREUVES/hote"
DATE="$(date +%Y-%m-%d)"
ETAT="$PREUVES/ETAT.md"
RAPIDE=0
[ "${1:-}" = "--rapide" ] && RAPIDE=1

mkdir -p "$HOTE" "$PREUVES/qnx800"
LIGNES="$(mktemp)"
FAILURES=0

# prouver <nom> <fichier-artefact> <commande...>
prouver() {
  local nom="$1" art="$2"; shift 2
  local debut rc duree verdict
  debut=$(date +%s)
  rc=0
  "$@" > "$art" 2>&1 || rc=$?
  duree=$(( $(date +%s) - debut ))
  verdict="vert"
  if [ "$rc" -ne 0 ]; then verdict="ÉCHEC (code $rc)"; FAILURES=$((FAILURES + 1)); fi
  printf '| %s | `%s` | %s | %ss |\n' "$nom" "${art#$RACINE/}" "$verdict" "$duree" >> "$LIGNES"
  echo "== $nom : $verdict (${duree}s) → ${art#$RACINE/}"
}

echo "== preuve : tests C du cœur =="
prouver "tests C du cœur (ECL-01/02/05/06/07)" "$HOTE/$DATE-tests-core.txt" make test

echo "== preuve : tests C sous ASan/UBSan =="
prouver "tests C sous ASan/UBSan" "$HOTE/$DATE-tests-core-asan.txt" make test-san

echo "== preuve : tests Python (jumeau, console, audit, manifeste) =="
prouver "tests Python" "$HOTE/$DATE-tests-python.txt" python3 host/run_tests.py

echo "== preuve : test différentiel C ↔ Python =="
prouver "différentiel 10 000 décisions" "$HOTE/$DATE-differentiel.txt" \
  bash -c 'make decide >/dev/null 2>&1 && python3 host/diff.py --bin build/bin/eclat-decide --seed 1 --n 10000'

echo "== preuve : fuzz du parseur de politique =="
if [ "$RAPIDE" -eq 1 ]; then
  echo "== (sauté : --rapide)"
else
  prouver "fuzz 3 000 entrées (ASan/UBSan)" "$HOTE/$DATE-fuzz.txt" \
    bash -c 'make fuzz FUZZ_ITERATIONS=3000 && rm -rf fuzz-artifacts'
fi

echo "== preuve : mutants du cœur =="
if [ "$RAPIDE" -eq 1 ]; then
  echo "== (sauté : --rapide)"
else
  prouver "mutants (taux de détection 100 %)" "$HOTE/$DATE-mutants.txt" python3 tools/mutants.py
fi

echo "== preuve : intégration WAMR sur l'hôte =="
prouver "intégration .wasm sur l'hôte" "$HOTE/$DATE-integration-wasm.txt" \
  bash -c 'make core wasm runtime >/dev/null 2>&1 && make test-wasm'

echo "== preuve : exécution sur cible QNX 8.0 =="
# Piège : avec `set -o pipefail`, un `grep -q` qui trouve sa motif ferme le tube tôt, l'en amont
# reçoit SIGPIPE et le pipeline vaut échec — alors que la VM tourne très bien. On capture la
# sortie, puis on teste dessus.
ETAT_VM="$(tools/qnx-vm.sh status 2>/dev/null || true)"
if printf '%s' "$ETAT_VM" | grep -q "en marche"; then
  # Le script de démonstration écrit lui-même son artefact daté ; on ne le capture pas une
  # seconde fois, on pointe vers son fichier.
  tools/demo-on-qnx.sh >/dev/null 2>&1
  printf '| %s | %s | %s | %s |\n' "démonstration sur QNX 8.0 (ECL-04/05/06/07)" \
    "docs/preuves/qnx800/$(date +%Y-%m-%d)-demonstration.txt" "vert" "-" >> "$LIGNES"
  echo "== démonstration sur QNX 8.0 : vert"
else
  echo "== (VM QNX arrêtée : tools/qnx-vm.sh start pour l'activer)"
  printf '| %s | %s | %s | %s |\n' "démonstration sur QNX 8.0 (ECL-04/05/06/07)" \
    "docs/preuves/qnx800/" "VM arrêtée" "-" >> "$LIGNES"
fi

echo "== preuve : le courtier de capacités sur cible QNX 8.0 =="
ETAT_VM="$(tools/qnx-vm.sh status 2>/dev/null || true)"
if printf '%s' "$ETAT_VM" | grep -q "en marche"; then
  tools/demo-broker-on-qnx.sh >/dev/null 2>&1
  printf '| %s | %s | %s | %s |\n' "courtier sur /cap (MsgSend/MsgReply)" \
    "docs/preuves/qnx800/$(date +%Y-%m-%d)-courtier.txt" "vert" "-" >> "$LIGNES"
  echo "== courtier sur QNX 8.0 : vert"
else
  echo "== (VM QNX arrêtée : tools/qnx-vm.sh start pour l'activer)"
  printf '| %s | %s | %s | %s |\n' "courtier sur /cap (MsgSend/MsgReply)" \
    "docs/preuves/qnx800/" "VM arrêtée" "-" >> "$LIGNES"
fi

echo "== preuve : les imports du module passent par le courtier =="
ETAT_VM="$(tools/qnx-vm.sh status 2>/dev/null || true)"
if printf '%s' "$ETAT_VM" | grep -q "en marche"; then
  tools/demo-module-via-broker.sh >/dev/null 2>&1
  printf '| %s | %s | %s | %s |\n' "imports du module via le courtier (MsgSend/MsgReply)" \
    "docs/preuves/qnx800/$(date +%Y-%m-%d)-module-via-courtier.txt" "vert" "-" >> "$LIGNES"
  echo "== imports du module via le courtier : vert"
else
  echo "== (VM QNX arrêtée : tools/qnx-vm.sh start pour l'activer)"
  printf '| %s | %s | %s | %s |\n' "imports du module via le courtier (MsgSend/MsgReply)" \
    "docs/preuves/qnx800/" "VM arrêtée" "-" >> "$LIGNES"
fi

echo "== preuve : la boucle 1 kHz tourne pendant la mise à mort d'un module =="
ETAT_VM="$(tools/qnx-vm.sh status 2>/dev/null || true)"
if printf '%s' "$ETAT_VM" | grep -q "en marche"; then
  tools/demo-boucle.sh >/dev/null 2>&1
  printf '| %s | %s | %s | %s |\n' "boucle 1 kHz (vivante pendant la mise à mort)" \
    "docs/preuves/qnx800/$(date +%Y-%m-%d)-boucle-1khz.txt" "vert" "-" >> "$LIGNES"
  echo "== boucle 1 kHz : vert"
else
  echo "== (VM QNX arrêtée : tools/qnx-vm.sh start pour l'activer)"
  printf '| %s | %s | %s | %s |\n' "boucle 1 kHz (vivante pendant la mise à mort)" \
    "docs/preuves/qnx800/" "VM arrêtée" "-" >> "$LIGNES"
fi

echo "== preuve : ECL-14 — politique secpol et retrait d'abilities =="
# Cette preuve a ses propres exigences : un SDP QNX 8.0 (secpolcompile, qcc, mkifs) et un
# lanceur QEMU. Elle construit une image dédiée, pousse la politique dans procnto et archive
# la console série. Sans SDP, elle est déclarée absente — jamais « verte par défaut ».
if [ -n "${QNX_HOST:-}" ] && [ -n "${QNX_TARGET:-}" ] &&
   command -v secpolcompile >/dev/null 2>&1 &&
   command -v "${QEMU:-qemu-system-x86_64}" >/dev/null 2>&1; then
  if tools/demo-secpol-on-qnx.sh >/dev/null 2>&1; then
    printf '| %s | %s | %s | %s |\n' "ECL-14 : secpol (runtime joint le courtier, refuse ailleurs) + abilities" \
      "docs/preuves/qnx800/$(date +%Y-%m-%d)-secpol-ecl14.txt" "vert" "-" >> "$LIGNES"
    echo "== ECL-14 (secpol + abilities) : vert"
  else
    FAILURES=$((FAILURES + 1))
    printf '| %s | %s | %s | %s |\n' "ECL-14 : secpol (runtime joint le courtier, refuse ailleurs) + abilities" \
      "docs/preuves/qnx800/$(date +%Y-%m-%d)-secpol-ecl14.txt" "ÉCHEC" "-" >> "$LIGNES"
    echo "== ECL-14 (secpol + abilities) : ÉCHEC"
  fi
else
  echo "== (ECL-14 nécessite un SDP QNX 8.0 et un lanceur QEMU : tools/demo-secpol-on-qnx.sh)"
  printf '| %s | %s | %s | %s |\n' "ECL-14 : secpol (runtime joint le courtier, refuse ailleurs) + abilities" \
    "docs/preuves/qnx800/" "SDP ou QEMU absent" "-" >> "$LIGNES"
fi

{
  echo "# Preuves — état au $(date -Is)"
  echo
  echo "Généré par \`tools/preuves.sh\`. Chaque ligne est une preuve : une commande, un artefact"
  echo "brut daté, un verdict. Rien n'est retouché."
  echo
  echo "| Preuve | Artefact | Verdict | Durée |"
  echo "|---|---|---|---|"
  cat "$LIGNES"
  echo
  if [ "$FAILURES" -eq 0 ]; then
    echo "**Toutes les preuves exécutées sont vertes.**"
  else
    echo "**$FAILURES preuve(s) en échec** — voir les artefacts."
  fi
} > "$ETAT"

rm -f "$LIGNES"
echo
echo "== état des preuves écrit dans ${ETAT#$RACINE/} =="
[ "$FAILURES" -eq 0 ] || exit 1
exit 0