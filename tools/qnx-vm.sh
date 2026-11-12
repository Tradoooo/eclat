#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — VM QNX 8.0 pour QEMU : construction, démarrage, arrêt, état.
#
# Enveloppe fine autour des scripts du dépôt amont wamr-qnx (Apache-2.0, © 2026 Selman Altınay,
# voir NOTICE) : scripts/vm/make-image.sh construit l'image avec le mkqnximage du SDP, et la ligne
# de lancement QEMU reprend la leur. Ce script ajoute ce qui manquait pour un usage répété sur
# Linux : génération de la clé SSH, détection de KVM, attente de sshd, port configurable.
#
# Usage : tools/qnx-vm.sh {build|start|stop|status|console}
#   QNX_VM_DIR   répertoire de la VM        (défaut .deps/qnx-vm)
#   QNX_SSH_PORT port SSH publié sur l'hôte (défaut 2280)
set -uo pipefail

RACINE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VM="${QNX_VM_DIR:-$RACINE/.deps/qnx-vm}"
PORT="${QNX_SSH_PORT:-2280}"
CLE="$HOME/.ssh/id_ed25519_qnx_eclat"
AMONT="$RACINE/.deps/wamr-qnx"
PID="$VM/output/qemu.pid"
SERIAL="$VM/output/qemu.out"

sdp() { source "$HOME/qnx800/qnxsdp-env.sh" 2>/dev/null || {
  echo "KO : SDP introuvable dans ~/qnx800 — source tools/setup-sdp.sh <chemin>" >&2; exit 2; }; }

cle() {
  [ -f "$CLE.pub" ] && return 0
  echo "== génération d'une clé SSH dédiée à la VM =="
  ssh-keygen -t ed25519 -N "" -C "eclat-qnx-vm" -f "$CLE" >/dev/null || exit 1
}

kvm() { [ -r /dev/kvm ] && [ -w /dev/kvm ] && echo "-enable-kvm -cpu host" || echo ""; }

cmd_build() {
  sdp; cle
  [ -f "$AMONT/scripts/vm/make-image.sh" ] || {
    echo "KO : dépôt amont absent — cd .deps && git clone https://github.com/altinayselman/wamr-qnx" >&2
    exit 2; }
  bash "$AMONT/scripts/vm/make-image.sh" "$VM" "$CLE.pub"
}

cmd_start() {
  [ -f "$VM/output/ifs.bin" ] && [ -s "$VM/output/ifs.bin" ] || {
    echo "KO : image absente ou vide — lancez tools/qnx-vm.sh build" >&2; exit 2; }
  if [ -f "$PID" ] && kill -0 "$(cat "$PID")" 2>/dev/null; then
    echo "déjà démarrée (pid $(cat "$PID"))"; return 0; fi
  rm -f "$SERIAL"
  # shellcheck disable=SC2046
  qemu-system-x86_64 $(kvm) -smp 2 -m 1G \
    -drive file="$VM/output/disk-qemu",format=raw,if=ide,id=drv0 \
    -kernel "$VM/output/ifs.bin" \
    -netdev user,id=net0,hostfwd=tcp:127.0.0.1:"$PORT"-:22 \
    -device virtio-net-pci,netdev=net0 \
    -object rng-random,filename=/dev/urandom,id=rng0 -device virtio-rng-pci,rng=rng0 \
    -display none -serial file:"$SERIAL" -pidfile "$PID" -daemonize
  echo "== attente de sshd sur 127.0.0.1:$PORT =="
  for _ in $(seq 1 30); do
    if ssh -i "$CLE" -p "$PORT" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
           -o ConnectTimeout=3 root@127.0.0.1 'true' 2>/dev/null; then
      echo "VM prête : ssh -i $CLE -p $PORT root@127.0.0.1"
      ssh -i "$CLE" -p "$PORT" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
          root@127.0.0.1 'uname -a' 2>/dev/null
      return 0
    fi
    sleep 2
  done
  echo "KO : sshd n'a pas répondu en 60 s — voir $SERIAL" >&2
  tail -5 "$SERIAL" 2>/dev/null >&2
  return 1
}

cmd_stop() {
  [ -f "$PID" ] || { echo "aucun pid enregistré"; return 0; }
  kill "$(cat "$PID")" 2>/dev/null && echo "VM arrêtée (pid $(cat "$PID"))" || echo "déjà arrêtée"
  rm -f "$PID"
}

cmd_status() {
  if [ -f "$PID" ] && kill -0 "$(cat "$PID")" 2>/dev/null; then
    echo "en marche (pid $(cat "$PID")), ssh 127.0.0.1:$PORT"
    grep -m1 "Startup complete" "$SERIAL" 2>/dev/null && echo "démarrage terminé"
  else
    echo "arrêtée"
  fi
  [ -s "$VM/output/ifs.bin" ] && echo "ifs.bin : $(stat -c%s "$VM/output/ifs.bin") octets" \
                               || echo "ifs.bin : absent ou vide"
}

cmd_console() { tail -n "${1:-40}" "$SERIAL" 2>/dev/null || echo "pas de console"; }

case "${1:-status}" in
  build)   cmd_build ;;
  start)   cmd_start ;;
  stop)    cmd_stop ;;
  status)  cmd_status ;;
  console) shift; cmd_console "${1:-40}" ;;
  *) echo "usage : $0 {build|start|stop|status|console}" >&2; exit 2 ;;
esac