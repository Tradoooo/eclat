# Port WAMR pour QNX 8.0 — recette `qnx-ports/build-files`

Recette destinée à être déposée dans `ports/wamr/` de
[qnx-ports/build-files](https://github.com/qnx-ports/build-files), pour que l'équipe des ports
puisse la consommer **sans adaptation**.

## État honnête

**Cette recette n'a jamais été exécutée.** Deux raisons, toutes deux documentées :

1. Le SDP QNX 8.0 n'est pas installé sur la machine de développement (licence QNX Everywhere
   gratuite mais action humaine requise).
2. WAMR amont ne contient **ni** `core/shared/platform/qnx` **ni** `product-mini/platforms/qnx`
   (vérifié le 2026-11-12, voir [`docs/E5-preuves-absence.md`](../../docs/E5-preuves-absence.md)).
   Créer cette cible est l'objet même de la contribution.

Les blocs non vérifiés sont marqués `HYPOTHÈSE` dans `common.mk`.

## Ce qui est vérifié

La **structure** de la recette est conforme à l'arbre réel des ports, lue sur `ports/zlib`
le 2026-11-12 :

```
ports/wamr/
├── Makefile                    → include recurse.mk
├── common.mk                   → qconfig.mk, qmacros.mk, qtargets.mk, NAME, PREFIX, CFLAGS
├── README.md
├── qnx.nto.toolchain.cmake     → à réutiliser depuis l'arbre des ports (non recopié ici)
├── nto-aarch64-le/Makefile     → include ../common.mk
└── nto-x86_64-o/Makefile       → include ../common.mk
```

Les noms d'options CMake de WAMR (`WAMR_BUILD_INTERP`, `WAMR_BUILD_AOT`,
`WAMR_BUILD_LIBC_BUILTIN`, `WAMR_BUILD_LIBC_WASI`, `WAMR_BUILD_FAST_INTERP`,
`WAMR_BUILD_THREAD_MGR`) sont vérifiés dans `doc/build_wamr.md` amont.

## Installation dans l'arbre des ports

```bash
git clone https://github.com/qnx-ports/build-files
cd build-files/ports
mkdir wamr
# copier le contenu de ce dossier dans ports/wamr/, sauf ce README
cp /chemin/vers/eclat/upstream/qnx-ports/{Makefile,common.mk} wamr/
cp -r /chemin/vers/eclat/upstream/qnx-ports/nto-* wamr/
# récupérer le fichier d'outillage CMake depuis un port existant (identique d'un port à l'autre)
cp zlib/qnx.nto.toolchain.cmake wamr/
```

Puis, avec le SDP sourcé (`QNX_HOST` et `QNX_TARGET` posés) :

```bash
cd wamr
make -j$(nproc)            # descend dans nto-x86_64-o/ puis nto-aarch64-le/ via recurse.mk
```

## Environnement Docker

L'arbre des ports fournit une image de construction reproductible (`docker/README.md` amont).
Le `Dockerfile` de ce dossier montre l'environnement minimal attendu.

**Le SDP n'est jamais inclus dans l'image** : il est sous licence QNX et ne peut pas être
redistribué. Il est monté depuis l'hôte au moment de la construction.

## Ce qu'il reste à faire

| Étape | Bloquant ? | Qui |
|---|---|---|
| Installer le SDP 8.0 | oui | humain (licence gratuite, ~1 h) |
| Créer `core/shared/platform/qnx/` (essai **E2**) | oui | contribution amont |
| Créer `product-mini/platforms/qnx/` | oui | contribution amont |
| Exécuter cette recette et corriger les `HYPOTHÈSE` | oui | après les deux précédents |
| Publier le résultat sur le tableau de bord `oss.qnx.com` | non | équipe des ports |