# SPDX-License-Identifier: Apache-2.0
"""Éclat — jumeau Python indépendant du moteur C (lot L3, ECL-03).

Réimplémente la sémantique de CONTRACT.md depuis le texte, sans lire le C :
- §2 : parse + validation + normalisation d'une politique TOML (via tomllib, stdlib).
- §3 : `decide`, les 7 étapes dans l'ordre strict.
- §4 : `Budget`, fenêtre glissante d'une seconde.

C'est l'une des deux implémentations indépendantes comparées par le test différentiel.
"""
import tomllib

CAPS = ("time_ns", "log", "read_sensor", "write_actuator", "kv_get", "kv_put", "net_send")
MAX_ALLOWS = 32
MAX_DENIES = 16
NAME_MAX = 64
UINT32_MAX = 0xFFFFFFFF
WINDOW_NS = 1_000_000_000
NS_PER_MS = 1_000_000


def _err(field, msg):
    return (field, msg)


def _is_int(v):
    return isinstance(v, int) and not isinstance(v, bool)


def parse_policy(data):
    """Parse, valide et normalise une politique TOML (octets ou str).

    Retourne `(policy, None)` si valide, `(None, (field, message))` sinon. Les conventions
    d'erreur sont les mêmes que le C (CONTRACT.md §6) : champ + cause.
    """
    if isinstance(data, str):
        data = data.encode("utf-8")
    try:
        raw = tomllib.loads(data.decode("utf-8"))
    except Exception as exc:  # tomllib.TOMLDecodeError ou UnicodeError
        return None, _err("toml", str(exc))
    mod = raw.get("module")
    if not isinstance(mod, dict):
        return None, _err("module", "section [module] absente")
    policy, err = _parse_module(mod)
    if err:
        return None, err
    err = _parse_allows(raw.get("allow", []), policy)
    if err:
        return None, err
    err = _parse_denies(raw.get("deny", []), policy)
    if err:
        return None, err
    return policy, None


def _parse_module(mod):
    name = mod.get("name")
    if not isinstance(name, str) or len(name) == 0:
        return None, _err("name", "absent ou vide")
    if len(name) > NAME_MAX:
        return None, _err("name", "plus long que 64 caractères")
    sha = mod.get("sha256")
    if not isinstance(sha, str) or len(sha) != 64:
        return None, _err("sha256", "doit être une chaîne de 64 hex")
    try:
        sha_bytes = bytes.fromhex(sha)
    except ValueError:
        return None, _err("sha256", "hexadécimal invalide")
    policy = {"name": name, "sha256": sha_bytes, "allows": [], "denies": []}
    for key in ("cpu_budget_ms_per_s", "mem_max_kib", "calls_per_s"):
        val = mod.get(key, 0)
        if not _is_int(val) or val < 0 or val > UINT32_MAX:
            return None, _err(key, "entier non signé hors plage")
        policy[key] = val
    return policy, None


def _parse_allows(allows, policy):
    if not isinstance(allows, list):
        allows = [allows]
    if len(allows) > MAX_ALLOWS:
        return _err("allow", "trop de règles [[allow]]")
    for entry in allows:
        if not isinstance(entry, dict):
            return _err("allow", "entrée invalide")
        cap = entry.get("cap")
        if cap not in CAPS:
            return _err("cap", "capacité inconnue dans allow")
        rule = {"cap": cap, "ids": None, "value_milli_range": None}
        err = _parse_ids(entry, rule)
        if err:
            return err
        err = _parse_value_range(entry, rule)
        if err:
            return err
        policy["allows"].append(rule)
    return None


def _parse_ids(entry, rule):
    if "ids" not in entry:
        return None
    ids = entry["ids"]
    if _is_int(ids):
        rule["ids"] = (ids, ids)
        return None
    if isinstance(ids, list) and ids and all(_is_int(x) for x in ids):
        rule["ids"] = (min(ids), max(ids))
        return None
    return _err("ids", "entier ou tableau d'entiers non vide requis")


def _parse_value_range(entry, rule):
    if "value_milli_range" not in entry:
        return None
    vr = entry["value_milli_range"]
    if not (isinstance(vr, list) and len(vr) == 2 and all(_is_int(x) for x in vr)):
        return _err("value_milli_range", "doit être [min, max]")
    if vr[0] > vr[1]:
        return _err("value_milli_range", "min > max")
    rule["value_milli_range"] = (vr[0], vr[1])
    return None


def _parse_denies(denies, policy):
    if not isinstance(denies, list):
        denies = [denies]
    if len(denies) > MAX_DENIES:
        return _err("deny", "trop de règles [[deny]]")
    for entry in denies:
        if not isinstance(entry, dict):
            return _err("deny", "entrée invalide")
        cap = entry.get("cap")
        if cap not in CAPS:
            return _err("cap", "capacité inconnue dans deny")
        policy["denies"].append(cap)
    return None


def _note(trace, n, check, outcome, detail):
    if trace is not None:
        trace.append({"step": n, "check": check, "outcome": outcome, "detail": detail})


