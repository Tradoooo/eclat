# SPDX-License-Identifier: Apache-2.0
"""Éclat — test différentiel C ↔ Python (lot L3, ECL-03).

Génère (par graine) des politiques et des requêtes, exécute le harnais C `eclat-decide`
(CONTRACT.md §6) et compare sa sortie à l'implémentation de référence Python `capsim.decide`.
En cas de désaccord : réduction au premier désaccord (la requête fautive isolée, rejouée seule
devant le binaire C pour confirmation).

Usage : diff.py --bin <eclat-decide> --seed <n> --n <n>
"""
import argparse
import os
import random
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capsim  # noqa: E402

GATED = ("read_sensor", "write_actuator", "kv_get", "kv_put", "net_send")
UNKNOWN_CAPS = ("bogus", "no_such_cap", "")


def gen_policy(rng):
    """Construit une politique TOML valide (sous-ensemble CONTRACT.md §2)."""
    sha = "".join(rng.choice("0123456789abcdef") for _ in range(64))
    lines = ['[module]', 'name = "m%d"' % rng.randint(0, 999), 'sha256 = "%s"' % sha]
    if rng.random() < 0.7:
        lines.append("cpu_budget_ms_per_s = %d" % rng.randint(0, 500))
    if rng.random() < 0.7:
        lines.append("calls_per_s = %d" % rng.randint(0, 1000))
    for _ in range(rng.randint(0, 4)):
        cap = rng.choice(capsim.CAPS)
        lines.append("[[allow]]")
        lines.append('cap = "%s"' % cap)
        r = rng.random()
        if r < 0.4:
            lo, hi = sorted((rng.randint(-5, 20), rng.randint(-5, 20)))
            lines.append("ids = [%d, %d]" % (lo, hi))
        elif r < 0.6:
            lines.append("ids = %d" % rng.randint(-5, 20))
        if cap == "write_actuator" and rng.random() < 0.6:
            lo, hi = sorted((rng.randint(-2000, 2000), rng.randint(-2000, 2000)))
            lines.append("value_milli_range = [%d, %d]" % (lo, hi))
    for _ in range(rng.randint(0, 2)):
        lines.append("[[deny]]")
        lines.append('cap = "%s"' % rng.choice(capsim.CAPS))
    return "\n".join(lines) + "\n"


def gen_requests(rng, n):
    """Liste de (cap, id_token, val_token) ; « - » = absent (CONTRACT.md §6)."""
    out = []
    for _ in range(n):
        if rng.random() < 0.05:
            cap = rng.choice(UNKNOWN_CAPS)
        elif rng.random() < 0.3:
            cap = rng.choice(("time_ns", "log"))
        else:
            cap = rng.choice(GATED)
        id_tok = "-" if rng.random() < 0.2 else str(rng.randint(-3, 12))
        val_tok = "-" if rng.random() < 0.5 else str(rng.randint(-3000, 3000))
        out.append((cap, id_tok, val_tok))
    return out


def ref_decisions(policy, requests):
    res = []
    for cap, id_tok, val_tok in requests:
        id_ = -1 if id_tok == "-" else int(id_tok)
        val = 0 if val_tok == "-" else int(val_tok)
        res.append(capsim.decide(policy, cap, id_, val))
    return res


def run_binary(binary, policy_path, requests):
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as rf:
        for cap, id_tok, val_tok in requests:
            rf.write("%s %s %s\n" % (cap, id_tok, val_tok))
        req_path = rf.name
    try:
        proc = subprocess.run([binary, policy_path, req_path], capture_output=True, text=True)
    finally:
        os.unlink(req_path)
    if proc.returncode != 0:
        raise RuntimeError("eclat-decide rc=%d stderr=%s" % (proc.returncode, proc.stderr.strip()))
    return proc.stdout.split("\n")[:-1] if proc.stdout.endswith("\n") else proc.stdout.split("\n")


def reduce_mismatch(binary, policy_path, requests, idx):
    """Rejoue la requête fautive seule devant le C pour confirmer le désaccord minimal."""
    single = run_binary(binary, policy_path, [requests[idx]])
    return single[0] if single else "<aucune sortie>"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--n", type=int, default=10000)
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        print("SKIP binaire absent (%s)" % args.bin)
        return 0

    rng = random.Random(args.seed)
    compared = 0
    with tempfile.TemporaryDirectory() as tmp:
        policy_path = os.path.join(tmp, "policy.toml")
        for trial in range(20):
            toml = gen_policy(rng)
            policy, err = capsim.parse_policy(toml)
            if err is not None:
                print("FAIL essai %d : politique générée invalide %s" % (trial, err))
                return 1
            with open(policy_path, "w", encoding="utf-8") as fp:
                fp.write(toml)
            per = max(1, args.n // 20)
            requests = gen_requests(rng, per)
            c_out = run_binary(args.bin, policy_path, requests)
            ref = ref_decisions(policy, requests)
            if len(c_out) != len(ref):
                print("FAIL essai %d : longueur C=%d Python=%d" % (trial, len(c_out), len(ref)))
                return 1
            for i, (c_dec, py_dec) in enumerate(zip(c_out, ref)):
                compared += 1
                if c_dec != py_dec:
                    conf = reduce_mismatch(args.bin, policy_path, requests, i)
                    print("FAIL essai %d requête %d : C=%s Python=%s (rejoué seul C=%s)"
                          % (trial, i, c_dec, py_dec, conf))
                    print("  politique:\n%s" % toml)
                    print("  requête: %s" % (requests[i],))
                    return 1
    print("DIFF OK %d decisions" % compared)
    return 0


if __name__ == "__main__":
    sys.exit(main())