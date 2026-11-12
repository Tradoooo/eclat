# Preuves — le principe du dépôt

**Aucune affirmation d'Éclat n'est sans preuve.** Chaque étape du projet livre trois choses :

1. **une commande unique** qui la reproduit ;
2. **un artefact brut daté** dans ce dossier, sortie de commande non retouchée ;
3. **une ligne** dans [`ETAT.md`](ETAT.md), avec le verdict de la dernière exécution.

Cela vaut pour le code (tests, mutants, fuzz, différentiel) comme pour la cible (démonstration sur
QNX 8.0). Quand une étape est annoncée dans un commit ou dans le README, l'artefact correspondant
est dans le même commit.

## Rejouer tout

```bash
tools/preuves.sh            # toutes les preuves, ~3 min
tools/preuves.sh --rapide   # sans les preuves lentes (mutants, fuzz long)
```

Le résultat est écrit dans [`ETAT.md`](ETAT.md) et les artefacts sont datés dans `hote/` et
`qnx800/`.

## Ce qui est archivé

| Dossier | Contenu |
|---|---|
| `hote/` | sorties brutes des tests C, des tests sous ASan/UBSan, des tests Python, du différentiel C ↔ Python, du fuzz, des mutants, de l'intégration WAMR sur Linux |
| `qnx800/` | console brute de la cible QNX 8.0 : identification du noyau, les quatre scénarios ECL-04/05/06/07, et la sortie du superviseur |

## Règles

- **Brut.** Un artefact est la sortie de la commande, pas un résumé rédigé. Les logs du courtier
  et le bilan du chargeur apparaissent tels quels, y compris quand ils s'entrelacent.
- **Daté.** Le nom du fichier porte la date d'exécution : deux exécutions ne se confondent pas.
- **Reproductible.** L'artefact dit quelle commande l'a produit. `tools/preuves.sh` sait les
  rejouer toutes.
- **Honête.** Un échec est archivé comme un échec. Une preuve verte est une preuve verte ; une
  preuve absente n'est pas une preuve.

## Et pour les étapes à venir

Chaque étape du courtier (`broker/`, gestionnaire de ressources QNX) suivra la même règle : le
sous-ensemble qui est fait, la preuve correspondante dans ce dossier, et le commit qui apporte
les deux. Une étape sans preuve n'est pas une étape faite.

## Provenance

Les preuves sur cible utilisent une image **QNX 8.0.0** (`qnxwamr 8.0.0 2026/06/05`) construite par
`mkqnximage --type=qemu` à partir du SDP 8.0 et lancée sous QEMU/KVM. Le portage QNX de WAMR
utilisé ici n'est pas le nôtre : c'est la PR amont
[#5200](https://github.com/bytecodealliance/wasm-micro-runtime/pull/5200) (voir
[`NOTICE`](../../NOTICE)).