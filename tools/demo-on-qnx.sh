#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — démonstration sur cible QNX 8.0 et capture de la preuve.
#
# Construit le lot de démonstration (eclat-run + les 3 modules .wasm + une politique par module,
# empreinte calculée ici), le copie sur la cible, exécute les quatre scénarios et enregistre la
# console telle quelle dans docs/preuves-qnx/.
#
# Prérequis : build/qnx-<arch>/eclat-run construit (tools/build-qnx.sh), build/wasm/*.wasm
#             (make wasm), et une cible QNX joignable en SSH.
# Usage : QNX_SSH_PORT=2280 QNX_SSH_KEY=~/.ssh/id_ed25519_qnx_eclat tools/demo-on-qnx.sh [arch]
set -uo pipefail

RACINE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$RACINE"

ARCH="${1:-x86_64}"
HOTE="${QNX_SSH_HOST:-127.0.0.1}"
PORT="${QNX_SSH_PORT:-2280}"
CLE="${QNX_SSH_KEY:-$HOME/.ssh/id_ed25519_qnx_eclat}"
DISTANT="${QNX_SSH_DEST:-/tmp/eclat}"
BIN="build/qnx-$ARCH/eclat-run"
LOT="$(mktemp -d)"
PREUVES="docs/preuves-qnx"

SSH="ssh -i $CLE -p $PORT -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

for f in "$BIN" build/wasm/honneste.wasm build/wasm/hostile.wasm build/wasm/gourmand.wasm; do
  [ -f "$f" ] || { echo "KO : $f absent" >&2; exit 2; }
done

# Politique par module. Le budget CPU n'est posé que pour le gourmand : c'est lui qui doit être tué.
ecrire_politique() {  # ecrire_politique <nom> <sha256> <budget_ms>
  cat > "$LOT/$1.toml" <<EOF
[module]
name = "$1"
sha256 = "$2"
cpu_budget_ms_per_s = $3
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

cp "$BIN" build/wasm/*.wasm "$LOT/"
for m in honneste hostile gourmand; do
  budget=0
  [ "$m" = gourmand ] && budget=300
  ecrire_politique "$m" "$(sha256sum "$LOT/$m.wasm" | cut -d' ' -f1)" "$budget"
done
# Module falsifié : un seul octet retourné, politique inchangée → ECL-07.
cp "$LOT/honneste.wasm" "$LOT/falsifie.wasm"
printf '\xff' | dd of="$LOT/falsifie.wasm" bs=1 seek=200 conv=notrunc status=none
cp "$LOT/honneste.toml" "$LOT/falsifie.toml"

mkdir -p "$PREUVES"
SORTIE="$PREUVES/$(date +%Y-%m-%d)-qnx800-$ARCH.txt"

{
  echo "# Éclat — démonstration sur cible QNX 8.0"
  echo "# Capturée le $(date -Is) par tools/demo-on-qnx.sh"
  echo "# Hôte : $HOTE:$PORT (SSH), cible $DISTANT, architecture $ARCH"
  echo "# Binaire : $BIN ($(stat -c%s "$BIN") octets)"
  echo "# Aucune sortie n'est retouchée : c'est la console de la cible, stderr fusionné sur place."
  echo "# (les journaisons partent sur stderr, le bilan sur stdout : l'ordre est celui de la cible)"
  echo
  echo "════════ Identification de la cible ════════"
  $SSH root@"$HOTE" 'uname -a; sysinfo 2>/dev/null | head -2' 2>&1 | grep -v '^Warning'
  echo
  echo "════════ Transfert ════════"
  $SSH root@"$HOTE" "mkdir -p $DISTANT" 2>&1 | grep -v '^Warning'
  scp -i "$CLE" -P "$PORT" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -q \
      "$LOT"/* root@"$HOTE":"$DISTANT"/ && echo "7 fichiers transférés"
  echo
  echo "════════ ECL-04 : module honnête (tout est autorisé) ════════"
  $SSH root@"$HOTE" "cd $DISTANT && ./eclat-run honneste.toml honneste.wasm 2>&1; echo code=\$?" 2>&1 | grep -v '^Warning'
  echo
  echo "════════ ECL-05 : module hostile (4 accès interdits) ════════"
  $SSH root@"$HOTE" "cd $DISTANT && ./eclat-run hostile.toml hostile.wasm 2>&1; echo code=\$?" 2>&1 | grep -v '^Warning'
  echo
  echo "════════ ECL-06 : module gourmand (boucle sans fin, budget 300 ms) ════════"
  $SSH root@"$HOTE" "cd $DISTANT && time ./eclat-run gourmand.toml gourmand.wasm 2>&1; echo code=\$?" 2>&1 | grep -v '^Warning'
  echo
  echo "════════ ECL-07 : module falsifié (1 octet modifié) ════════"
  $SSH root@"$HOTE" "cd $DISTANT && ./eclat-run falsifie.toml falsifie.wasm 2>&1; echo code=\$?" 2>&1 | grep -v '^Warning'
} | tee "$SORTIE"

rm -rf "$LOT"
echo
echo "Preuve enregistrée : $SORTIE"