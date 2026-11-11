# Éclat — WAMR et courtier de capacités sur QNX 8.0

Du code tierce non certifié s'exécute sur un calculateur QNX critique, il ne peut rien faire
d'autre que ce que la politique autorise, et la boucle de contrôle temps réel **ne bouge pas**
pendant qu'il tourne.

**Éclat** est le portage du runtime **WAMR** (WebAssembly Micro Runtime,
[`bytecodealliance/wamr`](https://github.com/bytecodealliance/wamr)) sur QNX 8.0, enrichi d'un
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

- [x] Cahier des charges ([`docs/cahier-des-charges.md`](docs/cahier-des-charges.md), 22 exigences ECL-01..22).
- [x] Contrat gelé ([`CONTRACT.md`](CONTRACT.md)).
- [x] `core/` : moteur de décision C11 pur + politique + budget + journal — **161 vérifications** vertes, ASan/UBSan clean (ECL-01, ECL-02, ECL-05, ECL-06).
- [x] `host/` : jumeau Python indépendant + test différentiel — **10 000 décisions** C ↔ Python concordantes par graine, réduction au premier désaccord (ECL-03).
- [ ] `platform/posix/` : WAMR sur Linux (ECL-04, ECL-07, ECL-08).
- [ ] `platform/qnx/` + courtier (ECL-10..16) — nécessite le SDP QNX 8.0.

Tout ce qui est coché **se construit et se teste sur Linux, sans aucune licence QNX** : c'est le
point d'architecture décisif du projet (deux couches plateforme partageant le même contrat).

## Structure

```
eclat/
├── CONTRACT.md              # contrat gelé : ABI, politique, codes, budget
├── core/                    # C11 portable PUR : politique, décisions, budget, journal
│   ├── include/eclat/       #   cap_policy.h cap_decision.h cap_budget.h cap_log.h
│   ├── src/                 #   policy.c decision.c budget.c log.c toml.c
│   └── tests/               #   unitaires (un binaire par exigence)
├── platform/posix/          # couche plateforme WAMR pour Linux
├── platform/qnx/            # couche plateforme WAMR pour QNX
├── broker/                  # courtier (resmgr QNX)
├── runtime/                 # intégration WAMR
├── modules/                 # 3 modules de démonstration (.wasm)
├── host/                    # Python 3 stdlib : cappoint, capsim, capbench
├── upstream/                # patches WAMR + qnx-ports, lettre d'accompagnement
└── docs/                    # spec-fr, spec-en, PORTING, DECISIONS, VALIDATION
```

## Construire et tester (n'a besoin d'aucun SDP)

```bash
make core       # compile core/ en C11 strict -std=c11 -O2 -Wall -Wextra -Werror
make test       # ≥ 300 vérifications, ASan/UBSan clean
make python     # jumeau Python + cappoint/capsim
make differential   # 10 000 décisions C ↔ Python, réduction dichotomique du 1er désaccord
```

## Licence

Apache-2.0 (compatible WAMR). Voir `LICENSE`.