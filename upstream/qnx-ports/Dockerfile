# SPDX-License-Identifier: Apache-2.0
# Environnement de construction minimal pour le port WAMR/QNX.
#
# ⚠ Le SDP QNX 8.0 n'est JAMAIS inclus dans cette image : il est sous licence QNX et sa
#   redistribution est interdise. Il est monté depuis l'hôte au moment de la construction :
#
#     docker build -t eclat-wamr-build upstream/qnx-ports
#     docker run --rm \
#       -v "$HOME/qnx800:/opt/qnx800:ro" \
#       -v "$HOME/.qnx:/root/.qnx:ro" \
#       -v "$PWD:/work" -w /work \
#       eclat-wamr-build make -C build-files/ports/wamr
#
# L'arbre des ports fournit sa propre image (docker/README.md amont) : en production, c'est
# elle qu'il faut préférer. Celle-ci sert à reproduire l'environnement hors de l'arbre.

FROM ubuntu:24.04

# Outils vérifiés nécessaires : cmake et ninja pour WAMR, git pour le clone amont,
# python3 pour les gardes du dépôt Éclat, make pour la recette.
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        python3 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Le SDP pose ces deux variables ; la recette CMake échoue explicitement si elles sont absentes
# (comportement vérifié dans ports/zlib/qnx.nto.toolchain.cmake).
ENV QNX_HOST="" \
    QNX_TARGET=""

# Version épinglée dans VERSIONS à la racine du dépôt.
ARG WAMR_VERSION=WAMR-2.4.5

WORKDIR /work

CMD ["/bin/bash"]
