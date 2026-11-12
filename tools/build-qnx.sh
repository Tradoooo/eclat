#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Éclat — construction pour QNX 8.0 (lot L7).
#
# Enchaîne : application des correctifs amont → configuration cmake avec le toolchain QNX →
# compilation de WAMR pour la cible → compilation de core/ + platform/qnx/ + runtime/ + target/
# avec qcc → édition de liens de `eclat-run`.
#
# Prérequis : SDP 8.0 sourcé (source tools/setup-sdp.sh) et dépendances présentes (make deps).
# Usage : tools/build-qnx.sh [x86_64|aarch64]        (défaut : x86_64)
set -euo pipefail

RACINE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$RACINE"

ARCH="${1:-x86_64}"
case "$ARCH" in
  x86_64)  VARIANT=gcc_ntox86_64 ; WAMR_TARGET=X86_64 ;;
  aarch64|aarch64le) ARCH=aarch64 ; VARIANT=gcc_ntoaarch64le ; WAMR_TARGET=AARCH64 ;;
  *) echo "architecture inconnue : $ARCH (x86_64 | aarch64)" >&2 ; exit 2 ;;
esac

WAMR="${ECLAT_WAMR_SRC:-$RACINE/.deps/wasm-micro-runtime}"
mkdir -p "$RACINE/build"
WAMR_INC="$WAMR/core/iwasm/include"
CMAKE_BIN="$RACINE/.deps/cmake-venv/bin/cmake"
TOOLCHAIN="$RACINE/upstream/qnx-ports/qnx.nto.toolchain.cmake"
BUILD_WAMR="$RACINE/.deps/wamr-qnx-build-$ARCH"
OUT="$RACINE/build/qnx-$ARCH"

# ECLAT_WAMR_LIB : bibliothèque WAMR/QNX déjà construite — par exemple le portage de la PR amont
# #5200 (scripts/build-qnx.sh de .deps/wamr-qnx). Quand elle est fournie, on n'applique PAS nos
# correctifs et on ne reconstruit PAS WAMR : nos patches porteraient sur un arbre déjà modifié.
WAMR_LIB="${ECLAT_WAMR_LIB:-$BUILD_WAMR/libiwasm.a}"
if [ -n "${ECLAT_WAMR_LIB:-}" ]; then SAUTE_WAMR=1; else SAUTE_WAMR=0; fi

[ -n "${QNX_HOST:-}" ] && [ -n "${QNX_TARGET:-}" ] || {
  echo "KO : QNX_HOST/QNX_TARGET absents — source tools/setup-sdp.sh d'abord" >&2; exit 2; }
[ -x "$QNX_HOST/usr/bin/qcc" ] || { echo "KO : qcc introuvable" >&2; exit 2; }
[ -d "$WAMR/.git" ] || { echo "KO : WAMR absent — lancez make deps" >&2; exit 2; }
[ -x "$CMAKE_BIN" ] || { echo "KO : cmake ≥ 3.31 absent (.deps/cmake-venv) — voir README" >&2; exit 2; }

echo "== 1/5 correctifs amont =="
if [ "$SAUTE_WAMR" -eq 1 ]; then
  echo "   ignorés : bibliothèque WAMR fournie ($WAMR_LIB)"
