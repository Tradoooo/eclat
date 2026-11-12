# Éclat — WebAssembly (WAMR) et courtier de capacités sur QNX 8.0

**Nom de travail : « Éclat »** (un fragment de code tiers qui brille sans pouvoir brûler la maison).
Version 0.1 — 11 novembre 2026 — projet candidat au QNX Global Hackathon, catégorie **Open-Source Contribution & Ports**.
Classement interne : 2ᵉ sur 140 idées notées (15/16). Calendrier : **5 semaines** (concours) ou **3 mois** (produit).

---

## 0. Lecture rapide

**Ce que c'est.** Le portage de **WAMR** (WebAssembly Micro Runtime, `bytecodealliance/wasm-micro-runtime`) sur QNX 8.0, enrichi d'un **courtier de capacités** : un module WebAssembly tiers s'exécute dans un processus sans privilèges, et chacun de ses accès au monde extérieur (capteur, actionneur, fichier, socket) devient un message synchrone vers un gestionnaire de ressources qui décide selon une politique.

**La promesse, en une phrase.** Du code tiers non certifié s'exécute sur un calculateur QNX critique, il ne peut rien faire d'autre que ce que la politique autorise, et la boucle de contrôle temps réel **ne bouge pas** pendant qu'il tourne.

**Trois idées qui portent le projet.**

1. **Trois couches d'isolation indépendantes.** Le bac à sable WebAssembly (mémoire linéaire bornée, pas d'appel système direct), le processus QNX sans abilities, et la politique de sécurité appliquée par le noyau. Aucune des trois ne suffit seule ; leur superposition est l'argument.
2. **Le courtier est un gestionnaire de ressources, donc un citoyen QNX de première classe.** Il a un nom dans l'espace de chemins, des priorités, un journal, et il est testable seul — indépendamment du runtime WebAssembly.
3. **La latence de la boucle critique est une mesure, pas une promesse.** Le module tiers est chargé à chaud, on le fait boucler, on le tue, et l'histogramme de la boucle critique est affiché avant/pendant/après.

**Trois niveaux d'ambition.** **S** (socle), **C** (complet), **X** (extrême), comme dans les autres cahiers.

**À valider avant d'écrire la moindre ligne de produit** (section 10) : cinq essais. Le plus critique est E2 — WAMR compile-t-il pour QNX avec la couche plateforme POSIX existante, ou faut-il écrire une couche dédiée ?

**Statut des affirmations.** Section 14 : ce qui est vérifié, ce qui reste hypothèse.

---

## 1. Contexte et cas d'usage

### 1.1 Qui a ce problème

| Acteur | Besoin | Coût actuel |
|---|---|---|
| Équipementier automobile | Exécuter une application de partenaire (service de recharge, diagnostic, infotainment tiers) sur un ECU critique | Certification ISO 26262 du code tiers : plusieurs mois, ou calculateur séparé |
| Fabricant de dispositifs médicaux | Ajouter une fonction d'un fournisseur sur un appareil déjà homologué | Requalification IEC 62304 complète, ou refus du fournisseur |
| Opérateur de flotte / atelier | Déployer un script de diagnostic sans mettre à jour l'image | Accès physique, ou outil propriétaire |
| Intégrateur industriel | Charger une logique métier spécifique client sur un automate | Fork du firmware par client |
| Développeur QNX | Tester une extension sans recompiler ni reflasher | Cycle complet image + flash |

### 1.2 Pourquoi WebAssembly plutôt qu'un scripteur

Un interpréteur scripté (Lua, Python, déjà portés sur QNX) donne un langage, pas une frontière : le code appelle la bibliothèque C sous-jacente et hérite de ses privilèges. WebAssembly donne une **mémoire linéaire bornée**, des imports **explicitement déclarés**, et un modèle où chaque interaction avec l'hôte passe par une fonction d'import que l'on contrôle. C'est cette propriété qui rend le courtier de capacités possible et démontrable.

### 1.3 Pourquoi maintenant

