# Makefile — Éclat. Construit core/, exécute les tests et le différentiel.
# Cible par défaut : core (lib statique + binaires de test).
CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Werror -Icore/include
SAN     ?=
BUILD   := build
LIB     := $(BUILD)/libeclat.a
FUZZ_CFLAGS := -std=c11 -O1 -g -Wall -Wextra -Werror -Icore/include \
               -fsanitize=address,undefined -fno-omit-frame-pointer
FUZZ_ITERATIONS ?= 2000
FUZZ_BIN := $(BUILD)/bin/fuzz-harness
BENCH_BIN := $(BUILD)/bin/eclat-bench

# --- Dépendances externes (WAMR, wasi-sdk) : .deps/, jamais committées ----------------
DEPS      := .deps
WAMR_ROOT := $(DEPS)/wasm-micro-runtime
WAMR_INC  := $(WAMR_ROOT)/core/iwasm/include
WAMR_LIB  := $(WAMR_ROOT)/product-mini/platforms/linux/build/libiwasm.a
WASI_SDK  := $(firstword $(wildcard $(DEPS)/wasi-sdk-*))
WASM_CC   := $(WASI_SDK)/bin/clang
# Le toolchain wasi-sdk invoque un `wasm-opt` trouvé dans PATH ; celui de ~/.local/bin est
# cassé sur cette machine (script Node ESM). On impose donc un PATH propre pour la
# compilation wasm — sans quoi l'édition de liens échoue.
WASM_PATH := /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
MODULES   := honneste gourmand hostile
WASM_BINS := $(patsubst %,$(BUILD)/wasm/%.wasm,$(MODULES))
RUN_BIN   := $(BUILD)/bin/eclat-run
RUNTIME_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -Icore/include -Iplatform/include \
                  -Iruntime -I$(WAMR_INC)

CORE_SRCS := $(sort $(wildcard core/src/*.c))
CORE_OBJS := $(patsubst core/src/%.c,$(BUILD)/obj/%.o,$(CORE_SRCS))
TEST_SRCS := $(sort $(wildcard core/tests/*.c))
TEST_BINS := $(patsubst core/tests/%.c,$(BUILD)/bin/%,$(TEST_SRCS))
DECIDE_BIN := $(BUILD)/bin/eclat-decide

.PHONY: all core test test-san decide python python-tests differential fuzz mutants bench deps wasm runtime test-wasm e2 clean size-check spdx

all: core

core: $(LIB)

$(LIB): $(CORE_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

$(BUILD)/obj/%.o: core/src/%.c core/include/eclat/*.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SAN) -c $< -o $@

$(BUILD)/bin/%: core/tests/%.c $(LIB) core/include/eclat/*.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SAN) $< $(LIB) -o $@

# Harnais CLI : lit une politique + une liste de requêtes sur stdin, imprime une décision par ligne.
$(DECIDE_BIN): tools/eclat-decide.c $(LIB) core/include/eclat/*.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SAN) $< $(LIB) -o $@

# Exécute tous les tests unitaires C (≥ 300 vérifications attendues).
test: $(TEST_BINS)
	@fail=0; for t in $(TEST_BINS); do echo "== $$t =="; ./$$t || fail=1; done; exit $$fail

# Même chose sous ASan/UBSan.
test-san: CFLAGS := -std=c11 -O1 -g -Wall -Wextra -Werror -Icore/include
test-san: SAN := -fsanitize=address,undefined -fno-omit-frame-pointer
test-san: $(TEST_BINS)
	@fail=0; for t in $(TEST_BINS); do echo "== $$t =="; ./$$t || fail=1; done; exit $$fail

decide: $(DECIDE_BIN)

python-tests:
	python3 host/run_tests.py

# Différentiel C ↔ Python : 10 000 décisions, réduction dichotomique du premier désaccord.
differential: decide
	python3 host/diff.py --bin $(DECIDE_BIN) --seed 1 --n 10000

fuzz: $(FUZZ_BIN)
	python3 tools/fuzz_policy.py --bin $(FUZZ_BIN) --iterations $(FUZZ_ITERATIONS) --seed 1

# Le harnais de fuzz est compilé AVEC les sources de core/ (tout en ASan/UBSan) : on ne lie
# pas une bibliothèque non instrumentée à un binaire instrumenté.
$(FUZZ_BIN): tools/fuzz_harness.c $(CORE_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(FUZZ_CFLAGS) tools/fuzz_harness.c $(CORE_SRCS) -o $@

mutants:
	python3 tools/mutants.py

bench: $(BENCH_BIN)
	python3 host/capbench.py --bin $(BENCH_BIN)

$(BENCH_BIN): tools/eclat_bench.c $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(LIB) -o $@

# --- Runtime WAMR sur Linux (lots L4/L5) --------------------------------------------
# Tout se construit et se teste sans SDP : c'est le point d'architecture du projet.
deps:
	tools/fetch-deps.sh

wasm: $(WASM_BINS)

$(BUILD)/wasm/%.wasm: modules/%.c modules/imports.h
	@mkdir -p $(dir $@)
	@test -x "$(WASM_CC)" || { echo "wasi-sdk absent : lance « make deps »"; exit 1; }
	PATH="$(WASM_PATH)" $(WASM_CC) --target=wasm32 -nostdlib -Wl,--no-entry \
	  -Wl,--export=run -O2 -Wall -Wextra -Werror -Imodules $< -o $@

runtime: $(RUN_BIN)

$(RUN_BIN): target/eclat_run.c runtime/eclat_runtime.c platform/posix/eclat_platform_posix.c \
            $(LIB) $(WAMR_LIB)
	@mkdir -p $(dir $@)
	@test -f "$(WAMR_LIB)" || { echo "libiwasm.a absente : lance « make deps »"; exit 1; }
	$(CC) $(RUNTIME_CFLAGS) target/eclat_run.c runtime/eclat_runtime.c \
	  platform/posix/eclat_platform_posix.c $(LIB) $(WAMR_LIB) -lpthread -lm -o $@

# Intégration : ECL-04 (exécution), ECL-05 (refus), ECL-06 (budget CPU), ECL-07 (empreinte).
test-wasm: runtime wasm
	@chmod +x tests/integration_wasm.sh
	tests/integration_wasm.sh

# Essai E2 du cahier des charges : la couche POSIX de WAMR compile-t-elle avec qcc ?
# Nécessite le SDP : source tools/setup-sdp.sh d'abord. --patched applique nos correctifs amont.
e2:
	tools/e2-probe.sh $(E2_ARGS)

size-check:
	python3 tools/sizecheck.py

spdx:
	python3 tools/spdx_guard.py

clean:
	rm -rf $(BUILD)