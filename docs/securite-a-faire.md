# Sécurité : les trois couches d'isolation, ce qui est démontré et ce qui reste

Les trois couches annoncées par le projet sont le bac à sable WebAssembly, le processus QNX
sans abilities, et la politique de sécurité appliquée par le noyau. Les deux dernières sont
**démontrées sur cible QNX 8.0** depuis le 13/11 : la preuve est archivée, brute et datée, dans
[`docs/preuves/qnx800/2026-11-13-secpol-ecl14.txt`](preuves/qnx800/2026-11-13-secpol-ecl14.txt),
et elle se rejoue par une commande unique :

```bash
tools/demo-secpol-on-qnx.sh x86_64
```

Ce document dit exactement ce qui a été mesuré, comment, et ce qui reste hors de portée. Aucune
affirmation de sécurité n'est avancée au-delà de ce qui a été observé sur la cible.

## Ce qui est en place et vérifié

| Élément | État |
|---|---|
| `secpolcompile` | **présent** : `$QNX_HOST/usr/bin/secpolcompile`. Usage relevé : `secpolcompile [-em] [-p policy-id] [-o out] fichier…`. L'option `-m` produit une politique **mutable**, remplaçable à chaud sur `procnto` |
| **Grammaire `secpol`** | **lue et gelée** : la page officielle *Security policy language* est embarquée dans le SDP (`com.qnx.doc.security.system/topic/manual/secpol_language.html`). Productions relevées : `type`, `range`, `allow <src> <dst>:ability { … }`, `allow <src> <dst>:channel connect`, `allow_attach`, `allow_link`, `default_spawn_type`, `derive_type`, `mount_point`. Le compilateur officiel est l'arbitre : il refuse une cible d'abilities autre que `self`, une ability inconnue, un type non déclaré |
| `secpolpush` / `secpolmonitor` | **présents sur la cible** : `$QNX_TARGET/x86_64/bin/`. `secpolpush` pousse la politique compilée dans `procnto` ; `secpolmonitor -a` interroge le noyau |
| Typage d'un processus | **`on -T <type> <programme>`** : le type est attribué au démarrage, et le noyau applique dès lors les règles de ce type |
| API abilities | `procmgr_ability(pid, ability, …)`, `procmgr_ability_lookup(name)`, `procmgr_ability_create(name, flags)` existent (`sys/procmgr.h`) ; le catalogue complet des abilities (`PROCMGR_AID_*`) est là, avec la liste des codes d'opération `PROCMGR_AOP_DENY` / `PROCMGR_AOP_ALLOW` / `PROCMGR_AOP_LOCK` et les domaines `PROCMGR_ADN_ROOT` / `PROCMGR_ADN_NONROOT` |
| Idioma documenté | `procmgr_ability(0, PROCMGR_ADN_ROOT \| PROCMGR_AOP_DENY \| PROCMGR_AOP_LOCK \| PROCMGR_AID_EOL)` — « drop all abilities while running as root and lock out any further changes » (C Library Reference, `procmgr_ability()`) |

## Couche 3 — la politique de sécurité (`secpol`) : DÉMONTRÉE

La politique est versionnée : [`docs/secpol/eclat.secpol`](secpol/eclat.secpol). Trois types :

- `eclat_broker` — le courtier de capacités, seul processus joignable par le runtime ;
- `eclat_runtime` — le runtime qui héberge le module WebAssembly non certifié ;
- `eclat_intrus` — un **serveur témoin**, distinct du courtier, qui sert de cible de refus.

Le runtime n'a qu'une seule règle : `allow eclat_runtime eclat_broker : channel connect;`.

**Ce qui est mesuré sur la cible QNX 8.0** (console brute archivée) :

```
--- [5] le runtime : type eclat_runtime, il tente les deux canaux + fork ---
  name_open(cap       ) : OUVERT
  name_open(intrus    ) : REFUSÉ (errno=1 Operation not permitted)
```

Le témoin est **réellement présent et à l'écoute** — le scénario le lance juste avant, et le
refus porte `EPERM`, pas `ENOENT`. La distinction est le cœur de la preuve : le runtime ne
joint pas le courtier *parce que le courtier est le seul qu'il lui est permis de joindre*, et
non parce que le service absent le ferait échouer de la même façon.

