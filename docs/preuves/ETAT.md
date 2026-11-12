# Preuves — état au 2026-11-12T21:18:50+01:00

Généré par `tools/preuves.sh`. Chaque ligne est une preuve : une commande, un artefact
brut daté, un verdict. Rien n'est retouché.

| Preuve | Artefact | Verdict | Durée |
|---|---|---|---|
| tests C du cœur (ECL-01/02/05/06/07) | `docs/preuves/hote/2026-11-12-tests-core.txt` | vert | 0s |
| tests C sous ASan/UBSan | `docs/preuves/hote/2026-11-12-tests-core-asan.txt` | vert | 0s |
| tests Python | `docs/preuves/hote/2026-11-12-tests-python.txt` | vert | 2s |
| différentiel 10 000 décisions | `docs/preuves/hote/2026-11-12-differentiel.txt` | vert | 1s |
| fuzz 3 000 entrées (ASan/UBSan) | `docs/preuves/hote/2026-11-12-fuzz.txt` | vert | 219s |
| mutants (taux de détection 100 %) | `docs/preuves/hote/2026-11-12-mutants.txt` | vert | 131s |
| intégration .wasm sur l'hôte | `docs/preuves/hote/2026-11-12-integration-wasm.txt` | vert | 2s |
| démonstration sur QNX 8.0 (ECL-04/05/06/07) | docs/preuves/qnx800/2026-11-12-demonstration.txt | vert | - |
| courtier sur /cap (MsgSend/MsgReply) | docs/preuves/qnx800/2026-11-12-courtier.txt | vert | - |
| imports du module via le courtier (MsgSend/MsgReply) | docs/preuves/qnx800/2026-11-12-module-via-courtier.txt | vert | - |
| boucle 1 kHz (vivante pendant la mise à mort) | docs/preuves/qnx800/2026-11-12-boucle-1khz.txt | vert | - |

**Toutes les preuves exécutées sont vertes.**