WAMR est absent de l'écosystème QNX (section 2.2, vérifié), alors que presque tout le reste y est déjà porté : la fenêtre de contribution est ouverte et la demande de fusion a une vraie chance d'être acceptée.

---

## 2. Ancrage documentaire

### 2.1 Ce que QNX fournit déjà

| Élément | Source |
|---|---|
| Passage de messages synchrone (`MsgSend`/`MsgReceive`/`MsgReply`), pulses, canaux, connexions, donneur de priorité | System Architecture |
| Gestionnaires de ressources (`resmgr_attach`, `io_funcs`, `msg_funcs`) | Writing a Resource Manager |
| Abilities du gestionnaire de processus (`procmgr_ability`), politiques de sécurité, Pathtrust, fonctions fortifiées | System Security Guide |
| HAM (supervision, battement de cœur, relance), PPS (pub/sub), `ionotify`, `TimerPulse` | guides OS Components |
| `qcrypto` (primitives cryptographiques, donc signature de modules possible) | `com.qnx.doc.qnxos.qcrypto` |
| Chaîne de build pour ports open source : dépôt `qnx-ports/build-files`, un dossier par port avec `README.md`, environnement Docker de construction | `github.com/qnx-ports` (README lu) |
| POSIX conforme : « many open-source projects can be compiled for QNX with little or no modification » | qnx.software — QNX Everywhere (lu) |

### 2.2 Absence vérifiée (le filon)

- L'index officiel des ports `github.com/qnx-ports/build-files/ports` (~150 entrées lues) ne contient **aucune** entrée `wasm`, `wamr`, `wasi`, `wasmtime`, `wasmer`.
- `https://github.com/qnx-ports/wamr` renvoie **404**.
- Sont en revanche déjà portés (donc inutilisables comme sujet) : `rust`, `cargo`, `lua`, `perl5`, `numpy`, `pandas`, `protobuf`, `grpc`, `ros2`, `Fast-DDS`, `opencv`, `onnxruntime`, `tensorflow`, `pytorch`, `qt`, `gtk`, `wayland`, `weston`, `SDL`, `zlib`, `zstd`, `gnutls`, `s2n-tls`, `sqlite`, `curl`, `mosquitto`, `epoll`, `gdal`, `ode`, `gz-sim`.
- **Réserve honnête** : le tableau de bord `oss.qnx.com` annonce ~2 500 ports et n'est pas interrogeable sans JavaScript. L'absence est certaine dans `qnx-ports/build-files`, pas absolument certaine ailleurs. À re-vérifier avec un vrai navigateur avant de soumettre.

### 2.3 Règles de la catégorie (à respecter à la lettre)

