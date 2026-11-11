# CONTRACT — Éclat (contrat gelé)

Ce document est **authoritative** : en cas de désaccord entre lui et n'importe quel autre
fichier, c'est lui qui fait foi. Toute l'implémentation (`core/`, `host/`, `platform/`,
`broker/`) s'écrit contre ce contrat.

Gelé le 11 novembre 2026. Toute modification doit être explicitement datée et motivée.

---

## 1. ABI des imports (signature figée)

Les imports sont exposés au module WebAssembly par l'hôte. Aucune chaîne terminée par zéro :
toute chaîne est un couple `(pointeur, longueur)`. Toute longueur est vérifiée contre la taille
du tampon déclaré **avant** copie. Tout import inconnu est refusé **à l'instantiation**, jamais
à l'exécution.

| Import | Signature (wasm) | Sémantique |
|---|---|---|
| `cap_time_ns` | `() -> i64` | Instant monotone en nanosecondes, fourni par l'hôte |
| `cap_log` | `(ptr:i32, len:i32, level:i32) -> i32` | Journalisation ; retourne un code d'erreur (`CAP_OK` en cas de succès) |
| `cap_read_sensor` | `(id:i32, out_ptr:i32, out_len:i32) -> i32` | Lecture soumise à politique ; remplit un tampon borné |
| `cap_write_actuator` | `(id:i32, value_milli:i32) -> i32` | Écriture soumise à politique, valeur entière en milli-unités |
| `cap_kv_get` / `cap_kv_put` | `(key_ptr:i32, key_len:i32, val_ptr:i32, val_len:i32) -> i32` | Stockage clés-valeurs borné |
| `cap_net_send` | `(chan:i32, ptr:i32, len:i32) -> i32` | Envoi sur un canal déclaré, soumis à politique |

Ordre des énumérés (`eclat_cap_t`) :

```
0 ECLAT_CAP_TIME_NS
1 ECLAT_CAP_LOG
2 ECLAT_CAP_READ_SENSOR
3 ECLAT_CAP_WRITE_ACTUATOR
4 ECLAT_CAP_KV_GET
5 ECLAT_CAP_KV_PUT
6 ECLAT_CAP_NET_SEND
7 ECLAT_CAP_COUNT
```

Noms textuels (utilisés en TOML et dans le journal) : `time_ns`, `log`, `read_sensor`,
`write_actuator`, `kv_get`, `kv_put`, `net_send`.

---

## 2. Format de politique (TOML restreint)

Le parseur C accepte un **sous-ensemble** de TOML (documenté ci-dessous). Le jumeau Python
utilise `tomllib` (bibliothèque standard) et restreint les mêmes champs : les deux doivent donc
accepter exactement les mêmes documents sur ce sous-ensemble.

```toml
[module]
name = "diag-partner"        # nom du module (≤ 64 caractères)
sha256 = "…"                 # empreinte obligatoire du .wasm (64 hex)
cpu_budget_ms_per_s = 50     # budget CPU par seconde glissante
mem_max_kib = 2048
calls_per_s = 200

[[allow]]
cap = "read_sensor"
ids = [1, 2, 7]              # ou un entier seul, ou absent (pas de contrainte d'id)

[[allow]]
cap = "write_actuator"
ids = [3]
value_milli_range = [-1000, 1000]

[[deny]]
cap = "net_send"
```

Ceci est du **TOML standard** : le jumeau Python le lit avec `tomllib` (bibliothèque
standard). Le parseur C en accepte le même sous-ensemble.

Règles du sous-ensemble accepté par le parseur C :

- Sections `[module]` (une seule, obligatoire) et tableaux de tables `[[allow]]` /
  `[[deny]]` (zéro ou plusieurs).
- Clés du module : `name` (chaîne, obligatoire, ≤ 64), `sha256` (chaîne, 64 hex, obligatoire),
  `cpu_budget_ms_per_s`, `mem_max_kib`, `calls_per_s` (entiers non signés).
- `cap` : une chaîne parmi les 7 noms textuels (identifiant inconnu → erreur de validation).
- `ids` : un entier, ou un tableau d'entiers (déclaré comme plage inclusive [min, max]).
- `value_milli_range` : tableau de **deux** entiers, min puis max.
- Commentaires `#`, chaînes entre guillemets droits `"…"` (échappements `\"`, `\\`, `\n`, `\t`),
  valeurs entières décimales éventuellement négatives (`-1000`).
- Toute clé inconnue est **ignorée** (tolérance) mais une valeur mal formée est une **erreur**.

Champs requis : `name`, `sha256`. Les trois budgets (`cpu_budget_ms_per_s`, `mem_max_kib`,
`calls_per_s`) ont une valeur par défaut si absents, respectivement `0`, `0`, `0` — un budget à
`0` signifie « non limité ». `sha256` absent ou mal formé (pas exactement 32 octets ↔ 64 hex)
est une **erreur fatale de validation**.

Une politique invalide est refusée avec un champ et une cause. Le refus par défaut est
**toujours** appliqué : une capacité non listée en `allow` est refusée.

---

## 3. Codes de décision

