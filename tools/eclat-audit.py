#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Éclat — journal d'audit à hachage enchaîné (ECL-21) : utilitaire en ligne de commande.

Usage :
    eclat-audit.py verify <export.jsonl> [--key-file <fichier>]
    eclat-audit.py chain  <policy.toml> <requests.txt> [--key-file <fichier>]

`chain` rejoue les décisions du courtier de capacités (via `host/capsim.py`,
CONTRACT.md §3) sur une politique TOML (§2) et un fichier de requêtes
« <cap> <id> <value_milli> » (§6, « - » = sans objet) et écrit l'export JSONL
chaîné sur stdout. `verify` recalcule la chaîne et répond « OK n lignes »
(code 0) ou « KO <raison> » (code 1).

La clé de signature est du texte brut, lu dans ECLAT_AUDIT_KEY (ou dans le
fichier passé à --key-file). Aucune clé par défaut n'est câblée en dur.
"""
import argparse
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "host"))

import audit  # noqa: E402
import capsim  # noqa: E402
import keys  # noqa: E402


def _fail(field, msg):
    """Erreur fatale (convention CONTRACT.md §6 : ERROR:<champ>:<cause>)."""
    sys.stderr.write("ERROR:%s:%s\n" % (field, msg))
    return 2


def _read(path, mode="rb"):
    try:
        with open(path, mode) as fp:
            return fp.read(), None
    except OSError as exc:
        return None, "%s : %s" % (path, exc.strerror)


def read_requests(path):
    """Lit « <cap> <id> <value_milli> » par ligne ; « - » → -1 / 0 (CONTRACT.md §6)."""
    text, err = _read(path, "r")
    if err is not None:
        return None, err
    out = []
    for lineno, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) != 3:
            return None, "ligne %d : 3 champs attendus (<cap> <id> <value_milli>)" % lineno
        cap, id_tok, val_tok = parts
        try:
            id_ = -1 if id_tok == "-" else int(id_tok)
            value = 0 if val_tok == "-" else int(val_tok)
        except ValueError:
            return None, "ligne %d : id ou value_milli non entier" % lineno
        out.append((cap, id_, value))
    return out, None


def decision_record(policy, cap, id_, value):
    """Une décision du courtier, au format du journal (CONTRACT.md §5)."""
    return {
        "module": policy["name"],
        "cap": cap,
        "id": id_,
        "value_milli": value,
        "decision": capsim.decide(policy, cap, id_, value),
    }


def cmd_chain(args):
    data, err = _read(args.policy)
    if err is not None:
        return _fail("policy", err)
    policy, perr = capsim.parse_policy(data)
    if perr is not None:
        return _fail(perr[0], perr[1])
    requests, rerr = read_requests(args.requests)
    if rerr is not None:
        return _fail("requests", rerr)
    try:
        key = keys.require_key(keys.AUDIT_KEY_ENV, args.key_file)
    except KeyError as exc:
        return _fail("clé", keys.message_of(exc))
    chain = audit.AuditChain(key)
    for cap, id_, value in requests:
        chain.append(decision_record(policy, cap, id_, value))
    for line in audit.export_lines(chain):
        sys.stdout.write(line + "\n")
    return 0


def cmd_verify(args):
    text, err = _read(args.export, "r")
    if err is not None:
        return _fail("export", err)
    lines = audit.load_lines(text)
    try:
        key = keys.require_key(keys.AUDIT_KEY_ENV, args.key_file)
    except KeyError as exc:
        return _fail("clé", keys.message_of(exc))
    ok, reason = audit.verify_chain(lines, key)
    if ok:
        sys.stdout.write("OK %d lignes\n" % len(lines))
        return 0
    sys.stdout.write("KO %s\n" % reason)
    return 1


def build_parser():
    ap = argparse.ArgumentParser(prog="eclat-audit.py", description="Journal d'audit chaîné (ECL-21).")
    ap.add_argument("--key-file", default=None, help="fichier de clé (défaut : $ECLAT_AUDIT_KEY)")
    sub = ap.add_subparsers(dest="command", required=True)
    p_verify = sub.add_parser("verify", help="vérifier un export JSONL chaîné")
    p_verify.add_argument("export", help="fichier JSONL produit par « chain »")
    p_chain = sub.add_parser("chain", help="rejouer des décisions et produire un export chaîné")
    p_chain.add_argument("policy", help="politique TOML (CONTRACT.md §2)")
    p_chain.add_argument("requests", help="requêtes « <cap> <id> <value_milli> » (CONTRACT.md §6)")
    return ap


def main(argv=None):
    args = build_parser().parse_args(argv)
    if args.command == "verify":
        return cmd_verify(args)
    return cmd_chain(args)


if __name__ == "__main__":
    sys.exit(main())
