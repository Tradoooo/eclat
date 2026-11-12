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

| Cible | Sans correctif | Avec les correctifs |
|---|---|---|
| `nto x86_64` (`-Vgcc_ntox86_64`) | 7 / 8 | **8 / 8** |
| `nto aarch64le` (`-Vgcc_ntoaarch64le`) | 8 / 8 | **8 / 8** |

**Verdict E2 : OUI.** La couche d'abstraction POSIX de WAMR compile pour QNX 8.0 avec **un seul
correctif d'une ligne** (`patches/0001`).

Drapeaux utilisés : `-D_QNX_SOURCE -DWASM_HAVE_MREMAP=0` et les en-têtes
`platform/linux`, `platform/include`, `common/posix`, `common/libc-util`, `core/shared/utils`,
`core/iwasm/include`, `core/iwasm/common`.

## Et au-delà de E2 : WAMR entier compile **et se lie** pour QNX 8.0

E2 ne portait que sur la couche plateforme. Depuis, la chaîne complète est passée
(`tools/build-qnx.sh`, cmake 4.4.4 + `upstream/qnx-ports/qnx.nto.toolchain.cmake`) :

| Artefact | Taille | Vérification |
|---|---|---|
| `libiwasm.a` pour QNX x86_64 | 758 928 o | archive produite par cmake, 0 erreur |
| `iwasm` (interpréteur) | 265 384 o | `ELF 64-bit LSB pie, interpreter /usr/lib/ldqnx-64.so.2` |
| `eclat-run` (chargeur Éclat) x86_64 | 382 056 o | idem ; symboles `eclat_decide`, `eclat_policy_parse`, `eclat_sha256`, `eclat_platform_get`, `eclat_runtime_run`, `wasm_runtime_load` présents |
| `eclat-run` aarch64 | — | construit par la même chaîne (`make qnx QNX_ARCH=aarch64`) |

Dépendances dynamiques du binaire : `libm.so.3`, `libc.so.6`, `libgcc_s.so.1` — **trois
seulement**, ce qui confirme qu'aucune `libdl` ni `libpthread` n'est requise sur QNX.

Un second correctif a été nécessaire pour l'édition de liens (`patches/0002`) : voir ci-dessous.

## Les deux écarts observés

### 1. `MAP_32BIT` n'existe pas sur QNX (`patches/0001`)

Message exact, `posix_memmap.c:88`, cible x86_64 :

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

**Correctif retenu** : tester la présence de la fonctionnalité plutôt que tenir une liste noire
de systèmes — `#if defined(MAP_32BIT)`. Strictement meilleur que l'existant : l'optimisation est
conservée là où elle existe (Linux) et toutes les autres cibles compilent sans modification.

### 2. QNX n'a ni `libpthread` ni `libdl` (`patches/0002`)

`product-mini/platforms/linux/CMakeLists.txt` lie en dur `-lm -ldl -lpthread`. À l'édition de
liens sur QNX :

```
x86_64-pc-nto-qnx8.0.0-ld: cannot find -ldl
x86_64-pc-nto-qnx8.0.0-ld: cannot find -lpthread
```

Vérifié dans le SDP 8.0 : `find $QNX_TARGET -name 'libpthread*' -o -name 'libdl*'` ne renvoie
**rien** — sur QNX, pthreads et `dlopen()` sont dans la libc. `libm` existe en revanche
(`libm.so.3`), donc `-lm` est conservé.

**Correctif retenu** : garder la ligne de liens existante partout ailleurs, et retirer `-ldl`
et `-lpthread` quand `CMAKE_SYSTEM_NAME` vaut `QNX`.

### Écart d'outillage (pas un écart de code)

`cmake` 3.30.5 (celui de l'hôte) n'a **pas** de module `Platform/QNX` : il faut cmake ≥ 3.31.
Le dépôt installe un cmake 4.4.4 isolé dans `.deps/cmake-venv` (venv Python, aucun paquet
système touché), et passe `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` parce que cmake 4.x rejette les
`cmake_minimum_required` anciens de WAMR.

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

Honnêteté sur la portée du résultat :

1. **Rien n'a été exécuté sur QNX.** Les binaires sont construits et vérifiés comme ELF QNX
   (`interpreter /usr/lib/ldqnx-64.so.2`), mais aucune cible QNX 8.0 n'a encore lancé
   `eclat-run`. L'image QEMU livrée par le SDP (`~/qnx800/images/qemu/`) est présente mais son
   `ifs.bin` fait 0 octet : sa construction a été interrompue par une saturation du disque.
2. **Aucun module `.wasm` n'a tourné sur QNX.** Les 12 vérifications d'intégration
   (`make test-wasm`) tournent sur Linux.
3. **`platform/qnx/` existe et compile** (horloge native `ClockTime`, CPU par
   `CLOCK_THREAD_CPUTIME_ID`), mais la couche plateforme **WAMR** utilisée est encore
   `platform/linux` + `common/posix` — c'est ce que E2 a validé. Une couche `platform/qnx`
   dédiée côté WAMR reste à écrire si l'amont la préfère.
4. Le courtier en gestionnaire de ressources (`broker/`, `MsgSend`/`MsgReply`), la politique
   `secpol`, le banc de latence sur cible et la supervision HAM : tout cela reste à faire
   (lots L8, L9).
5. `slogger2.h` est absent des en-têtes du SDP installé : le journal des décisions part sur
   stderr, pas vers le journal système de la cible. Branchement à faire.

Ce qui a changé, c'est le **risque** : le cahier des charges conditionnait la poursuite du projet
à E2 (« sinon on arrête là »). E2 est positif avec deux correctifs minimes, et l'interpréteur
complet se lie pour les deux cibles. La suite est du travail, plus une inconnue bloquante.

## Environnement de mesure

SDP QNX 8.0 installé dans `/home/baptiste/qnx800` (5,9 Go), `qcc` 12.2.0, binutils 2.43.0,
`QNX_HOST=$SDP/host/linux/x86_64`, `QNX_TARGET=$SDP/target/qnx`, cibles `x86_64` et `aarch64le`.
Licence : QNX SDP 8.0 Education, classe Non-Commercial/Academic, série 911285-10384620.
Reproductible : `source tools/setup-sdp.sh && tools/e2-probe.sh [--patched]`.