```
CAP_OK = 0
CAP_DENIED_POLICY
CAP_DENIED_RANGE
CAP_DENIED_BUDGET
CAP_DENIED_UNKNOWN
CAP_BAD_ARG
CAP_KILLED_CPU
CAP_KILLED_MEM
```

`eclat_decide` (voir `core/include/eclat/cap_decision.h`) ne produit que `CAP_OK`,
`CAP_DENIED_POLICY`, `CAP_DENIED_RANGE` et `CAP_DENIED_UNKNOWN` :

1. `cap` hors de `[0, ECLAT_CAP_COUNT)` → `CAP_DENIED_UNKNOWN`.
2. `cap` présent dans `policy->denies` → `CAP_DENIED_POLICY`.
3. `cap` égal à `TIME_NS` ou `LOG` → `CAP_OK` (facilités hôte non soumises à la liste `allow`).
4. Aucune règle `allow` dont `rule.cap == cap` → `CAP_DENIED_POLICY`.
5. La règle porte une contrainte d'id (`has_ids`) et `id` est hors de `[id_min, id_max]` →
   `CAP_DENIED_RANGE`.
6. `cap == WRITE_ACTUATOR`, la règle porte une contrainte de valeur (`has_value_range`) et
   `value_milli` est hors de `[val_min, val_max]` → `CAP_DENIED_RANGE`.
7. Sinon → `CAP_OK`.

L'ordre est strictement du plus prioritaire (1) au moins prioritaire (7). Le jumeau Python
implémente **exactement** ces sept étapes dans cet ordre.

Les codes `CAP_DENIED_BUDGET`, `CAP_KILLED_CPU`, `CAP_KILLED_MEM` et `CAP_BAD_ARG` sont
produits par l'**hôte** (pas par `eclat_decide`) : le budget est vérifié autour de l'appel, et
les arguments de l'ABI (longueur hors tampon) sont vérifiés au moment de la copie.

## 4. Budget (fenêtre glissante d'une seconde)

- `cpu_budget_ms_per_s` : CPU maximale par fenêtre. `cpu_ns_used` est la somme du temps CPU
  compté dans la fenêtre courante. `budget = cpu_budget_ms_per_s * 1 000 000` nanosecondes.
- `calls_per_s` : nombre d'appels par fenêtre. `0` (ou `cpu_budget_ms_per_s == 0`) = illimité.
- La fenêtre dure `1 000 000 000` ns. Quand `now_ns - win_start_ns >= 1e9`, la fenêtre repart :
  `win_start_ns = now_ns`, compteurs remis à zéro, drapeaux d'excès remis à zéro.
- `eclat_budget_charge` (voir `core/include/eclat/cap_budget.h`) compte le temps puis vérifie :
  retour `0` si OK, `1` si le budget CPU est dépassé, `2` si le quota d'appels est dépassé.
  Les drapeaux `cpu_exceeded`/`calls_exceeded` restent levés jusqu'à la fin de la fenêtre.

## 5. Journal des décisions

Une ligne par **refus ou dépassement** (les décisions accordées ne sont journalisées qu'en mode
audit). Format cible : JSON d'une ligne :

```json
{"seq":12,"module":"diag-partner","cap":"write_actuator","decision":"CAP_DENIED_RANGE"}
```

`core/log.c` fournit `eclat_log_format_decision` qui écrit cette ligne dans un tampon fourni
par l'appelant (aucune allocation, aucun état global). L'émission réelle (fichier, socket) est
le travail de l'hôte, jamais de `core/`.

## 6. Harnais différentiel (`tools/eclat-decide`)

Le binaire `eclat-decide` est l'interface entre le moteur C et le jumeau Python. Contrat figé :

```
Usage : eclat-decide <policy.toml> <requests.txt>
policy.toml  : politique au format §2 (TOML restreint).
requests.txt : une requête par ligne, 3 champs séparés par des espaces :
    <cap_name> <id> <value_milli>
    cap_name ∈ {time_ns, log, read_sensor, write_actuator, kv_get, kv_put, net_send}
    id         : entier décimal, ou "-" si sans objet (→ -1)
    value_milli: entier décimal, ou "-" si sans objet (→ 0)
Sortie : une ligne par requête, le nom du code de décision
    (CAP_OK / CAP_DENIED_POLICY / CAP_DENIED_RANGE / CAP_DENIED_UNKNOWN).
Politique invalide : stderr "ERROR:<field>:<message>" et code de sortie 2.
```

Le jumeau Python (`host/diff.py`) génère des couples (policy, requests), exécute ce binaire et
compare sa sortie à sa propre implémentation de référence de `eclat_decide` (même algorithme,
§3). En cas de désaccord, réduction dichotomique sur la liste des requêtes pour isoler le
premier écart.

## 7. Règles de portabilité (rappel dur)

- `core/` est du C11 **pur** : ni horloge, ni fichier, ni socket, ni allocation, ni état global
  mutable. Les entrées/sorties (heure, fichiers) sont injectées par l'appelant.
- `platform/posix/` et `platform/qnx/` partagent le **même contrat d'interface** (une seule
  déclaration d'interface, deux implémentations). Jamais de code copié d'une couche à l'autre.
- Moins de 500 lignes par fichier, moins de 50 lignes par fonction.