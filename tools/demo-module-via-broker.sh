#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — preuve de l'étape « les imports du module passent par le courtier ».
#
# Sur la cible QNX 8.0 : on démarre le courtier, puis on exécute les modules AVEC --broker cap.
# Chaque import du module devient alors un MsgSend vers le courtier, qui répond par un code de
# décision. Le bilan affiché par eclat-run (appels, refus) provient donc du courtier, d'un
# AUTRE processus.
#
# Usage : QNX_SSH_PORT=2280 tools/demo-module-via-broker.sh [arch]
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

for b in eclat-broker eclat-run; do
  [ -f "build/qnx-$ARCH/$b" ] || { echo "KO : build/qnx-$ARCH/$b absent (make qnx)" >&2; exit 2; }
done

mkdir -p "$SORTIE_DIR"
SORTIE="$SORTIE_DIR/$(date +%Y-%m-%d)-module-via-courtier.txt"

# Une politique : capteur 1 autorisé, actionneur 3 borné, réseau interdit.
politique() {  # politique <nom-fichier> <sha256>
  cat > "$1" <<EOF
[module]
name = "via-broker"
sha256 = "$2"
cpu_budget_ms_per_s = 0
calls_per_s = 0

[[allow]]
cap = "read_sensor"
ids = [1]

[[allow]]
cap = "write_actuator"
ids = [3]
value_milli_range = [-1000, 1000]
EOF
}
politique "$HOME/tmp-honn.toml" "$(sha256sum build/wasm/honneste.wasm | cut -d' ' -f1)"
politique "$HOME/tmp-host.toml" "$(sha256sum build/wasm/hostile.wasm | cut -d' ' -f1)"

{
  echo "# Éclat — les imports du module passent par le courtier"
  echo "# Capturée le $(date -Is) par tools/demo-module-via-broker.sh"
  echo "# Hôte : $HOTE:$PORT (SSH), cible $DISTANT, architecture $ARCH"
  echo "# eclat-run est lancé avec --broker cap : chaque import du module devient un MsgSend."
  echo "# Le bilan (appels, refus) est donc la trace du courtier, d'un AUTRE processus."
  echo "# Aucune sortie n'est retouchée."
  echo
  echo "════════ Identification de la cible ════════"
  $SSH root@"$HOTE" 'uname -a' 2>&1 | grep -v '^Warning'
  echo
  echo "════════ Transfert ════════"
  $SSH root@"$HOTE" "rm -rf $DISTANT && mkdir -p $DISTANT" 2>&1 | grep -v '^Warning'
  $SCP -q build/qnx-$ARCH/eclat-broker build/qnx-$ARCH/eclat-run \
    build/wasm/honneste.wasm build/wasm/hostile.wasm \
    "$HOME/tmp-honn.toml" "$HOME/tmp-host.toml" \
    root@"$HOTE":"$DISTANT/" && echo "6 fichiers transférés"
  echo
  echo "════════ Démarrage du courtier ════════"
  $SSH root@"$HOTE" "cd $DISTANT && slay -f eclat-broker 2>/dev/null; slay -f eclat-run 2>/dev/null; \
    (./eclat-broker tmp-honn.toml > courtier.log 2>&1 &); sleep 2; cat courtier.log" \
    2>&1 | grep -v '^Warning'
  echo
  echo "════════ ECL-04 via courtier : module honnête ════════"
  $SSH root@"$HOTE" "cd $DISTANT && ./eclat-run tmp-honn.toml honneste.wasm --broker cap 2>&1; echo code=\$?" \
    2>&1 | grep -v '^Warning'
  echo
  echo "════════ ECL-05 via courtier : module hostile ════════"
  $SSH root@"$HOTE" "cd $DISTANT && ./eclat-run tmp-host.toml hostile.wasm --broker cap 2>&1; echo code=\$?" \
    2>&1 | grep -v '^Warning'
  echo
  echo "════════ Arrêt du courtier ════════"
  $SSH root@"$HOTE" "slay -f eclat-broker 2>/dev/null; sleep 1; echo 'arrêté'" 2>&1 | grep -v '^Warning'
} | tee "$SORTIE"

rm -f "$HOME/tmp-honn.toml" "$HOME/tmp-host.toml"
echo
echo "Preuve enregistrée : $SORTIE"