else
shopt -s nullglob
for p in "$RACINE"/upstream/wamr/patches/*.patch; do
  if git -C "$WAMR" apply --check -p1 "$p" 2>/dev/null; then
    git -C "$WAMR" apply -p1 "$p"
    echo "   appliqué  $(basename "$p")"
  elif git -C "$WAMR" apply --check -R -p1 "$p" 2>/dev/null; then
    echo "   déjà là   $(basename "$p")"
  else
    echo "   KO        $(basename "$p") ne s'applique ni en avant ni en arrière" >&2
    exit 1
  fi
done
shopt -u nullglob
fi

if [ "$SAUTE_WAMR" -eq 1 ]; then
  echo "== 2-3/5 WAMR : bibliothèque fournie, construction ignorée =="
  [ -f "$WAMR_LIB" ] || { echo "KO : $WAMR_LIB introuvable" >&2; exit 2; }
  ls -la "$WAMR_LIB"
  mkdir -p "$OUT"
else
echo "== 2/5 configuration cmake (cible $ARCH) =="
# cmake ≥ 3.31 fournit Platform/QNX ; CMAKE_POLICY_VERSION_MINIMUM accommode les
# cmake_minimum_required anciens de WAMR sous cmake 4.x.
"$CMAKE_BIN" -S "$WAMR/product-mini/platforms/linux" -B "$BUILD_WAMR" \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DQNX_TARGET_ARCH="$ARCH" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
  -DWAMR_BUILD_INTERP=1 -DWAMR_BUILD_FAST_INTERP=1 \
  -DWAMR_BUILD_AOT=0 -DWAMR_BUILD_JIT=0 \
  -DWAMR_BUILD_LIBC_BUILTIN=1 -DWAMR_BUILD_LIBC_WASI=0 -DWAMR_BUILD_THREAD_MGR=0 \
  -DWAMR_BUILD_TARGET="$WAMR_TARGET" -DWAMR_BUILD_PLATFORM=linux > "$OUT-cmake.log" 2>&1 || {
    echo "KO : configuration cmake en échec, voir $OUT-cmake.log" >&2; tail -15 "$OUT-cmake.log" >&2; exit 1; }
mkdir -p "$OUT"
echo "   ok (journal : $OUT-cmake.log)"

echo "== 3/5 compilation de WAMR pour QNX =="
if [ -f "$BUILD_WAMR/libiwasm.a" ]; then
  echo "   libiwasm.a déjà présente"
else
  make -C "$BUILD_WAMR" -j"$(nproc)" > "$OUT-wamr.log" 2>&1 || {
    echo "KO : build WAMR en échec, voir $OUT-wamr.log" >&2
    grep -m5 -iE "error|cannot find" "$OUT-wamr.log" >&2 || true; exit 1; }
  echo "   ok (journal : $OUT-wamr.log)"
fi
ls -la "$BUILD_WAMR"/libiwasm.a
fi

echo "== 4/5 compilation d'Éclat pour QNX =="
CFLAGS_QNX=(-std=c11 -O2 -Wall -Wextra -Werror -D_QNX_SOURCE
            -Icore/include -Iplatform/include -Iruntime -I"$WAMR_INC")
mkdir -p "$OUT/obj"
nb=0
for f in core/src/*.c platform/qnx/*.c runtime/*.c target/*.c; do
  "$QNX_HOST/usr/bin/qcc" "-V$VARIANT" "${CFLAGS_QNX[@]}" -c "$f" \
      -o "$OUT/obj/$(echo "$f" | tr '/' '_').o"
  nb=$((nb + 1))
done
echo "   $nb fichiers compilés avec qcc -V$VARIANT"

echo "== 5/5 édition de liens =="
BIN="$OUT/eclat-run"
# -lsocket : sur QNX l'API socket BSD est dans libsocket (le shared_platform.cmake du port amont
# fait link_libraries(socket m)). -lm : libm.so.3 existe bien sur QNX.
"$QNX_HOST/usr/bin/qcc" "-V$VARIANT" "${CFLAGS_QNX[@]}" \
  "$OUT"/obj/*.o "$WAMR_LIB" -lsocket -lm -o "$BIN"
echo
echo "== résultat =="
file "$BIN" | cut -c1-140
ls -la "$BIN"
echo
echo "Binaire QNX produit : $BIN"
echo "Interpréteur attendu : /usr/lib/ldqnx-64.so.2"
echo
echo "Pour l'exécuter sur cible :"
echo "  tools/qnx-vm.sh build && tools/qnx-vm.sh start   # image QNX 8.0 QEMU (mkqnximage)"
echo "  tools/demo-on-qnx.sh $ARCH                       # les 4 scénarios, preuve archivée"