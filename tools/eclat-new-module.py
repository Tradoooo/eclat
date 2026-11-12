# SPDX-License-Identifier: Apache-2.0
"""Éclat — SDK module tiers : `eclat new module` (idée D43, lot L11).

Génère un squelette de module prêt à tester **sur Linux, sans licence QNX** : logique métier,
ABI des imports, harnais natif branché sur le vrai moteur de décision, politique d'exemple et
Makefile.

Usage :
    python3 tools/eclat-new-module.py <nom> [--dir <répertoire>] [--list]
"""
import argparse
import os
import re
import shutil
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEMPLATE = os.path.join(REPO, "templates", "module")
NOM_VALIDE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$")
PLACEHOLDER_SHA = "0" * 64
JETONS = ("{{MODULE}}", "{{ECLAT_ROOT}}", "{{SHA256}}")


def valider_nom(nom):
    """Refuse tout ce qui pourrait sortir du répertoire cible ou casser un Makefile."""
    if not nom or nom in (".", "..") or "/" in nom or "\\" in nom:
        return "nom invalide : %r (pas de chemin, pas de « . » ni « .. »)" % nom
    if not NOM_VALIDE.match(nom):
        return ("nom invalide : %r (lettres, chiffres, « - », « _ », 64 caractères maximum)"
                % nom)
    return None


def substituer(texte, nom):
    return (texte.replace("{{MODULE}}", nom)
                 .replace("{{ECLAT_ROOT}}", REPO)
                 .replace("{{SHA256}}", PLACEHOLDER_SHA))


def generer(nom, cible):
    if os.path.exists(cible):
        return "le répertoire existe déjà : %s" % cible
    shutil.copytree(TEMPLATE, cible)
    for racine, _dirs, fichiers in os.walk(cible):
        for fichier in fichiers:
            chemin = os.path.join(racine, fichier)
            with open(chemin, encoding="utf-8") as fp:
                texte = fp.read()
            if any(j in texte for j in JETONS):
                with open(chemin, "w", encoding="utf-8") as fp:
                    fp.write(substituer(texte, nom))
    return None


def main(argv):
    ap = argparse.ArgumentParser(prog="eclat-new-module",
                                 description="Génère un squelette de module tiers Éclat.")
    ap.add_argument("nom", nargs="?", help="nom du module")
    ap.add_argument("--dir", default=".", help="répertoire parent (défaut : courant)")
    ap.add_argument("--list", action="store_true", help="liste les fichiers du gabarit")
    args = ap.parse_args(argv[1:])

    if args.list:
        for nom in sorted(os.listdir(TEMPLATE)):
            print(nom)
        return 0
    if not args.nom:
        ap.error("le nom du module est requis")

    erreur = valider_nom(args.nom)
    if erreur:
        sys.stderr.write("ERROR:nom:%s\n" % erreur)
        return 2
    if not os.path.isdir(TEMPLATE):
        sys.stderr.write("ERROR:gabarit:introuvable (%s)\n" % TEMPLATE)
        return 2

    cible = os.path.abspath(os.path.join(args.dir, args.nom))
    erreur = generer(args.nom, cible)
    if erreur:
        sys.stderr.write("ERROR:cible:%s\n" % erreur)
        return 2

    print("module « %s » créé dans %s" % (args.nom, cible))
    print("")
    print("  cd %s" % cible)
    print("  make test        # rejoue le module contre le vrai moteur de décision")
    print("")
    print("Rappel : le sha256 de policy.toml est un placeholder (%s…)." % PLACEHOLDER_SHA[:12])
    print("Remplace-le par l'empreinte réelle dès qu'un .wasm est produit.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))