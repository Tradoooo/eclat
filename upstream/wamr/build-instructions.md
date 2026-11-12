# Instructions de construction

Deux volets : ce qui est **vérifié aujourd'hui** (Linux, sans licence QNX) et ce qui **ne l'est
pas** (QNX 8.0, en attente du SDP). Chaque commande non exécutée est marquée
`# NON VÉRIFIÉ (SDP absent)`.

Versions épinglées : [`VERSIONS`](../../VERSIONS).

---

## 1. Sur Linux — vérifié

Prérequis vérifiés sur la machine de développement : Ubuntu 24.04, `gcc 13.3.0`, `python 3.12.3`
(`tomllib` en bibliothèque standard), `make`. Aucun cmake, aucun wasi-sdk, aucune cible QNX.

```bash
git clone https://github.com/Tradoooo/eclat
cd eclat

make core            # libeclat.a en C11 strict
make test            # 161 vérifications
make test-san        # les mêmes sous ASan/UBSan
make python-tests    # 106 tests du jumeau Python
make differential    # 10 000 décisions C ↔ Python comparées
make fuzz            # 3 000 entrées mutées sur le parseur, 0 plantage attendu
make mutants         # 22 mutants injectés, 22 tués
make bench           # banc de latence C ↔ Python
make size-check      # < 500 lignes/fichier, < 50 lignes/fonction
make spdx            # en-tête SPDX sur chaque source
```

Sortie attendue de `make test` :

```
== build/bin/test_budget ==     OK 36 checks
== build/bin/test_decision ==   OK 45 checks
== build/bin/test_log ==        OK 24 checks
== build/bin/test_policy ==     OK 56 checks
```

### Harnais différentiel seul

```bash
make decide
./build/bin/eclat-decide politique.toml requetes.txt
# une décision par ligne ; politique invalide → stderr "ERROR:<champ>:<motif>", exit 2
python3 host/diff.py --bin build/bin/eclat-decide --seed 1 --n 10000
```

### Console d'exploitation et SDK

```bash
python3 host/eclatctl.py explain politique.toml read_sensor 99 -   # pourquoi ce refus, étape par étape
python3 tools/eclat-new-module.py mon-module --dir /tmp            # squelette de module testable
cd /tmp/mon-module && make test                                    # rejoue contre le vrai moteur
```

---

## 2. Modules de démonstration `.wasm` — non vérifié

Le compilateur n'est ni installé ni épinglé à ce jour.

```bash
# NON VÉRIFIÉ (wasi-sdk absent) — télécharger puis épingler la version dans VERSIONS
wget https://github.com/WebAssembly/wasi-sdk/releases/download/<version>/wasi-sdk-<version>-x86_64-linux.tar.gz
tar xf wasi-sdk-*.tar.gz -C "$HOME"
make -C /tmp/mon-module wasm WASI_SDK="$HOME/wasi-sdk"
```

Le squelette généré par `tools/eclat-new-module.py` produit déjà un objet `wasm32` si wasi-sdk est
présent, et **échoue proprement** avec un message explicite s'il est absent. Son test natif
(`make test`) ne dépend pas de wasi-sdk.

---

## 3. WAMR sur Linux — non vérifié

Prérequis manquants sur la machine au 2026-11-12 : `cmake`, `ninja`.

```bash
# NON VÉRIFIÉ — aucun clone de WAMR n'a encore été effectué
git clone --branch WAMR-2.4.5 --depth 1 https://github.com/bytecodealliance/wasm-micro-runtime
cd wasm-micro-runtime/product-mini/platforms/linux/build
cmake -DWAMR_BUILD_INTERP=1 -DWAMR_BUILD_AOT=0 -DWAMR_BUILD_LIBC_BUILTIN=1 \
      -DWAMR_BUILD_LIBC_WASI=0 ..
cmake --build . --parallel
```

Les noms d'options sont **vérifiés** dans `doc/build_wamr.md` amont ; la séquence, elle, ne l'est
pas. `product-mini/platforms/posix` existe également et pourrait être préférable — à trancher au
premier clone (lot L4).

---

## 4. Sur QNX 8.0 — non vérifié, bloqué par le SDP

```bash
# NON VÉRIFIÉ (SDP absent) — la licence QNX Everywhere est gratuite mais exige une action humaine
# 1. myQNX → QNX Everywhere → licence (~1 h) → QNX Software Center → SDP 8.0
# 2. sourcer l'environnement
source "$HOME/qnx800/qnxsdp-env.sh"        # NON VÉRIFIÉ : nom exact à confirmer
echo "$QNX_HOST" "$QNX_TARGET"             # les deux doivent être posés

# 3. construire le port via la recette de l'arbre des ports
cd build-files/ports/wamr && make -j$(nproc)

# 4. construire le courtier Éclat pour la cible
qcc -Vgcc_ntoaarch64le -std=c11 -O2 -Wall -Wextra -Werror \
    -Icore/include platform/qnx/*.c broker/*_qnx.c target/eclatd.c -o eclatd   # NON VÉRIFIÉ

# 5. compiler la politique de sécurité et lancer
secpolcompile image/secpol.conf image/secpol.bin    # NON VÉRIFIÉ
./eclatd &                                          # NON VÉRIFIÉ
```

Tant que l'étape 3 n'a pas réussi, **l'essai E2 du cahier des charges n'est pas tranché** et les
lots L7 à L10 ne peuvent pas démarrer. C'est le seul chemin critique du projet.