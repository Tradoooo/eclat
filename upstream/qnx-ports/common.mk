# SPDX-License-Identifier: Apache-2.0
# common.mk — variables partagées par toutes les cibles du port WAMR.
#
# Structure et variables reprises du port de référence ports/zlib/common.mk, lu le 2026-11-12 :
#   https://github.com/qnx-ports/build-files/blob/main/ports/zlib/common.mk
# Les blocs marqués « HYPOTHÈSE » ne sont pas vérifiables sans le SDP ni un clone de WAMR.

ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

include $(MKFILES_ROOT)/qmacros.mk

NAME=wamr

QNX_PROJECT_ROOT ?= $(PRODUCT_ROOT)/../../

# INSTALL_ROOT_$(OS) pointe vers $QNX_TARGET par défaut, sauf déroutement vers une zone de
# staging en posant à la fois INSTALL_ROOT_nto et USE_INSTALL_ROOT (commentaire du port zlib).
INSTALL_ROOT ?= $(INSTALL_ROOT_$(OS))

# Préfixe vu DE LA CIBLE, distinct d'INSTALL_ROOT qui est un chemin de l'hôte (idem zlib).
PREFIX ?= /usr/local

CMAKE_BUILD_TYPE ?= Release

ALL_DEPENDENCIES = $(NAME)_all
.PHONY: $(NAME)_all install check clean

CFLAGS += $(FLAGS)

# -D_QNX_SOURCE est requis par la libc++ LLVM sur QNX (commentaire du port zlib).
# -O3 remplacé par -O2 : le courtier de capacités vise le déterminisme, pas le débit brut.
CFLAGS += -D_QNX_SOURCE -O2 -fPIC
LDFLAGS += -Wl,--build-id=md5

include $(MKFILES_ROOT)/qtargets.mk

# --- Spécifique WAMR -------------------------------------------------------

# Version épinglée dans VERSIONS à la racine du dépôt Éclat.
WAMR_VERSION   ?= WAMR-2.4.5
WAMR_SRC       ?= $(QNX_PROJECT_ROOT)/wasm-micro-runtime

# HYPOTHÈSE : WAMR se construit par CMake depuis product-mini/platforms/<cible>/build.
# VÉRIFIÉ en revanche : `product-mini/platforms/` ne contient PAS de dossier `qnx`
# (17 cibles : alios-things android common cosmopolitan darwin esp-idf freebsd ios linux-sgx
# linux nuttx posix riot rt-thread vxworks windows zephyr). Créer `qnx` est précisément
# l'objet de la contribution amont ; jusque-là cette recette ne peut pas aboutir.
WAMR_PLATFORM  ?= qnx

# Options de build : noms VÉRIFIÉS dans doc/build_wamr.md amont le 2026-11-12.
# Valeurs choisies conformes au périmètre du cahier des charges (§3.2) : interpréteur seul,
# pas de JIT, pas de WASI complet, libc interne seulement.
WAMR_BUILD_INTERP       ?= 1
WAMR_BUILD_FAST_INTERP  ?= 1
WAMR_BUILD_AOT          ?= 0
WAMR_BUILD_JIT          ?= 0
WAMR_BUILD_LIBC_BUILTIN ?= 1
WAMR_BUILD_LIBC_WASI    ?= 0
WAMR_BUILD_THREAD_MGR   ?= 0

# HYPOTHÈSE : le fichier d'outillage CMake est celui de l'arbre des ports, identique d'un port
# à l'autre (vérifié présent dans ports/zlib/qnx.nto.toolchain.cmake). Il n'est PAS recopié ici :
# le port doit réutiliser celui de l'arbre, pour ne pas diverger.
CMAKE_ARGS += \
	-DCMAKE_TOOLCHAIN_FILE=$(PRODUCT_ROOT)/qnx.nto.toolchain.cmake \
	-DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) \
	-DCMAKE_INSTALL_PREFIX=$(INSTALL_ROOT)$(PREFIX) \
	-DWAMR_BUILD_INTERP=$(WAMR_BUILD_INTERP) \
	-DWAMR_BUILD_FAST_INTERP=$(WAMR_BUILD_FAST_INTERP) \
	-DWAMR_BUILD_AOT=$(WAMR_BUILD_AOT) \
	-DWAMR_BUILD_JIT=$(WAMR_BUILD_JIT) \
	-DWAMR_BUILD_LIBC_BUILTIN=$(WAMR_BUILD_LIBC_BUILTIN) \
	-DWAMR_BUILD_LIBC_WASI=$(WAMR_BUILD_LIBC_WASI) \
	-DWAMR_BUILD_THREAD_MGR=$(WAMR_BUILD_THREAD_MGR)

# HYPOTHÈSE : séquence de construction. À confirmer contre le CMakeLists réel de WAMR une fois
# le clone effectué. Aucune de ces lignes n'a été exécutée à ce jour.
$(NAME)_all:
	@echo "WAMR $(WAMR_VERSION) pour $(NAME) — cible $(notdir $(PRODUCT_ROOT))"
	cd $(WAMR_SRC)/product-mini/platforms/$(WAMR_PLATFORM)/build && \
		cmake $(CMAKE_ARGS) .. && \
		cmake --build . --parallel

install: $(NAME)_all
	cd $(WAMR_SRC)/product-mini/platforms/$(WAMR_PLATFORM)/build && \
		cmake --install .

check: $(NAME)_all
	@echo "Aucun test amont n'est câblé pour une cible QNX à ce jour."

clean:
	rm -rf $(WAMR_SRC)/product-mini/platforms/$(WAMR_PLATFORM)/build/*
