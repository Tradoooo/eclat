# SPDX-License-Identifier: Apache-2.0
#!/usr/bin/env python3
"""spdx_guard — vérifie la présence d'un identifiant SPDX en tête de chaque fichier source.

Usage : tools/spdx_guard.py [racine]
"""
import os
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
SUFFIXES = (".c", ".h", ".py")
HEAD = 4
fail = 0

for dirpath, dirs, files in os.walk(ROOT):
    dirs[:] = [d for d in dirs if d not in ("build", "__pycache__", ".git", ".deps", "fuzz-artifacts")]
    for f in files:
        if not f.endswith(SUFFIXES):
            continue
        path = os.path.join(dirpath, f)
        with open(path, encoding="utf-8") as fp:
            top = "".join(fp.readline() for _ in range(HEAD))
        if "SPDX-License-Identifier" not in top:
            print(f"FAIL {path}: SPDX absent des {HEAD} premières lignes", file=sys.stderr)
            fail += 1

if fail:
    print(f"spdx_guard: {fail} fichier(s) sans SPDX", file=sys.stderr)
    sys.exit(1)
print("spdx_guard: OK")