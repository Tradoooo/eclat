# `upstream/` — la contribution amont

Éclat est soumis dans la catégorie **Open-Source Contribution & Ports**. Les règles de la
catégorie imposent : un dépôt nouveau, le projet amont référencé **par URL**, son historique
**non copié**, **aucun fork soumis**, et l'URL de la demande de fusion si elle est ouverte.

Ce dossier contient tout ce qui est destiné à quitter le dépôt pour rejoindre l'amont.

## Amont

| | |
|---|---|
| Projet | **WAMR** — WebAssembly Micro Runtime |
| URL | https://github.com/bytecodealliance/wasm-micro-runtime |
| Version épinglée | `WAMR-2.4.5` (release du 2026-06-29) — voir [`VERSIONS`](../VERSIONS) |
| Licence amont | Apache-2.0 avec exception LLVM (**à confirmer** dans `LICENSE` avant redistribution) |
| Demande de fusion | **non ouverte à ce jour** — le SDP QNX 8.0 n'est pas installé, donc la couche `platform/qnx/` n'est pas encore compilée. Une PR non compilée ne serait pas recevable |

> Le cahier des charges citait initialement `github.com/bytecodealliance/wamr`. Cette URL renvoie
> **404** ; le dépôt réel est `bytecodealliance/wasm-micro-runtime`. Correction vérifiée le
> 2026-11-12 et consignée dans [`docs/DECISIONS.md`](../docs/DECISIONS.md).

## Contenu

| Chemin | Destiné à | État |
|---|---|---|
| `qnx-ports/` | `qnx-ports/build-files`, dossier `ports/wamr/` | recette écrite, **non exécutée** (pas de SDP) |
| `wamr/` | `bytecodealliance/wasm-micro-runtime` | lettre, instructions de build, notes de portage |

## Pourquoi le trou est réel

Vérifié le 2026-11-12, reproductible — voir [`docs/E5-preuves-absence.md`](../docs/E5-preuves-absence.md) :

- aucune entrée `wasm` / `wamr` / `wasi` / `wasmtime` / `wasmer` dans l'index officiel
  `qnx-ports/build-files` (~150 ports lus) ;
- `github.com/qnx-ports/wamr` → **404** ;
- recherche `org:qnx-ports wasm` → **0 dépôt** ;
- `core/shared/platform/` amont : 15 couches système, **aucune `qnx`** ;
- `product-mini/platforms/` amont : 17 cibles, **aucune `qnx`**.

## Ce qui manque pour ouvrir la PR

1. Le SDP QNX 8.0 installé (licence QNX Everywhere, gratuite, ~1 h) — **action humaine**.
2. La compilation effective de `core/shared/platform/qnx/` avec `qcc` (essai **E2**, bloquant).
3. Les patches formatés `git format-patch` produits depuis un clone réel de WAMR.

Les étapes 1 à 3 sont hors de ce dépôt tant que le SDP manque. Rien ici ne prétend avoir été
compilé pour QNX.