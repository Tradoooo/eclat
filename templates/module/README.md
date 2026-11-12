# {{MODULE}} — module tiers pour le courtier de capacités Éclat

Ce squelette contient tout ce qu'il faut pour écrire et **tester un module sur Linux, sans
licence QNX, sans SDP et sans cible matérielle**.

## Tester en trois commandes

```bash
make test          # compile le harnais natif et rejoue le module contre le vrai moteur
make wasm          # facultatif : compile le module en wasm32 (exige wasi-sdk)
make clean
```

`make test` affiche chaque appel du module au courtier et la décision rendue :

```
module {{MODULE}} — politique « {{MODULE}} »
  [log niv=1] {{MODULE}} démarré
  [ 1] read_sensor     id=1     valeur=0        -> CAP_OK
  [ 2] write_actuator  id=3     valeur=2100     -> CAP_DENIED_RANGE
  [ 3] net_send        id=9     valeur=0        -> CAP_DENIED_POLICY
```

Un refus n'est **pas** une erreur : c'est la politique qui fait son travail. Pour changer le
comportement, modifie `policy.toml`, pas le module.

## Ce qu'il y a dans ce dossier

| Fichier | Rôle |
|---|---|
| `module.c` | la logique métier. C'est ce fichier qui sera compilé en `.wasm` |
| `module_imports.h` | l'ABI des imports, recopie fidèle de `CONTRACT.md` §1 |
| `harness.c` | fournit les imports en les faisant passer par le **vrai** moteur `eclat_decide` |
| `policy.toml` | la politique appliquée au module (défaut : tout refuser) |
| `Makefile` | `test` (natif, toujours disponible), `wasm` (facultatif), `clean` |

## Pourquoi le harnais natif est honnête

Le harnais n'implémente **aucune** logique de décision : il appelle `eclat_decide` de
`build/libeclat.a`, le même moteur que le courtier utilisera sur cible. Ce qui change entre Linux
et QNX, c'est uniquement la transportation de l'appel (fonction ici, message synchrone
`MsgSend`/`MsgReply` sur cible) — jamais la décision. C'est exactement le point d'architecture du
projet : deux couches plateforme, un seul contrat.

## Règles à respecter dans le module

1. Aucune chaîne terminée par zéro : toujours **pointeur + longueur**.
2. Aucun accès direct au monde extérieur : tout passe par un import `cap_*`.
3. Aucune hypothèse sur ce qui est autorisé : un import peut toujours retourner un refus, il faut
   le traiter.
4. Moins de 500 lignes par fichier, moins de 50 lignes par fonction.

## Pour aller sur cible QNX

Il faudra : le SDP QNX 8.0 installé, la couche `platform/qnx/` (lot L7) et le courtier
`eclatd` (lot L8). Le module, lui, **ne change pas** : c'est tout l'intérêt du contrat gelé.