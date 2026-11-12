# E5 — Preuves de l'absence d'un port WebAssembly pour QNX

L'essai de faisabilité **E5** du cahier des charges (§10) est bloquant : s'il existe déjà un port
WebAssembly pour QNX, le sujet tombe. Ce document consigne ce qui a été **vérifié**, comment, et
ce qui ne l'est **pas** encore. Aucune affirmation n'est avancée sans sa source.

Dernière campagne de vérification : **12 novembre 2026, 01 h–02 h (Europe/Paris)**, par
requêtes API GitHub et lectures directes de fichiers bruts.

---

## 1. Ce qui est vérifié

| # | Affirmation | Méthode | Résultat |
|---|---|---|---|
| 1 | Aucun port Wasm dans l'index officiel des ports QNX | Lecture intégrale de `qnx-ports/build-files/README.md` (table « Port Status », ~150 entrées) | **Aucune** entrée `wasm`, `wamr`, `wasi`, `wasmtime`, `wasmer`, `v8`, `quickjs-wasm` |
| 2 | Aucun dépôt `wamr` dans l'organisation `qnx-ports` | `GET /repos/qnx-ports/wamr` | **404 Not Found** |
| 3 | Aucun dépôt de l'organisation `qnx-ports` ne mentionne « wasm » | `GET /search/repositories?q=org:qnx-ports+wasm` | **total_count = 0** |
| 4 | WAMR amont n'a **pas** de couche plateforme QNX | `GET /repos/bytecodealliance/wasm-micro-runtime/contents/core/shared/platform` | 17 entrées : `alios android common cosmopolitan darwin ego esp-idf freebsd include linux-sgx linux nuttx riot rt-thread vxworks windows zephyr` + `README.md` — **pas de `qnx`** |
| 5 | WAMR amont n'a **pas** de produit QNX | `GET .../contents/product-mini/platforms` | 19 entrées : `alios-things android common cosmopolitan darwin esp-idf freebsd ios linux-sgx linux nuttx posix riot rt-thread vxworks windows zephyr` — **pas de `qnx`** |
| 6 | L'URL amont citée par le cahier était fausse | `GET /repos/bytecodealliance/wamr` et `git ls-remote` | **404 / « Repository not found »**. Le vrai dépôt est `bytecodealliance/wasm-micro-runtime` (6 202 ★, description « WebAssembly Micro Runtime (WAMR) »). Corrigé partout dans le dépôt |
| 7 | Dernière release amont | `GET .../releases/latest` | **WAMR-2.4.5**, publiée le 2026-06-29 |
| 8 | Conventions réelles de l'arbre des ports | Lecture de `ports/zlib/` : `Makefile`, `common.mk`, `qnx.nto.toolchain.cmake`, `nto-aarch64-le/`, `nto-x86_64-o/` | `Makefile` = `include recurse.mk` ; `common.mk` inclut `qconfig.mk`, `qmacros.mk`, `qtargets.mk` ; un sous-répertoire par cible dont le Makefile fait `include ../common.mk` |
| 9 | Noms des options de build WAMR | Lecture de `doc/build_wamr.md` amont | Options confirmées : `WAMR_BUILD_INTERP`, `WAMR_BUILD_AOT`, `WAMR_BUILD_LIBC_BUILTIN`, `WAMR_BUILD_LIBC_WASI`, `WAMR_BUILD_FAST_INTERP`, `WAMR_BUILD_TARGET`, `WAMR_BUILD_PLATFORM`, `WAMR_BUILD_THREAD_MGR`, `WAMR_BUILD_SANITIZER` |

**Conclusion sur E5.** Dans tout ce qui est interrogeable sans exécuter de JavaScript — l'index
officiel des ports, l'organisation GitHub `qnx-ports`, et l'arbre amont de WAMR lui-même — il
n'existe **aucun** port WebAssembly pour QNX. Le filon est réel.

## 2. Ce qui n'est PAS vérifié (réserve honnête)

| Source | Pourquoi ça reste ouvert | Comment trancher |
|---|---|---|
| `https://oss.qnx.com/` (tableau de bord, ~2 500 ports annoncés) | Application JavaScript, non interrogeable en HTTP simple ; un port pourrait y figurer sans dépôt public dans `qnx-ports` | Un navigateur réel, 10 minutes |
| Discord QNX et r/qnx | Un portage interne ou abandonné peut n'avoir laissé aucune trace dans les dépôts | Poser la question, 1 heure |
| Fournisseurs tiers ( BlackBerry Drive, intégrateurs) | Un port privé n'est pas visible | Question directe à QNX |

La réserve porte sur l'**absence absolue**. L'absence dans l'écosystème public open source QNX,
elle, est établie — et c'est ce que la catégorie *Open-Source Contribution & Ports* évalue.

## 3. Effet sur l'essai E2 — **tranché le 2026-11-12**

E2 demandait : « existe-t-il une couche plateforme POSIX réutilisable ? ». Les vérifications 4 et 5
y répondaient partiellement, sans compiler :

- `core/shared/platform/` ne contient **pas** de couche `posix` générique : les couches sont par
  système (`linux`, `darwin`, `freebsd`, `nuttx`, `zephyr`, …), plus `common` et `include`.
- `product-mini/platforms/` contient en revanche **un dossier `posix`**.
- Les ports de petits systèmes temps réel (`nuttx`, `zephyr`, `rt-thread`, `riot`, `vxworks`)
  montrent le gabarit attendu pour une cible embarquée : c'est le précédent à suivre pour QNX.

**Réponse mesurée depuis** (SDP QNX 8.0 installé, `qcc` 12.2.0) : **OUI**. Les 8 fichiers que le
cmake amont retient pour notre configuration — `common/posix/{posix_time,posix_thread,posix_sleep,
posix_blocking_op,posix_malloc,posix_memmap}.c`, `common/memory/mremap.c` et
`linux/platform_init.c` — compilent pour `nto x86_64` **et** `nto aarch64le`, avec **un seul
correctif d'une ligne** (`MAP_32BIT`, drapeau `mmap` propre à Linux que l'amont n'excluait que
pour `__APPLE__`). 16/16 avec le correctif, 15/16 sans.

Détail, méthode et portée exacte du verdict : [`upstream/wamr/PORTING-NOTES.md`](../upstream/wamr/PORTING-NOTES.md)
et `docs/DECISIONS.md`. Reproduction : `source tools/setup-sdp.sh && tools/e2-probe.sh --patched`.

**Ce que ce verdict ne dit pas** : rien n'est encore lié, le cœur de l'interpréteur n'est pas
compilé pour QNX, et aucun module `.wasm` n'a tourné sur cible.

## 4. Reproduction

Toutes les vérifications sont reproductibles avec un jeton GitHub en lecture seule :

```bash
gh api repos/qnx-ports/wamr                                    # → 404
gh api "search/repositories?q=org:qnx-ports+wasm" --jq .total_count   # → 0
gh api repos/bytecodealliance/wasm-micro-runtime/contents/core/shared/platform --jq '.[].name'
gh api repos/bytecodealliance/wasm-micro-runtime/contents/product-mini/platforms --jq '.[].name'
gh api repos/bytecodealliance/wasm-micro-runtime/releases/latest --jq .tag_name
curl -sL https://raw.githubusercontent.com/qnx-ports/build-files/main/README.md
```