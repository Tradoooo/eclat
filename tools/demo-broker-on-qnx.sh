#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — preuve de l'étape « le courtier sert la politique par message synchrone ».
#
# Sur la cible QNX 8.0 : on démarre le courtier (qui s'enregistre sous /dev/cap), puis on lance
# le client qui pose les mêmes questions qu'un module. Chaque réponse affichée est une décision
# prise par le courtier, de l'autre côté de la frontière IPC.
#
# Usage : QNX_SSH_PORT=2280 tools/demo-broker-on-qnx.sh [arch]
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

for b in eclat-broker eclat-client; do
  [ -f "build/qnx-$ARCH/$b" ] || { echo "KO : build/qnx-$ARCH/$b absent (make qnx)" >&2; exit 2; }
done

mkdir -p "$SORTIE_DIR"
SORTIE="$SORTIE_DIR/$(date +%Y-%m-%d)-courtier.txt"

# Politique du module de démonstration : capteur 1 autorisé, actionneur 3 borné, réseau interdit.
POL="/tmp/pol-broker.toml"
cat > "$POL" <<EOF
[module]
name = "broker-demo"
sha256 = "$(printf 'ab%.0s' $(seq 32))"
cpu_budget_ms_per_s = 100
calls_per_s = 0

[[allow]]
cap = "read_sensor"
ids = [1]

[[allow]]
cap = "write_actuator"
ids = [3]
value_milli_range = [-1000, 1000]
EOF

{
  echo "# Éclat — le courtier de capacités sur cible QNX 8.0"
  echo "# Capturée le $(date -Is) par tools/demo-broker-on-qnx.sh"
  echo "# Hôte : $HOTE:$PORT (SSH), cible $DISTANT, architecture $ARCH"
  echo "# Le courtier s'enregistre sous /dev/cap ; le client lui parle par MsgSend/MsgReply."
  echo "# Aucune sortie n'est retouchée."
  echo
  echo "════════ Identification de la cible ════════"
  $SSH root@"$HOTE" 'uname -a' 2>&1 | grep -v '^Warning'
  echo
  echo "════════ Transfert ════════"
  $SSH root@"$HOTE" "rm -rf $DISTANT && mkdir -p $DISTANT" 2>&1 | grep -v '^Warning'
  # Destination = le RÉPERTOIRE : avec plusieurs sources, scp exige un répertoire
  # (sinon il crée un répertoire portant le nom de la destination et y fourre tout).
  $SCP -q build/qnx-$ARCH/eclat-broker build/qnx-$ARCH/eclat-client "$POL" \
    root@"$HOTE":"$DISTANT/" && echo "3 fichiers transférés"
  echo
  echo "════════ Démarrage du courtier ════════"
  $SSH root@"$HOTE" "cd $DISTANT && (./eclat-broker pol-broker.toml > courtier.log 2>&1 &) ; sleep 2 ; cat courtier.log" \
    2>&1 | grep -v '^Warning'
  echo
  echo "════════ Le client interroge le courtier ════════"
  $SSH root@"$HOTE" "cd $DISTANT && ./eclat-client" 2>&1 | grep -v '^Warning'
  echo
  echo "════════ Journal du courtier ════════"
  $SSH root@"$HOTE" "cd $DISTANT && cat courtier.log" 2>&1 | grep -v '^Warning'
  echo
  echo "════════ Arrêt du courtier ════════"
  # QNX n'a pas pkill : l'outil s'appelle slay.
  $SSH root@"$HOTE" "slay -f eclat-broker 2>/dev/null; sleep 1; echo 'arrêté'" 2>&1 | grep -v '^Warning'
} | tee "$SORTIE"

echo
echo "Preuve enregistrée : $SORTIE"