Nouveau dépôt créé pendant la période de soumission, contenant le travail de portage (patches ou couche de portage, fichiers de construction QNX, instructions de build et d'exécution, documentation). Le projet amont est référencé **par URL**, son historique n'est **pas** copié. **Interdiction de soumettre un fork.** Fournir l'URL amont **et** l'URL de la demande de fusion si elle est ouverte.

---

## 3. Périmètre

### 3.1 Dans le périmètre

- Couche plateforme QNX pour WAMR (threads, horloge, mémoire, sortie, verrous) et fichiers de construction.
- Exécution d'un module `.wasm` compilé en dehors du projet, chargé depuis le système de fichiers.
- **Courtier de capacités** : gestionnaire de ressources QNX, politique déclarative, journal des décisions.
- Bibliothèque d'imports de référence : `cap_read_sensor`, `cap_write_actuator`, `cap_log`, `cap_time_ns`, `cap_kv_get/put`, `cap_net_send` (chacun soumis à politique).
- Isolation : budget CPU, plafond mémoire, tuer le module en cas de dépassement, relance par HAM.
- Signature et vérification d'un module avant chargement (via `qcrypto` ou `minisign` côté hôte).
- Charge utile de démonstration : 3 modules (un honnête, un gourmand, un hostile).
- Demande de fusion amont.

### 3.2 Hors périmètre

Compilateur vers WebAssembly (on utilise `clang --target=wasm32` ou `wasi-sdk` existants) ; WASI complet ; threads WebAssembly et mémoire partagée ; support du JIT (interprète et AOT seulement) ; exécution de code natif ; système de paquets de modules ; tout usage réseau non soumis au courtier.

---

## 4. Architecture

```
eclat/
├── CONTRACT.md              # contrat gelé : ABI des imports, format de politique, codes de décision
├── core/                    # C11 portable PUR : politique, décisions, journal, budget — ni horloge ni fichier ni socket
│   ├── include/eclat/       #   cap_policy.h cap_decision.h cap_budget.h cap_log.h
│   ├── src/                 #   policy.c decision.c budget.c log.c
│   └── tests/               #   test_policy.c test_decision.c test_budget.c test_util.c
├── platform/qnx/            # couche plateforme WAMR pour QNX : os_thread, os_time, os_mem, os_mutex, libc_builtin
├── platform/posix/          # couche homologue pour Linux — permet de TOUT tester sur PC
├── broker/                  # le courtier : resmgr QNX, table de capacités, file de décisions
│   └── *_qnx.c              # glue isolée (#ifdef __QNX__)
├── runtime/                 # intégration WAMR : instantiation, résolution des imports, appels
├── modules/                 # les 3 modules de démonstration (source C/Rust + .wasm précompilé + empreinte)
├── target/                  # binaires QNX : eclatd (courtier), eclat-run (chargeur), eclatctl
├── host/                    # Python 3 stdlib : cappoint (politique), capsim (simulateur de décisions), capbench
├── upstream/                # patches formatés pour WAMR + pour qnx-ports/build-files, lettre d'accompagnement
├── tests/                   # différentiels, fuzz du parseur de politique, banque de politiques fautives
├── image/                   # fragment de buildfile d'image + politique de sécurité QNX
├── codelab/                 # 4 chapitres : « exécuter du code tiers en sécurité sur QNX »
└── docs/                    # spec-fr.md, spec-en.md, PORTING.md, DECISIONS.md, VALIDATION.md
```

**Règle de portabilité.** La décision de politique vit dans `core/` et ne connaît ni QNX ni WAMR. Le même moteur de décision est donc testé sur Linux avec la couche `platform/posix/`, ce qui permet de valider **toute la logique** avant d'avoir le SDP. C'est le point qui rend ce projet livrable même si la licence tarde.

---

## 5. Contrat

### 5.1 ABI des imports (authoritative : `CONTRACT.md`)

| Import | Signature | Sémantique |
|---|---|---|
| `cap_time_ns` | `(void) -> i64` | Instant monotone en nanosecondes, fourni par l'hôte |
| `cap_log` | `(ptr:i32, len:i32, level:i32) -> i32` | Journalisation ; retour 0 ou code d'erreur |
| `cap_read_sensor` | `(id:i32, out_ptr:i32, out_len:i32) -> i32` | Lecture soumise à politique ; remplit un tampon borné |
| `cap_write_actuator` | `(id:i32, value_milli:i32) -> i32` | Écriture soumise à politique, valeur entière en milli-unités |
| `cap_kv_get` / `cap_kv_put` | `(key_ptr:i32, key_len:i32, val_ptr:i32, val_len:i32) -> i32` | Stockage clés-valeurs borné |
| `cap_net_send` | `(chan:i32, ptr:i32, len:i32) -> i32` | Envoi sur un canal déclaré, soumis à politique |

Règles : aucune chaîne terminée par zéro admise (pointeur + longueur systématiques) ; toute longueur est vérifiée contre la taille du tampon déclaré avant copie ; tout import inconnu est refusé à l'instantiation, jamais à l'exécution.

### 5.2 Politique (`policy/*.toml`, validée par `cappoint`)

```toml
[module "diag-partner"]
sha256 = "…"                 # empreinte obligatoire du .wasm
cpu_budget_ms_per_s = 50     # budget CPU par seconde glissante
mem_max_kib = 2048
calls_per_s = 200

[[allow]]
cap = "read_sensor"
ids = [1, 2, 7]

[[allow]]
cap = "write_actuator"
ids = [3]
value_milli_range = [-1000, 1000]

[[deny]]
cap = "net_send"
```

Décision par défaut : **tout refuser**. Une capacité non listée explicitement en `allow` est refusée.

### 5.3 Codes de décision et journal

`CAP_OK`, `CAP_DENIED_POLICY`, `CAP_DENIED_RANGE`, `CAP_DENIED_BUDGET`, `CAP_DENIED_UNKNOWN`, `CAP_BAD_ARG`, `CAP_KILLED_CPU`, `CAP_KILLED_MEM`.
Journal : une ligne JSON par décision refusée ou par dépassement de budget (les décisions accordées ne sont journalisées qu'en mode audit).

---

## 6. Exigences

| ID | Niveau | Exigence | Vérification |
|---|---|---|---|
| ECL-01 | S | Le moteur de décision (`core/`) compile en C11 strict sur Linux, sans QNX ni WAMR, et passe ses tests | `make core`, ≥ 300 vérifications |
| ECL-02 | S | La politique est analysée et validée ; politique invalide refusée avec champ et cause ; **tout refus par défaut** | banque de 30 politiques fautives |
| ECL-03 | S | Le jumeau Python rejoue les mêmes décisions ; test différentiel par graine avec réduction dichotomique du premier désaccord | 10 000 décisions comparées |
| ECL-04 | S | Un module `.wasm` s'exécute sous la couche POSIX sur Linux et appelle les imports de référence | test d'intégration hôte |
| ECL-05 | S | Le module hostile (accès non autorisé) reçoit `CAP_DENIED_POLICY` et le refus est journalisé | test dirigé + capture |
| ECL-06 | S | Le budget CPU est respecté : un module qui boucle est tué, et le dépassement est journalisé | test avec boucle infinie |
| ECL-07 | S | L'empreinte du module est vérifiée avant chargement ; un octet modifié refuse le chargement | test de falsification |
| ECL-08 | S | Aucune copie hors bornes : tout import avec longueur excessive est refusé, fuzzé sous ASan sans crash ni fuite | fuzz 100 000 entrées |
| ECL-09 | S | CI ≤ 5 min par job, `fetch-depth: 1`, `concurrency` ; gardes de taille et de licence | garde `ci-budget`, `sizecheck.py` |
| ECL-10 | C | WAMR compile pour QNX 8.0 (cible `aarch64` et/ou `x86_64`) avec la couche `platform/qnx/` | essai E2, puis build réel |
| ECL-11 | C | Le courtier tourne sur QNX comme gestionnaire de ressources et sert les imports par message synchrone | essai E4, capture vidéo |
| ECL-12 | C | Chargement **à chaud** d'un module pendant qu'une boucle de contrôle à 1 kHz tourne, sans échéance manquée | banc de latence, chiffres publiés |
| ECL-13 | C | La boucle critique reste dans sa borne pendant qu'un module hostile sature son budget, et pendant son exécution par HAM | histogramme avant/pendant/après |
| ECL-14 | C | Une politique de sécurité QNX interdit au processus du runtime tout accès hors courtier ; test de refus à l'appui | `secpolcompile` + essai |
| ECL-15 | C | Le parseur de politique est fuzzé ; le runtime est fuzzé avec des `.wasm` malformés | campagne ASan/UBSan |
| ECL-16 | C | Demande de fusion amont ouverte (WAMR et/ou `qnx-ports/build-files`) avec lettre d'accompagnement et instructions de build | URL dans la soumission |
| ECL-17 | C | `PORTING.md` documente chaque écart POSIX rencontré et sa résolution — c'est la valeur pour la communauté | relecture à froid |
| ECL-18 | X | Mode AOT : compilation ahead-of-time du module, mesure du gain et du pire temps d'exécution | comparaison chiffrée |
| ECL-19 | X | Isolation multi-modules : plusieurs modules simultanés, budgets indépendants, aucun effet croisé | test de cohabitation |
| ECL-20 | X | Migration assistée : exécuter un plugin Lua existant comme module Wasm, à comportement identique | test de bascule |
| ECL-21 | X | Attestation : le courtier signe chaque décision critique, journal vérifiable par un tiers | vérificateur indépendant |
| ECL-22 | X | Contribution amont fusionnée (pas seulement ouverte) | URL de fusion |

---

## 7. Plan de vérification

1. **Unitaires C** sur `core/` : politique, décision, budget, journal. Binaires autonomes, `OK <n> checks`.
2. **Différentiel C ↔ Python** : deux implémentations indépendantes du moteur de décision, comparaison par graine, réduction dichotomique du premier désaccord (méthode éprouvée sur Ambre et Passerelle).
3. **Banque de cas hostiles** : 30 politiques fautives, 12 modules malformés, 8 séquences d'imports interdites.
4. **Fuzz** : parseur de politique et instanciation de modules, sous ASan/UBSan.
5. **Mutants** : au moins 8 mutations du moteur de décision (ordre des règles, défaut du refus, comparaison de plage, compteur de budget) — chacune doit être tuée.
6. **Banc de latence** : boucle de contrôle à 1 kHz mesurée sans module, avec module honnête, avec module hostile tué. Trois histogrammes publiés, aucune valeur inventée.
7. **Gardes maison** : `sizecheck.py`, `spdx-guard`, `ci-budget`.

---

## 8. CI

- Deux jobs parallèles, `timeout-minutes: 5` chacun, `fetch-depth: 1`, `concurrency` avec annulation.
- `host` : `core/` + couche POSIX + WAMR sur Linux + tests différentiels + fuzz court.
- `python` : `cappoint`, `capsim`, `capbench`, jumeau de décision.
- Job `qnx` hors PR, sur déclenchement manuel, dès qu'un runner a le SDP.
- Un job `wasm` compile les modules de démonstration avec `wasi-sdk` épinglé (version figée dans le dépôt).

---

## 9. Jalons

### 9.1 Option 5 semaines

| Semaine | Livrable |
|---|---|
| S1 | Essais E1–E5 tranchés ; `CONTRACT.md` gelé ; `core/` politique + décision + tests ; jumeau Python |
| S2 | Couche `platform/posix` + WAMR sur Linux : un module s'exécute et appelle les imports ; banque de cas hostiles |
| S3 | Couche `platform/qnx` + build pour QNX (nécessite le SDP) ; courtier en gestionnaire de ressources |
| S4 | Banc de latence, chargement à chaud, politique de sécurité QNX, fuzz et mutants ; demande de fusion amont ouverte |
| S5 | `PORTING.md`, codelab, docs EN, vidéo 3 min, dépôt public, formulaire Devpost, marge |

**Repli si le SDP manque en S3** : livrer S1–S2 comme « couche de portage validée sur POSIX + moteur de capacité prouvé », publier les patches QNX non exécutés en le disant explicitement. Soumission plus faible : le règlement exige la preuve que le projet construit ou tourne réellement sous QNX 8.0.

### 9.2 Option 3 mois

M1 = moteur de capacité prouvé + WAMR sur POSIX + modules de démonstration. M2 = portage QNX complet, courtier, politique de sécurité, mesures de latence, AOT. M3 = multi-modules, attestation, codelab étendu, contribution amont suivie jusqu'à fusion, dossier de validation.

---

## 10. Essais de faisabilité (semaine 1)

| ID | Question | Comment trancher | Bloquant si |
|---|---|---|---|
| E1 | Le SDP 8.0 s'installe-t-il ? | myQNX → QNX Everywhere → licence (~1 h) → QNX Software Center | non (repli POSIX) |
| E2 | WAMR compile-t-il pour QNX avec la couche POSIX existante ? | Cloner WAMR, lire `core/shared/platform/` : existe-t-il une couche générique POSIX réutilisable ? Tester d'abord une compilation croisée minimale (`qcc`) d'un fichier de la plateforme | **oui** : si aucune couche POSIX n'est réutilisable, le portage dépasse 5 semaines → basculer sur un runtime plus petit ou sur Tamis |
| E3 | `clang --target=wasm32` ou `wasi-sdk` produit-il des modules que WAMR accepte ? | Compiler les 3 modules de démonstration, les charger sous Linux | oui pour la démo |
| E4 | Un gestionnaire de ressources peut-il servir un import en moins de 100 µs aller-retour ? | Micro-banc `MsgSend`/`MsgReply` sur QEMU ; à défaut, mesurer sous Linux avec un tube et annoncer l'écart | non : publier la mesure réelle |
| E5 | Existe-t-il déjà un port WebAssembly pour QNX hors `qnx-ports` ? | Chercher « wasm » et « webassembly » sur oss.qnx.com avec un navigateur réel, sur `gitlab.com/qnx`, et sur le Discord QNX | **oui** : changer de sujet |

---

## 11. Risques

| Risque | Probabilité | Impact | Parade |
|---|---|---|---|
| E2 échoue : la couche plateforme exige trop d'adaptation | moyenne | fort | Limiter le périmètre à l'interprète sans threads ; ou porter un runtime plus petit ; décision en fin de S1 |
| Le SDP tarde | moyenne | moyen | Toute la logique est validée sur POSIX ; la couche QNX est isolée dans `platform/qnx/` et `broker/*_qnx.c` |
| Performance de l'interprète sur Raspberry Pi 5 | moyenne | moyen | Mode AOT (ECL-18) ; mesurer et publier, ne pas promettre |
| La demande de fusion amont n'aboutit pas avant le 15/12 | forte | faible | Le règlement demande l'URL de la demande **si** elle est ouverte, pas sa fusion |
| Un port Wasm existe déjà ailleurs | faible | fort | Essai E5 avant tout code |
| Le juge trouve « bac à sable Wasm » banal | moyenne | moyen | Insister sur les **trois couches** et sur la mesure de non-perturbation de la boucle critique, pas sur le runtime |

---

## 12. Script vidéo (3 minutes)

| Temps | Plan |
|---|---|
| 0:00–0:20 | « Aucun runtime WebAssembly n'est porté sur QNX » — capture de l'index officiel des ports et du 404 |
| 0:20–0:50 | Le courtier : `/dev/cap` visible, politique affichée, module chargé après vérification d'empreinte |
| 0:50–1:35 | Module honnête : lit un capteur autorisé, écrit un actionneur dans la plage autorisée, journal |
| 1:35–2:10 | Module hostile : accès refusé (`CAP_DENIED_POLICY`), puis boucle infinie → tué par le budget CPU, HAM le relance |
| 2:10–2:40 | La preuve qui compte : histogramme de la boucle de contrôle à 1 kHz, avant / pendant / après — aucune échéance manquée |
| 2:40–3:00 | Les trois couches d'isolation, la demande de fusion amont, les tests (nombre de vérifications, mutants tués) |

---

## 13. Dossier de soumission Devpost

1. **Dépôt public** créé pendant la période de soumission, licence ouverte visible (Apache-2.0, compatible WAMR).
2. **Catégorie** : Open-Source Contribution & Ports.
3. **URL amont** : `https://github.com/bytecodealliance/wasm-micro-runtime` — **ne pas soumettre un fork** : le dépôt contient la couche de portage, les patches formatés dans `upstream/`, les fichiers de construction QNX et la documentation.
4. **URL de la demande de fusion** : à renseigner (WAMR pour la couche plateforme QNX ; et/ou `qnx-ports/build-files` pour l'entrée de port).
5. **Preuve d'usage de QNX** : `platform/qnx/*`, `broker/*_qnx.c`, `target/eclatd.c`, `image/` (buildfile + politique), captures sur QNX 8.0 avec version du SDP et cible.
6. **Description** : trou (aucun runtime Wasm porté) → contribution (couche plateforme + courtier + politique + mesures) → preuve (tests, fuzz, mutants, histogrammes) → reproductibilité.
7. **Instructions de build et d'exécution** : version QNX, cible, toolchain, version de WAMR épinglée, version de `wasi-sdk`.
8. **Bonus Codelab** : URL du codelab (4 chapitres).
9. **Langue** : anglais, ou traduction fournie.

---

## 14. Statut des affirmations

**Vérifié** : absence de WAMR/wasm/wasi/wasmtime/wasmer dans l'index officiel `qnx-ports/build-files/ports` (lecture directe de la liste des ~150 entrées) ; `github.com/qnx-ports/wamr` renvoie 404 ; existence de la chaîne de portage `qnx-ports/build-files` avec un dossier par port et un environnement Docker de construction ; QNX se déclare conforme POSIX et annonce que de nombreux projets open source compilent avec peu ou pas de modification ; existence de `qcrypto` ; mécanismes QNX cités (messages, gestionnaires de ressources, abilities, politiques, HAM, PPS).

**Hypothèses de conception** : qu'une couche plateforme POSIX de WAMR soit réutilisable pour QNX (E2) ; que la latence aller-retour du courtier soit compatible avec une boucle à 1 kHz (E4) ; que le mode AOT soit disponible pour les cibles QNX ; que l'empreinte de module puisse être vérifiée avec `qcrypto` sans dépendance supplémentaire.

**Non vérifié** : l'absence d'un port WebAssembly ailleurs que dans `qnx-ports` (le tableau de bord oss.qnx.com n'est pas interrogeable sans JavaScript) — essai E5 obligatoire.

**Volontairement non affirmé** : aucun chiffre de latence avant mesure ; aucune affirmation de sécurité au sens certification (le projet fournit une isolation et une politique, pas une qualification).

---

## 15. Règles de code et délégation

**Règles maison** : C11, `-std=c11 -O2 -Wall -Wextra -Werror` ; `core/` pur (ni horloge, ni fichier, ni socket, ni allocation, ni état global mutable) ; code QNX confiné dans `platform/qnx/`, `broker/*_qnx.c`, `target/`, gardé par `#ifdef __QNX__` ; moins de 500 lignes par fichier, moins de 50 lignes par fonction ; aucune duplication — la couche `platform/posix/` et la couche `platform/qnx/` partagent le même contrat d'interface, jamais du code copié ; Python 3 standard uniquement ; chaque exigence citée en commentaire au-dessus du code et dans le nom du test.

**Découpage en lots pour sous-agents** :

| Lot | Contenu | Dépend de |
|---|---|---|
| L1 | `core/` politique : parseur TOML restreint, validation, banque de cas fautifs | CONTRAT gelé |
| L2 | `core/` décision + budget + journal | L1 |
| L3 | Jumeau Python + test différentiel avec réduction dichotomique | L2 |
| L4 | `platform/posix/` + intégration WAMR sur Linux + résolution des imports | L2 |
| L5 | Modules de démonstration (honnête, gourmand, hostile) + empreintes | L4 |
| L6 | Fuzz (politique + `.wasm` malformés) + mutants + gardes | L1–L5 |
| L7 | `platform/qnx/` + build croisé (nécessite le SDP) | E1, E2 |
| L8 | Courtier QNX (`broker/`, `target/eclatd`) + politique de sécurité + `image/` | L7, E4 |
| L9 | Banc de latence + campagne de mesures + `host/capbench` | L8 |
| L10 | `upstream/` (patches formatés, lettre d'accompagnement) + demande de fusion | L7 |
| L11 | `PORTING.md` + codelab + docs EN/FR + script vidéo | L1–L9 |

Chaque brief de sous-agent rappelle : contexte complet, critère de succès mesurable, bornes (temps, essais, comportement en cas d'échec), règles shell, et « fais-le toi-même, sans déléguer ».
