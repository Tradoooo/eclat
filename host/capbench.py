# SPDX-License-Identifier: Apache-2.0
"""Éclat — banc de latence comparatif C ↔ Python (ECL-03, ECL-12 partiel, idée B25).

Mesure le coût d'une décision par la même méthode des lots que `tools/eclat_bench.c` : mesurer
chaque décision isolément serait dominé par le coût de l'horloge. Le coût de l'horloge est mesuré
séparément et rapporté, jamais soustrait en douce.

Ces chiffres sont mesurés sur **Linux**, sans SDP : ils ne disent rien du comportement sur QNX
et ne doivent jamais être présentés comme tels.

Usage : capbench.py --bin build/bin/eclat-bench [--policy <fichier>] [--batches N] [--size N]
"""
import argparse
import json
import os
import statistics
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capsim  # noqa: E402

DEFAULT_POLICY = """[module]
name = "bench"
sha256 = "%s"
[[allow]]
cap = "read_sensor"
ids = [1, 7]
[[allow]]
cap = "write_actuator"
ids = [3]
value_milli_range = [-1000, 1000]
[[deny]]
cap = "net_send"
""" % ("ab" * 32)


def bench_python(batches, bsize):
    policy, err = capsim.parse_policy(DEFAULT_POLICY)
    if err is not None:
        raise SystemExit("politique de banc invalide : %s" % (err,))
    # Même charge de travail que tools/eclat_bench.c : cap = i % 7, id = i % 17,
    # value = (i % 2001) - 1000. Les deux bancs doivent être comparables.
    reqs = [(capsim.CAPS[i % len(capsim.CAPS)], i % 17, (i % 2001) - 1000) for i in range(bsize)]
    samples = []
    for _ in range(batches):
        t0 = time.perf_counter_ns()
        for cap, id_, val in reqs:
            capsim.decide(policy, cap, id_, val)
        samples.append(time.perf_counter_ns() - t0)
    samples.sort()
    return {
        "engine": "python",
        "batches": batches,
        "batch_size": bsize,
        "mean_ns_per_decision": statistics.fmean(samples) / bsize,
        "p50_ns": samples[batches // 2] / bsize,
        "p99_ns": samples[(batches * 99) // 100] / bsize,
        "p999_ns": samples[(batches * 999) // 1000] / bsize,
        "min_ns": samples[0] / bsize,
        "max_ns": samples[-1] / bsize,
    }


def bench_c(binary, policy_path, batches, bsize):
    if not os.path.exists(binary):
        return None
    import subprocess
    proc = subprocess.run([binary, policy_path, str(batches), str(bsize)],
                          capture_output=True, text=True)
    if proc.returncode != 0:
        raise SystemExit("eclat-bench a échoué : %s" % proc.stderr.strip())
    return json.loads(proc.stdout.strip())


def fmt(row):
    return "| %-7s | %8.1f | %8.1f | %8.1f | %8.1f |" % (
        row["engine"], row["mean_ns_per_decision"], row["p50_ns"], row["p99_ns"], row["p999_ns"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default="build/bin/eclat-bench")
    ap.add_argument("--policy", default="")
    ap.add_argument("--batches", type=int, default=200)
    ap.add_argument("--size", type=int, default=1000)
    ap.add_argument("--out", default="build/bench-report.md")
    args = ap.parse_args()

    policy_path = args.policy
    tmp = None
    if not policy_path:
        import tempfile
        tmp = tempfile.NamedTemporaryFile("w", suffix=".toml", delete=False)
        tmp.write(DEFAULT_POLICY)
        tmp.close()
        policy_path = tmp.name

    rows = []
    c_row = bench_c(args.bin, policy_path, args.batches, args.size)
    if c_row is not None:
        rows.append(c_row)
    rows.append(bench_python(args.batches, args.size))
    if tmp is not None:
        os.unlink(tmp.name)

    lines = []
    lines.append("# Banc de latence — décision de capacité")
    lines.append("")
    lines.append("Mesuré sur **Linux**, sans SDP. Ces chiffres ne concernent pas QNX.")
    lines.append("")
    lines.append("| moteur | moyenne ns | p50 ns | p99 ns | p99.9 ns |")
    lines.append("|---|---|---|---|---|")
    for row in rows:
        lines.append(fmt(row))
    lines.append("")
    lines.append("Méthode : lots de %d décisions, horloge monotone ; le coût d'un appel d'horloge "
                 "est mesuré à part et rapporté, jamais soustrait." % args.size)
    if rows and rows[0]["engine"] == "c":
        lines.append("")
        lines.append("Coût d'un `clock_gettime` isolé : %.1f ns — devant être comparé à la "
                     "moyenne par décision." % rows[0]["clock_overhead_ns"])
    lines.append("")
    lines.append("L'interpréteur Python est structurellement plus lent : c'est un témoin de "
                 "cohérence (le jumeau doit rendre le même verdict), pas une performance visée.")

    report = "\n".join(lines)
    print(report)
    if args.out:
        os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as fp:
            fp.write(report + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())