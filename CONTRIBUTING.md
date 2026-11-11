# Contribuer à Éclat

Merci de votre intérêt. Éclat est un portage de [WAMR](https://github.com/bytecodealliance/wamr)
et un **courtier de capacités** destiné à QNX 8.0. Le projet est candidat au
[QNX Global Hackathon 2026](https://qnx.com) (catégorie *Open-Source Contribution & Ports*).

## Règles non négociables

Ces règles viennent du cahier des charges du projet et des règles du concours. Elles sont
appliquées par la CI : une pull request qui les enfreint est rejetée.

1. **C11 strict** : `-std=c11 -O2 -Wall -Wextra -Werror`. Aucun avertissement toléré.
2. **`core/` est pur** : ni horloge, ni fichier, ni socket, ni allocation, ni état global
   mutable. Tout ce qui touche au monde extérieur est injecté par l'appelant.
3. **Une seule implémentation par logique** : `platform/posix/` et `platform/qnx/` partagent
   le *même contrat d'interface*, jamais du code copié. Zéro duplication.
4. **Taille** : moins de 500 lignes par fichier, moins de 50 lignes par fonction
   (garde : `make size-check`).
5. **SPDX** : première ligne de chaque fichier source
   (`// SPDX-License-Identifier: Apache-2.0` ou `# SPDX-License-Identifier: Apache-2.0`).
6. **Traçabilité** : chaque exigence `ECL-nn` est citée en commentaire au-dessus du code
   concerné **et** dans le nom du test qui la vérifie.
7. **Le contrat est gelé** : `CONTRACT.md` fait foi. Toute modification doit être justifiée dans
   `docs/DECISIONS.md` et datée.
8. **Licence** : Apache-2.0 (compatible avec WAMR). Historique amont **non copié** — on ne
   soumet pas de fork.

## Boucle de travail

```bash
make core          # compile core/ en C11 strict
make test          # tests unitaires (≥ 300 vérifications attendues)
make test-san      # les mêmes sous ASan/UBSan
make python-tests  # jumeau Python
make differential  # 10 000 décisions comparées C ↔ Python
make size-check    # garde de taille
make spdx          # garde SPDX
```

Tout doit être vert **en local** avant d'ouvrir la pull request. La CI rejoue exactement ces
commandes, avec un budget de 5 minutes par job.

## Where to start

| Lot | Contenu | Dépend de |
|---|---|---|
| L1 | `core/` politique : parseur TOML restreint, validation, banque de cas fautifs | contrat gelé |
| L2 | `core/` décision + budget + journal | L1 |
| L3 | jumeau Python + test différentiel | L2 |
| L4 | `platform/posix/` + intégration WAMR sur Linux | L2 |
| L5 | modules de démonstration (honnête, gourmand, hostile) | L4 |
| L6 | fuzz, mutants, gardes | L1–L5 |
| L7 | `platform/qnx/` + build croisé | SDP QNX installé |
| L8 | courtier QNX (`broker/`, `target/eclatd`) | L7 |
| L9 | banc de latence + mesures | L8 |
| L10 | `upstream/` : patches formatés + lettre d'accompagnement | L7 |
| L11 | `PORTING.md`, codelab, docs FR/EN, script vidéo | L1–L9 |

Les lots **L1 à L6 ne demandent aucune licence QNX** : ils se font et se testent sur Linux.
C'est le point clé du projet — toute la logique est vérifiable avant d'avoir le SDP.

## Branches et commits

- Une branche par sujet : `feat/…`, `fix/…`, `docs/…`.
- Commits petits, messages explicites, en français ou en anglais.
- Pas de réécriture d'historique partagé (`master` est protégé).

## Licence des contributions

En ouvrant une pull request, vous acceptez que votre contribution soit publiée sous
Apache-2.0, la licence du projet.
