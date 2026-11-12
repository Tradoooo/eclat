# Éclat — 100 idées pour aller plus loin

Générées le 12 novembre 2026, 01 h (Europe/Paris), sur la base de l'état réel du dépôt :
`core/` (politique, décision, budget, journal) et `host/` (jumeau Python + différentiel) sont
**verts** — 161 vérifications C, 28 tests Python, 10 000 décisions différentielles concordantes.
Aucune ligne QNX, aucun SDP installé.

Légende : **[FREE]** faisable maintenant sur Linux, zéro coût · **[SDP]** exige la licence QNX
8.0 installée · **[HW]** exige du matériel (Raspberry Pi 5, cible) · **[PAID]** service payant.
Effort : f = faible, m = moyen, é = élevé. Gain : 1 à 5.

Les 25 idées du lot 3 de `qnx-100-aller-plus-loin.md` sont reprises et **étendues** ici ; les
nouvelles idées sont signalées par « (nouveau) ».

---

## A. Durcissement et sécurité (1–16)

1. **Fuzz du parseur de politique** (nouveau) — mutateur Python + harnais C sous ASan/UBSan, corpus de politiques mutées ; ECL-08/15. f · FREE · 5
2. **Mutants du moteur de décision** (nouveau) — ≥ 8 mutations de `decision.c` (ordre des étapes, défaut du refus, comparaison de plage, compteur), chacune doit être tuée par les tests ; prouve la force de la suite. f · FREE · 5
3. **Fuzz d'instanciation de modules** — `.wasm` malformés injectés au runtime sous ASan ; ECL-15. m · FREE (avec L4) · 4
4. **Journal d'audit à hachage enchaîné** — chaque décision signe la précédente (HMAC-SHA256), export infalsifiable pour ISO 21434 / FDA ; idée lot3 #9. f · FREE · 4
5. **Vérificateur d'audit indépendant** — un tiers rejoue la chaîne et détoute toute suppression/altération de ligne ; complète #4. f · FREE · 4
6. **Manifeste de module signé** — CLI générant (auteur, empreinte, capacités, WCRT) signé, vérifié à l'admission ; idée lot3 #10. f · FREE · 4
7. **Hygiène mémoire inter-appels** — effacement des zones rendues, pages de garde, allocation déterministe ; ferme le canal de rémanence. idée lot3 #5. m · FREE (conception + core) · 3
8. **Kill-switch par révocation d'empreinte** — liste noire synchronisée, mise à mort immédiate ; idée lot3 #7 (version Linux : fichier/inotify ; version QNX : ionotify). f · FREE (Linux) · 4
9. **Post-mortem de violation** — à la mise à mort, vidage pile/registres/derniers appels dans un fichier exploitable ; idée lot3 #13. m · SDP (complet) / FREE (Linux partiel) · 3
10. **Jeton de capacité atténué** — un module délègue à un sous-module un jeton HMAC à droits réduits, révocable ; idée lot3 #4. é · FREE (conception+Python) · 5
11. **Séparation des rôles politique/signature** — la clé qui signe les manifestes ne peut pas modifier les politiques (deux autorités) ; (nouveau). f · FREE · 3
12. **Mode « audit » complet** — journaliser aussi les décisions accordées avec horodatage, pour rejouer une session ; (nouveau). f · FREE · 3
13. **Politique de sécurité QNX d'exemple** — `secpol` interdisant au runtime tout accès hors courtier ; ECL-14. f · FREE (écriture) / SDP (test) · 4
14. **Garde « défaut = tout refuser » testée explicitement** — un test dédié qui supprime tout `allow` et vérifie le refus systématique ; (nouveau). f · FREE · 4
15. **Détection de politique trop permissive** — lint signalant `ids` non bornés, `value_milli_range` absents, capacités sensibles en allow ; (nouveau). f · FREE · 4
16. **Comparateur sémantique de politiques** — diff deux politiques et lister les décisions qui changent (pas un diff texte) ; (nouveau). m · FREE · 4

