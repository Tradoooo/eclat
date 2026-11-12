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
- [x] `core/` : moteur de décision C11 pur + politique + budget + journal — **161 vérifications**, ASan/UBSan clean (ECL-01, ECL-02, ECL-05, ECL-06).
- [x] `host/` : jumeau Python indépendant + test différentiel — **10 000 décisions** C ↔ Python concordantes par graine (ECL-03). C'est lui qui a attrapé le bug de la sentinelle `-1` ([`docs/DECISIONS.md`](docs/DECISIONS.md)).
- [x] **Fuzz** du parseur : 3 000 entrées mutées sous ASan/UBSan, 0 plantage (ECL-08, ECL-15).
- [x] **Mutants** : 22 régressions volontaires injectées dans `core/`, **22 tuées** — taux de détection 100 % (ECL-15).
- [x] Chaîne d'approvisionnement : journal d'audit à **hachage enchaîné** + vérificateur indépendant, **manifeste de module signé**.
- [x] Outillage : `eclatctl` (dont `explain`, qui déroule les 7 étapes d'une décision) et SDK `eclat-new-module` (un module testable sur Linux en 3 commandes).
- [x] Banc de latence `capbench` (C ≈ 4 ns/décision, jumeau Python ≈ 260 ns — mesuré sur Linux, **pas** sur QNX).
- [x] Amont : recette `qnx-ports` conforme à l'arbre réel, lettre d'accompagnement, notes de portage, versions épinglées.
- [x] Preuves d'absence de port Wasm existant ([`docs/E5-preuves-absence.md`](docs/E5-preuves-absence.md), essai **E5**).

**Bloqué par le SDP QNX 8.0 (non installé) :**

- [ ] `platform/posix/` : WAMR sur Linux (ECL-04, ECL-07, ECL-08) — gratuit mais demande un clone + cmake.
- [ ] `platform/qnx/` + courtier + politique de sécurité (ECL-10..16).
- [ ] Essai **E2** : WAMR compile-t-il avec `qcc` ? — **le seul chemin critique du projet**.

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
├── platform/posix/          # (à venir) couche plateforme WAMR pour Linux
├── platform/qnx/            # (à venir) couche plateforme WAMR pour QNX — exige le SDP
├── broker/                  # (à venir) courtier, gestionnaire de ressources QNX
└── modules/                 # (à venir) 3 modules de démonstration .wasm
```

## Construire et tester (n'a besoin d'aucun SDP)

```bash
make core            # libeclat.a en C11 strict -std=c11 -O2 -Wall -Wextra -Werror
make test            # 161 vérifications
make test-san        # les mêmes sous ASan/UBSan
make python-tests    # 129 tests du jumeau, de la console, de l'audit et du manifeste
make differential    # 10 000 décisions C ↔ Python comparées
make fuzz            # 3 000 entrées mutées sur le parseur (FUZZ_ITERATIONS pour changer)
make mutants         # 22 mutants, tous doivent être tués
make bench           # banc de latence C ↔ Python
make size-check      # < 500 lignes/fichier, < 50 lignes/fonction
make spdx            # en-tête SPDX sur chaque source
```

Voir [`CONTRIBUTING.md`](CONTRIBUTING.md) pour les règles non négociables et les lots restants, et
[`docs/100-idees-aller-plus-loin.md`](docs/100-idees-aller-plus-loin.md) pour les 100 pistes
d'amélioration classées (dont 89 gratuites).

## Démonstration immédiate

```bash
python3 tools/eclat-new-module.py demo --dir /tmp && cd /tmp/demo && make test
```

Un module généré, rejoué contre le vrai moteur de décision :

```
module demo — politique « demo »
  [log niv=1] demo démarré
  [ 1] read_sensor     id=1    valeur=0       -> CAP_OK
  [ 2] write_actuator  id=3    valeur=2100    -> CAP_DENIED_RANGE
  [ 3] net_send        id=9    valeur=0       -> CAP_DENIED_POLICY
```

La consigne est refusée parce que la politique borne l'actionneur à ±1000 milli-unités, et le
réseau parce qu'aucune règle `allow` ne le couvre — **le défaut est de tout refuser**.

## Licence

Apache-2.0 (compatible WAMR). Voir `LICENSE`.