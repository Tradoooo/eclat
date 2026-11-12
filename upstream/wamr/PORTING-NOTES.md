# Notes de portage — QNX 8.0

**Statut : mesuré, plus spéculatif.** Ce document a été réécrit le 12 novembre 2026 après
l'installation réelle du SDP QNX 8.0 et l'exécution de l'essai **E2**. La version précédente
listait des écarts *attendus* ; celle-ci liste des écarts *observés*.

## Périmètre exact de ce qui a été mesuré

`tools/e2-probe.sh` compile, avec le vrai `qcc` du SDP 8.0, **le jeu de fichiers que le cmake
amont retient pour notre configuration** (interpréteur seul, `LIBC_WASI=0`, `DEBUG_INTERP=0`,
pas de `mremap`) :

```
core/shared/platform/common/posix/posix_time.c
core/shared/platform/common/posix/posix_thread.c
core/shared/platform/common/posix/posix_sleep.c
core/shared/platform/common/posix/posix_blocking_op.c
core/shared/platform/common/posix/posix_malloc.c
core/shared/platform/common/posix/posix_memmap.c
core/shared/platform/common/memory/mremap.c
core/shared/platform/linux/platform_init.c
```

Ce choix n'est pas arbitraire — il est **lu dans le cmake amont**
(`common/posix/platform_api_posix.cmake`) :

- si `WAMR_BUILD_LIBC_WASI != 1` → `posix_file.c` **et** `posix_clock.c` sont retirés du build ;
- si `LIBC_WASI != 1` **et** `DEBUG_INTERP != 1` → `posix_socket.c` est retiré, et `libc-util`
  n'est pas inclus (donc `libc_errno.c` hors build) ;
- si `mremap` est absent → `common/memory/` est **ajouté** au build ;
- `common/math/math.c` n'est référencé par aucun de ces cmake : il ne fait pas partie du build.

Tester un jeu de fichiers différent produirait un verdict hors sujet. C'est exactement l'erreur
de la première version de cette sonde : elle compilait 11 fichiers dont 3 que l'amont exclut dans
notre configuration, et annonçait « 13/22 » alors que les échecs portaient sur du code non
construit.

## Résultat

| Cible | Sans correctif | Avec `patches/0001` |
|---|---|---|
| `nto x86_64` (`-Vgcc_ntox86_64`) | 7 / 8 | **8 / 8** |
| `nto aarch64le` (`-Vgcc_ntoaarch64le`) | 8 / 8 | **8 / 8** |

**Verdict E2 : OUI.** La couche d'abstraction POSIX de WAMR compile pour QNX 8.0 avec **un seul
correctif d'une ligne**.

Drapeaux utilisés : `-D_QNX_SOURCE -DWASM_HAVE_MREMAP=0` et les en-têtes
`platform/linux`, `platform/include`, `common/posix`, `common/libc-util`, `core/shared/utils`,
`core/iwasm/include`, `core/iwasm/common`.

## Le seul écart observé

**`MAP_32BIT` n'existe pas sur QNX.** Message exact, `posix_memmap.c:88`, cible x86_64 :

```
error: 'MAP_32BIT' undeclared (first use in this function); did you mean 'MMAP_MAP_32BIT'?
```

Code amont en cause :

```c
#if defined(BUILD_TARGET_X86_64) || defined(BUILD_TARGET_AMD_64)
#ifndef __APPLE__
    if (flags & MMAP_MAP_32BIT)
        map_flags |= MAP_32BIT;
#endif
#endif
```

`MAP_32BIT` est un drapeau spécifique à `mmap(2)` sous Linux (allouer dans les 2 premiers Gio).
L'amont n'exclut que `__APPLE__` : toute autre cible non-Linux qui définit `BUILD_TARGET_X86_64`
casse. Le `<sys/mman.h>` de QNX ne définit pas `MAP_32BIT`. La variante `aarch64le` n'était pas
touchée, `BUILD_TARGET_X86_64` n'y étant pas posé.

**Correctif retenu** (`upstream/wamr/patches/0001-platform-posix-gate-MAP_32BIT-on-feature-test.patch`) :
tester la présence de la fonctionnalité plutôt que tenir une liste noire de systèmes.

```c
#if defined(MAP_32BIT)
```

Ce correctif est strictement meilleur que l'existant : il conserve l'optimisation là où elle
existe (Linux) et laisse toutes les autres cibles compiler sans modification. Vérifié :
`git apply --check` passe sur WAMR-2.4.5, et les deux cibles QNX compilent après application.

## Écarts anticipés qui ne se sont PAS produits

Les hypothèses de la version précédente de ce document, toutes infirmées par la mesure :

| Anticipé | Observé |
|---|---|
| `clock_gettime(CLOCK_MONOTONIC)` à adapter | compile tel quel |
| threads POSIX / attributs de pile | compile tel quel |
| `pthread_condattr_setclock` absent | non atteint dans ce jeu de fichiers |
| `posix_memalign` / `aligned_alloc` manquants | `posix_malloc.c` compile tel quel |
| pages de garde `mmap(PROT_NONE)` | `posix_memmap.c` compile (hors `MAP_32BIT`) |
| atomiques `__atomic_*` sur aarch64le | compilent |
| `epoll` nécessaire | non requis dans notre configuration |

`platform_internal.h` de la couche `linux` inclut `pthread.h`, `semaphore.h`, `dirent.h`,
`fcntl.h`, `unistd.h`, `poll.h`, `sched.h`, `errno.h`, `math.h`, `signal.h` : **tous présents**
dans `$QNX_TARGET/usr/include`. QNX se déclare conforme POSIX et le vérifie ici.

## Ce qui n'est PAS encore fait

Honnêteté sur la portée du verdict :

1. **Compilation d'objets seulement.** Rien n'est encore **lié** : aucune bibliothèque, aucun
   binaire QNX n'existe à ce stade.
2. **Le cœur de l'interpréteur n'est pas compilé** (`core/iwasm/interpreter`, `core/shared/utils`,
   `wasm_runtime`…) — seule la couche plateforme l'a été. C'était la question posée par E2, pas
   le portage complet.
3. **Aucun module `.wasm` n'a été exécuté sur QNX.** Les 12 vérifications d'intégration
   (`make test-wasm`) tournent sur Linux.
4. **`platform/qnx/` n'existe pas encore** : la sonde compile la couche `linux` + `common/posix`,
   ce qui prouve la réutilisabilité mais ne constitue pas encore la cible QNX du dépôt.
5. Le banc de latence sur cible, la politique `secpol`, le courtier en gestionnaire de
   ressources : tout cela reste à faire (lots L7 à L9).

Ce qui a changé, c'est le **risque** : le cahier des charges conditionnait la poursuite du projet
à E2 (« sinon on arrête là »). E2 est positif, avec un correctif d'une ligne. La suite est du
travail, plus une inconnue bloquante.

## Environnement de mesure

SDP QNX 8.0 installé dans `/home/baptiste/qnx800` (5,9 Go), `qcc` 12.2.0, binutils 2.43.0,
`QNX_HOST=$SDP/host/linux/x86_64`, `QNX_TARGET=$SDP/target/qnx`, cibles `x86_64` et `aarch64le`.
Licence : QNX SDP 8.0 Education, classe Non-Commercial/Academic, série 911285-10384620.
Reproductible : `source tools/setup-sdp.sh && tools/e2-probe.sh [--patched]`.