## B. Runtime et performance (17–30)

17. **WAMR sur Linux (`platform/posix`)** — cloner WAMR, le compiler, exécuter un `.wasm` qui appelle les imports ; ECL-04/07/08. **Le plus gros gain gratuit.** é · FREE · 5
18. **Modules de démonstration** — honnête, gourmand, hostile, compilés en `.wasm` (wasi-sdk), avec empreintes ; L5. m · FREE (avec wasi-sdk) · 5
19. **Chargement à chaud sous boucle 1 kHz** — mesurer la gigue pendant le chargement ; ECL-12. m · FREE (Linux) / SDP (QNX) · 4
20. **Mode AOT** — compilation ahead-of-time, mesure du gain et du WCRT ; ECL-18. m · FREE (Linux) · 3
21. **Créneaux TDMA** — cantonner chaque module dans un créneau périodique ; idée lot3 #1. m · SDP · 4
22. **Certificat de pire temps d'exécution (WCRT)** — mesure instrumentée consignée dans la politique comme condition de chargement ; idée lot3 #2. m · FREE (Linux) / SDP · 4
23. **Appels asynchrones** — `cap_call_async`/`cap_await` avec pulse de fin et budget d'appels en vol ; idée lot3 #3. m · SDP · 4
24. **Isolation multi-modules** — plusieurs modules simultanés, budgets indépendants, aucun effet croisé ; ECL-19. m · FREE (avec L4) · 4
25. **Banc de latence de décision** — ns/décision, histogramme p50/p99/p999, comparaison C ↔ Python ; idée lot3 #25 / L9. f · FREE · 4
26. **Migration assistée depuis Lua** — exécuter un plugin Lua existant comme module Wasm à comportement identique ; ECL-20. é · FREE (Linux) · 3
27. **Budget mémoire avec comptage fin** — suivre l'allocation réelle du module, pas seulement le plafond déclaré ; (nouveau). m · FREE (avec L4) · 3
28. **Dessin de la boucle critique** — un script produisant l'histogramme avant/pendant/après (matplotlib ou SVG) ; (nouveau). f · FREE · 4
29. **Mesure reproductible C nu / interpréteur / AOT** — fiche gigure en une page, chiffres citables ; idée lot3 #25. f · FREE (Linux) / SDP (QNX) · 4
30. **Profileur de coût par import** — quel import coûte le plus cher en aller-retour ; (nouveau). f · FREE (avec L4) · 3

## C. Chaîne d'approvisionnement et amont (31–42)

31. **Recette `qnx-ports` prête** — Makefile conforme `qnx-ports/build-files` + cible Docker ; l'équipe des ports consomme sans adaptation ; idée lot3 #15. f · FREE (écriture) · 5
32. **Correctif libc amont minimal** — un second PR WAMR isolant le plus petit delta `platform/qnx` ; idée lot3 #16. f · FREE (prép) / SDP (validation) · 5
33. **Lettre d'accompagnement amont** — explique le trou (aucun port Wasm), la couche, les tests ; ECL-16. f · FREE · 5
34. **Sous-ensemble WASI sur QNX** — une dizaine d'imports `wasi_snapshot_preview1` mappés sur QNX ; idée lot3 #17. m · SDP · 4
35. **Paquet installable QNX** — archive auto-extractible conforme SDP ; idée lot3 #19. m · SDP · 3
36. **Épinglage reproductible des versions** — WAMR, wasi-sdk, toolchain figés (fichier `VERSIONS` + checksums) ; (nouveau). f · FREE · 4
37. **Pont vers ports QNX existants** — `cap_kv` sur sqlite, `cap_net_send` sur mosquitto ; idée lot3 #20. f · SDP · 4
38. **Guide de portage générique** — « porter un langage/runtime sur QNX » réutilisable ; idée lot3 #18, ECL-17. f · FREE · 3
39. **PORTING.md documenté** — chaque écart POSIX rencontré et sa résolution ; ECL-17. f · FREE (au fil de L4) · 4
40. **Vérification d'empreinte à l'admission** — un octet modifié refuse le chargement ; ECL-07. f · FREE · 4
41. **Registre de provenance des modules** — qui a signé quoi, quand, quelle politique ; (nouveau). m · FREE · 3
42. **Pipeline de signature en CI** — signer automatiquement les `.wasm` de démo à chaque build ; (nouveau). f · FREE · 3

