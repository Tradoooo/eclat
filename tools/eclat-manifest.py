#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Éclat — manifeste de module signé (ECL-07) : utilitaire en ligne de commande.

Usage :
    eclat-manifest.py create <module.wasm> --name N --caps a,b [--wcrt-us N]
                             [--version V] [--out m.json] [--key-file <fichier>]
    eclat-manifest.py verify <m.json> [<module.wasm>] [--key-file <fichier>]

`create` calcule l'empreinte sha256 du module, signe le manifeste et l'écrit
dans --out (stdout par défaut). `verify` contrôle la signature, les champs,
les capacités demandées et — si le .wasm est fourni — son empreinte ; il répond
« OK ... » (code 0) ou « KO <raison> » (code 1).

La clé est du texte brut, lu dans ECLAT_MANIFEST_KEY (ou --key-file). Aucune clé
par défaut n'est câblée en dur.
"""
import argparse
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "host"))

import keys  # noqa: E402
import manifest  # noqa: E402


def _fail(field, msg):
    sys.stderr.write("ERROR:%s:%s\n" % (field, msg))
    return 2


def _read_bytes(path):
    try:
        with open(path, "rb") as fp:
            return fp.read(), None
    except OSError as exc:
        return None, "fichier %s illisible (%s)" % (path, exc.strerror)


def _load_key(args):
    try:
        return keys.require_key(keys.MANIFEST_KEY_ENV, args.key_file), None
    except KeyError as exc:
        return None, keys.message_of(exc)


def parse_caps(spec):
    """« a,b,c » → liste triée ; refuse une capacité hors CAPS (CONTRACT.md §1)."""
    caps = [c.strip() for c in spec.split(",") if c.strip()]
    unknown = [c for c in caps if c not in manifest.CAPS]
    if unknown:
        return None, "capacité inconnue : %s (attendues : %s)" % (
            ", ".join(sorted(unknown)), ", ".join(manifest.CAPS))
    return caps, None


def cmd_create(args):
    module_bytes, err = _read_bytes(args.module)
    if err is not None:
        return _fail("module", err)
    caps, err = parse_caps(args.caps)
    if err is not None:
        return _fail("caps", err)
    key, err = _load_key(args)
    if err is not None:
        return _fail("clé", err)
    built = manifest.build_manifest(module_bytes, args.name, caps, args.wcrt_us, args.version)
    signed = manifest.sign_manifest(built, key)
    text = json.dumps(signed, sort_keys=True, indent=2) + "\n"
    if args.out:
        try:
            with open(args.out, "w", encoding="utf-8") as fp:
                fp.write(text)
        except OSError as exc:
            return _fail("out", "%s : %s" % (args.out, exc.strerror))
    else:
        sys.stdout.write(text)
    return 0


def cmd_verify(args):
    raw, err = _read_bytes(args.manifest)
    if err is not None:
        return _fail("manifeste", err)
    try:
        data = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        return _fail("manifeste", "JSON invalide (%s)" % exc)
    module_bytes = None
    if args.module:
        module_bytes, err = _read_bytes(args.module)
        if err is not None:
            return _fail("module", err)
    key, err = _load_key(args)
    if err is not None:
        return _fail("clé", err)
    ok, reason = manifest.verify_manifest(data, key, module_bytes)
    if ok:
        sys.stdout.write("OK manifeste %s v%s (%d octets, %d capacités%s)\n" % (
            data["name"], data["version"], data["size"], len(data["caps"]),
            "" if module_bytes is None else ", empreinte vérifiée"))
        return 0
    sys.stdout.write("KO %s\n" % reason)
    return 1


def build_parser():
    ap = argparse.ArgumentParser(prog="eclat-manifest.py", description="Manifeste de module signé (ECL-07).")
    ap.add_argument("--key-file", default=None, help="fichier de clé (défaut : $ECLAT_MANIFEST_KEY)")
    sub = ap.add_subparsers(dest="command", required=True)
    p_create = sub.add_parser("create", help="créer et signer le manifeste d'un module")
    p_create.add_argument("module", help="fichier .wasm")
    p_create.add_argument("--name", required=True, help="nom du module")
    p_create.add_argument("--caps", required=True, help="capacités, séparées par des virgules")
    p_create.add_argument("--wcrt-us", type=int, default=None, help="budget WCRT en µs (optionnel)")
    p_create.add_argument("--version", default="0.1", help="version du module (défaut : 0.1)")
    p_create.add_argument("--out", default=None, help="fichier de sortie (défaut : stdout)")
    p_verify = sub.add_parser("verify", help="vérifier un manifeste signé")
    p_verify.add_argument("manifest", help="fichier JSON du manifeste")
    p_verify.add_argument("module", nargs="?", default=None, help="fichier .wasm à contrôler (optionnel)")
    return ap


def main(argv=None):
    args = build_parser().parse_args(argv)
    if args.command == "create":
        return cmd_create(args)
    return cmd_verify(args)


if __name__ == "__main__":
    sys.exit(main())
