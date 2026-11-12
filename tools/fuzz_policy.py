# SPDX-License-Identifier: Apache-2.0
"""Éclat — fuzz du parseur de politique (ECL-08, ECL-15).

Mutateur guidé par corpus : part de politiques valides, applique 1 à 3 mutations aléatoires,
soumet le résultat au harnais C compilé sous ASan/UBSan. Un code de sortie autre que 0 (acceptée)
ou 2 (refusée) — ou une sortie ASan — est un bogue : l'entrée est conservée dans `--artifacts`.

Usage : fuzz_policy.py --bin build/bin/fuzz-harness --iterations 2000 --seed 1
"""
import argparse
import os
import random
import subprocess
import sys

SEEDS = [
    '[module]\nname = "diag"\nsha256 = "%s"\ncpu_budget_ms_per_s = 50\ncalls_per_s = 200\n'
    '[[allow]]\ncap = "read_sensor"\nids = [1, 2, 7]\n'
    '[[allow]]\ncap = "write_actuator"\nids = [3]\nvalue_milli_range = [-1000, 1000]\n'
    '[[deny]]\ncap = "net_send"\n',
    '[module]\nname = "m"\nsha256 = "%s"\n',
    '[module]\nname = "kv"\nsha256 = "%s"\n[[allow]]\ncap = "kv_get"\nids = 5\n'
    '[[allow]]\ncap = "kv_put"\n[[deny]]\ncap = "log"\n',
]
SHA = "ab" * 32
CAPS = ["time_ns", "log", "read_sensor", "write_actuator", "kv_get", "kv_put", "net_send"]


def corpus(rng):
    return [s % SHA for s in SEEDS]


def mut_flip(b, rng):
    if not b:
        return b
    i = rng.randrange(len(b))
    return b[:i] + bytes([b[i] ^ (1 << rng.randrange(8))]) + b[i + 1 :]


def mut_insert(b, rng):
    i = rng.randrange(len(b) + 1)
    return b[:i] + bytes([rng.randrange(256)]) + b[i:]


def mut_delete(b, rng):
    if not b:
        return b
    i = rng.randrange(len(b))
    return b[:i] + b[i + 1 :]


def mut_truncate(b, rng):
    if len(b) < 2:
        return b
    return b[: rng.randrange(1, len(b))]


def mut_duplicate_line(b, rng):
    lines = b.split(b"\n")
    if len(lines) < 2:
        return b
    i = rng.randrange(len(lines))
    lines.insert(i, lines[i])
    return b"\n".join(lines)


def mut_swap_lines(b, rng):
    lines = b.split(b"\n")
    if len(lines) < 2:
        return b
    i, j = rng.randrange(len(lines)), rng.randrange(len(lines))
    lines[i], lines[j] = lines[j], lines[i]
    return b"\n".join(lines)


def mut_replace_number(b, rng):
    txt = b.decode("utf-8", "replace")
    digits = [i for i, c in enumerate(txt) if c.isdigit()]
    if not digits:
        return b
    i = rng.choice(digits)
    return (txt[:i] + rng.choice(["-", "99999999999999", "0", "[", '"']) + txt[i + 1 :]).encode()


def mut_replace_token(b, rng):
    txt = b.decode("utf-8", "replace")
    tok = rng.choice(CAPS + ["[module]", "[[allow]]", "[[deny]]", "sha256", "ids", "=", '"'])
    return (txt.replace(rng.choice(CAPS), tok, 1)).encode()


def mut_drop_section_close(b, rng):
    return b.replace(b"]", b"", 1)


def mut_inject_control(b, rng):
    i = rng.randrange(len(b) + 1)
    return b[:i] + rng.choice([b"\x00", b"\n\n\n", b"\\\\", b'"']) + b[i:]


MUTATIONS = [
    mut_flip, mut_insert, mut_delete, mut_truncate, mut_duplicate_line,
    mut_swap_lines, mut_replace_number, mut_replace_token, mut_drop_section_close,
    mut_inject_control,
]


def run_one(binary, path, timeout=10):
    try:
        proc = subprocess.run([binary, path], capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return -99, "timeout"
    blob = (proc.stdout or "") + (proc.stderr or "")
    if "runtime error" in blob or "AddressSanitizer" in blob or "SUMMARY:" in blob:
        return -98, blob[:400]
    return proc.returncode, blob[:200]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--iterations", type=int, default=2000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--artifacts", default="fuzz-artifacts")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        print("SKIP binaire de fuzz absent (%s)" % args.bin)
        return 0

    rng = random.Random(args.seed)
    seeds = corpus(rng)
    os.makedirs(args.artifacts, exist_ok=True)
    tmp = os.path.join(args.artifacts, "current.toml")
    stats = {"ok": 0, "rejected": 0, "crash": 0}
    crashes = []

    for it in range(args.iterations):
        data = rng.choice(seeds).encode()
        for _ in range(rng.randint(1, 3)):
            data = rng.choice(MUTATIONS)(data, rng)
        with open(tmp, "wb") as fp:
            fp.write(data)
        rc, blob = run_one(args.bin, tmp)
        if rc == 0:
            stats["ok"] += 1
        elif rc == 2:
            stats["rejected"] += 1
        else:
            stats["crash"] += 1
            art = os.path.join(args.artifacts, "crash-%05d.toml" % it)
            with open(art, "wb") as fp:
                fp.write(data)
            crashes.append((art, rc, blob))

    print("FUZZ %d entrées : acceptées=%d refusées=%d plantages=%d"
          % (args.iterations, stats["ok"], stats["rejected"], stats["crash"]))
    if crashes:
        for art, rc, blob in crashes[:5]:
            print("  CRASH rc=%s %s\n    %s" % (rc, art, blob.replace("\n", " ")[:200]))
        return 1
    if stats["ok"] + stats["rejected"] != args.iterations:
        print("FUZZ KO : entrées non classées")
        return 1
    print("FUZZ OK : aucun plantage, aucune erreur ASan/UBSan")
    return 0


if __name__ == "__main__":
    sys.exit(main())