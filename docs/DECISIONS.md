# DECISIONS — journal des décisions de conception

## 2026-11-12 — Le courtier QNX : la politique passe par une frontière de message synchrone

**Décision.** `broker/` tient le courtier de capacités : il s'enregistre dans l'espace de noms de
QNX, reçoit les demandes de capacité par `MsgSend`/`MsgReply`, applique la politique de `core/`
et répond avec un code de décision. Le client de test (`broker/eclat_client.c`) pose les mêmes
questions qu'un module.

**Preuve** (`docs/preuves/qnx800/2026-11-12-courtier.txt`, console brute de la cible QNX 8.0) :

```
cap_read_sensor(id=1)      -> CAP_OK             (payload=4200)
cap_net_send(chan=9)       -> CAP_DENIED_POLICY
cap_write_actuator(99999)  -> CAP_DENIED_RANGE
cap_read_sensor(id=42)     -> CAP_DENIED_RANGE
cap_kv_put                 -> CAP_DENIED_POLICY
```

La valeur du capteur (4200) a traversé la frontière : ce n'est pas un appel de fonction déguisé.
C'est la propriété de sécurité d'Éclat, enfin démontrée : la décision est prise **d'un seul
côté**, dans un processus dont on peut retirer les abilities.

**Quatre pièges QNX, tous payés et consignés** (ils coûtent des heures ; ils se retrouvent dans
chaque projet QNX, donc les écrire vaut de l'or) :

1. **`name_attach()` n'accepte qu'un NOM.** Un chemin avec des « / » est refusé (EINVAL, vérifié).
   `/dev/cap` exige donc `resmgr_attach()` et le jeu complet de fonctions `iofunc` — c'est
   l'étape suivante du lot L8. En attendant, le courtier s'enregistre sous le nom « cap ».
2. **`name_attach()` CRÉE le canal et le rend dans `name_attach_t.chid`.** Le serveur doit lire
   SUR CE CANAL. Créer son propre `ChannelCreate()` et y lire fait que le client bloque
   indéfiniment dans `name_open()` : le gestionnaire de chemins envoie la poignée de connexion
   au canal du gestionnaire, pas au vôtre. C'est l'erreur classique, et elle ne se manifeste que
   par un blocage silencieux.
3. **Le serveur doit traiter le message `_IO_CONNECT`** (répondre `MsgReply(rcvid, EOK, NULL, 0)`)
   et les pulsations (`_PULSE_CODE_DISCONNECT` → `ConnectDetach`) avant tout message applicatif.
4. **Un message applicatif doit porter un type AU-DESSUS de `_IO_MAX` (0x1ff)** — sinon il est
   interprété par le noyau ou le gestionnaire de chemins. Notre type est `0xec01`. En prime,
   `MsgReply()` prend le **statut en deuxième argument** sur QNX 8.0 : `MsgReply(rcvid, 0, &rep, n)`.

Deux détails d'outillage : QNX n'a pas `pkill` mais `slay` ; et avec `set -o pipefail`, un
`grep -q` qui trouve son motif ferme le tube tôt, l'en amont reçoit SIGPIPE et le pipeline vaut
échec — un test « la VM est-elle vivante ? » répondait « non » alors qu'elle tournait.

**Ce qui reste pour ECL-11** : publier le courtier sous `/dev/cap` avec `resmgr_attach()`, puis
faire passer les imports du module par ce courtier au lieu de l'appel de fonction.

## 2026-11-12 — Chaque étape livre sa preuve, dans le dépôt

**Décision.** `docs/preuves/` est un dossier de preuves, pas de rapports : pour chaque affirmation,
une commande unique, un **artefact brut daté** (sortie de commande non retouchée) et une ligne
dans `docs/preuves/ETAT.md`. `tools/preuves.sh` rejoue tout et régénère l'état ; `make preuves`
est le raccourci. Les artefacts des cibles sont archivés dans `docs/preuves/qnx800/`, ceux de
l'hôte dans `docs/preuves/hote/`.

**Pourquoi.** Une affirmation sans artefact est une opinion. Après l'étape « le courtier marche »,
la preuve a été l'artefact lui-même : la console de la cible QNX 8.0, datée, avec les quatre
décisions rendues par le courtier de l'autre côté de la frontière IPC. On peut la relire, la
différer, et vérifier qu'elle correspond au code livré.

**Règle retenue pour la suite.** Une étape sans preuve n'est pas une étape faite. Les échecs sont
archivés comme des échecs : un artefact rouge vaut mieux qu'une ligne absente.

## 2026-11-12 — Contribution amont : une recette pour l'arbre des ports QNX (PR #337)

**Décision.** La contribution amont d'Éclat est la **recette de port WAMR pour
[qnx-ports/build-files](https://github.com/qnx-ports/build-files)** — ouverte en
[PR #337](https://github.com/qnx-ports/build-files/pull/337) — et non le portage lui-même.

**Pourquoi.** Après la découverte de la PR #5200, il fallait un dépôt amont où notre travail
apporte réellement quelque chose. L'arbre des ports QNX est ce dépôt : vérifié le 2026-11-12, il ne
contient **aucune** entrée WebAssembly (`~150` ports lus, `github.com/qnx-ports/wamr` en 404,
recherche `org:qnx-ports wasm` → 0 dépôt). La recette est donc le chaînon manquant entre le
portage QNX de WAMR et l'outillage de build officiel de QNX.

**Testée avant d'être ouverte, pas après.** Le framework `mk` du SDP 8.0
(`$QNX_TARGET/usr/include/mk/recurse.mk`, présent dans l'installation) permet d'exécuter la
recette localement :

```
JLEVEL=$(nproc) QNX_PROJECT_ROOT="<WAMR avec le support QNX>" make -C build-files/ports/wamr install
```

→ `nto x86_64` : `libiwasm.a` 1 348 796 o, `iwasm` 715 024 o ; `nto aarch64le` : 1 371 320 o et
744 464 o. Les deux cibles **construisent et s'installent sans erreur**. Une recette non
exécutée n'aurait pas été proposée : elle aurait été une supposition.

**Le piège qui a coûté une heure, et qui est écrit pour le suivant.** La correspondance de cible
doit se faire sur **`CPUVARDIR`**, pas sur `CPU` : sur cet arbre `CPU` vaut `aarch64` et
`CPUVARDIR` vaut `aarch64le` — le même décalage que les binutils (`ntoaarch64-ar`) et la
variante qcc (`-Vgcc_ntoaarch64le`). Se fier à `CPU` produit un `WAMR_BUILD_TARGET` vide et cmake
s'arrête sur « Build target isn't set », sans message parlant de la cible.

**Dépendance documentée dans la PR.** L'arbre WAMR amont n'a pas de couche plateforme QNX : la
recette compile donc la branche de la PR #5200, et la PR #337 le dit, propose de basculer sur le
tag amont dès la fusion, et pose la question aux relecteurs. Sur les deux ports déjà présents dans
cet arbre, la CI d'une PR externe s'affiche `action_required` : c'est l'état normal, le
mainteneur doit approuver l'exécution sur son runner dédié.

## 2026-11-12 — Éclat tourne sur QNX 8.0.0, en s'appuyant sur le portage amont #5200

**Décision.** Plutôt que de maintenir nos deux correctifs, Éclat se construit sur le portage
QNX de la PR amont #5200 (dépôt `altinayselman/wamr-qnx`, Apache-2.0, © 2026 Selman Altınay) :
leurs patches, leur `cmake/qnx-toolchain.cmake`, leurs `scripts/fetch-upstream.sh` et
`scripts/build-qnx.sh`, et leur recette d'image QEMU. Attribution complète dans `NOTICE`.

**Pourquoi.** Leur portage est supérieur au nôtre sur les deux écarts que nous avions trouvés
(`MAP_BELOW` plutôt que renoncer à `MAP_32BIT` ; un vrai `product-mini/platforms/qnx/` plutôt
qu'un spécial-cas dans le CMakeLists Linux), il en traite trois que nous n'avions pas rencontrés,
et surtout **il a été exécuté sur cible**. Le réutiliser fait gagner des jours ; le dupliquer
n'aurait servi personne.

**Ce que ça a donné, mesuré sur `QNX qnxwamr 8.0.0 2026/06/05 x86pc x86_64`** (QEMU/KVM, image
construite par `mkqnximage --type=qemu`) — console brute archivée dans `docs/preuves-qnx/` :

| Exigence | Résultat sur cible |
|---|---|
| ECL-04 | module honnête : 4 appels au courtier, **0 refus**, code 0 |
| ECL-05 | module hostile : **4 refus** (`CAP_DENIED_POLICY` net_send, `CAP_DENIED_RANGE` actionneur 99999 hors ±1000, `CAP_DENIED_RANGE` capteur 42, `CAP_DENIED_POLICY` kv_put), journalisés en JSON |
| ECL-06 | module gourmand en boucle sans fin **tué à 1,30 s** pour un budget de 300 ms |
| ECL-07 | module falsifié (1 octet) **refusé**, empreinte citée, code 1 |

Le mécanisme de mise à mort par processus enfant (`fork` + `pipe` + `poll` + `SIGKILL`) fonctionne
sur QNX sans adaptation : c'était le choix fait après l'échec du thread chien de garde, et la cible
le valide.

**Ce que nous ajoutons, et qui n'existe ni dans #5200 ni ailleurs.** Le dépôt amont expose aux
modules un passage de messages QNX avec une **liste blanche de noms** côté hôte. Éclat apporte un
**moteur de politique déclaratif** : TOML à refus par défaut, plages d'identifiants et de valeurs
par capacité, budget CPU et quota d'appels, vérification d'empreinte avant chargement, journal à
hachage enchaîné — le tout validé par un jumeau Python indépendant (10 000 décisions comparées),
22 mutants tués et du fuzz. C'est cette couche qui est notre contribution.

**Pièges rencontrés.** `/root` n'existe pas sur QNX (dépôt dans `/tmp`) ; `ssh` sépare stdout et
stderr en deux canaux, d'où un entrelacement illisible dans la première capture — corrigé en
fusionnant `2>&1` **dans la commande distante**, pas côté client ; `/dev/kvm` est accessible à
l'utilisateur par **ACL** (`getfacl` montre `user:baptiste:rw-`) alors que `id` ne liste pas le
groupe `kvm` — un lancement sans `-enable-kvm` démarre quand même, en émulation logicielle, et
ne produit aucune sortie série en 20 s.

## 2026-11-12 — Pas de demande de fusion amont : le portage QNX existe déjà (PR #5200)

**Décision.** Aucune pull request ne sera ouverte sur `bytecodealliance/wasm-micro-runtime` pour
le portage QNX. Les deux correctifs écrits et vérifiés ce jour restent dans
`upstream/wamr/patches/` comme trace de la vérification indépendante, et la lettre
d'accompagnement est marquée **périmée, ne pas soumettre**.

**Pourquoi.** Recherche de doublons avant publication : la
[PR #5200 « Add QNX 8.0 platform support »](https://github.com/bytecodealliance/wasm-micro-runtime/pull/5200)
est ouverte depuis le **2026-11-04** par `altinayselman`, état `OPEN` / `MERGEABLE`, et le
mainteneur `woodsmc` l'a accueillie le 2026-11-10. Elle ajoute `core/shared/platform/qnx/` et
`product-mini/platforms/qnx/` (9 fichiers, +493/−2) et couvre les deux écarts que nous avions
trouvés — avec de **meilleures** solutions :

- `MAP_32BIT` : nous y renoncions (`#if defined(MAP_32BIT)`) ; eux exploitent `MAP_BELOW` de QNX,
  dont le hint est une borne haute, et **conservent** l'allocation sous les 2 Gio pour le code AOT.
- `-ldl` / `-lpthread` : nous mettions un spécial-cas QNX dans le CMakeLists de
  `platforms/linux` ; eux créent un vrai `product-mini/platforms/qnx/` avec
  `link_libraries(socket m)`.

Ils traitent en plus trois écarts que nous n'avions pas rencontrés (pile de thread via `__tls()`
faute de `pthread_getattr_np()`, `mprotect` de longueur nulle renvoyant `EINVAL`, `%p` sans
préfixe `0x`), et surtout **ils ont exécuté sur QNX 8.0** sous QEMU : 133 des 137 tests unitaires
amont passent. Nous n'avons rien exécuté sur cible.

**Ce que ça change pour Éclat.** La catégorie *Open-Source Contribution & Ports* demande l'URL
d'une contribution. Le portage n'est plus la nôtre. Trois pistes réellement libres :

1. **`qnx-ports/build-files`** : aucune entrée `wamr` (vérifié). Notre recette et notre
   `qnx.nto.toolchain.cmake` sont écrits et notre chaîne `tools/build-qnx.sh` prouve qu'ils
   fonctionnent — c'est une contribution à un **autre** dépôt amont, non prise.
2. **Le harnais de tests amont sur QNX** : l'auteur de #5200 écrit que les adaptations nécessaires
   (`/proc/self/exefile` au lieu de `/proc/self/exe`, ne pas passer `NULL` à `%s`) « are not part
   of this PR » et « **can be sent separately if wanted** ». Offerte, petite, non prise.
3. **Le courtier de capacités** : l'apport réel d'Éclat, indépendant du portage, que personne ne
   propose. C'est désormais lui, et pas le portage, qui porte l'argument du projet.

**Leçon.** Deux minutes de recherche de doublons ont évité de proposer publiquement, à un projet
de 6 200 étoiles, une solution inférieure à celle déjà en revue. La vérification des doublons
devient une étape obligatoire avant toute contribution amont — elle est désormais écrite ici.

**Piège d'outillage.** L'API `search/issues` de GitHub renvoie `422 Validation Failed` sur ce
dépôt (« cannot be searched… ») alors qu'il est public et que `has_issues` vaut true. Contournement
qui fonctionne : `gh pr list --state all --limit 100 --json number,title` puis filtrage local.
Ne pas conclure « aucun doublon » sur la seule foi d'une recherche en erreur.

## 2026-11-12 — QNX n'a ni `libpthread` ni `libdl` : deuxième correctif amont

**Décision.** `patches/0002-product-mini-linux-no-dl-pthread-on-qnx.patch` retire `-ldl` et
`-lpthread` de l'édition de liens quand `CMAKE_SYSTEM_NAME` vaut `QNX`, et conserve `-lm`.

**Pourquoi.** La bibliothèque WAMR entière compilait pour QNX (40 objets, 0 erreur) mais le
binaire ne se liait pas :

```
x86_64-pc-nto-qnx8.0.0-ld: cannot find -ldl
x86_64-pc-nto-qnx8.0.0-ld: cannot find -lpthread
```

Vérifié dans le SDP : `find $QNX_TARGET -name 'libpthread*' -o -name 'libdl*'` → rien. Sur QNX,
pthreads et `dlopen()` sont dans la libc. `libm` existe (`libm.so.3`), d'où le `-lm` conservé —
confirmé par les dépendances du binaire final : `libm.so.3`, `libc.so.6`, `libgcc_s.so.1`, trois
seulement.

**Conséquence outillage.** `cmake` 3.30.5 (hôte) n'a pas de module `Platform/QNX` : il faut
cmake ≥ 3.31. Plutôt que toucher au système, un cmake 4.4.4 est installé dans un venv Python
isolé (`.deps/cmake-venv`, gitignoré), avec `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` pour que cmake 4
accepte les `cmake_minimum_required` anciens de WAMR.

**Piège C.** `-D_QNX_SOURCE` passé globalement **et** `#define _QNX_SOURCE` dans le fichier
produit une redéfinition, donc une erreur avec `-Werror`. Le define du fichier est désormais
gardé par `#ifndef`.

## 2026-11-12 — E2 tranché : OUI, avec un correctif amont d'une ligne

**Décision.** Le projet continue. L'essai bloquant E2 du cahier des charges (« WAMR compile-t-il
pour QNX ? ») est **positif** : les 8 fichiers de la couche plateforme que le cmake amont retient
pour notre configuration compilent avec `qcc` 12.2.0 sur **les deux cibles** (`nto x86_64` et
`nto aarch64le`), après application d'un unique correctif d'une ligne.

**Mesure.** `tools/e2-probe.sh` : 15/16 sans correctif, **16/16 avec**
`upstream/wamr/patches/0001-platform-posix-gate-MAP_32BIT-on-feature-test.patch`
(`git apply --check` vérifié sur WAMR-2.4.5). Seul écart réel observé : `MAP_32BIT`, drapeau
`mmap` propre à Linux, que l'amont n'exclut que pour `__APPLE__`. Le correctif remplace la liste
noire d'OS par un test de présence de la fonctionnalité — strictement meilleur pour toutes les
cibles, pas seulement QNX.

**Erreur de méthode corrigée au passage.** La première version de la sonde compilait 11 fichiers
et annonçait « 13/22 ». Trois de ces fichiers (`posix_file.c`, `posix_clock.c`, `posix_socket.c`)
sont **exclus du build par le cmake amont** dès que `LIBC_WASI=0`, et `common/math/math.c` n'est
référencé par aucun cmake de la plateforme. Le verdict portait donc en partie sur du code qui ne
sera jamais compilé. La sonde lit désormais `platform_api_posix.cmake` pour retenir exactement le
bon jeu de fichiers — et y ajoute `common/memory/mremap.c`, que l'amont **ajoute** quand
`WASM_HAVE_MREMAP=0`. Un verdict de portage doit porter sur le build réel, pas sur une liste
plausible.

**Portée, sans illusion.** Objets compilés seulement : rien n'est lié, le cœur de l'interpréteur
n'est pas encore compilé, aucun `.wasm` n'a tourné sur QNX, `platform/qnx/` n'existe pas encore.
E2 répondait à la question du risque, pas à celle du travail restant.

## 2026-11-12 — SDP QNX 8.0 installé, sans interface graphique

**Décision.** Installation pilotée en ligne de commande : `qnxsoftwarecenter_clt` (fourni par QNX
Software Center) et non l'application Eclipse graphique.

**Pourquoi.** QNX Software Center est une application Eclipse RCP : la piloter à l'écran coûte
cher et casse facilement. Son CLI expose tout ce qu'il faut (`-listLicenseKeys`, `-listInstalled`,
`-installPackage`, `-destination`, `-offline`, `-importAndInstall`). L'installeur `.run` est une
archive Makeself dont `start.sh` accepte `--target`, `--agree` et `--disable-auto-start` :
installation sans racine, sans GUI, dans `~/qnx`.

**Piège consigné.** Lancer le binaire `qnxsoftwarecenter` sans `timeout` ouvre l'application
graphique et **bloque indéfiniment** : la commande a dû être tuée après 1 800 s. Ne jamais lancer
un binaire GUI sans `timeout`.

**Intégrité vérifiée.** L'installeur téléchargé (104 701 288 octets) a une empreinte MD5
`bca161a97ab3e14189cf8e7c8227fb7d` **identique à celle publiée** par le Download Center — même
démarche que pour le tarball wasi-sdk : rien n'est exécuté sans vérification préalable.

## 2026-11-12 — WAMR sur Linux : le runtime existe, et le budget CPU se règle à deux niveaux

**Décision.** Les lots L4 (couche `platform/posix` + intégration WAMR) et L5 (trois modules de
démonstration compilés en `.wasm`) sont implémentés et testés sur Linux. Le budget CPU est
appliqué à **deux niveaux** : refus synchrone dans le runtime à chaque import, et mise à mort du
**processus** par le chargeur quand une boucle de calcul n'appelle aucun import.

**Pourquoi deux niveaux.** La première version voulait un thread chien de garde appelant
`wasm_runtime_terminate()`. Lecture du code amont : cette fonction se contente de poser une
exception (`wasm_set_exception(module_inst, "terminated by user")`), et sans
`WASM_ENABLE_THREAD_MGR` l'interpréteur ne la consulte pas pendant une boucle — le module
gourmand tournait donc indéfiniment (constaté : test d'intégration bloqué au-delà de 120 s).
Plutôt que d'activer le gestionnaire de threads amont (hors périmètre, cf. cahier §3.2), le
chargeur exécute le module dans un processus enfant et le tue à l'échéance. C'est aussi
**l'architecture visée sur QNX** : le courtier refuse de façon synchrone, le gestionnaire de
processus met à mort. La couche `runtime/` ne crée donc plus aucun thread, ce qui la rend
portable telle quelle.

**Conséquence mesurée.** Module gourmand avec un budget de 200 ms : tué en 1 205 ms (budget +
marge de 1 000 ms). La marge est un choix du chargeur, pas une propriété du noyau ; sur QNX elle
disparaîtra au profit d'un TimerPulse.

## 2026-11-12 — Le toolchain wasi-sdk casse si un `wasm-opt` pourri traîne dans PATH

**Décision.** Le Makefile compile les modules `.wasm` avec un `PATH` imposé
(`/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`), variable `WASM_PATH`.

**Pourquoi.** Le `clang` de wasi-sdk invoque un `wasm-opt` trouvé dans `PATH`. Celui de
`~/.local/bin` est un script Node ESM cassé sous Node 26 (`ReferenceError: require is not defined
in ES module scope`), et l'édition de liens échoue avec un simple « linker command failed », noyé
sous 25 000 lignes de code JavaScript déversé sur stderr. Diagnostic long, cause invisible :
c'est consigné ici pour ne pas le repayer.

**Impact.** `Makefile` (`WASM_PATH`), `VERSIONS` (section `piege_environnement`).

## 2026-11-12 — SHA-256 écrit dans `core/`, et décodage hexadécimal unique

**Décision.** SHA-256 (FIPS 180-4) est implémenté dans `core/src/sha256.c`, en C11 pur, et
`core/src/policy.c` réutilise `eclat_sha256_from_hex` au lieu de son propre décodage.

**Pourquoi.** ECL-07 exige la vérification de l'empreinte avant chargement, donc un SHA-256 sans
dépendance (ni OpenSSL, ni `qcrypto` qui n'existe que sur cible). Le hachage est un calcul pur :
il a sa place dans `core/`. Supprimer le décodage hexadécimal dupliqué de `policy.c` applique la
règle « une seule implémentation par logique » — et le mutant `pol-longueur-sha256-affaiblie` a dû
être remplacé par `pol-sha256-jamais-verifie`, l'ancien fragment n'existant plus.

**Vérification.** Vecteurs NIST (message vide, « abc », 448 bits, un million de « a ») recoupés
avec `sha256sum` ; hachage incrémental comparé au one-shot sur 20 longueurs touchant toutes les
frontières du bourrage (0, 1, 55, 56, 57, 63, 64, 65, 119…1000) ; comparaison d'empreintes en
temps constant. 37 vérifications.

## 2026-11-12 — L'URL amont du cahier des charges était fausse

**Décision.** Toutes les références à `github.com/bytecodealliance/wamr` sont remplacées par
`github.com/bytecodealliance/wasm-micro-runtime`.

**Pourquoi.** Vérification du 2026-11-12 : `GET /repos/bytecodealliance/wamr` renvoie **404** et
`git ls-remote https://github.com/bytecodealliance/wamr.git` répond « Repository not found ». Le
dépôt réel est `bytecodealliance/wasm-micro-runtime` (6 202 ★, description « WebAssembly Micro
Runtime (WAMR) », branche par défaut `main`, dernière release `WAMR-2.4.5` du 2026-06-29).
Soumettre une PR vers une URL morte aurait discrédité le dossier : la catégorie exige l'URL amont.

**Impact.** `README.md`, `CONTRIBUTING.md`, `docs/cahier-des-charges.md` (2 occurrences),
`upstream/`, `VERSIONS`. Leçons consignées dans `docs/E5-preuves-absence.md`.

## 2026-11-12 — E5 partiellement tranché, et E2 reformulé

**Décision.** L'essai E5 (existe-t-il déjà un port WebAssembly pour QNX ?) est considéré comme
**établi pour tout l'écosystème public open source QNX**, et toujours ouvert pour `oss.qnx.com`
(application JavaScript non interrogeable). L'essai E2 est reformulé : il ne s'agit plus de savoir
si « une couche POSIX générique » existe — elle n'existe pas — mais de dériver la couche QNX de
`core/shared/platform/linux` en réutilisant `common` et le contrat de `include`.

**Pourquoi.** Vérifications reproductibles du 2026-11-12 : aucune entrée Wasm dans l'index
`qnx-ports/build-files` (~150 ports lus), `qnx-ports/wamr` en 404, recherche
`org:qnx-ports wasm` à **0** dépôt, et côté amont 15 couches système dans
`core/shared/platform/` **sans `qnx`** — mais 17 cibles dans `product-mini/platforms/` **dont
`posix`**. Les ports `nuttx`, `zephyr`, `rt-thread`, `riot` et `vxworks` constituent le précédent
à suivre pour une cible embarquée.

**Impact.** `docs/E5-preuves-absence.md` (nouveau), `upstream/wamr/PORTING-NOTES.md`,
`upstream/qnx-ports/`.

## 2026-11-11 — Format de politique : TOML standard `[module]` + `name`

**Décision.** La section module s'écrit `[module]` avec une clé `name = "…"`, et non
`[module "nom"]`.

**Pourquoi.** `[module "nom"]` n'est pas du TOML valide (deux clés séparées par un espace, sans
point). Le jumeau Python (lot L3) lit la politique avec `tomllib` de la bibliothèque standard ;
il faut donc un document TOML standard pour que les deux implémentations consomment exactement
les mêmes fichiers. Le parseur C en accepte le même sous-ensemble.

**Impact.** `CONTRACT.md` §2, `core/src/policy.c` (en-tête de section + clé `name`),
`core/tests/test_policy.c`.

## 2026-11-11 — Contraintes d'id et de valeur : drapeaux explicites, pas de sentinelle `-1`

**Décision.** `eclat_allow_rule_t` porte deux drapeaux `has_ids` et `has_value_range`. Une
contrainte est appliquée si et seulement si le drapeau correspondant vaut 1.

**Pourquoi.** La première version utilisait `id_min == -1` / `val_min == -1` comme sentinelle
« absence de contrainte ». Or `-1` est aussi une valeur d'id légitime (`ids = -1` ou
`ids = [-1, 5]`). Avec la sentinelle, `ids = [-1, 5]` était interprété comme « aucune contrainte
d'id » au lieu de la plage `[-1, 5]`. C'est exactement le désaccord qu'a révélé le test
différentiel C ↔ Python (ECL-03) : sur `ids = -1` avec une requête `id = 10`, le C répondait
`CAP_OK` (sentinelle) et le jumeau Python `CAP_DENIED_RANGE` (contrainte réelle). Les drapeaux
lèvent l'ambiguïté ; le jumeau Python, qui distinguait déjà `None` d'un tuple, était correct.

**Impact.** `core/include/eclat/cap_policy.h`, `core/src/policy.c` (parse + validate),
`core/src/decision.c` (étapes 5 et 6), `CONTRACT.md` §3, tests C.