## D. Expérience développeur et outillage (43–56)

43. **SDK module tiers `eclat new module`** — gabarit C + Makefile Linux + harnais de test identique au courtier, sans licence QNX ; idée lot3 #11. m · FREE · 5
44. **Console d'exploitation `eclatctl`** — état vivant des budgets, motifs de refus, santé des modules ; idée lot3 #14. m · FREE (Linux) · 4
45. **Éditeur de politique dans le navigateur** — coller du TOML, prévisualiser les décisions côté client ; idée lot3 #24. m · FREE · 4
46. **Codelab 4 chapitres** — « exécuter du code tiers en sécurité sur QNX » ; L11. m · FREE · 4
47. **Émulateur de courtier en Python** — un faux courtier qui rejoue les décisions pour tester un module sans C ; (nouveau). f · FREE · 4
48. **LSP / complétion pour le TOML de politique** — diagnostics en ligne dans l'éditeur ; idée lot1 #13 adaptée. m · FREE · 3
49. **Générateur de politiques d'exemple** — `eclat gen-policy` produit des politiques types (capteur, actionneur, réseau) ; (nouveau). f · FREE · 3
50. **Documentation développeur de module** — comment écrire un module, tester en local, soumettre ; (nouveau). f · FREE · 4
51. **Harnais de test de module réutilisable** — le même harnais que le courtier, côté module, sur Linux ; idée lot3 #11 (extrait). f · FREE · 4
52. **`eclat doctor`** — diagnostique l'installation (wasi-sdk, cmake, WAMR, SDP) et dit ce qui manque ; (nouveau). f · FREE · 3
53. **Visualiseur de décisions** — sortie graphique (SVG) de l'arbre allow/deny d'une politique ; (nouveau). f · FREE · 3
54. **Mode dry-run** — évaluer une politique contre un journal de requêtes sans rien exécuter ; (nouveau). f · FREE · 4
55. **Bibliothèque cliente C minimale** — un `libeclatclient` qui parle au courtier (stub Linux via tube) ; (nouveau). m · FREE · 3
56. **Gabarit de bogue amont** — modèle de rapport pour WAMR/qnx-ports ; (nouveau). f · FREE · 2

## E. Documentation et argumentaire juge (57–68)

57. **Diapositive « cinq technologies QNX »** — abilities, Pathtrust, HAM, ionotify, qcrypto dans une architecture annotée ; bonus QNX-specific ; idée lot3 #22. f · FREE · 5
58. **Dossier de sécurité mappé** — chaque couche d'isolation reliée à ISO 26262 / IEC 62304 ; idée lot3 #12. f · FREE · 4
59. **Script vidéo coupe-circuit (3 min)** — module hostile tué en direct, gigure plate ; idée lot3 #21. f · FREE (script) · 5
60. **`spec-en.md`** — la spec en anglais (soumission Devpost) ; L11. f · FREE · 4
61. **Argumentaire « trois couches d'isolation »** — la diapo qui dit pourquoi ce n'est pas « un runtime Wasm de plus » ; (nouveau). f · FREE · 5
62. **Preuve de non-perturbation de la boucle 1 kHz** — le graphique avant/pendant/après, expliqué ; (nouveau). m · FREE (Linux) · 5
63. **Fiche « reproductibilité »** — une page : comment tout rejouer depuis un clone propre ; (nouveau). f · FREE · 4
64. **`DECISIONS.md` tenu** — journal des décisions de conception (déjà amorcé) ; (nouveau). f · FREE · 3
65. **Étude de cas partenaire** — scénario rédigé « un équipementier charge un module de diagnostic tiers » ; (nouveau). f · FREE · 3
66. **Comparaison avec l'existant** — pourquoi pas wasmtime/wasmer/Lua : tableau honnête ; (nouveau). f · FREE · 4
67. **README orienté juge** — le trou, la contribution, la preuve, la reproductibilité en 4 blocs ; (nouveau). f · FREE · 4
68. **Mentions légales et licence amont** — Apache-2.0, attribution WAMR, historique non copié ; (nouveau). f · FREE · 3

