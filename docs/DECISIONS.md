# DECISIONS — journal des décisions de conception

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