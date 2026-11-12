#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — la boucle 1 kHz continue de tourner pendant qu'un module hostile est chargé puis tué.
#
# Trois mesures de gigue, capturées telle quelle :
#   1. à vide (baseline) ;
#   2. pendant qu'un module gourmand (boucle sans fin) est chargé par eclat-run et TUÉ par le
#      superviseur de budget ;
#   3. après.
#
# ⚠ HONNÊTETÉ : sous QEMU/KVM, la gigue absolue est dominée par l'émulation (pas de vrai périphérique
#   timer, pas d'isolation de vCPU). Ces nombres ne sont PAS une preuve de performance temps réel ;
#   ce sont la preuve que la boucle CONTINUE de tourner, sans s'arrêter ni s'écrouler, pendant la
#   mise à mort d'un module. Les chiffres exacts demandent une cible matérielle.
#
# Usage : QNX_SSH_PORT=2280 tools/demo-boucle.sh [arch]
set -uo pipefail

RACINE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$RACINE"

ARCH="${1:-x86_64}"
HOTE="${QNX_SSH_HOST:-127.0.0.1}"
PORT="${QNX_SSH_PORT:-2280}"
CLE="${QNX_SSH_KEY:-$HOME/.ssh/id_ed25519_qnx_eclat}"
DISTANT="${QNX_SSH_DEST:-/tmp/eclat-broker}"
SORTIE_DIR="docs/preuves/qnx800"

SSH="ssh -i $CLE -p $PORT -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"
SCP="scp -i $CLE -P $PORT -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

for b in eclat-broker eclat-boucle eclat-run; do
  [ -f "build/qnx-$ARCH/$b" ] || { echo "KO : build/qnx-$ARCH/$b absent (make qnx)" >&2; exit 2; }
done

mkdir -p "$SORTIE_DIR"
SORTIE="$SORTIE_DIR/$(date +%Y-%m-%d)-boucle-1khz.txt"

# Politique du gourmand : un budget CPU court, pour qu'il soit tué pendant la mesure.
sha=$(sha256sum build/wasm/gourmand.wasm | cut -d' ' -f1)
printf '[module]\nname = "gourmand"\nsha256 = "%s"\ncpu_budget_ms_per_s = 300\ncalls_per_s = 0\n\n[[allow]]\ncap = "read_sensor"\nids = [1]\n' "$sha" > /tmp/gourd.toml

# Politique du courtier (pour la boucle) : lecture du capteur autorisée, actionneur borné.
printf '[module]\nname = "broker-demo"\nsha256 = "%s"\ncpu_budget_ms_per_s = 100\ncalls_per_s = 0\n\n[[allow]]\ncap = "read_sensor"\nids = [1]\n\n[[allow]]\ncap = "write_actuator"\nids = [3]\nvalue_milli_range = [-1000, 1000]\n' \
  "$(printf 'ab%.0s' $(seq 32))" > /tmp/pol-broker.toml

{
  echo "# Éclat — la boucle 1 kHz et la mise à mort d'un module gourmand"
  echo "# Capturée le $(date -Is) par tools/demo-boucle.sh"
  echo "# Hôte : $HOTE:$PORT (SSH), cible $DISTANT, architecture $ARCH"
  echo "# ⚠ Sous QEMU/KVM : la gigue absolue est dominée par l'émulation. Ce qui est prouvé ici,"
  echo "#   c'est que la boucle CONTINUE de tourner pendant la mise à mort d'un module ennemi."
  echo "# Aucune sortie n'est retouchée."
  echo
  echo "════════ Identification de la cible ════════"
  $SSH root@"$HOTE" 'uname -a' 2>&1 | grep -v '^Warning'
  echo
  echo "════════ Transfert ════════"
  $SSH root@"$HOTE" "mkdir -p $DISTANT" 2>&1 | grep -v '^Warning'
  $SCP -q build/qnx-$ARCH/eclat-broker build/qnx-$ARCH/eclat-boucle build/qnx-$ARCH/eclat-run \
    build/wasm/gourmand.wasm /tmp/gourd.toml /tmp/pol-broker.toml root@"$HOTE":"$DISTANT/" \
    && echo "6 fichiers transférés"
  echo
  echo "════════ Démarrage du courtier ════════"
  $SSH root@"$HOTE" "cd $DISTANT && slay -f eclat-broker 2>/dev/null; slay -f eclat-run 2>/dev/null; \
    (./eclat-broker pol-broker.toml > courtier.log 2>&1 &); sleep 2; cat courtier.log" \
    2>&1 | grep -v '^Warning'
  echo
  echo "════════ 1. baseline (boucle seule) ════════"
  $SSH root@"$HOTE" "cd $DISTANT && ./eclat-boucle 3000" 2>&1 | grep -v '^Warning'
  echo
  echo "════════ 2. pendant la mise à mort du module gourmand ════════"
  $SSH root@"$HOTE" "cd $DISTANT && (./eclat-run gourd.toml gourmand.wasm --broker cap >/dev/null 2>&1 &); \
    ./eclat-boucle 3000" 2>&1 | grep -v '^Warning'
  echo
  echo "════════ 3. après ════════"
  $SSH root@"$HOTE" "cd $DISTANT && ./eclat-boucle 3000" 2>&1 | grep -v '^Warning'
  echo
  echo "════════ Arrêt ════════"
  $SSH root@"$HOTE" "slay -f eclat-broker 2>/dev/null; slay -f eclat-run 2>/dev/null; echo arrêté" \
    2>&1 | grep -v '^Warning'
} | tee "$SORTIE"

rm -f /tmp/gourd.toml
echo
echo "Preuve enregistrée : $SORTIE"