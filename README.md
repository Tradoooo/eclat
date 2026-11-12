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
- [x] Vérifications d'absence de port Wasm dans l'écosystème QNX publié ([`docs/E5-preuves-absence.md`](docs/E5-preuves-absence.md)) — **et découverte d'une PR amont ouverte** qui change la stratégie, voir ci-dessous.

**SDP QNX 8.0 installé le 12/11 (`~/qnx800`, `qcc` 12.2.0) — l'essai bloquant E2 est tranché :**

- [x] **E2 : OUI.** Les 8 fichiers de la couche plateforme retenus par le cmake amont compilent
      avec `qcc` sur `nto x86_64` **et** `nto aarch64le` — **16/16** avec les correctifs amont
      ([`upstream/wamr/patches/`](upstream/wamr/patches), [`PORTING-NOTES.md`](upstream/wamr/PORTING-NOTES.md)).
      Reproductible : `source tools/setup-sdp.sh && tools/e2-probe.sh --patched`.
- [x] **WAMR 2.4.5 compile ET se lie pour QNX** : `libiwasm.a` (759 Ko) et l'interpréteur `iwasm`
      (ELF QNX, `interpreter /usr/lib/ldqnx-64.so.2`). Deux correctifs amont suffisent :
      `MAP_32BIT` (drapeau `mmap` propre à Linux) et `-ldl -lpthread` (inexistants sur QNX).
- [x] **`platform/qnx/`** : horloge native `ClockTime()`, CPU par `CLOCK_THREAD_CPUTIME_ID`.
- [x] **`eclat-run` construit pour QNX 8.0** sur les deux cibles (`make qnx`, `make qnx QNX_ARCH=aarch64`) :
      382 Ko avec nos correctifs, 656 Ko avec le portage amont #5200, 3 bibliothèques dynamiques seulement.
- [x] **EXÉCUTÉ SUR QNX 8.0.0** (`qnxwamr 8.0.0 2026/06/05`, QEMU/KVM, image construite avec
      `mkqnximage --type=qemu`) — les quatre exigences vérifiées sur cible, console brute
      archivée dans [`docs/preuves-qnx/`](docs/preuves-qnx) :

      | Exigence | Résultat sur QNX 8.0 |
      |---|---|
      | **ECL-04** | module honnête exécuté : 4 appels au courtier, **0 refus**, code 0 |
      | **ECL-05** | module hostile : **4 refus** — `CAP_DENIED_POLICY` (net_send), `CAP_DENIED_RANGE` (actionneur 99999 hors ±1000), `CAP_DENIED_RANGE` (capteur 42), `CAP_DENIED_POLICY` (kv_put) — journalisés en JSON |
      | **ECL-06** | module gourmand en boucle sans fin **tué par le superviseur** à 1,30 s pour un budget de 300 ms (`fork`/`pipe`/`poll`/`SIGKILL` fonctionnent sur QNX) |
      | **ECL-07** | module falsifié (1 octet) **refusé** avec l'empreinte citée, code 1 |

- [x] **Démo plus réelle** : un **capteur vivant** (sinusoïde 1 Hz -> les lectures changent), la
      **boucle de contrôle à 1 kHz** avec la gigue mesurée (`eclat-boucle`), et la mise à mort d'un
      module gourmand pendant que la boucle continue de tourner. Preuves :
      [`docs/preuves/qnx800/`](docs/preuves/qnx800). ⚠ La gigue absolue est dominée par QEMU
      (pas de vrai timer, pas d'isolation de vCPU) : ce qui est prouvé, c'est la **vivacité** de
      la boucle, pas sa performance temps réel.
- [x] **Chemin `/dev/cap` publié** via `pathmgr_link` (le nom « cap » reste en repli).

**Chaque étape livre sa preuve** — commande unique, artefact brut daté, verdict :

```bash
make preuves        # rejoue tout, régénère docs/preuves/ETAT.md (~2 min)
```

Voir [`docs/preuves/INDEX.md`](docs/preuves/INDEX.md). Une étape sans preuve n'est pas une étape faite.

Le **dossier de soumission Devpost** est dans [`docs/devpost/`](docs/devpost) (description,
réponses, reproduction, budget/liens) et le **[script vidéo de 3 min](docs/video-script.md)** est
prêt. Ce qui reste à démontrer côté sécurité (abilities + `secpol`) est consigné sans survente
dans [`docs/securite-a-faire.md`](docs/securite-a-faire.md).

**Reste à faire (plus de bloquant, du travail) :**

- [ ] Publier le courtier sous `/dev/cap` avec `resmgr_attach()` (aujourd'hui c'est le nom
      « cap » : `name_attach()` refuse les chemins avec des « / »), puis faire passer les imports
      du module par le courtier au lieu de l'appel de fonction.
- [ ] Politique de sécurité `secpol` interdisant au runtime tout accès hors courtier (ECL-14).
- [ ] Banc de latence **sur cible** avec la boucle à 1 kHz (ECL-12, ECL-13).

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
├── runtime/                 # intégration WAMR : instanciation, imports natifs, transport, budget
├── modules/                 # 3 modules de démonstration (honnête, gourmand, hostile)
├── target/                  # eclat-run, le chargeur superviseur
├── tests/                   # intégration WAMR (ECL-04/05/06/07)
├── platform/qnx/            # couche plateforme QNX — horloge, CPU, transport
└── broker/                  # le courtier, serveur de messages sous « cap »
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
make qnx             # eclat-run pour QNX 8.0 (nos correctifs)
make qnx-amont       # eclat-run pour QNX 8.0 avec le portage amont #5200 — celui qui a tourné
tools/qnx-vm.sh build && tools/qnx-vm.sh start   # image QNX 8.0 pour QEMU (mkqnximage) + démarrage
make demo-qnx        # les 4 scénarios sur cible, console archivée dans docs/preuves-qnx/
make spdx            # en-tête SPDX sur chaque source
```

Voir [`CONTRIBUTING.md`](CONTRIBUTING.md) pour les règles non négociables et les lots restants, et
[`docs/100-idees-aller-plus-loin.md`](docs/100-idees-aller-plus-loin.md) pour les 100 pistes
d'amélioration classées (dont 89 gratuites).

## ⚠ Un port QNX de WAMR est déjà en cours en amont

La [PR #5200 « Add QNX 8.0 platform support »](https://github.com/bytecodealliance/wasm-micro-runtime/pull/5200)
est ouverte depuis le 2026-11-04, `MERGEABLE`, et accueillie par un mainteneur. Elle fait ce que
nous préparions, **mieux** (elle exploite `MAP_BELOW` au lieu de renoncer à `MAP_32BIT`, et elle a
réellement tourné sur QNX 8.0 sous QEMU : 133/137 tests unitaires amont).

**Nous n'ouvrons donc pas de demande de fusion sur le portage** — ce serait un doublon de moindre
qualité. Nos deux correctifs restent dans `upstream/wamr/patches/` comme trace de la vérification
indépendante qui nous a permis de compiler et lier WAMR pour QNX sur cette machine.

Ce que ça change pour le projet, et les pistes de contribution qui restent libres :
[`upstream/README.md`](upstream/README.md). En une ligne : l'apport d'Éclat n'est **pas** le
portage, c'est le **courtier de capacités** — et celui-là, personne ne le propose.

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