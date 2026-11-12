# SPDX-License-Identifier: Apache-2.0
# Fichier d'outillage CMake pour QNX 8.0 (nto), pour la construction de WAMR.
#
# Suit la convention de l'arbre des ports QNX : chaque port embarque sa propre copie de ce
# fichier, qui lit les deux variables que common.mk fournit :
#   CMAKE_SYSTEM_PROCESSOR        → nom des binutils : nto${CMAKE_SYSTEM_PROCESSOR}-ar
#   CMAKE_SYSTEM_PROCESSOR_ENDIAN → variante de qcc   : -Vgcc_nto${CMAKE_SYSTEM_PROCESSOR_ENDIAN}
#
# Ces deux valeurs ne portent pas le même nom sur toutes les cibles : sur aarch64, le premier
# vaut "aarch64" (d'où ntoaarch64-ar) et le second "aarch64le" (d'où -Vgcc_ntoaarch64le).
# common.mk fait donc la correspondance, pas ce fichier.
#
# Basé sur ports/zlib/qnx.nto.toolchain.cmake de l'arbre (même rôle, même contenu utile).

if("$ENV{QNX_HOST}" STREQUAL "")
    message(FATAL_ERROR "QNX_HOST environment variable not found. Please set the variable to your host's build tools")
endif()
if("$ENV{QNX_TARGET}" STREQUAL "")
    message(FATAL_ERROR "QNX_TARGET environment variable not found. Please set the variable to the qnx target location")
endif()

if(CMAKE_HOST_WIN32)
    set(HOST_EXECUTABLE_SUFFIX ".exe")
    #convert windows paths to cmake paths
    file(TO_CMAKE_PATH "$ENV{QNX_HOST}" QNX_HOST)
    file(TO_CMAKE_PATH "$ENV{QNX_TARGET}" QNX_TARGET)
else()
    set(QNX_HOST "$ENV{QNX_HOST}")
    set(QNX_TARGET "$ENV{QNX_TARGET}")
endif()

message(STATUS "using QNX_HOST ${QNX_HOST}")
message(STATUS "using QNX_TARGET ${QNX_TARGET}")

set(QNX TRUE)
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_C_COMPILER ${QNX_HOST}/usr/bin/qcc)
set(CMAKE_CXX_COMPILER ${QNX_HOST}/usr/bin/q++)
set(CMAKE_ASM_COMPILER ${QNX_HOST}/usr/bin/qcc)
set(CMAKE_AR "${QNX_HOST}/usr/bin/nto${CMAKE_SYSTEM_PROCESSOR}-ar${HOST_EXECUTABLE_SUFFIX}" CACHE PATH "archiver")
set(CMAKE_RANLIB "${QNX_HOST}/usr/bin/nto${CMAKE_SYSTEM_PROCESSOR}-ranlib${HOST_EXECUTABLE_SUFFIX}" CACHE PATH "ranlib")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Vgcc_nto${CMAKE_SYSTEM_PROCESSOR_ENDIAN} ${EXTRA_CMAKE_C_FLAGS}" CACHE STRING "c_flags")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Vgcc_nto${CMAKE_SYSTEM_PROCESSOR_ENDIAN} ${EXTRA_CMAKE_CXX_FLAGS}" CACHE STRING "cxx_flags")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Vgcc_nto${CMAKE_SYSTEM_PROCESSOR_ENDIAN} ${EXTRA_CMAKE_ASM_FLAGS}" CACHE STRING "asm_flags")

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EXTRA_CMAKE_LINKER_FLAGS}" CACHE STRING "exe_linker_flags")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${EXTRA_CMAKE_LINKER_FLAGS}" CACHE STRING "so_linker_flags")