def _step5_ids(rule, id_, trace):
    """Étape 5 : contrainte d'id inclusive. Retourne le code bloquant ou None."""
    check = "id dans la plage autorisée ?"
    if rule["ids"] is None:
        _note(trace, 5, check, "pass", "aucune contrainte d'id")
        return None
    lo, hi = rule["ids"]
    if id_ < lo or id_ > hi:
        _note(trace, 5, check, "stop", "id=%d hors [%d, %d]" % (id_, lo, hi))
        return "CAP_DENIED_RANGE"
    _note(trace, 5, check, "pass", "id=%d dans [%d, %d]" % (id_, lo, hi))
    return None


def _step6_value(cap_name, rule, value_milli, trace):
    """Étape 6 : contrainte de valeur, write_actuator seulement."""
    check = "valeur dans la plage autorisée ?"
    if cap_name != "write_actuator" or rule["value_milli_range"] is None:
        _note(trace, 6, check, "pass", "non applicable")
        return None
    lo, hi = rule["value_milli_range"]
    if value_milli < lo or value_milli > hi:
        _note(trace, 6, check, "stop", "value=%d hors [%d, %d]" % (value_milli, lo, hi))
        return "CAP_DENIED_RANGE"
    _note(trace, 6, check, "pass", "value=%d dans [%d, %d]" % (value_milli, lo, hi))
    return None


def _decide(policy, cap_name, id_, value_milli, trace):
    """CONTRACT.md §3 : les 7 étapes dans l'ordre strict.

    Implémentation UNIQUE de l'algorithme de décision : `decide()` et `explain()` passent toutes
    deux par ici, donc la décision et son explication ne peuvent pas diverger. `trace` est `None`
    pour une décision seule (aucun dictionnaire alloué, aucun surcoût) ou une liste à remplir pour
    obtenir le détail étape par étape.
    """
    if cap_name not in CAPS:
        _note(trace, 1, "capacité connue ?", "stop", "%r n'est pas un import du contrat" % cap_name)
        return "CAP_DENIED_UNKNOWN"
    _note(trace, 1, "capacité connue ?", "pass", cap_name)

    if cap_name in policy["denies"]:
        _note(trace, 2, "interdite explicitement ?", "stop", "présente dans [[deny]]")
        return "CAP_DENIED_POLICY"
    _note(trace, 2, "interdite explicitement ?", "pass", "absente de [[deny]]")

    if cap_name in ("time_ns", "log"):
        _note(trace, 3, "facilité hôte non soumise à allow ?", "stop", cap_name)
        return "CAP_OK"
    _note(trace, 3, "facilité hôte non soumise à allow ?", "pass", "capacité soumise à politique")

    rule = next((a for a in policy["allows"] if a["cap"] == cap_name), None)
    if rule is None:
        _note(trace, 4, "une règle [[allow]] existe ?", "stop", "aucune règle pour %s" % cap_name)
        return "CAP_DENIED_POLICY"
    _note(trace, 4, "une règle [[allow]] existe ?", "pass", "règle trouvée")

    blocked = _step5_ids(rule, id_, trace)
    if blocked:
        return blocked
    blocked = _step6_value(cap_name, rule, value_milli, trace)
    if blocked:
        return blocked

    _note(trace, 7, "décision finale", "stop", "autorisé")
    return "CAP_OK"


def decide(policy, cap_name, id_, value_milli):
    """Décision seule (CONTRACT.md §3). Aucune trace construite : zéro surcoût."""
    return _decide(policy, cap_name, id_, value_milli, None)


def explain(policy, cap_name, id_, value_milli):
    """Décision **et** trace étape par étape. Retourne `(decision, trace)`.

    La dernière entrée de la trace porte `outcome == "stop"` : c'est l'étape qui a décidé. Même
    implémentation que `decide()` (`_decide`), donc les deux ne peuvent pas diverger.
    """
    trace = []
    return _decide(policy, cap_name, id_, value_milli, trace), trace


class Budget:
    """CONTRACT.md §4 : budget CPU + quota d'appels, fenêtre glissante d'une seconde."""

    def __init__(self, cpu_budget_ms_per_s=0, calls_per_s=0):
        self.cpu_budget_ms_per_s = cpu_budget_ms_per_s
        self.calls_per_s = calls_per_s
        self.win_start_ns = None
        self.cpu_ns_used = 0
        self.calls_used = 0
        self.cpu_exceeded = False
        self.calls_exceeded = False

    def charge(self, now_ns, cpu_ns, n_calls):
        """Retourne 0 (OK), 1 (CPU dépassé) ou 2 (quota dépassé, prime sur 1)."""
        if self.win_start_ns is None or now_ns - self.win_start_ns >= WINDOW_NS:
            self.win_start_ns = now_ns
            self.cpu_ns_used = 0
            self.calls_used = 0
            self.cpu_exceeded = False
            self.calls_exceeded = False
        self.cpu_ns_used += cpu_ns
        self.calls_used += n_calls
        budget_ns = self.cpu_budget_ms_per_s * NS_PER_MS
        if self.cpu_budget_ms_per_s != 0 and self.cpu_ns_used > budget_ns:
            self.cpu_exceeded = True
        if self.calls_per_s != 0 and self.calls_used > self.calls_per_s:
            self.calls_exceeded = True
        if self.calls_exceeded:
            return 2
        if self.cpu_exceeded:
            return 1
        return 0