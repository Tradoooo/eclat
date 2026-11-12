<!-- SPDX-License-Identifier: Apache-2.0 -->
# Cinq technologies QNX dans l'architecture d'Éclat

> **Une page, une idée.** Éclat n'est pas « un runtime WebAssembly de plus ». C'est une
> superposition de **trois couches d'isolation indépendantes** : le bac à sable WebAssembly, le
> processus QNX sans abilities, et la politique de sécurité appliquée par le noyau.
> Les cinq mécanismes QNX présentés ici ne *créent* pas ces couches : ils les **renforcent** à
> trois endroits précis — en amont (provenance du code), pendant l'exécution (privilèges), et après
> la défaillance (relance, révocation).

**La même chose en image :** [`docs/qnx-five-technologies.svg`](qnx-five-technologies.svg).
**Ce document n'est pas un dossier de conformité :** voir
[`docs/security-mapping.md`](security-mapping.md).

---

## 1. Les cinq mécanismes, un par un

### 1.1 Abilities (`procmgr_ability`)

- **Ce que c'est.** Le mécanisme d'abilities du gestionnaire de processus de QNX : les opérations
  privilégiées sont gardées par des identifiants (« abilities ») ; un processus qui ne possède pas
  l'ability requise se voit **refuser l'opération par le noyau**, et non par convention.
- **Ce qu'Éclat en fait.** Le processus qui héberge le runtime (`eclat-run`) est lancé **sans
  aucune ability**. Il n'a donc aucun moyen d'ouvrir une ressource privilégiée, de court-circouter
  le courtier, ni de modifier la politique — même si son code est corrompu.
- **Ce que ça apporte.** **Couche 2 (processus QNX).** C'est la propriété qui fait que le bac à
  sable Wasm n'est pas la seule couche : un débordement du runtime (bug WAMR, module hostile) reste
  confiné au processus et ne devient pas une capacité privilégiée.

### 1.2 Pathtrust

- **Ce que c'est.** Le cadre de confiance par chemin de QNX : chaque chemin du système de fichiers
  porte un **niveau de confiance**, et le code chargé depuis ce chemin hérite de ce niveau pour les
  opérations privilégiées qu'il peut entreprendre.
