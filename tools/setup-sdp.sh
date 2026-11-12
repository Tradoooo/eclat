#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — branchement d'un SDP QNX installé.
#
# Localise qnxsdp-env.sh dans une arborescence de SDP, le source, vérifie que qcc répond,
# et affiche les deux variables que tout le reste consomme (QNX_HOST, QNX_TARGET).
# Ne télécharge rien, n'installe rien, ne touche à aucune licence : il suppose le SDP déjà
# présent sur la machine (déposé par un humain, cf. docs/sdp-handover.md).
#
# Usage :  source tools/setup-sdp.sh [chemin-du-sdp]
#          (source, pas exécution : les variables doivent survivre dans ton shell)
set -uo pipefail

CANDIDATS=("$@" "$HOME/qnx800" "$HOME/qnxsdp" "/opt/qnx800" "/opt/qnx" "$HOME/sdp/qnx800")
ENVSH=""
for c in "${CANDIDATS[@]}"; do
  [ -z "$c" ] && continue
  for p in "$c/qnxsdp-env.sh" "$c/usr/qnxsdp-env.sh" "$c"/*/qnxsdp-env.sh; do
    if [ -f "$p" ]; then ENVSH="$p"; break 2; fi
  done
done

if [ -z "$ENVSH" ]; then
  echo "KO : qnxsdp-env.sh introuvable parmi : ${CANDIDATS[*]}" >&2
  echo "     Passe le chemin du SDP en argument :  source tools/setup-sdp.sh /chemin/vers/sdp" >&2
  return 1 2>/dev/null || exit 1
fi

# shellcheck disable=SC1090
source "$ENVSH"
echo "SDP trouvé : $ENVSH"

QCC="${QNX_HOST:-}/usr/bin/qcc"
if [ -n "${QNX_HOST:-}" ] && [ -x "$QCC" ]; then
  echo "qcc        : $("$QCC" --version 2>/dev/null | head -1)"
  echo "QNX_HOST   : $QNX_HOST"
  echo "QNX_TARGET : $QNX_TARGET"
  echo
  echo "Prêt pour l'essai E2 :  tools/e2-probe.sh"
else
  echo "KO : qcc absent de ${QCC:-<QNX_HOST indéfini>}" >&2
  return 1 2>/dev/null || exit 1
fi