# SPDX-License-Identifier: Apache-2.0
"""Éclat — console d'exploitation `eclatctl` (idée D44).

Commandes :
  policy   <policy.toml>                  valide et résume une politique
  simulate <policy.toml> <requests.txt>   rejoue des requêtes et compte les refus
  explain  <policy.toml> <cap> <id> <val> explique POURQUOI une décision est prise (7 étapes)
  budget   <cpu_ms> <calls> <now_ns> <cpu_ns> <n_calls>   état du budget

Tout est simulé côté Python via `host/capsim.py` (l'implémentation unique du contrat) : aucune
cible QNX, aucun binaire C nécessaire. Codes de sortie : 0 succès, 2 erreur d'entrée.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capsim  # noqa: E402


def load_policy(path):
    """Charge et valide une politique. Retourne (policy, None) ou (None, message)."""
    try:
        with open(path, "rb") as fp:
            data = fp.read()
    except OSError as exc:
        return None, "ERROR:policy:%s" % exc
    policy, err = capsim.parse_policy(data)
    if err is not None:
        return None, "ERROR:%s:%s" % (err[0], err[1])
    return policy, None


def cmd_policy(args):
    policy, err = load_policy(args.policy)
    if err:
        sys.stderr.write(err + "\n")
        return 2
    print("module          : %s" % policy["name"])
    print("empreinte       : %s…" % policy["sha256"].hex()[:16])
    print("cpu (ms/s)      : %s" % (policy["cpu_budget_ms_per_s"] or "illimité"))
    print("mémoire (KiB)   : %s" % (policy["mem_max_kib"] or "illimité"))
    print("appels/s        : %s" % (policy["calls_per_s"] or "illimité"))
    print("règles allow    : %d" % len(policy["allows"]))
    for rule in policy["allows"]:
        print("  - %-15s ids=%s valeur=%s" % (rule["cap"], _fmt(rule["ids"]),
                                              _fmt(rule["value_milli_range"])))
    print("règles deny     : %d%s" % (len(policy["denies"]),
                                       "".join("\n  - " + d for d in policy["denies"])))
    return 0


def _fmt(pair):
    return "libre" if pair is None else "[%d, %d]" % pair


def _parse_request(line):
    """Une requête « cap id value », « - » = absent (CONTRACT.md §6)."""
    parts = line.split()
    if not parts:
        return None
    cap = parts[0]
    id_ = -1 if len(parts) < 2 or parts[1] == "-" else int(parts[1])
    val = 0 if len(parts) < 3 or parts[2] == "-" else int(parts[2])
    return cap, id_, val


def cmd_simulate(args):
    policy, err = load_policy(args.policy)
    if err:
        sys.stderr.write(err + "\n")
        return 2
    try:
        with open(args.requests, encoding="utf-8") as fp:
            lines = fp.readlines()
    except OSError as exc:
        sys.stderr.write("ERROR:requests:%s\n" % exc)
        return 2
    counts = {}
    print("%-16s %6s %8s  %s" % ("capacité", "id", "valeur", "décision"))
    for line in lines:
        req = _parse_request(line)
        if req is None:
            continue
        cap, id_, val = req
        decision = capsim.decide(policy, cap, id_, val)
        counts[decision] = counts.get(decision, 0) + 1
        print("%-16s %6d %8d  %s" % (cap, id_, val, decision))
    total = sum(counts.values())
    print("\n%d requêtes : %s" % (total, ", ".join("%s=%d" % kv for kv in sorted(counts.items()))))
    refused = total - counts.get("CAP_OK", 0)
    print("%d refus (%.0f %%) — le défaut est de tout refuser" % (refused, 100.0 * refused / max(total, 1)))
    return 0


def cmd_explain(args):
    policy, err = load_policy(args.policy)
    if err:
        sys.stderr.write(err + "\n")
        return 2
    decision, trace = capsim.explain(policy, args.cap, args.id, args.value)
    print("requête : cap=%s id=%d valeur=%d" % (args.cap, args.id, args.value))
    print("politique : %s\n" % policy["name"])
    for step in trace:
        mark = "✗" if step["outcome"] == "stop" else "·"
        print("  %s étape %d — %-38s %s" % (mark, step["step"], step["check"], step["detail"]))
    print("\ndécision : %s" % decision)
    return 0


def cmd_budget(args):
    budget = capsim.Budget(args.cpu_ms, args.calls)
    rc = budget.charge(args.now_ns, args.cpu_ns, args.n_calls)
    verdict = {0: "dans le budget", 1: "BUDGET CPU DÉPASSÉ", 2: "QUOTA D'APPELS DÉPASSÉ"}[rc]
    print("budget cpu (ms/s) : %s" % (args.cpu_ms or "illimité"))
    print("quota appels/s    : %s" % (args.calls or "illimité"))
    print("fenêtre démarrée  : %s ns" % budget.win_start_ns)
    print("cpu consommé      : %d ns" % budget.cpu_ns_used)
    print("appels consommés  : %d" % budget.calls_used)
    print("verdict           : %s (code %d)" % (verdict, rc))
    return 0


def _absent(default):
    """Convertisseur d'argument : « - » signifie absent (CONTRACT.md §6)."""
    def convertir(tok):
        return default if tok == "-" else int(tok)
    return convertir


def main(argv):
    ap = argparse.ArgumentParser(prog="eclatctl", description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="command", required=True)

    p = sub.add_parser("policy", help="valider et résumer une politique")
    p.add_argument("policy")
    p.set_defaults(func=cmd_policy)

    s = sub.add_parser("simulate", help="rejouer des requêtes")
    s.add_argument("policy")
    s.add_argument("requests")
    s.set_defaults(func=cmd_simulate)

    e = sub.add_parser("explain", help="expliquer une décision, étape par étape")
    e.add_argument("policy")
    e.add_argument("cap")
    e.add_argument("id", type=_absent(-1), nargs="?", default=-1)
    e.add_argument("value", type=_absent(0), nargs="?", default=0)
    e.set_defaults(func=cmd_explain)

    b = sub.add_parser("budget", help="état du budget après une charge")
    b.add_argument("cpu_ms", type=int)
    b.add_argument("calls", type=int)
    b.add_argument("now_ns", type=int)
    b.add_argument("cpu_ns", type=int)
    b.add_argument("n_calls", type=int)
    b.set_defaults(func=cmd_budget)

    args = ap.parse_args(argv[1:])
    try:
        return args.func(args)
    except ValueError as exc:
        sys.stderr.write("ERROR:args:%s\n" % exc)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))