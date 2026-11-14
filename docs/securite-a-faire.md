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

## Couche 3 — le retrait *ciblé* : ce que le runtime garde, ce qu'il lâche

L'abandon total est incompatible avec un runtime qui charge un module : il perd `fork()` (donc
le superviseur qui tue un module hors budget, ECL-06) et la lecture du fichier `.wasm`. La voie
praticable est un retrait **ciblé** : une liste courte d'abilities, chacune justifiée.

La liste vit dans `platform/qnx/eclat_abilities_qnx.c` (une seule implémentation, avec
`platform/posix/` pour l'hôte) et le runtime l'applique **avant** de charger le module — c'est le
code non certifié qui tourne dans ce processus, pas le runtime qui le charge.

| Ability retirée | Pourquoi le runtime s'en passe |
|---|---|
| `prot_write_and_exec` | une page inscriptible **et** exécutable : le module est interprété, pas du code machine |
| `public_channel` | le runtime se *connecte* au courtier, il ne publie aucun canal |
| `interrupt` | piloter les interruptions matérielles |
| `io` | les ports d'entrée/sortie du matériel |
| `priority` | le module ne réordonne pas le système |
| `trace` | espionner les autres processus |
| `pathspace` | créer des chemins : le runtime publie par le courtier |
| `aps_root` | modifier le partitionnement adaptatif |

Ce qui reste, et pourquoi : `fork()` (le superviseur), la lecture du fichier (le `.wasm` et son
empreinte), et la connexion au courtier (chaque import). **Mesuré sur cible**, dans la console
série archivée :

```
— AVANT le retrait ciblé —
  name_open(cap       ) : OUVERT
  mmap(PROT_WRITE|EXEC) : ACCORDÉ
  fork()               : ACCORDÉ (enfant 110601)
  lecture /proc/boot/pol-broker.toml : ACCORDÉE (64 octets)
  retrait ciblé : 8 visées, 8 retirées, 0 refusées
— APRÈS le retrait ciblé —
  name_open(cap       ) : OUVERT
  name_attach()        : REFUSÉ (errno=22 Invalid argument)
  mmap(PROT_WRITE|EXEC) : REFUSÉ (errno=1 Operation not permitted)
  fork()               : ACCORDÉ (enfant 110602)
  lecture /proc/boot/pol-broker.toml : ACCORDÉE (64 octets)
  VERDICT-CIBLE : wx=REFUSEE fork=ACCORDE lecture=ACCORDEE
```

Le W+X est refusé, et tout ce dont le runtime a besoin tient. C'est exactement la différence
avec l'abandon total.

### La règle qui manquait : `unlock` dans la politique

Mesuré sur la cible le 2026-11-14 : **sous un type `secpol`, `procmgr_ability()` est accepté
mais sans effet.** La politique gouverne les abilities du processus ; sans `unlock`, une ability
accordée par la politique est verrouillée pour le processus. La conséquence est que le retrait
ciblé ne mordait que hors type — le runtime typé gardait son W+X.

La correction est dans la politique (`docs/secpol/eclat.secpol`) : les abilities que le runtime
doit pouvoir retirer lui-même sont déclarées `unlock`.

```
allow eclat_runtime self:ability {
    unlock
    prot_write_and_exec
    public_channel
    interrupt
    io
    priority
    trace
    pathspace
    aps_root
};
```

Après cette correction, la même mesure donne `wx=REFUSEE fork=ACCORDE lecture=ACCORDEE` **sous
le type `eclat_runtime`** — les deux lignes `[8]` et `[9]` de l'archive.

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

1. **Les chiffres de gigue sous charge restent ceux de l'émulation.** La gigue mesurée sous
   QEMU est dominée par l'absence de timer matériel et d'isolation de vCPU : ce qui est prouvé,
   c'est la vivacité de la boucle, pas sa performance temps réel (ECL-12/13).
2. **La politique n'est pas encore embarquée dans l'image de production.** La preuve utilise une
   IFS dédiée (`tools/image-secpol.buildfile`) avec la politique à `/proc/boot/secpol.bin` ;
   l'image de démonstration principale n'a pas de politique, faute d'infrastructure pour
   typer tous ses services sans les énumérer un par un.

## Les étapes exactes pour aller plus loin

1. Embarquer la politique dans l'image de démonstration principale et typer ses services.
2. Chiffrer la gigue sur matériel réel (ECL-12/13).
