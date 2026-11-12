<!-- SPDX-License-Identifier: Apache-2.0 -->
# Dossier de sécurité mappé — Éclat

> **À lire avant tout.** Ce document est un **argumentaire de correspondance**, pas une preuve de
> conformité. Il relie chaque couche d'isolation et chaque exigence `ECL-nn` aux exigences des
> normes **ISO 26262** (parties 6 et 9) et **IEC 62304** auxquelles elles se rattachent. Éclat
> **ne prétend à
> aucune certification ni qualification** — voir §5, qui le dit noir sur blanc.

**Ce que ce document n'est pas :** un rapport d'audit, un dossier de sécurité automobile
(« safety case »), une déclaration de conformité, ni une classification ASIL.

---

## 1. Objet et méthode

**Objet.** Rendre lisible, pour un lecteur d'équipementier ou de fabricant de dispositif médical,
ce qu'Éclat apporte exactement en matière de sûreté, et à quelles exigences de normes cet
apport se rattache.

**Méthode.** Trois colonnes : (a) ce qu'Éclat met en œuvre, (b) l'exigence normative de référence
**citée par son nom** (jamais par un numéro inventé), (c) ce qu'il reste à faire. Les numéros de
paragraphe de norme ne sont pas employés : quand la référence est incertaine, le **nom** de
l'exigence est écrit, ce qui est la forme la plus honnête et la plus utile.

**Portée des normes citées.**

- **ISO 26262**, parties **6** (développement du composant logiciel) et **9** (analyses
  orientées ASIL et sécurité : **liberté d'interférence**, analyses de défaillances
  dépendantes, évaluation des mécanismes de sécurité). Le projet ne s'attribue **aucun ASIL** : il
  est écrit pour pouvoir **héberger** un élément dont l'ASIL est fixé par l'intégrateur, sans
  l'entraîner avec lui.
- **IEC 62304**, logiciel de dispositif médical : **classification de sécurité logicielle**
  (classes A, B, C), **gestion des risques logiciels**, **SOUP** (logiciel de provenance inconnue),
  gestion de configuration, résolution de problèmes.

---

## 2. Les trois couches d'isolation

| Couche | Ce qu'elle contient | Menace qu'elle adresse | Propriété de sûreté |
|---|---|---|---|
| **1 — Bac à sable WebAssembly** | Mémoire linéaire bornée, pas d'appel système direct, imports déclarés, tout import inconnu refusé à l'instantiation | Exécution native involontaire, corruption mémoire hors du bac à sable, accès direct au système | Le module ne peut pas *agir* sur le système : il ne peut que *demander* |
| **2 — Processus QNX sans abilities** | Processus du runtime sans ability, sous la supervision de HAM | Élévation de privilèges après exploitation d'un défaut du runtime | Un défaut du runtime ne devient pas une capacité privilégiée ; le noyau refuse l'opération |
| **3 — Politique de sécurité du noyau** | Politique de sécurité QNX : le seul processus autorisé à toucher les ressources est le courtier | Contournement du courtier, accès direct aux ressources système | Les droits effectifs sont appliqués **par le noyau**, pas par le code du projet |

**Le courtier est le quatrième pilier, à ne pas confondre avec les trois couches** : c'est là que la
politique déclarative est évaluée (refus par défaut), que le budget à fenêtre glissante est
compté, que la décision est journalisée. Les trois couches sont *structurelles* ; le courtier est
*la règle du jeu*, et il s'appuie sur elles.

> Le détail des cinq mécanismes QNX qui renforcent ces couches (abilities, Pathtrust, HAM,
> `ionotify`, `qcrypto`) est dans [`docs/qnx-five-technologies.md`](qnx-five-technologies.md) ;
> il n'est pas répété ici.

---

## 3. Correspondance couche ↔ exigences

