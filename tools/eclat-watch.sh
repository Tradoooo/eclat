#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — observer la cible QNX en temps réel : un battement toutes les 2 secondes.
#
# Démarre (ou réutilise) le courtier, puis lance « eclat-pouls » sur la cible et en affiche la
# sortie au fil de l'eau : l'uptime du noyau avance et le capteur (sinus 1 Hz) évolue — la preuve
# que ce n'est pas une capture figée. Ctrl-C pour arrêter.
#
# Usage : QNX_SSH_PORT=2280 tools/eclat-watch.sh [arch]
set -uo pipefail

RACINE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$RACINE"

ARCH="${1:-x86_64}"
HOTE="${QNX_SSH_HOST:-127.0.0.1}"
PORT="${QNX_SSH_PORT:-2280}"
CLE="${QNX_SSH_KEY:-$HOME/.ssh/id_ed25519_qnx_eclat}"
DISTANT="${QNX_SSH_DEST:-/tmp/eclat-broker}"

SSH="ssh -i $CLE -p $PORT -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"
SCP="scp -i $CLE -P $PORT -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

for b in eclat-broker eclat-pouls; do
  [ -f "build/qnx-$ARCH/$b" ] || { echo "KO : build/qnx-$ARCH/$b absent (make qnx)" >&2; exit 2; }
done

# Politique du courtier : lecture du capteur autorisée.
POL="[module]\nname = \"watch\"\nsha256 = \"$(printf 'ab%.0s' $(seq 32))\"\ncpu_budget_ms_per_s = 100\ncalls_per_s = 0\n\n[[allow]]\ncap = \"read_sensor\"\nids = [1]\n"
printf "$POL" > /tmp/watch-pol.toml

echo "== préparation de la cible =="
# On tue les éventuels pouls/courtier restants AVANT le transfert : écraser un binaire en cours
# d'exécution échoue (« fichier occupé »), et un pouls orphelin finirait par battre en double.
$SSH root@"$HOTE" "slay -f eclat-pouls 2>/dev/null; slay -f eclat-broker 2>/dev/null; sleep 1; mkdir -p $DISTANT" \
  2>&1 | grep -v '^Warning'
$SCP -q build/qnx-$ARCH/eclat-broker build/qnx-$ARCH/eclat-pouls /tmp/watch-pol.toml \
  root@"$HOTE":"$DISTANT/" || { echo "KO: transfert"; exit 2; }

echo "== démarrage du courtier =="
$SSH root@"$HOTE" "cd $DISTANT && (./eclat-broker watch-pol.toml > watch.log 2>&1 &); sleep 2; cat watch.log" \
  2>&1 | grep -v '^Warning'

echo "== pouls en direct (Ctrl-C pour arrêter) =="
echo
# Le pouls tourne en boucle infinie sur la cible ; sa sortie nous revient EN CONTINU.
# Pas de `grep` ici : un grep dans un tube bufferiserait la sortie et le flux cesserait d'être
# « temps réel » dès que la sortie n'est pas un terminal.
$SSH root@"$HOTE" "cd $DISTANT && ./eclat-pouls 2000"

rm -f /tmp/watch-pol.toml