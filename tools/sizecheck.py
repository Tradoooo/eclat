# SPDX-License-Identifier: Apache-2.0
#!/usr/bin/env python3
"""sizecheck — garde de taille maison (règle user : < 500 lignes/fichier, < 50 lignes/fonction).

Vérifie les fichiers C/H du projet. Sortie 0 si tout passe, 1 sinon.
Usage : tools/sizecheck.py [racine]
"""
import os
import re
import sys

FILE_MAX = 500
FUNC_MAX = 50
ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
SUFFIXES = (".c", ".h")

fn_re = re.compile(r"^[A-Za-z_][A-Za-z0-9_ \*]*\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*$")

fail = 0


def report(p):
    global fail
    fail += 1
    print(f"FAIL {p}", file=sys.stderr)


for dirpath, dirs, files in os.walk(ROOT):
    dirs[:] = [d for d in dirs if d not in ("build", "__pycache__", ".git", ".deps", "fuzz-artifacts")]
    for f in files:
        if not f.endswith(SUFFIXES):
            continue
        path = os.path.join(dirpath, f)
        with open(path, encoding="utf-8") as fp:
            lines = fp.readlines()
        if len(lines) > FILE_MAX:
            report(f"{path}: {len(lines)} lignes > {FILE_MAX}")
        depth = 0
        fstart = 0
        for i, line in enumerate(lines):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("*") or stripped.startswith("/*"):
                continue
            if fn_re.match(stripped) and depth == 0:
                fstart = i
            depth += stripped.count("{")
            depth -= stripped.count("}")
            if depth == 0 and fstart:
                start = lines[fstart].count("{")
                if start == 0:  # ouverture sur la même ligne
                    length = i - fstart + 1
                    if length > FUNC_MAX:
                        report(f"{path}:{fstart+1}: fonction de {length} lignes > {FUNC_MAX}")
                    fstart = 0

if fail:
    print(f"sizecheck: {fail} violation(s)", file=sys.stderr)
    sys.exit(1)
print("sizecheck: OK (< %d lignes/fichier, < %d lignes/fonction)" % (FILE_MAX, FUNC_MAX))