| Couche / exigence Éclat | ISO 26262 (partie 6 / 9) — exigence citée par son nom | IEC 62304 — exigence citée par son nom | Reste à faire |
|---|---|---|---|
| Couche 1 — bac à sable Wasm | P6 : conception de l'architecture logicielle ; vérification de la conception ; test des unités logicielles ; traçabilité exigence ↔ logiciel ↔ test | Classification de sécurité logicielle (classes A/B/C) ; processus de développement logiciel ; SOUP : évaluation et suivi d'anomalies | Classification A/B/C **à fixer par l'intégrateur** selon l'usage ; dossier SOUP à compléter au-delà du `.wasm` |
| Couche 2 — processus sans abilities | P9 : **analyse de la liberté d'interférence** entre éléments de criticité différente ; analyse des défaillances dépendantes ; évaluation des mécanismes de sécurité | Gestion des risques logiciels : mesures de maîtrise du risque ; processus de résolution de problèmes | Implémentation QNX et campagne de mesures sous cible |
| Couche 3 — politique de sécurité noyau | P6 : architecture logicielle (frontière de l'élément) ; P9 : mécanismes de sécurité (le noyau applique la politique, il ne la suit pas seulement) | Gestion des risques logiciels : mesures de maîtrise du risque au niveau système | Rédiger la politique QNX (`secpol`) et la rejouer par un test de refus |
| Courtier — politique (refus par défaut) | P6 : traçabilité exigence ↔ règle de décision ↔ test ; P9 : mécanismes de sécurité (refus par défaut = comportement sûr en cas de défaillance) | Gestion des risques logiciels ; politique de configuration (SOUP + configuration) | Valider l'ASIL cible avec l'intégrateur |
| Courtier — budget à fenêtre glissante | P9 : mécanismes de sécurité (arrêt sûr d'un élément hors enveloppe) ; P6 : test des unités logicielles (fenêtre, compteurs, drapeaux) | Gestion des risques logiciels : mesure de maîtrise du risque « temps » | Banc de latence sous QNX (chiffres **à mesurer**) |
| Vérification d'empreinte avant chargement | P6 : vérification du logiciel livré (intégrité du livrable) ; traçabilité | SOUP : identification et contrôle de la provenance ; gestion de configuration | Signature de manifeste (ECL-21, partiellement) |
| Journal des décisions | P6 : traçabilité et traçabilité des vérifications ; P9 : traçabilité des analyses (trace exploitable en audit) | Gestion de configuration (journalisation des modifications) ; traçabilité | Chaîne de hachage signée (`qcrypto`) et vérificateur indépendant |
| Preuve différentielle C ↔ Python | P6 : stratégie de vérification indépendante ; test des unités logicielles | Processus de développement logiciel : stratégie de test | Élargir à la couche hôte |
| Fuzz / mutants | P6 : test des unités logicielles (tests négatifs, analyse de couverture) ; P9 : analyse des défaillances potentielles du logiciel | Résolution de problèmes ; développement logiciel | Campagne ASan/UBSan complète sous QNX |

---

## 4. Les 22 exigences ECL, une par une

| ID | Ce qu'elle exige (rappel) | Rattachement norme | Statut |
|---|---|---|---|
| ECL-01 | Moteur de décision C11 strict, testé, sans QNX ni WAMR | P6 : stratégie de vérification ; IEC : développement logiciel | **Fait** (161 vérifications) |
| ECL-02 | Politique analysée, validée, **refus par défaut** | P9 : mécanismes de sécurité ; IEC : gestion des risques | **Fait** |
| ECL-03 | Jumeau Python + test différentiel avec réduction dichotomique | P6 : vérification indépendante ; IEC : stratégie de test | **Fait** (10 000 décisions concordantes) |
| ECL-04 | Module `.wasm` exécuté sous la couche POSIX, imports appelés | P6 : test des unités logicielles | En cours |
| ECL-05 | Module hostile : `CAP_DENIED_POLICY` + journal | P9 : mécanismes de sécurité ; IEC : maîtrise du risque | **Fait** (dans `core/`) |
| ECL-06 | Budget CPU respecté, module tué, dépassement journalisé | P9 : mécanismes de sécurité (arrêt sûr) | **Fait** (dans `core/`) |
| ECL-07 | Empreinte vérifiée **avant** chargement | P6 : vérification du logiciel livré ; IEC : SOUP | À faire (QNX) |
| ECL-08 | Aucune copie hors bornes, fuzz sous ASan | P6 : test des unités logicielles | En cours |
| ECL-09 | CI ≤ 5 min, gardes de taille et de licence | *Hors périmètre sûreté* (qualité projet) | En place |
| ECL-10 | WAMR compile pour QNX 8.0 | P6 : environnement de développement / configuration | **Pas fait** (SDP absent) |
| ECL-11 | Le courtier tourne comme gestionnaire de ressources | P6 : architecture logicielle ; P9 : liberté d'interférence | **Pas fait** |
| ECL-12 | Chargement à chaud sans échéance manquée (1 kHz) | P6 : performance / vérification ; IEC : maîtrise du risque temps | **À mesurer** |
| ECL-13 | Boucle critique stable pendant un module hostile | P9 : liberté d'interférence ; IEC : maîtrise du risque | **À mesurer** |
| ECL-14 | Politique de sécurité QNX interdisant tout accès hors courtier | P9 : mécanismes de sécurité appliqués par le noyau | **Pas fait** |
| ECL-15 | Fuzz du parseur et du runtime (ASan/UBSan) | P6 : tests négatifs ; IEC : résolution de problèmes | En cours |
| ECL-16 | Demande de fusion amont ouverte | *Hors périmètre sûreté* (processus open source) | À faire |
| ECL-17 | `PORTING.md` : chaque écart POSIX documenté | P6 : traçabilité / maîtrise des écarts | À faire |
| ECL-18 | Mode AOT, gain et pire temps d'exécution | P6 : pire temps d'exécution (vérification) | À faire |
| ECL-19 | Isolation multi-modules, budgets indépendants | P9 : liberté d'interférence entre instances | À faire |
| ECL-20 | Migration assistée d'un plugin Lua | P6 : stratégie de migration / régression | À faire |
| ECL-21 | Le courtier signe chaque décision critique | P6 : traçabilité ; IEC : journalisation / audit | À faire (`qcrypto`) |
| ECL-22 | Contribution amont fusionnée | *Hors périmètre sûreté* | À faire |

---

## 5. Ce que ce document n'est PAS

Écrivons-le explicitement, parce que c'est le point le plus important du dossier :

1. **Éclat n'est pas certifié.** Aucune certification ISO 26262, IEC 62304, IEC 61508 ou autre
   n'est demandée, obtenue, ou revendiquée. Ce document ne constitue ni une *qualification*, ni
   une *déclaration de conformité*, ni une *approbation de type*.
2. **Éclat ne s'attribue aucun ASIL.** Rien ici ne dit qu'Éclat est ASIL A, B, C ou D. Un module
   hébergeant un élément de criticité donnée doit être intégré et qualifié par l'intégrateur, qui
   fixe le niveau applicable.
3. **Une table de correspondance n'est pas une preuve de conformité.** ISO 26262 exige un
   *processus* (organisation, planning, gestion de configuration, gestion des problèmes, audits
   internes), des *analyses* (DFA, FMEA, FTA), une *justification* des niveaux ASIL (décomposition,
   coexistence), la *qualification des outils*, et une *évaluation indépendante*. Éclat ne fournit
   ni ce processus, ni ces analyses, ni cette évaluation.
4. **Les IEC 62304 « classes A/B/C » ne sont pas attribuées ici.** La classe dépend de l'effet
   possible d'une défaillance du logiciel **sur la sécurité du patient** : c'est une décision
   clinique et réglementaire de l'intégrateur, pas une propriété du projet.
5. **Le mot « SOUP » décrit un statut, pas un certificat.** Un module WebAssembly tiers *est*, du
   point de vue d'IEC 62304, un logiciel de provenance inconnue à qualifier (identification,
   évaluation, anomalies connues, contrôle des modifications) — Éclat fournit la **mécanique**
   d'admission (empreinte, politique, journal), pas la qualification du SOUP.
6. **Rien n'est mesuré sur cible QNX à ce jour.** Le SDP n'est pas installé ; les chiffres de
   latence ne sont pas publiés ; aucune performance n'est revendiquée.

> **Le projet assume cette limite et l'écrit.** `docs/cahier-des-charges.md` §14, « Volontairement
> non affirmé » : *« aucune affirmation de sécurité au sens certification (le projet fournit une
> isolation et une politique, pas une qualification) »*. Ce document ne fait qu'appliquer cette
> règle ; il ne l'assouplit pas.

---

## 6. Ce qu'il faudrait pour aller plus loin

Dans cet ordre, et sans rien promettre de dates :

1. Fixer la classe de sécurité (IEC 62304) et l'ASIL cible **avec l'intégrateur**, pas tout seul.
2. Écrire le dossier SOUP du module : provenance, version, anomalies connues, procédure de mise à
   jour.
3. Réaliser la campagne de mesures sous QNX (banc 1 kHz, avant / pendant / après) et publier les
   chiffres — y compris s'ils sont défavorables.
4. Documenter la politique de sécurité QNX (`secpol`) et la rejouer par un test de refus automatisé.
5. Ajouter la chaîne de journal signée (`qcrypto`) et un vérificateur indépendant (ECL-21).
6. Faire relire le dossier par un tiers indépendant — c'est la seule façon d'approcher d'une
   argumentation de sûreté recevable.

---

## 7. Références croisées

- [`docs/qnx-five-technologies.md`](qnx-five-technologies.md) — les cinq mécanismes QNX et leur
  rôle (pas de doublon ici).
- [`docs/qnx-five-technologies.svg`](qnx-five-technologies.svg) — le diagramme annoté.
- [`docs/spec-en.md`](spec-en.md) — la spécification du contrat en anglais.
- [`CONTRACT.md`](../CONTRACT.md) — contrat gelé : ABI, politique, 7 étapes de décision, budget.
- [`docs/cahier-des-charges.md`](cahier-des-charges.md) §6 (ECL-01..22) et §14 (statut des
  affirmations).
- [`README.md`](../README.md) — état vérifié du projet.
