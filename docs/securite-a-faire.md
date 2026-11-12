# Sécurité : ce qui est en place, et ce qui manque encore

Derrière les trois couches d'isolation annoncées, deux restent à **démontrer sur cible** — la
couche « processus sans abilities » et la politique `secpol`. Ce document dit exactement ce qui
est disponible dans le SDP, ce qui a été vérifié, et ce qui manque pour clore ECL-14. Aucune
affirmation de sécurité n'est avancée au-delà de ce qui a été mesuré.

## Ce qui est en place et vérifié

| Élément | État |
|---|---|
| `secpolcompile` | **présent** : `$QNX_HOST/usr/bin/secpolcompile` (compilateur hôte des politiques de sécurité). Usage relevé : `secpolcompile [-em] [-p policy-id] [-o out] fichier…`. L'option `-m` produit une politique **mutable** qui se laisse remplacer à chaud sur `procnto` — exactement ce qu'il faut pour une démo sans recompiler l'image |
| Bibliothèques cible | `libsecpol.so.1` et `libsecpolev.so.1` **embarquent l'image QEMU** (vues dans le buildfile de `mkqnximage`) : le noyau de la cible sait charger les politiques |
| API hôte | `secpol_open(path, SECPOL_USE_AS_DEFAULT)` pose la politique par défaut au moment de l'exécution ; le fichier par défaut est `/proc/boot/secpol.bin` (`secpol/secpol.h` lu) |
| API abilities | `procmgr_ability(pid, ability, …)`, `procmgr_ability_lookup(name)`, `procmgr_ability_create(name, flags)` existent (`sys/procmgr.h`) ; le catalogue complet des abilities (`PROCMGR_AID_*`) est là |

## Ce qui manque, précisément (et pourquoi ce n'est pas fait)

1. **La syntaxe du langage source de `secpolcompile`.** La documentation de référence
   (*Security Policies*, guide secpol) n'est **pas embarquée** dans ce SDP (recherché : aucun
   `*.secpol`, aucune doc HTML). Un essai de syntaxe minimale (`allow * *:*`) renvoie
   `invalid token '*'`. **Je ne devine pas la grammaire d'une politique de sécurité** : une
   règle mal écrite qui « fonctionnerait » serait une fausse preuve, pire que pas de preuve. La
   grammaire doit être lue dans la doc officielle (en ligne, ou via le package de documentation
   `com.qnx.qnx800…secpol` du QNX Software Center), puis gelée.

2. **Les constantes du masque d'abilities** (`_NTO_PA_PUTBITS`, `PROCMGR_ABILITY_MASK_SIZE`).
   L'appel documenté `procmgr_ability(pid, ability, _NTO_PA_PUTBITS, size, mask)` n'est pas
   utilisable ici : ces symboles ne sont pas dans les en-têtes publics installés. Il faut soit
   inclure l'en-tête privé, soit confirmer les valeurs dans la doc.

## Les étapes exactes pour finir (quand la doc sera sous la main)

1. Écrire la politique : le runtime (`eclat-run`) ne peut se connecter qu'au courtier, rien
   d'autre.
2. `secpolcompile -m -o secpol.bin politique.secpol`
3. Sur la cible : pousser `secpol.bin` comme politique par défaut, ou la placer en
   `/proc/boot/secpol.bin` au démarrage.
4. Preuve (console brute, comme les autres) : une connexion que la politique interdit est
   refusée avec l'erreur secpol attendue, et la connexion légitime au courtier passe.
5. Même démarche pour les abilities : faire rendre au courtier (ou au runtime) ses abilities à
   l'appel, puis montrer qu'une opération interdite échoue.

Tant que ces deux étapes ne sont pas passées, le README les liste comme « reste à faire », et
rien ne prétend qu'elles sont démontrées.