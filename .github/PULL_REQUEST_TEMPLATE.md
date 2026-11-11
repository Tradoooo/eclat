## Ce que la PR change

<!-- Une phrase. -->

## Exigence(s) couverte(s)

<!-- ECL-nn, une par ligne. -->

## Preuve

<!-- Commandes exécutées et leur résultat. Exemple : -->

```
make core && make test
# OK 312 checks
make differential
# DIFF OK 10000 decisions
```

## Garde-fous

- [ ] `make test` vert
- [ ] `make test-san` vert (ASan/UBSan)
- [ ] `make size-check` vert (< 500 lignes/fichier, < 50 lignes/fonction)
- [ ] `make spdx` vert
- [ ] `core/` reste pur (ni horloge, ni fichier, ni socket, ni allocation)
- [ ] aucune duplication (une seule implémentation par logique)
- [ ] le contrat `CONTRACT.md` n'est pas modifié, ou la modification est justifiée dans `docs/DECISIONS.md`
