#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — récupération et construction reproductibles des dépendances externes.
#
# Tout atterrit dans .deps/ (jamais committé, cf. .gitignore) : ce dépôt ne contient que la
# couche de portage et le courtier, pas l'historique amont — les règles de la catégorie
# Open-Source Contribution & Ports interdisent de soumettre un fork.
#
# L'empreinte sha256 du tarball wasi-sdk est vérifiée avant extraction : la chaîne
# d'approvisionnement commence ici.
#
# Usage : tools/fetch-deps.sh [--skip-build]
set -euo pipefail

RACINE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS="$RACINE/.deps"

# Versions épinglées — voir VERSIONS à la racine du dépôt.
WAMR_TAG="WAMR-2.4.5"
WAMR_URL="https://github.com/bytecodealliance/wasm-micro-runtime"
WASI_SDK_TAG="wasi-sdk-34"
WASI_SDK_FILE="wasi-sdk-34.0-x86_64-linux.tar.gz"
WASI_SDK_URL="https://github.com/WebAssembly/wasi-sdk/releases/download/${WASI_SDK_TAG}/${WASI_SDK_FILE}"
# Empreinte vérifiée le 2026-11-12 sur le fichier réellement téléchargé (192 383 077 octets).
WASI_SDK_SHA256="b761e3a0721dbae9c09a0059e5fdb2bf917d1b4a8a7b430fb3b5aafb0984b2c4"

# Options WAMR conformes au périmètre du cahier des charges (§3.2) : interpréteur seul, pas de
# JIT, pas d'AOT, libc interne, pas de WASI, pas de threads Wasm.
# Noms d'options vérifiés dans doc/build_wamr.md amont.
WAMR_CMAKE_OPTS=(
  -DCMAKE_BUILD_TYPE=Release
  -DWAMR_BUILD_INTERP=1
  -DWAMR_BUILD_FAST_INTERP=1
  -DWAMR_BUILD_AOT=0
  -DWAMR_BUILD_JIT=0
  -DWAMR_BUILD_LIBC_BUILTIN=1
  -DWAMR_BUILD_LIBC_WASI=0
  -DWAMR_BUILD_THREAD_MGR=0
  -DWAMR_BUILD_TARGET=X86_64
  -DWAMR_BUILD_PLATFORM=linux
)

mkdir -p "$DEPS"

if [ ! -d "$DEPS/wasm-micro-runtime/.git" ]; then
  echo "== clone WAMR ${WAMR_TAG} =="
  git clone --depth 1 --branch "$WAMR_TAG" "$WAMR_URL" "$DEPS/wasm-micro-runtime"
else
  echo "== WAMR déjà cloné =="
fi

if ! find "$DEPS" -maxdepth 1 -name 'wasi-sdk-*' -type d | grep -q .; then
  echo "== téléchargement wasi-sdk ${WASI_SDK_TAG} (~192 Mo) =="
  curl -sL -o "$DEPS/$WASI_SDK_FILE" "$WASI_SDK_URL"
  echo "== vérification de l'empreinte sha256 =="
  if ! echo "${WASI_SDK_SHA256}  ${DEPS}/${WASI_SDK_FILE}" | sha256sum -c -; then
    echo "KO : empreinte du tarball wasi-sdk invalide — téléchargement refusé"
    rm -f "$DEPS/$WASI_SDK_FILE"
    exit 1
  fi
  tar xf "$DEPS/$WASI_SDK_FILE" -C "$DEPS"
  rm -f "$DEPS/$WASI_SDK_FILE"
else
  echo "== wasi-sdk déjà présent =="
fi

if [ "${1:-}" = "--skip-build" ]; then
  echo "== construction WAMR ignorée (--skip-build) =="
  exit 0
fi

BUILD="$DEPS/wasm-micro-runtime/product-mini/platforms/linux/build"
if [ -f "$BUILD/libiwasm.a" ]; then
  echo "== libiwasm.a déjà construite =="
else
  echo "== construction WAMR (interpréteur, x86_64, linux) =="
  mkdir -p "$BUILD"
  cmake -S "$DEPS/wasm-micro-runtime/product-mini/platforms/linux" -B "$BUILD" \
        "${WAMR_CMAKE_OPTS[@]}"
  make -C "$BUILD" -j"$(nproc)"
fi

echo
echo "Dépendances prêtes :"
echo "  WAMR     : $DEPS/wasm-micro-runtime  ($WAMR_TAG)"
echo "  libiwasm : $BUILD/libiwasm.a"
echo "  wasi-sdk : $(find "$DEPS" -maxdepth 1 -name 'wasi-sdk-*' -print -quit)"
echo
echo "Ensuite : make wasm && make runtime && make test-wasm"