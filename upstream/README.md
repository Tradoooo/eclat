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
| `qnx-ports/` | recette de port + fichier d'outillage CMake QNX **vérifié** (variante `qcc`, noms des binutils). **Piste de contribution toujours libre** |
| `wamr/patches/` | 2 correctifs **vérifiés par compilation et liaison**, conservés comme trace de la vérification indépendante. **Non soumis** |
| `wamr/cover-letter.md` | **périmée, ne pas soumettre** — elle demande s'il existe un effort QNX en cours ; la réponse est oui |
| `wamr/build-instructions.md` | à jour : Linux vérifié de bout en bout, QNX vérifié en compilation + liaison |
| `wamr/PORTING-NOTES.md` | à jour : résultats mesurés, écarts comparés à ceux de #5200 |

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