# Éclat — où cela en est, et comment le pousser au maximum

**Date de l'état : mardi 11 novembre 2026, 23 h 20 (Europe/Paris).**
**Échéance du concours : mardi 15 décembre 2026, 20 h (Paris). Il reste 33 jours.**

Cahier des charges complet : [`ECLAT-cahier-des-charges.md`](ECLAT-cahier-des-charges.md)
(326 lignes, 22 exigences ECL-01 à ECL-22, 5 essais de faisabilité, 11 lots délégables).
Ce document ne le remplace pas : il dit **où en est le projet** et **comment le pousser au maximum**.

---

## 0. Avertissement : ce projet est le plus risqué des trois

Une analyse de portefeuille produite ce soir recommande **d'abandonner Éclat**, au motif que
« porter WAMR plus un courtier de capacités en 5 semaines sans SDP installé est le pire ratio
risque/retour du portefeuille, **sauf si le harnais QEMU est vert avant J-30** ».

**J-30, c'est le 15 novembre. Dans 4 jours.**

Le harnais QEMU n'existe pas encore (lot `qnx-kit/harnais/` lancé à 22 h 55, aucun fichier
produit à 23 h 20). La condition n'est donc **pas remplie aujourd'hui**. Deux issues honnêtes :

- **Décider maintenant de reporter Éclat** au-delà du 15 décembre (horizon 3 mois, celui que tu
  as demandé plus tôt) et concentrer les 33 jours sur Tamis + Glane + les trois projets existants.
- **Ou maintenir Éclat** en acceptant que le SDP soit installé **avant le 14 novembre** et que
  l'essai E2 (WAMR compile-t-il pour QNX via une couche POSIX réutilisable ?) soit tranché avant
  le 17. Passé ces dates, le risque d'une soumission bâclée dépasse le bénéfice.

Ce document décrit la seconde option. La première est parfaitement défendable.

---

## 1. État des lieux, sans complaisance

| Élément | État |
|---|---|
| Cahier des charges | **Écrit et publié** (dépôt public `BaptisteFaisy/qnx-dossier`) |
| Lignes de code produit | **0** |
| SDP QNX 8.0 installé | **Non** |
| WAMR cloné, version épinglée | **Non** |
| Essai E2 tranché (couche plateforme POSIX réutilisable ?) | **Non — risque bloquant** |
| Essai E5 tranché (aucun port Wasm ailleurs dans l'écosystème QNX ?) | **Partiellement** |
| Vérification d'absence faite | **Oui, par moi** : aucun `wasm`/`wamr`/`wasi`/`wasmtime`/`wasmer` dans l'index officiel `qnx-ports/build-files/ports` (~150 entrées lues), et `github.com/qnx-ports/wamr` renvoie **404** |

**Réserve à lever (E5)** : le tableau de bord `oss.qnx.com` annonce ~2 500 ports et n'est pas
interrogeable sans JavaScript. L'absence est **certaine dans `qnx-ports`**, pas absolument
certaine ailleurs. Un navigateur réel, le Discord QNX ou r/qnx tranchent en une heure.

**Ce qui est certain** : la chaîne de portage officielle existe (`qnx-ports/build-files`, un
dossier par port avec son `README.md`, environnement de build Docker montant `~/.qnx` et
`~/qnx800`) ; QNX se déclare conforme POSIX et annonce que « beaucoup de projets open source
peuvent être compilés pour QNX avec peu ou pas de modification » ; `qcrypto` fournit les
primitives cryptographiques nécessaires à la signature de modules.

---

## 2. Rappel du contrat (extrait du cahier)

Six imports, signature figée : `cap_time_ns`, `cap_log`, `cap_read_sensor`, `cap_write_actuator`,
`cap_kv_get`/`cap_kv_put`, `cap_net_send`. Règles : aucune chaîne terminée par zéro (pointeur +
longueur systématiquement), toute longueur vérifiée contre le tampon déclaré **avant** copie,
tout import inconnu refusé **à l'instantiation** et non à l'exécution.

Politique TOML : empreinte SHA-256 obligatoire, budget CPU par seconde glissante, plafond
mémoire, quota d'appels, plages de valeurs, `allow`/`deny` — **et le défaut est de tout refuser**.

Le point d'architecture décisif : **deux couches plateforme** (`platform/posix/` et
`platform/qnx/`) partageant le même contrat. Toute la logique se teste donc sur Linux **avant**
d'avoir le SDP. C'est ce qui rend le projet livrable même si la licence tarde.

Répartition des 22 exigences : **9 au niveau S**, **8 au niveau C**, **5 au niveau X**.

---

## 3. Comment le pousser au maximum — 25 leviers triés

Les 25 idées viennent de `qnx-100-aller-plus-loin.md` (lot 3) : **12 à effort faible**, 12 moyen,
1 élevé. C'est le lot le plus « rentable » des trois : beaucoup de petits ajouts à fort rendement.

