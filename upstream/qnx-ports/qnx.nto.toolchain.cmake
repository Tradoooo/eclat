# SPDX-License-Identifier: Apache-2.0
# Fichier d'outillage CMake pour QNX 8.0 (nto), inspiré de celui de l'arbre des ports QNX
# (ports/zlib/qnx.nto.toolchain.cmake, Apache-2.0) et adapté : les deux variables
# CMAKE_SYSTEM_PROCESSOR et CMAKE_SYSTEM_PROCESSOR_ENDIAN sont posées explicitement parce que
# les noms d'outils et les variantes de qcc ne suivent pas la même convention.
#
#   CMAKE_SYSTEM_PROCESSOR        → nom des binutils : nto${CMAKE_SYSTEM_PROCESSOR}-ar
#   CMAKE_SYSTEM_PROCESSOR_ENDIAN → variante de qcc   : -Vgcc_nto${CMAKE_SYSTEM_PROCESSOR_ENDIAN}
#
# Valeurs vérifiées dans le SDP 8.0 (ls $QNX_HOST/usr/bin, qcc -V) :
#   x86_64   : PROCESSOR=x86_64  ENDIAN=x86_64    (ntox86_64-ar,   -Vgcc_ntox86_64)
#   aarch64  : PROCESSOR=aarch64 ENDIAN=aarch64le (ntoaarch64-ar,  -Vgcc_ntoaarch64le)
#
# Exige QNX_HOST et QNX_TARGET dans l'environnement (source tools/setup-sdp.sh).
# Nécessite cmake ≥ 3.31 (module Platform/QNX) ; en dessous, utiliser -DCMAKE_SYSTEM_NAME=Generic.

if("$ENV{QNX_HOST}" STREQUAL "")
    message(FATAL_ERROR "QNX_HOST absent de l'environnement. Sourcez le SDP : source tools/setup-sdp.sh")
endif()
if("$ENV{QNX_TARGET}" STREQUAL "")
    message(FATAL_ERROR "QNX_TARGET absent de l'environnement. Sourcez le SDP : source tools/setup-sdp.sh")
endif()

set(QNX_HOST "$ENV{QNX_HOST}")
set(QNX_TARGET "$ENV{QNX_TARGET}")

# Cible par défaut : x86_64. Surcharge : -DQNX_TARGET_ARCH=aarch64
if(NOT DEFINED QNX_TARGET_ARCH)
    set(QNX_TARGET_ARCH "x86_64")
endif()

if(QNX_TARGET_ARCH STREQUAL "aarch64" OR QNX_TARGET_ARCH STREQUAL "aarch64le")
    set(CMAKE_SYSTEM_PROCESSOR "aarch64")
    set(CMAKE_SYSTEM_PROCESSOR_ENDIAN "aarch64le")
else()
    set(CMAKE_SYSTEM_PROCESSOR "x86_64")
    set(CMAKE_SYSTEM_PROCESSOR_ENDIAN "x86_64")
endif()

message(STATUS "QNX_HOST   = ${QNX_HOST}")
message(STATUS "QNX_TARGET = ${QNX_TARGET}")
message(STATUS "cible      = ${CMAKE_SYSTEM_PROCESSOR} (qcc -Vgcc_nto${CMAKE_SYSTEM_PROCESSOR_ENDIAN})")

set(QNX TRUE)
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 8.0.0)

set(CMAKE_C_COMPILER   ${QNX_HOST}/usr/bin/qcc)
set(CMAKE_CXX_COMPILER ${QNX_HOST}/usr/bin/q++)
set(CMAKE_ASM_COMPILER ${QNX_HOST}/usr/bin/qcc)
set(CMAKE_AR      "${QNX_HOST}/usr/bin/nto${CMAKE_SYSTEM_PROCESSOR}-ar"      CACHE PATH "archiver")
set(CMAKE_RANLIB  "${QNX_HOST}/usr/bin/nto${CMAKE_SYSTEM_PROCESSOR}-ranlib"  CACHE PATH "ranlib")

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Vgcc_nto${CMAKE_SYSTEM_PROCESSOR_ENDIAN} ${EXTRA_CMAKE_C_FLAGS}"   CACHE STRING "c_flags")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Vgcc_nto${CMAKE_SYSTEM_PROCESSOR_ENDIAN} ${EXTRA_CMAKE_CXX_FLAGS}" CACHE STRING "cxx_flags")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Vgcc_nto${CMAKE_SYSTEM_PROCESSOR_ENDIAN} ${EXTRA_CMAKE_ASM_FLAGS}" CACHE STRING "asm_flags")

set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} ${EXTRA_CMAKE_LINKER_FLAGS}"    CACHE STRING "exe_linker_flags")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${EXTRA_CMAKE_LINKER_FLAGS}" CACHE STRING "so_linker_flags")

# Recherche : jamais dans l'arborescence hôte (on croise-compile), toujours dans le SDP.
set(CMAKE_FIND_ROOT_PATH ${QNX_TARGET})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)