Le client du courtier, lancé sous le même type, traverse la frontière sans incident :
`ping` et les quatre lectures du capteur reviennent `CAP_OK`, les quatre demandes hors
politique reviennent `CAP_DENIED_POLICY` / `CAP_DENIED_RANGE`.

## Couche 2 — le processus sans abilities : DÉMONTRÉE

Un processus root ordinaire abandonne toutes ses abilities avec l'idiome documenté, et le noyau
mesure la différence avant/après (même archive) :

```
— AVANT l'abandon des abilities (processus root ordinaire) —
  name_open(cap       ) : OUVERT
  mmap(PROT_WRITE|EXEC) : ACCORDÉ
  fork()               : ACCORDÉ (enfant 90121)
  procmgr_ability(DENY|LOCK|EOL) : APPLIQUÉ
— APRÈS l'abandon des abilities —
  name_open(cap       ) : REFUSÉ (errno=1 Operation not permitted)
  mmap(PROT_WRITE|EXEC) : REFUSÉ (errno=1 Operation not permitted)
  fork()               : REFUSÉ (errno=1 Operation not permitted)
```

Le verrouillage (`PROCMGR_AOP_LOCK`) empêche la reprise : sans lui, un processus qui a lâché
ses privileges pourrait les réclamer à nouveau.

## Ce que ces deux couches font — et ne font pas

Il faut être précis, parce que c'est là que les projets de sécurité se trompent :

- **`secpol` restreint les canaux et les abilities *privilégiées*.** Il ne touche pas aux
  abilities non privilégiées : sous le type `eclat_runtime`, `fork()` et `mmap(PROT_WRITE|PROT_EXEC)`
  restent accordés (mesuré, ligne `[5]` de l'archive). Le type retire ce qui est *privilégié*,
  pas ce qui est ordinaire.
- **Abandonner ses abilities retire tout, y compris l'ordinaire** — `fork()` compris. C'est
  plus fort, et c'est incompatible avec un runtime qui doit charger un module : le
  chargement et l'exécution d'un `.wasm` ont besoin de lire un fichier et de charger du code.
- **Les deux couches ne se substituent donc pas.** La politique `secpol` dessine le *périmètre
  de communication* du runtime ; le retrait d'abilities dessine son *périmètre de privilèges*.
  Un produit qui veut les deux les compose : d'abord le type (qui autorise le courtier et rien
  d'autre), puis l'abandon ciblé des abilities que le runtime n'utilise pas.

C'est précisément ce que fait Éclat, et la preuve mesure les deux séparément plutôt que de les
confondre en une phrase.

## Ce qui reste hors de portée, et pourquoi

1. **Le retrait d'abilities du *runtime* lui-même n'est pas fait, et ne peut pas l'être tel
   quel.** Le runtime charge un module depuis le disque ; `procmgr_ability(DENY|EOL)` le
   priverait de ce dont il a besoin. La voie praticable est un retrait **ciblé** — deny
   quelques abilities, pas toutes — qui reste à faire et à prouver.
2. **Les chiffres de gigue sous charge restent ceux de l'émulation.** La gigue mesurée sous
   QEMU est dominée par l'absence de timer matériel et d'isolation de vCPU : ce qui est prouvé,
   c'est la vivacité de la boucle, pas sa performance temps réel (ECL-12/13).
3. **La politique n'est pas encore embarquée dans l'image de production.** La preuve utilise une
   IFS dédiée (`tools/image-secpol.buildfile`) avec la politique à `/proc/boot/secpol.bin` ;
   l'image de démonstration principale n'a pas de politique, faute d'infrastructure pour
   typer tous ses services sans les énumérer un par un.

## Les étapes exactes pour aller plus loin

1. Retrait ciblé d'abilities dans `eclat-run` : `procmgr_ability(0, PROCMGR_ADN_ROOT |
   PROCMGR_AOP_DENY | PROCMGR_AID_FORK, PROCMGR_AID_EOL)` et voisins, puis mesurer.
2. Embarquer la politique dans l'image de démonstration principale et typer ses services.
3. Chiffrer la gigue sur matériel réel (ECL-12/13).