### Palier 1 — effort faible, à faire même si le projet est reporté

| Idée | Ce que ça ajoute | Pourquoi c'est décisif |
|---|---|---|
| **Recette `qnx-ports` prête** | Makefile conforme à `qnx-ports/build-files` + cible Docker | L'équipe des ports peut consommer le travail **sans adaptation**. C'est ce qui fait accepter une contribution |
| **Correctif libc amont minimal** | Un second PR isolant le plus petit delta `platform/qnx` imaginable | La catégorie Ports exige l'URL d'une pull request ; un petit PR a une vraie chance d'être fusionné en quelques jours |
| **Scénario vidéo coupe-circuit** | 3 minutes : module hostile chargé en direct pendant que la boucle pilote un actionneur, tué, graphique de gigue plat | La démonstration tient en un plan |
| **Diapositive « cinq technologies QNX »** | abilities, Pathtrust, HAM, `ionotify` et `qcrypto` dans une seule architecture annotée | **Le bonus « QNX-specific Technologies » démontré d'un coup d'œil** |
| **Kill-switch `ionotify`** | La révocation d'une empreinte est diffusée à tous les courtiers : mise à mort immédiate, liste noire synchronisée | Petit, et très parlant en démo |
| **Journal d'audit à hachage enchaîné** | Chaque décision signe la précédente via `qcrypto` : export infalsifiable pour audit ISO 21434 ou FDA | Argument de marché, coût faible |
| **Manifeste de module signé** | CLI générant un manifeste (auteur, empreinte, capacités, pire temps d'exécution) signé et vérifié à l'admission | Chaîne d'approvisionnement complète |
| **Dossier de sécurité mappé** | Document reliant chaque couche d'isolation aux exigences ISO 26262 / IEC 62304 | C'est l'argument que lit un équipementier avant d'oser |
| **Pont vers des ports existants** | Brancher `cap_kv` sur le port QNX de **sqlite** et `cap_net_send` sur **mosquitto** | Preuve de synergie avec l'écosystème QNX, à coût quasi nul |
| **Fiche gigue reproductible** | Un script produisant la comparaison C nu / interpréteur / AOT sur QNX et Linux | Des chiffres citables en une page — **à condition d'accepter qu'ils soient défavorables face au C** |
| **Hygiène mémoire inter-appels** | Effacement des zones rendues, pages de garde, allocation déterministe : ferme le canal auxiliaire de rémanence | Répond à l'objection « le bac à sable Wasm fuit par la mémoire » |
| **Guide de portage générique** | Transformer `PORTING.md` en « porter un langage ou un runtime sur QNX » réutilisable | Valeur communautaire, et codelab gratuit |

### Palier 2 — effort moyen, avant le gel du 5 décembre

| Idée | Ce que ça ajoute | Prix visé |
|---|---|---|
| **SDK module tiers** | `eclat new module` : gabarit, harnais de test Linux identique au courtier, documentation du développeur **sans licence QNX** | Critère « produit cohérent » — et élargit l'audience au-delà de QNX |
| **Créneaux TDMA** | Cantonne chaque module dans un créneau périodique payé sur l'horloge QNX : garantit le passage de la boucle 1 kHz | Le meilleur argument temps réel du projet |
| **Certificat de pire temps d'exécution** | Mesure instrumentée du WCRT de chaque module, consignée dans la politique comme condition de chargement | Innovation : personne ne fait ça |
| **Appels asynchrones** | `cap_call_async` / `cap_await` avec pulse de fin et budget d'appels en vol, sans jamais bloquer la boucle | Usage non évident des pulses QNX |
| **Mise à jour A/B atomique** | Bascule à chaud vers un module v2 après drainage des appels en vol, repli automatique si la politique refuse | Réutilise le savoir-faire d'**Ambre** |
| **Console `eclatctl`** | État vivant des budgets, motifs de refus et santé des modules, en une commande | Outil d'exploitation crédible |
| **Sous-ensemble WASI sur QNX** | Une dizaine d'imports `wasi_snapshot_preview1` mappés sur QNX | Comble un manque réel de WAMR amont → renforce la pull request |
| **Banc Raspberry Pi 5 physique** | Servomoteur et LED pilotés par les modules Wasm, politique modifiable depuis un portable | **Vote Community Choice** — le public vote pour ce qu'il voit bouger |
| **Chapitre « bac à sable navigateur »** | Éditeur de politique TOML avec prévisualisation des décisions côté client | Codelab accessible **sans aucune cible QNX** |

### Palier 3 — après le 15 décembre

**Jeton de capacité atténué** (un module délègue à un sous-module un jeton HMAC `qcrypto` à
droits réduits, révocable individuellement — gain maximal mais **conception crypto à ne pas
improviser**) · **Post-mortem de violation** (vidage pile, registres et derniers appels à la
mise à mort) · **Capacités natives PPS** · **Paquet installable QNX**.

---

## 4. Chemin critique daté (si le projet est maintenu)

| Date | Jalon | Condition de passage |
|---|---|---|
| **12/11** | Licence QNX demandée | Action humaine, 10 minutes |
| **13/11** | **E5 tranché** : aucun port Wasm ailleurs (oss.qnx.com avec un vrai navigateur, Discord QNX, r/qnx) | Sinon abandon immédiat |
| **14/11** | SDP installé ; WAMR cloné et version épinglée ; `wasi-sdk` épinglé | |
| **17/11** | **E2 tranché** : WAMR compile pour QNX via une couche POSIX réutilisable | **Sinon on arrête là** et on bascule le temps sur Tamis + Glane |
| **21/11** | Moteur de décision en C11 pur + jumeau Python + test différentiel ; politique et banque de cas hostiles | ECL-01 à ECL-03 |
| **25/11** | Couche `platform/posix` + WAMR sur Linux : un module s'exécute et appelle les imports | ECL-04 à ECL-08 |
| **28/11** | Couche `platform/qnx` + courtier en gestionnaire de ressources + politique de sécurité | ECL-10, ECL-11, ECL-14 |
| **02/12** | Banc de latence, chargement à chaud, fuzz, mutants, **pull request amont ouverte** | ECL-12, ECL-13, ECL-16 |
| **05/12** | **GEL FONCTIONNEL** | Plus aucune fonctionnalité |
| **12/12** | Vidéo 3 min, dépôt public, formulaire Devpost avec URL amont **et** URL de la PR | Marge de 3 jours |

---

## 5. Ce qui maximise les prix

- **Catégorie Open-Source Contribution & Ports** (4 000 $ + dauphin 2 000 $). **Vide dans ton
  portefeuille.** Règles strictes : nouveau dépôt créé pendant la période de soumission, projet
  amont référencé **par URL**, historique **non copié**, **interdiction de soumettre un fork**,
  URL amont **et** URL de la pull request à fournir.
- **Bonus « Best use of QNX-specific Technologies »** (3 500 $) : la diapositive des cinq
  technologies (abilities, Pathtrust, HAM, `ionotify`, `qcrypto`) est l'actif principal.
- **Community Choice** : le banc Raspberry Pi 5 avec servomoteur et LED.
- **Grand prix** (10 000 $) : peu probable — Relais est ton meilleur candidat sur ce plan.

**L'argument à mettre en avant** : ce n'est pas « un runtime WebAssembly de plus ». Ce sont
**trois couches d'isolation indépendantes** (le bac à sable Wasm, le processus QNX sans
abilities, la politique appliquée par le noyau) et la **preuve chiffrée** qu'une boucle de
contrôle à 1 kHz ne bouge pas pendant qu'un module hostile sature son budget et se fait tuer.
Sur un noyau monolithique, la première couche est la seule : un débordement du runtime emporte
tout le système.

