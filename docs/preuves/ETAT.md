# Preuves — état au 2026-11-12T20:28:24+01:00

Généré par `tools/preuves.sh`. Chaque ligne est une preuve : une commande, un artefact
brut daté, un verdict. Rien n'est retouché.

| Preuve | Artefact | Verdict | Durée |
|---|---|---|---|
| tests C du cœur (ECL-01/02/05/06/07) | `docs/preuves/hote/2026-11-12-tests-core.txt` | vert | 2s |
| tests C sous ASan/UBSan | `docs/preuves/hote/2026-11-12-tests-core-asan.txt` | vert | 0s |
| tests Python | `docs/preuves/hote/2026-11-12-tests-python.txt` | vert | 1s |
| différentiel 10 000 décisions | `docs/preuves/hote/2026-11-12-differentiel.txt` | vert | 0s |
| fuzz 3 000 entrées (ASan/UBSan) | `docs/preuves/hote/2026-11-12-fuzz.txt` | vert | 91s |
| mutants (taux de détection 100 %) | `docs/preuves/hote/2026-11-12-mutants.txt` | vert | 67s |
| intégration .wasm sur l'hôte | `docs/preuves/hote/2026-11-12-integration-wasm.txt` | vert | 2s |
| démonstration sur QNX 8.0 (ECL-04/05/06/07) | docs/preuves/qnx800/2026-11-12-demonstration.txt | vert | - |
| courtier sur /cap (MsgSend/MsgReply) | docs/preuves/qnx800/2026-11-12-courtier.txt | vert | - |
| imports du module via le courtier (MsgSend/MsgReply) | docs/preuves/qnx800/2026-11-12-module-via-courtier.txt | vert | - |

**Toutes les preuves exécutées sont vertes.**
