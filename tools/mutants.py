# SPDX-License-Identifier: Apache-2.0
"""Éclat — test de mutants du cœur C (ECL-15, plan de vérification §7.5).

Chaque mutant est une régression volontaire et minimale injectée dans `core/src/`. Un mutant est
**tué** si la suite de tests C échoue sur le code muté. Un mutant **survivant** signifie que les
tests ne couvrent pas ce comportement : c'est un trou réel dans la suite, pas un faux positif.

Le travail se fait sur une copie du dépôt dans un répertoire temporaire : l'arbre de travail n'est
jamais modifié.

Usage : mutants.py [--only <substr>] [--keep] [--timeout <s>]
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IGNORE = shutil.ignore_patterns(".git", ".deps", "build", "fuzz-artifacts", "__pycache__", "*.o", "*.a")

# (nom, fichier relatif, fragment exact, remplacement) — chaque fragment doit être unique.
MUTANTS = [
    ("dec-etape1-cap-inconnu-accepte", "core/src/decision.c",
     "(unsigned)req->cap >= (unsigned)ECLAT_CAP_COUNT", "0"),
    ("dec-etape2-deny-ignore", "core/src/decision.c",
     "if (is_denied(policy, req->cap)) {", "if (0) {"),
    ("dec-etape3-facilites-hote-supprimees", "core/src/decision.c",
     "req->cap == ECLAT_CAP_TIME_NS || req->cap == ECLAT_CAP_LOG", "0"),
    ("dec-etape4-premiere-regle-allow-fausse", "core/src/decision.c",
     "if (policy->allows[i].cap == cap) {", "if (policy->allows[i].cap != cap) {"),
    ("dec-etape5-plage-id-desactivee", "core/src/decision.c",
     "return rule->has_ids && (id < rule->id_min || id > rule->id_max);", "return 0;"),
    ("dec-etape5-borne-basse-exclusive", "core/src/decision.c",
     "id < rule->id_min", "id <= rule->id_min"),
    ("dec-etape5-borne-haute-exclusive", "core/src/decision.c",
     "id > rule->id_max", "id >= rule->id_max"),
    ("dec-etape6-plage-valeur-desactivee", "core/src/decision.c",
     "return rule->has_value_range && (value_milli < rule->val_min || value_milli > rule->val_max);",
     "return 0;"),
    ("dec-etape6-borne-basse-exclusive", "core/src/decision.c",
     "value_milli < rule->val_min", "value_milli <= rule->val_min"),
    ("dec-etape6-jamais-appliquee", "core/src/decision.c",
     "if (req->cap == ECLAT_CAP_WRITE_ACTUATOR && value_out_of_range(rule, req->value_milli)) {",
     "if (0) {"),
    ("dec-etape7-defaut-inverse", "core/src/decision.c",
     "return ECLAT_CAP_OK;\n}", "return ECLAT_CAP_DENIED_POLICY;\n}"),
    ("bud-cpu-borne-exclusive", "core/src/budget.c",
     "b->cpu_ns_used > cpu_budget_ns", "b->cpu_ns_used >= cpu_budget_ns"),
    ("bud-calls-borne-exclusive", "core/src/budget.c",
     "b->calls_used > b->calls_per_s", "b->calls_used >= b->calls_per_s"),
    ("bud-quota-ne-prime-plus", "core/src/budget.c",
     "return 2; /* le quota d'appels prime */", "return 1; /* le quota d'appels prime */"),
    ("bud-fenetre-hors-borne", "core/src/budget.c",
     ">= ECLAT_BUDGET_WINDOW_NS", "> ECLAT_BUDGET_WINDOW_NS"),
    ("log-decision-toujours-ok", "core/src/log.c",
     "const char *dec_name = eclat_decision_name(d);",
     "const char *dec_name = eclat_decision_name(ECLAT_CAP_OK);"),
    ("pol-sha256-jamais-present", "core/src/policy.c", "*have_sha = 1;", "*have_sha = 0;"),
    ("pol-name-toujours-vide", "core/src/policy.c", "out->name_len = nlen;", "out->name_len = 0;"),
    ("pol-validate-plage-id-ignoree", "core/src/policy.c",
     "if (r->has_ids && r->id_min > r->id_max) {", "if (0) {"),
    ("pol-sha256-jamais-verifie", "core/src/policy.c",
     "return eclat_sha256_from_hex(buf, blen, out);", "return 0;"),
    ("toml-tableau-non-ferme-accepte", "core/src/toml.c",
     "if (v.n < 2 || v.p[0] != '[' || v.p[v.n - 1] != ']') {", "if (v.n < 2) {"),
    ("names-code-decision-constant", "core/src/names.c",
     "return DECISION_NAMES[d];", "return DECISION_NAMES[0];"),
]


def snapshot(dest):
    shutil.copytree(REPO, dest, ignore=IGNORE, dirs_exist_ok=True)


def run_tests(root, timeout):
    proc = subprocess.run(["make", "test"], cwd=root, capture_output=True, text=True,
                          timeout=timeout)
    return proc.returncode, (proc.stdout + proc.stderr)[-600:]


def apply_mutant(root, relpath, old, new):
    path = os.path.join(root, relpath)
    with open(path, encoding="utf-8") as fp:
        src = fp.read()
    n = src.count(old)
    if n != 1:
        return False, "fragment présent %d fois (attendu 1) dans %s" % (n, relpath)
    with open(path, "w", encoding="utf-8") as fp:
        fp.write(src.replace(old, new, 1))
    return True, ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--timeout", type=int, default=180)
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()

    workdir = tempfile.mkdtemp(prefix="eclat-mutants-")
    try:
        base = os.path.join(workdir, "baseline")
        snapshot(base)
        rc, out = run_tests(base, args.timeout)
        if rc != 0:
            print("MUTANTS KO : la suite de base échoue, impossible de juger les mutants")
            print(out)
            return 1
        print("baseline : make test vert")

        killed, survived, broken = [], [], []
        for name, relpath, old, new in MUTANTS:
            if args.only and args.only not in name:
                continue
            root = os.path.join(workdir, name)
            snapshot(root)
            ok, why = apply_mutant(root, relpath, old, new)
            if not ok:
                broken.append((name, why))
                print("  CONFIG %s : %s" % (name, why))
                continue
            try:
                rc, _ = run_tests(root, args.timeout)
            except subprocess.TimeoutExpired:
                rc = -1
            if rc != 0:
                killed.append(name)
                print("  tué      %s" % name)
            else:
                survived.append(name)
                print("  SURVIVANT %s  ← les tests ne couvrent pas ce comportement" % name)
            if not args.keep:
                shutil.rmtree(root, ignore_errors=True)

        total = len(killed) + len(survived)
        print("mutants : %d tués / %d (survivants : %d, config cassée : %d)"
              % (len(killed), total, len(survived), len(broken)))
        if survived or broken:
            return 1
        if total == 0:
            print("MUTANTS KO : aucun mutant exécuté")
            return 1
        print("MUTANTS OK : taux de détection 100 %")
        return 0
    finally:
        if not args.keep:
            shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())