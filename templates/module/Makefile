# SPDX-License-Identifier: Apache-2.0
# {{MODULE}} — construit et teste le module sur Linux, sans licence QNX ni SDP.
#
#   make test    compile le harnais natif et rejoue le module contre le vrai moteur de décision
#   make wasm    compile le module en .wasm (facultatif : exige wasi-sdk)
#   make clean
#
# ECLAT_ROOT pointe vers le dépôt Éclat (renseigné à la génération). Le harnais se lie à
# build/libeclat.a : c'est le MÊME moteur de décision que le courtier, jamais une copie.

ECLAT_ROOT ?= {{ECLAT_ROOT}}
CC         ?= cc
CFLAGS     ?= -std=c11 -O2 -Wall -Wextra -Werror -I$(ECLAT_ROOT)/core/include
WASI_SDK   ?= $(HOME)/wasi-sdk
WASM_CC     = $(WASI_SDK)/bin/clang

.PHONY: all test wasm clean

all: test

test: harness
	./harness policy.toml

harness: harness.c module.c module_imports.h
	@$(MAKE) -s -C $(ECLAT_ROOT) core
	$(CC) $(CFLAGS) harness.c module.c $(ECLAT_ROOT)/build/libeclat.a -o harness

# Facultatif : produit un objet wasm32. L'édition finale en .wasm attend le lot L4 (runtime).
wasm:
	@if [ -x "$(WASM_CC)" ]; then \
		$(WASM_CC) --target=wasm32 -O2 -Wall -Wextra -Werror -c module.c -o {{MODULE}}.o; \
		echo "objet wasm32 produit : {{MODULE}}.o"; \
		echo "l'édition finale en .wasm sera disponible avec le runtime (lot L4)"; \
	else \
		echo "wasi-sdk absent ($(WASI_SDK)) — cible 'wasm' indisponible."; \
		echo "Le test natif 'make test' ne dépend PAS de wasi-sdk."; \
		exit 1; \
	fi

clean:
	rm -f harness {{MODULE}}.o {{MODULE}}.wasm