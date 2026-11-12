# Éclat — WAMR et courtier de capacités sur QNX 8.0

Du code tierce non certifié s'exécute sur un calculateur QNX critique, il ne peut rien faire
d'autre que ce que la politique autorise, et la boucle de contrôle temps réel **ne bouge pas**
pendant qu'il tourne.

**Éclat** est le portage du runtime **WAMR** (WebAssembly Micro Runtime,
[`bytecodealliance/wasm-micro-runtime`](https://github.com/bytecodealliance/wasm-micro-runtime)) sur QNX 8.0, enrichi d'un
**courtier de capacités** : chaque accès d'un module WebAssembly au monde extérieur (capteur,
actionneur, fichier, socket) devient un message synchrone vers un gestionnaire de ressources qui
décide selon une politique déclarative.

Projet candidat au **QNX Global Hackathon 2026**, catégorie **Open-Source Contribution &
Ports**.

## Le point clé : trois couches d'isolation indépendantes

1. Le **bac à sable WebAssembly** (mémoire linéaire bornée, pas d'appel système direct).
2. Le **processus QNX sans abilities**.
3. La **politique de sécurité** appliquée par le noyau.

Sur un noyau monolithique seule la première existe ; leur superposition est l'argument.

## État

**Vert et vérifié sur Linux, sans aucune licence QNX :**

- [x] Cahier des charges ([`docs/cahier-des-charges.md`](docs/cahier-des-charges.md), 22 exigences ECL-01..22).
- [x] Contrat gelé ([`CONTRACT.md`](CONTRACT.md)) + spec anglaise ([`docs/spec-en.md`](docs/spec-en.md)).
- [x] `core/` : moteur de décision C11 pur + politique + budget + journal + SHA-256 — **198 vérifications**, ASan/UBSan clean (ECL-01, ECL-02, ECL-05, ECL-06, ECL-07).
- [x] `host/` : jumeau Python indépendant + test différentiel — **10 000 décisions** C ↔ Python concordantes par graine (ECL-03). C'est lui qui a attrapé le bug de la sentinelle `-1` ([`docs/DECISIONS.md`](docs/DECISIONS.md)).
- [x] **Fuzz** du parseur : 3 000 entrées mutées sous ASan/UBSan, 0 plantage (ECL-08, ECL-15).
- [x] **Mutants** : 22 régressions volontaires injectées dans `core/`, **22 tuées** — taux de détection 100 % (ECL-15).
- [x] Chaîne d'approvisionnement : journal d'audit à **hachage enchaîné** + vérificateur indépendant, **manifeste de module signé**.
- [x] Outillage : `eclatctl` (dont `explain`, qui déroule les 7 étapes d'une décision) et SDK `eclat-new-module` (un module testable sur Linux en 3 commandes).
- [x] Banc de latence `capbench` (C ≈ 4 ns/décision, jumeau Python ≈ 500 ns — mesuré sur Linux, **pas** sur QNX).
- [x] **WAMR sur Linux** : runtime `WAMR-2.4.5` compilé, couche `platform/posix/` écrite, et **trois modules `.wasm` réels** exécutés avec leurs 7 imports routés par le moteur de décision (ECL-04).
- [x] Empreinte vérifiée avant chargement : un octet modifié refuse le module (ECL-07), vecteurs NIST recoupés avec `sha256sum`.
- [x] Budget CPU appliqué : le module gourmand qui boucle sans fin est **tué** (ECL-06).
- [x] Amont : recette `qnx-ports` conforme à l'arbre réel, lettre d'accompagnement, notes de portage, versions épinglées et empreinte wasi-sdk vérifiée.
- [x] Preuves d'absence de port Wasm existant ([`docs/E5-preuves-absence.md`](docs/E5-preuves-absence.md), essai **E5**).

**SDP QNX 8.0 installé le 12/11 (`~/qnx800`, `qcc` 12.2.0) — l'essai bloquant E2 est tranché :**

- [x] **E2 : OUI.** Les 8 fichiers de la couche plateforme retenus par le cmake amont compilent
      avec `qcc` sur `nto x86_64` **et** `nto aarch64le` — **16/16** avec un correctif amont d'une
      ligne ([`upstream/wamr/patches/`](upstream/wamr/patches), [`PORTING-NOTES.md`](upstream/wamr/PORTING-NOTES.md)).
      Reproductible : `source tools/setup-sdp.sh && tools/e2-probe.sh --patched`.

**Reste à faire (plus de bloquant, du travail) :**

- [ ] `platform/qnx/` : la cible QNX du dépôt (la sonde compile la couche `linux` + `common/posix`).
- [ ] Lier le cœur de l'interpréteur pour QNX, exécuter un `.wasm` sur cible (ECL-10).
- [ ] `broker/` : le courtier en gestionnaire de ressources, imports servis par `MsgSend` (ECL-11).
- [ ] Politique de sécurité `secpol` + banc de latence sur cible (ECL-12 à ECL-14).
- [ ] Demande de fusion amont (ECL-16) — le correctif `MAP_32BIT` est prêt et testé.

## Structure

```
eclat/
├── CONTRACT.md              # contrat gelé : ABI, politique, 7 étapes, budget, harnais
├── VERSIONS                 # épinglage reproductible (vérifié / à vérifier)
├── core/                    # C11 portable PUR : ni horloge, ni fichier, ni socket, ni allocation
│   ├── include/eclat/       #   cap_policy.h cap_decision.h cap_budget.h cap_log.h
│   ├── src/                 #   policy.c toml.c decision.c budget.c log.c names.c
│   └── tests/               #   un binaire par exigence
├── host/                    # Python 3 stdlib : capsim cappoint diff eclatctl audit manifest capbench
├── tools/                   # eclat-decide eclat-bench fuzz_harness mutants eclat-new-module gardes
├── templates/module/        # gabarit du SDK : un module testable sur Linux sans QNX
├── upstream/                # la contribution amont : qnx-ports/ et wamr/
├── docs/                    # cahier, spec-en, DECISIONS, E5, sécurité mappée, 5 technos QNX, 100 idées
├── platform/include/        # contrat de la couche plateforme (UNE déclaration)
├── platform/posix/          # implémentation Linux — horloge, CPU, journal
├── runtime/                 # intégration WAMR : instanciation, imports natifs, budget
├── modules/                 # 3 modules de démonstration (honnête, gourmand, hostile)
├── target/                  # eclat-run, le chargeur superviseur
├── tests/                   # intégration WAMR (ECL-04/05/06/07)
├── platform/qnx/            # (à venir) couche plateforme QNX — exige le SDP
└── broker/                  # (à venir) courtier, gestionnaire de ressources QNX
```

Les dépendances externes (WAMR, wasi-sdk) vivent dans `.deps/`, jamais committées : le règlement
de la catégorie interdit de soumettre un fork de l'amont.

## Construire et tester (n'a besoin d'aucun SDP)

```bash
make core            # libeclat.a en C11 strict -std=c11 -O2 -Wall -Wextra -Werror
make test            # 198 vérifications
make test-san        # les mêmes sous ASan/UBSan
make python-tests    # 131 tests du jumeau, de la console, de l'audit et du manifeste
make differential    # 10 000 décisions C ↔ Python comparées
make fuzz            # 3 000 entrées mutées sur le parseur (FUZZ_ITERATIONS pour changer)
make mutants         # 22 mutants, tous doivent être tués
make bench           # banc de latence C ↔ Python
make size-check      # < 500 lignes/fichier, < 50 lignes/fonction
make deps            # WAMR épinglé + wasi-sdk (empreinte vérifiée) → .deps/
make wasm runtime    # 3 modules .wasm + le chargeur eclat-run
make test-wasm       # intégration : ECL-04/05/06/07, 12 vérifications
make e2              # essai E2 : la couche POSIX de WAMR face à qcc (exige le SDP, make e2 E2_ARGS=--patched)
make spdx            # en-tête SPDX sur chaque source
```

Voir [`CONTRIBUTING.md`](CONTRIBUTING.md) pour les règles non négociables et les lots restants, et
[`docs/100-idees-aller-plus-loin.md`](docs/100-idees-aller-plus-loin.md) pour les 100 pistes
d'amélioration classées (dont 89 gratuites).

## Démonstration immédiate

**Le runtime WebAssembly, pour de vrai** (clone WAMR + wasi-sdk, ~800 Mo dans `.deps/`, jamais
committés) :

```bash
tools/fetch-deps.sh          # WAMR-2.4.5 épinglé + wasi-sdk 34.0 (empreinte sha256 vérifiée)
make wasm runtime test-wasm  # 3 modules .wasm, chargeur, 12 vérifications d'intégration
build/bin/eclat-run politique.toml build/wasm/hostile.wasm
```

Le module hostile, exécuté sous WAMR, dont chaque import passe par le moteur de décision :

```
[eclat log niv=2] {"seq":1,"module":"hostile","cap":"net_send","decision":"CAP_DENIED_POLICY"}
[eclat log niv=2] {"seq":2,"module":"hostile","cap":"write_actuator","decision":"CAP_DENIED_RANGE"}
[eclat log niv=2] {"seq":3,"module":"hostile","cap":"read_sensor","decision":"CAP_DENIED_RANGE"}
[eclat log niv=2] {"seq":4,"module":"hostile","cap":"kv_put","decision":"CAP_DENIED_POLICY"}
```

**Un module tiers généré par le SDK**, rejoué contre le vrai moteur :

```bash
python3 tools/eclat-new-module.py demo --dir /tmp && cd /tmp/demo && make test
```

```
module demo — politique « demo »
  [ 1] read_sensor     id=1    valeur=0       -> CAP_OK
  [ 2] write_actuator  id=3    valeur=2100    -> CAP_DENIED_RANGE
  [ 3] net_send        id=9    valeur=0       -> CAP_DENIED_POLICY
```

La consigne est refusée parce que la politique borne l'actionneur à ±1000 milli-unités, et le
réseau parce qu'aucune règle `allow` ne le couvre — **le défaut est de tout refuser**.

## Licence

Apache-2.0 (compatible WAMR). Voir `LICENSE`.