- **Ce qu'Éclat en fait.** La **provenance** du module devient une entrée explicite du contrôle
  d'admission : le chemin de chargement est un intrant de confiance (Pathtrust), le **contenu** est
  un second intrant indépendant (l'empreinte SHA-256 du module). Un chemin compromis ne peut
  pas faire passer silencieusement un autre module.
- **Ce que ça apporte.** **Chaîne de provenance en amont des trois couches** : elle ne remplace ni
  le bac à sable, ni la politique — elle empêche qu'un module non déclaré entre par la porte de
  service.

### 1.3 HAM (High Availability Manager)

- **Ce que c'est.** Le gestionnaire de haute disponibilité de QNX : il surveille des processus
  (signaux de vie) et peut les relancer après un arrêt, selon des limites configurées.
- **Ce qu'Éclat en fait.** Le processus du runtime est **supervisé**. Lorsqu'un module dépasse son
  budget CPU et est tué (`CAP_KILLED_CPU`), ou que le runtime s'arrête, la reprise est une
  **relance propre et ordonnée** : état réinitialisé, journal conservé, module rechargé depuis sa
  source vérifiée.
- **Ce que ça apporte.** **Disponibilité et confinement de la défaillance.** Un module hostile qui
  sature son budget ne laisse pas le système dégradé : il est tué, et le retour à la normale est
  une opération maîtrisée, pas un effet de bord.

### 1.4 `ionotify`

- **Ce que c'est.** Le mécanisme de notification d'E/S de QNX : un processus peut s'abonner à des
  événements d'E/S sur un chemin et les recevoir de façon **asynchrone**.
- **Ce qu'Éclat en fait.** Le **kill-switch** : la révocation de l'empreinte d'un module est
  diffusée par notification à **tous** les courtiers ; chacun invalide le module et arrête
  l'instance qui l'exécute. Un module révoqué ne continue pas de tourner « parce qu'il était
  chargé avant ».
- **Ce que ça apporte.** **Réactivité de la politique (couche 3).** La politique n'est pas seulement
  évaluée à l'admission : elle peut être **révoquée en cours d'exécution**, de façon homogène sur
  toutes les instances.

### 1.5 `qcrypto`

- **Ce que c'est.** La bibliothèque cryptographique de QNX : primitives de hachage, de signature et
  de génération d'aléa, exposées par une API commune.
- **Ce qu'Éclat en fait.** Deux usages : (a) le **calcul et la vérification de l'empreinte
  SHA-256** du `.wasm` avant tout chargement (exigence ECL-07) ; (b) la **signature de chaque
  décision critique** dans le journal, pour rendre la trace vérifiable par un tiers (ECL-21).
- **Ce que ça apporte.** **Intégrité et traçabilité.** Attention, et c'est important : un module
  *signé* reste du code non certifié ; la signature prouve **l'origine**, jamais la sûreté. La
  politique reste l'autorité.

---

## 2. Pourquoi ces cinq ensemble

Aucune de ces cinq technologies ne suffit seule, et c'est exactement le point. `qcrypto` prouve
l'origine d'un binaire mais n'arrête rien ; une ability manquante rend un processus pauvre mais
n'empêche pas un module de lire un capteur autorisé ; HAM relance un processus mort mais ne dit pas
ce qu'il avait le droit de faire ; `ionotify` propage un ordre mais n'ordonne rien ; Pathtrust
classe un chemin mais ne filtre pas une requête. **L'argument d'Éclat n'est donc pas une liste de
technologies QNX, c'est leur superposition** — et, au-dessus d'elles, trois couches qui n'ont pas
besoin d'elles pour fonctionner : le bac à sable Wasm borne la mémoire et supprime l'appel système
direct ; le processus sans abilities borne les privilèges ; la politique du courtier décide, par
défaut **refus**, de ce qui est autorisé. Les cinq mécanismes QNX rendent cette superposition
*opérationnelle sur QNX* : ils fournissent la provenance, la révocation, la preuve et la reprise —
c'est-à-dire ce qui manque à une démonstration pour devenir une propriété du système.

---

## 3. Encadré honnête — ce qui existe, ce qui n'existe pas encore

| État | Contenu | Où |
|---|---|---|
| **Implémenté et testé** | Moteur de décision C11 pur (politique, décision, budget, journal) : **161 vérifications** vertes, **ASan/UBSan propre** | `core/` |
| **Implémenté et testé** | Jumeau Python indépendant + harnais différentiel : **28 tests Python**, **10 000 décisions C ↔ Python concordantes** | `host/` |
| **Implémenté et testé** | Contrat gelé (ABI, format de politique, 7 étapes de décision, budget, journal) | [`CONTRACT.md`](../CONTRACT.md) |
| **Pas encore fait** | Couche plateforme QNX, courtier en gestionnaire de ressources, politique de sécurité QNX, branchement réel aux cinq mécanismes ci-dessus | `platform/qnx/`, `broker/`, `image/` |
| **Pas encore fait** | Le **SDP QNX 8.0 n'est pas installé** : rien de ce qui est décrit ici n'a été exécuté sur cible QNX | — |
| **Pas mesuré** | Toute latence (aller-retour courtier, gigue de la boucle 1 kHz, temps de mise à mort) : **« à mesurer »**, aucune valeur annoncée ici | — |

En une phrase : **le cœur de la politique est écrit et vérifié ; le branchement QNX est un
travail à faire.** Ce document décrit une architecture, il ne rapporte pas une campagne de mesures.

---

## 4. Références croisées

- [`docs/qnx-five-technologies.svg`](qnx-five-technologies.svg) — le même contenu en diagramme.
- [`docs/security-mapping.md`](security-mapping.md) — correspondance des couches et des exigences
  ECL-01..22 avec les exigences ISO 26262 et IEC 62304, et ce que ce document **n'est pas**.
- [`docs/spec-en.md`](spec-en.md) — la spécification du contrat en anglais (soumission Devpost).
- [`CONTRACT.md`](../CONTRACT.md) — le contrat gelé, source de vérité (ABI, politique, budget).
- [`docs/cahier-des-charges.md`](cahier-des-charges.md) §2.1 (ce que QNX fournit déjà),
  §6 (exigences ECL-01..22), §14 (statut des affirmations).
