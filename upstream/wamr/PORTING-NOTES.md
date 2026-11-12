# Notes de portage — écarts POSIX attendus sur QNX

Ce document anticipe les écarts entre ce que WAMR attend de sa couche plateforme et ce que QNX
8.0 fournit. **Aucun de ces écarts n'a été constaté par compilation** : le SDP n'est pas installé.
Chaque ligne est donc soit une attente fondée sur la documentation, soit une hypothèse assumée.

Le document sera réécrit au fur et à mesure des constats réels (exigence **ECL-17** : c'est la
valeur de la contribution pour la communauté). Les statuts sont : `DOC` = fondé sur la
documentation QNX, `HYP` = hypothèse à trancher par le code, `PREC` = précédent observé dans un
autre port WAMR.

## Point de départ

`core/shared/platform/` ne contient pas de couche POSIX générique : les couches sont par système
(`linux`, `darwin`, `freebsd`, `nuttx`, …), avec un socle `common` et un contrat d'interface dans
`include`. En revanche `product-mini/platforms/` contient **un dossier `posix`**.

Deux stratégies possibles, à trancher avec les mainteneurs (question posée dans la lettre
d'accompagnement) :

| Stratégie | Avantage | Risque |
|---|---|---|
| dériver de `platform/linux` | QNX est conforme POSIX, `qcc` est un frontal GCC : le delta devrait être minime | `linux` peut utiliser des spécifiques Linux (epoll, `pthread_setaffinity_np`, `/proc`) |
| partir de `platform/common` + couche fine | delta explicite, rien d'inutile | plus de code à écrire et à maintenir |

Les ports `nuttx`, `zephyr`, `rt-thread` et `riot` sont les précédents à lire avant de choisir
(`PREC`) : ils montrent ce qu'un port de micro-noyau temps réel doit réellement fournir.

## Écart par écart

| Domaine | Ce que WAMR attend | Ce que QNX fournit | Écart attendu | Statut |
|---|---|---|---|---|
| Horloge monotone | `clock_gettime(CLOCK_MONOTONIC)` en ns | POSIX conforme ; `CLOCK_MONOTONIC` documenté | aucun en principe ; vérifier la résolution réelle | DOC |
| Horloge de sommeil | `clock_nanosleep` ou `nanosleep` | `nanosleep` POSIX | `clock_nanosleep` peut manquer → repli `nanosleep` | HYP |
| Threads | `pthread_create/join/detach`, attributs de pile | pthreads POSIX | vérifier le comportement de la taille de pile minimale | DOC |
| Verrous | `pthread_mutex_t`, éventuellement `PTHREAD_MUTEX_RECURSIVE` | pthreads POSIX | vérifier les attributs récursifs | HYP |
| Variables de condition | `pthread_cond_t` avec horloge monotone (`pthread_condattr_setclock`) | à confirmer | si absent : interdire les threads Wasm (déjà hors périmètre, cahier §3.2) | HYP |
| Mémoire alignée | `posix_memalign` / `aligned_alloc` | à confirmer | repli possible : sur-allouer + aligner à la main | HYP |
| Mémoire protégée | pages de garde via `mmap(PROT_NONE)` | `mmap` POSIX | vérifier les drapeaux acceptés | HYP |
| Taille de page | `sysconf(_SC_PAGESIZE)` | POSIX | aucun | DOC |
| `libc_builtin` | jeu réduit de `memcpy`, `memset`, `strlen`, `printf`… | libc QNX | aucun en principe | DOC |
| Sortie d'erreur | `os_printf` / `os_vprintf` | à mapper sur la libc QNX | mapping direct attendu | HYP |
| Atomiques | `__atomic_*` de GCC/Clang | `qcc` est un frontal GCC | vérifier le support sur aarch64le et x86_64 | HYP |
| Affinité CPU | `pthread_setaffinity_np` (spécifique Linux/glibc) | **probablement absent** ; QNX a ses propres mécanismes | à neutraliser ou à remplacer — c'est l'écart le plus probable | HYP |
| `epoll` | utilisé par certaines couches | QNX fournit un port `epoll` (présent dans l'index `qnx-ports`) | disponible, mais à épingler | DOC |
| Signaux / `SIGSTKSZ` | pile alternative | à confirmer | peut différer ; non critique si les threads Wasm restent désactivés | HYP |

## Ce qui est déjà tranché par le périmètre

Le cahier des charges (§3.2) met **hors périmètre** : les threads WebAssembly et la mémoire
partagée, le support JIT (interpréteur et AOT seulement), WASI complet. Trois familles d'écarts
potentiels disparaissent donc d'emblée — c'est ce qui rend le portage tenable en cinq semaines.

## Garde-fou de conception

Le contrat d'interface de la couche plateforme est **le même** pour `platform/posix/` et
`platform/qnx/` : une seule déclaration, deux implémentations, jamais de code recopié. Toute la
logique de décision se teste donc sur Linux avant d'avoir le SDP — c'est déjà le cas aujourd'hui
(161 vérifications, 22 mutants tués, 10 000 décisions différentielles).

## Règle de rédaction

Quand un écart sera constaté pour de vrai, cette ligne devra être remplacée par : l'écart exact
(message du compilateur ou comportement observé), la résolution retenue, et si la résolution
s'écarte de l'amont, la raison. Pas de « devrait fonctionner » dans un document de portage.