---

## 6. Blocages, et qui les lève

| Blocage | Qui | Effort |
|---|---|---|
| Décision go/no-go (section 0) | **Toi** | 5 minutes, mais avant le 15/11 |
| Licence QNX | **Toi** | 10 minutes + ~1 h d'attente |
| E5 : absence de port Wasm hors `qnx-ports` | Un agent avec navigateur réel, ou toi sur le Discord QNX | 1 h |
| E2 : WAMR compile-t-il pour QNX ? | Un agent, une fois le SDP installé | 1 journée |
| Raspberry Pi 5 + servomoteur + LED pour la démo physique | **Toi** | sinon démo QEMU uniquement |

**Le risque que je ne peux pas lever** : la réactivité du relecteur amont. La pull request peut
être ouverte à temps et non fusionnée — le règlement n'exige que l'URL de la PR, pas sa fusion.

---

## 7. Premier lot à lancer demain matin

**E5 d'abord** (une heure, et il peut tuer le projet — donc avant tout code) : chercher
`wasm`, `webassembly`, `wamr`, `wasmtime`, `wasmer` sur `oss.qnx.com` avec un navigateur réel,
sur `gitlab.com/qnx/projects`, et poser la question sur le Discord QNX et r/qnx.

**Puis, si E5 passe, les lots L1 à L3 du cahier** (ils ne dépendent pas du SDP) :

1. `core/` : parseur TOML restreint + validation + banque de **30 politiques fautives** ;
   le refus par défaut doit être testé explicitement.
2. `core/` : moteur de décision, budget CPU par seconde glissante, plafond mémoire, journal.
3. `host/` : jumeau Python du moteur + test différentiel par graine sur **10 000 décisions**,
   avec réduction dichotomique du premier désaccord.

Critère de succès : `make test` vert sur Linux, ≥ 300 vérifications, ASan/UBSan clean,
garde de taille verte, et **aucune** ligne de code QNX écrite avant que E2 soit tranché.
