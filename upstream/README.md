# `upstream/` — la contribution amont

Éclat est soumis dans la catégorie **Open-Source Contribution & Ports**. Les règles de la
catégorie imposent : un dépôt nouveau, le projet amont référencé **par URL**, son historique
**non copié**, **aucun fork soumis**, et l'URL de la demande de fusion si elle est ouverte.

Ce dossier contient ce qui concerne l'amont — et, depuis le 12 novembre 2026, un constat qui
change la stratégie.

## ⚠ Un port QNX de WAMR existe déjà en amont

[PR #5200 « Add QNX 8.0 platform support »](https://github.com/bytecodealliance/wasm-micro-runtime/pull/5200)
— ouverte le **2026-11-04** par `altinayselman` (Selman Altınay), état **OPEN / MERGEABLE**,
`reviewDecision: REVIEW_REQUIRED`, accueillie le 2026-11-10 par le mainteneur `woodsmc` :
*« Awesome great to see a port for QNX. »*

Elle ajoute `core/shared/platform/qnx/` et `product-mini/platforms/qnx/` (9 fichiers, +493/−2),
et a été **exécutée sur QNX 8.0** — image construite avec `mkqnximage --type=qemu`, sous
QEMU/KVM, avec interpréteur, interpréteur rapide et AOT : **133 des 137 tests unitaires amont
passent** (les 4 échecs viennent d'une VM de 1 Gio face à une mémoire linéaire de 4 Gio, QNX ne
sur-allouant pas). Nous n'avons rien exécuté sur cible.

**Nous n'ouvrons donc aucune demande de fusion sur le portage.** Ce serait un doublon, et leurs
solutions aux deux écarts que nous avions trouvés sont supérieures aux nôtres :

| Écart | Notre correctif | Le leur |
|---|---|---|
| `MAP_32BIT` absent | `#if defined(MAP_32BIT)` → on **renonce** à l'optimisation | `MAP_BELOW` de QNX (hint = borne haute) → optimisation **préservée** |
| `-ldl` / `-lpthread` | spécial-cas QNX dans le CMakeLists de `platforms/linux` | un vrai `product-mini/platforms/qnx/` + `link_libraries(socket m)` |

Détail complet, daté et sourcé : [`docs/E5-preuves-absence.md`](../docs/E5-preuves-absence.md).

## Amont

| | |
|---|---|
| Projet | **WAMR** — WebAssembly Micro Runtime |
| URL | https://github.com/bytecodealliance/wasm-micro-runtime |
| Version épinglée | `WAMR-2.4.5`, commit `25bd7eb` — voir [`VERSIONS`](../VERSIONS) |
| Licence amont | Apache-2.0 **WITH LLVM-exception** (vérifié ligne 204 du `LICENSE`) |
| Port QNX en amont | **PR #5200, ouverte, non fusionnée à ce jour** — ce n'est pas la nôtre |
| Notre demande de fusion | **aucune, volontairement** (doublon de #5200) |

> Le cahier des charges citait initialement `github.com/bytecodealliance/wamr`. Cette URL renvoie
> **404** ; le dépôt réel est `bytecodealliance/wasm-micro-runtime`. Correction vérifiée le
> 2026-11-12, consignée dans [`docs/DECISIONS.md`](../docs/DECISIONS.md).

## Contenu de ce dossier

| Chemin | État |
|---|---|
| `qnx-ports/` | recette de port **testée** : les deux cibles (`nto x86_64`, `nto aarch64le`) construisent et s'installent sans erreur. **PR ouverte : [qnx-ports/build-files#337](https://github.com/qnx-ports/build-files/pull/337)** |
| `wamr/patches/` | 2 correctifs **vérifiés par compilation et liaison**, conservés comme trace de la vérification indépendante. **Non soumis** |
| `wamr/cover-letter.md` | **périmée, ne pas soumettre** — elle demande s'il existe un effort QNX en cours ; la réponse est oui |
| `wamr/build-instructions.md` | à jour : Linux vérifié de bout en bout, QNX vérifié en compilation + liaison + exécution |
| `wamr/PORTING-NOTES.md` | à jour : résultats mesurés, écarts comparés à ceux de #5200 |

## La PR `qnx-ports/build-files#337` — ce qu'elle est, et comment elle a été testée

**Elle ajoute `ports/wamr/`** : la recette qui construit WAMR 2.4.5 pour QNX 8.0 dans l'arbre
officiel des ports QNX, plus le workflow CI correspondant. C'est le **seul dépôt amont où le
portage WAMR n'existe pas encore** — c'est donc une contribution réellement disponible, et non un
doublon.

Testée avant d'être ouverte, sur cette machine, avec le SDP 8.0 et le framework `mk` du SDP
(`$QNX_TARGET/usr/include/mk/recurse.mk`, qui fait partie de l'installation) :

```
JLEVEL=$(nproc) QNX_PROJECT_ROOT="<WAMR avec le support QNX>" make -C build-files/ports/wamr install
```

| Cible | `libiwasm.a` | `iwasm` |
|---|---|---|
| `nto x86_64` | 1 348 796 o | 715 024 o |
| `nto aarch64le` | 1 371 320 o | 744 464 o |

Les deux cibles **construisent et s'installent, sans erreur**.

Un détail qui a coûté une heure et qui vaut d'être écrit pour le prochain port : la correspondance
de cible doit se faire sur **`CPUVARDIR`**, pas sur `CPU`. Sur cet arbre `CPU` vaut `aarch64`
alors que `CPUVARDIR` vaut `aarch64le` — exactement le même décalage que les binutils
(`ntoaarch64-ar`) et la variante qcc (`-Vgcc_ntoaarch64le`). Se fier à `CPU` donne un
`WAMR_BUILD_TARGET` vide, et cmake s'arrête sur « Build target isn't set ».

**Dépendance assumée et documentée dans la PR** : l'arbre WAMR amont n'a pas encore de couche
plateforme QNX — elle est proposée par la PR #5200, encore ouverte. La CI de la recette compile
donc la branche de cette PR, et la PR le dit explicitement, en proposant de basculer sur le tag
amont dès la fusion. C'est aussi l'une des deux questions posées aux relecteurs.

## Ce qui reste libre comme contribution amont

1. **`qnx-ports/build-files`** — aucune entrée `wamr` (vérifié le 2026-11-12 : ~150 ports lus,
   `github.com/qnx-ports/wamr` en 404, recherche `org:qnx-ports wasm` → **0 dépôt**). Une recette
   de port y a toute sa place, idéalement une fois #5200 fusionnée. Notre recette et notre
   `qnx.nto.toolchain.cmake` sont écrits, et notre chaîne `tools/build-qnx.sh` prouve qu'ils
   fonctionnent.
2. **Le harnais de tests amont sur QNX** — l'auteur de #5200 écrit noir sur blanc que les tests
   demandent « `/proc/self/exefile` instead of `/proc/self/exe`, and not passing NULL to `%s` »,
   que ces adaptations « are not part of this PR », et qu'« **They can be sent separately if
   wanted** ». Contribution petite, utile, explicitement offerte et non prise.
3. **Le courtier de capacités** — l'apport réel d'Éclat, que personne ne propose : chaque import
   d'un module WebAssembly devient une décision de politique, avec budget CPU, empreinte vérifiée
   et journal infalsifiable. Indépendant du portage, donc indépendant de #5200.

## Leçon

Vérifier les doublons **avant** de publier. Nos deux correctifs étaient prêts, testés, et la lettre
rédigée ; la recherche de doublons a pris deux minutes et a évité de proposer publiquement à un
projet de 6 200 étoiles une solution inférieure à celle déjà en revue. La recherche GitHub
`search/issues` renvoyait `422` sur ce dépôt ; `gh pr list --json number,title` filtré ensuite a
fonctionné — à retenir.