## F. Tests et CI (69–80)

69. **Test différentiel étendu (property-based)** — générateur aléatoire de politiques/requêtes, N graines en CI ; ECL-03. f · FREE · 5
70. **Job CI fuzz** — campagne courte ASan/UBSan à chaque PR ; ECL-09. f · FREE · 4
71. **Job CI mutants** — vérifie que le taux de mutants tués reste à 100 % ; (nouveau). f · FREE · 4
72. **Couverture de code (gcov/lcov)** — seuil minimal, rapport en artifact ; (nouveau). f · FREE · 4
73. **Job CI `wasm`** — compile les modules de démo avec wasi-sdk épinglé ; cahier §8. m · FREE · 4
74. **Garde de taille dans CI** — déjà fait (`sizecheck`) ; étendre aux fonctions et à tous langages ; (nouveau). f · FREE · 3
75. **Garde SPDX dans CI** — déjà fait ; étendre aux fichiers de build et docs ; (nouveau). f · FREE · 3
76. **Badge CI + badges couverture** — lisibles d'un coup d'œil sur le README ; (nouveau). f · FREE · 3
77. **Test de régression du contrat** — un test qui échoue si `CONTRACT.md` change sans mise à jour du code ; (nouveau). f · FREE · 4
78. **Rejeu bit à bit du jumeau** — les valeurs en milli-unités entières permettent un rejeu exact ; (nouveau). f · FREE · 4
79. **Banque de cas hostiles étendue** — 30 politiques fautives (fait), ajouter 12 modules malformés + 8 séquences d'imports interdites ; cahier §7.3. m · FREE · 4
80. **Intégration d'un runner auto-hébergé** — CI sur r630 pour les jobs lourds (fuzz long, AOT) ; (nouveau). m · FREE (matériel dispo) · 3

## G. Écosystème et intégration (81–90)

81. **Capacités natives PPS** — exposer le courtier comme objets PPS ; idée lot3 #8. m · SDP · 3
82. **Supervision HAM** — relance du module tué par HAM, battement de cœur ; cahier §3.1. m · SDP · 4
83. **Jumeau numérique Python** — rejeu d'une session QNX en Python pour analyse hors cible ; (nouveau). m · FREE · 4
84. **Pont ROS 2** — un module Wasm qui publie/abonne via le port ROS 2 de QNX ; (nouveau). m · SDP · 3
85. **Intégration io-sock** — `cap_net_send` sur la pile réseau QNX ; (nouveau). m · SDP · 3
86. **Stockage PPS pour `cap_kv`** — backend PPS au lieu de sqlite ; (nouveau). f · SDP · 3
87. **Multi-courtiers fédérés** — plusieurs courtiers partageant une liste noire de révocation ; (nouveau). é · SDP · 3
88. **Passerelle de diagnostic** — exposer les décisions en Prometheus/JSON pour l'observabilité ; (nouveau). f · FREE · 3
89. **Module de référence « mise à jour A/B »** — réutilise le savoir-faire Ambre ; idée lot3 #6. m · SDP · 4
90. **Bibliothèque de capacités étendue** — `cap_kv` transactionnel, `cap_net` avec accusé ; (nouveau). m · FREE (contrat) · 3

