# Contribution amont à WAMR — couche plateforme QNX

| | |
|---|---|
| Amont | [`bytecodealliance/wasm-micro-runtime`](https://github.com/bytecodealliance/wasm-micro-runtime) (WAMR) |
| Version visée | `WAMR-2.4.5` (release du 2026-06-29) |
| Objet | ajouter `core/shared/platform/qnx/` et `product-mini/platforms/qnx/` |
| État | **non compilé** — le SDP QNX 8.0 n'est pas installé ; PR non ouverte |

## Le trou

Vérifié le 2026-11-12 (méthode et commandes reproductibles dans
[`docs/E5-preuves-absence.md`](../../docs/E5-preuves-absence.md)) :

- `core/shared/platform/` contient 15 couches système — `alios android common cosmopolitan
  darwin ego esp-idf freebsd linux-sgx linux nuttx riot rt-thread vxworks windows zephyr` —
  et **aucune `qnx`**.
- `product-mini/platforms/` contient 17 cibles, dont `posix`, et **aucune `qnx`**.
- Côté QNX, l'index officiel `qnx-ports/build-files` (~150 ports) ne contient **aucun** runtime
  WebAssembly, et `github.com/qnx-ports/wamr` renvoie **404**.

Autrement dit : QNX 8.0 équipe des calculateurs critiques, et aucun runtime WebAssembly n'y est
porté. WAMR y a toute sa place — ses ports `nuttx`, `zephyr`, `rt-thread` et `riot` montrent que
les micro-noyaux temps réel sont une cible naturelle du projet.

## Ce qui est prêt dans ce dépôt

| Fichier | Contenu |
|---|---|
| `cover-letter.md` | la lettre d'accompagnement de la PR (anglais) |
| `build-instructions.md` | comment construire, sur Linux (vérifiable aujourd'hui) et sur QNX (à venir) |
| `PORTING-NOTES.md` | chaque écart POSIX attendu et sa résolution |

## Ce qui n'est PAS prêt

Rien n'a été compilé pour QNX. Les fichiers `platform/qnx/` n'existent pas encore dans ce dépôt :
ils seront écrits une fois le SDP installé (essai **E2** du cahier des charges, bloquant). Publier
une couche plateforme non compilée ne rendrait service ni à l'amont ni au concours.

## Ce qui EST vérifié et fonctionne aujourd'hui

Le **courtier de capacités** — la partie qui donne son intérêt au portage — est écrit, testé et
vert sur Linux, sans aucune dépendance à QNX :

- `core/` en C11 strict (`-std=c11 -O2 -Wall -Wextra -Werror`), pur : ni horloge, ni fichier, ni
  socket, ni allocation, ni état global mutable — **161 vérifications** ;
- propre sous **ASan/UBSan** ;
- **fuzz** du parseur de politique : 3 000 entrées mutées, 0 plantage ;
- **22 mutants** injectés dans le cœur, **22 tués** (taux de détection 100 %) ;
- **jumeau Python indépendant** et test différentiel : **10 000 décisions** comparées par graine,
  concordantes (graines 1, 2, 3, 5, 7) ;
- contrat gelé dans [`CONTRACT.md`](../../CONTRACT.md), spécifié en anglais dans
  [`docs/spec-en.md`](../../docs/spec-en.md).

C'est ce moteur que la couche `platform/qnx/` exposera aux modules WebAssembly, via un
gestionnaire de ressources QNX — chaque import devenant un `MsgSend` soumis à politique.