## H. Démonstration (91–100)

91. **Banc Raspberry Pi 5 physique** — servomoteur + LED pilotés par les modules, politique modifiable en direct ; vote Community Choice ; idée lot3 #23. m · HW · 4
92. **Démo QEMU en un clic** — image préconstruite, scénario prêt, sans installer le SDP ; idée lot1 #23 adaptée. m · FREE (QEMU) / SDP (image QNX) · 4
93. **Attaque rejouée en vidéo** — le module hostile saturé son budget, tué, la boucle reste plate ; idée lot3 #21. f · FREE (script) · 5
94. **Terrain de jeu navigateur** — coller une politique + un module, voir les décisions en direct (core compilé en Wasm) ; idée lot3 #24. m · FREE · 4
95. **Avant/après chiffré** — gigure C nu vs module Wasm, en une page ; idée lot3 #25. f · FREE · 4
96. **Démo « refuser puis tuer »** — séquence courte : accès refusé, boucle infinie, kill, relance ; (nouveau). f · FREE (Linux) · 4
97. **Tableau de bord temps réel** — `eclatctl --watch` avec budgets et refus qui défilent ; (nouveau). f · FREE · 3
98. **Scénario « supply chain »** — module signé, empreinte vérifiée, manifeste contrôlé à l'admission ; (nouveau). f · FREE · 4
99. **Comparaison live avec un noyau monolithique** — la même débordement emporte tout vs rien ici ; (nouveau). m · FREE (narratif) · 4
100. **Mini-site GitHub Pages** — codelab + éditeur de politique + preuves, hébergé gratuitement ; (nouveau). f · FREE · 4

---

## TOP 10 GRATUIT — retenues pour implémentation immédiate

Critères : **[FREE]** (zéro coût, sans SDP ni matériel), construit sur le `core/` déjà vert,
fort gain pour le concours, **complétable et vérifiable cette nuit**. Les idées #17/#18 (WAMR
sur Linux) sont le plus gros gain gratuit mais demandent une session dédiée (clone WAMR +
wasi-sdk, build cmake) — elles sont classées « prochaine vague », pas dans ce top 10.

| # | Idée (réf. ci-dessus) | Pourquoi c'est dans le top 10 |
|---|---|---|
| 1 | **Fuzz du parseur de politique** (A1) | ECL-08/15, gratuit, prouve la robustesse |
| 2 | **Mutants du moteur de décision** (A2) | ECL-15, prouve que les tests tuent les régressions |
| 3 | **Journal d'audit à hachage enchaîné + vérificateur** (A4+A5) | argument ISO 21434/FDA, coût faible |
| 4 | **Manifeste de module signé** (C6) | chaîne d'approvisionnement complète, gratuit |
| 5 | **SDK module tiers `eclat new module`** (D43) | critère « produit cohérent », élargit l'audience |
| 6 | **Console `eclatctl`** (D44) | outil d'exploitation crédible, gratuit |
| 7 | **Recette `qnx-ports` prête** (C31) | la catégorie Ports exige une contribution consommable |
| 8 | **Lettre amont WAMR + épinglage versions** (C32+C33+C36) | ECL-16, l'URL de PR est exigée |
| 9 | **Banc de latence `capbench` + job CI fuzz/mutants/couverture** (B25+F70+F71+F72) | chiffres citables + CI durcie |
| 10 | **Docs juge : diapo « cinq technologies QNX » + dossier ISO 26262/IEC 62304 + spec-en** (E57+E58+E60) | bonus QNX-specific + argument équipementier |

**Prochaine vague gratuite (hors top 10, plus lourde)** : #17 WAMR sur Linux, #18 modules de
démo `.wasm`, #45/#94 éditeur/terrain de jeu navigateur, #69 différentiel property-based étendu.