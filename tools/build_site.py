#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Éclat — génère le site statique depuis ses sources. Une seule vérité, aucune dérive.

Sources :
  site/content.json         le texte du projet (pitch, couches, mécanismes, exigences, liens)
  docs/preuves/ETAT.md      l'état des preuves (tableau, lu tel quel)
  docs/preuves/qnx800/*.txt les consoles brutes de la cible QNX 8.0

Sortie :
  site/index.html           le site généré (le gabarit est site/template.html)
  site/preuves/             copie brute des artefacts (téléchargeables depuis le site)

Usage :
  build_site.py                  régénère site/ en place
  build_site.py --out <dossier>  copie site/ puis régénère là (contrôle de dérive en CI)
"""
from __future__ import annotations

import html
import json
import pathlib
import re
import shutil
import sys

RACINE = pathlib.Path(__file__).resolve().parent.parent
SITE = RACINE / "site"
PREUVES = RACINE / "docs" / "preuves"


def inline(texte: str) -> str:
    """Échappe le HTML puis rend `code` et **gras** (sous-ensemble Markdown)."""
    t = html.escape(str(texte))
    t = re.sub(r"`([^`]+)`", r"<code>\1</code>", t)
    return re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", t)


def charge() -> dict:
    return json.loads((SITE / "content.json").read_text(encoding="utf-8"))


def bloc_stats(c: dict) -> str:
    lignes = []
    for s in c["stats"]:
        lignes.append(f'<div class="stat"><b>{inline(s["v"])}</b><span>{inline(s["l"])}</span></div>')
    return "\n".join(lignes)


def bloc_layers(c: dict) -> str:
    lignes = ["<ul>"]
    for l in c["layers"]:
        lignes.append(f'<li><b>{inline(l["t_fr"])}</b> — {inline(l["d_fr"])}</li>')
    lignes.append("</ul>")
    return "\n".join(lignes)


def bloc_mechanisms(c: dict) -> str:
    lignes = ["<ul>"]
    for m in c["mechanisms"]:
        lignes.append(f"<li>{inline(m)}</li>")
    lignes.append("</ul>")
    return "\n".join(lignes)


def bloc_requirements(c: dict) -> str:
    lignes = []
    for r in c["requirements"]:
        lignes.append(
            f'<tr><td class="id">{inline(r["id"])}</td><td><b>{inline(r["fr"])}</b></td>'
            f'<td>{inline(r["res"])}</td></tr>'
        )
    return "\n".join(lignes)


def bloc_attribution(c: dict) -> str:
    lignes = []
    for a in c["attribution"]:
        lignes.append(
            f'<tr><td>{inline(a["what"])}</td><td>{inline(a["who"])}</td>'
            f'<td><a href="{a["url"]}">{a["url"]}</a></td></tr>'
        )
    return "\n".join(lignes)


def bloc_honesty(c: dict) -> str:
    return "\n".join(f"<li>{inline(h)}</li>" for h in c["honesty"])


def bloc_liens(c: dict) -> str:
    lignes = []
    for l in c["links"]:
        u = l["u"]
        href = u if u.startswith("http") else u
        lignes.append(f'<li>{inline(l["l"])} : <a href="{href}">{inline(u)}</a></li>')
    return "\n".join(lignes)


def lignes_preuves() -> list:
    """Les lignes du tableau des preuves, lues dans ETAT.md (source unique)."""
    etat = (PREUVES / "ETAT.md").read_text(encoding="utf-8")
    out = []
    for ligne in etat.splitlines():
        if not ligne.strip().startswith("|"):
            continue
        cellules = [c.strip() for c in ligne.strip().strip("|").split("|")]
        if len(cellules) < 3:
            continue
        if cellules[0] in ("Preuve", "---") or set(cellules[0]) <= {"-", ":"}:
            continue
        out.append(cellules)
    return out


def bloc_preuves() -> str:
    """Une ligne par preuve ; l'artefact devient un lien vers la copie brute du site."""
    lignes = []
    for nom, artefact, verdict, *_ in lignes_preuves():
        brut = re.sub(r"`", "", artefact).strip()
        base = brut.rsplit("/", 1)[-1]
        lien = f'<a href="preuves/{base}"><code>{inline(brut)}</code></a>' if base else inline(artefact)
        cls = "verdict" if verdict.strip().lower() == "vert" else ""
        lignes.append(
            f"<tr><td>{inline(nom)}</td><td>{lien}</td>"
            f'<td class="{cls}">{inline(verdict)}</td></tr>'
        )
    return "\n".join(lignes)


def console(nom: str) -> str:
    """La console brute de la cible, telle quelle (artefact, pas résumé)."""
    chemin = sorted((PREUVES / "qnx800").glob(f"*{nom}.txt"))
    if not chemin:
        return "(artefact absent)"
    return html.escape(chemin[-1].read_text(encoding="utf-8").rstrip())


def copie_artefacts(cible: pathlib.Path) -> None:
    """Copie les artefacts bruts dans le site : téléchargeables, non retouchés."""
    src = cible / "preuves"
    if src.exists():
        shutil.rmtree(src)
    src.mkdir(parents=True)
    for f in sorted(PREUVES.rglob("*")):
        if f.is_file() and f.name != "ETAT.md":
            shutil.copy2(f, src / f.name)


def jetons(c: dict) -> dict:
    return {
        "__TITLE__": c["meta"]["title_fr"],
        "__DESC__": c["meta"]["description_fr"],
        "__NAME__": c["meta"]["name"],
        "__TAGLINE__": inline(c["hero"]["tagline_fr"]),
        "__TAGLINE_EN__": inline(c["hero"]["tagline_en"]),
        "__PITCH__": inline(c["hero"]["pitch_fr"]),
        "__STATS__": bloc_stats(c),
        "__LAYERS__": bloc_layers(c),
        "__MECHANISMS__": bloc_mechanisms(c),
        "__REQROWS__": bloc_requirements(c),
        "__ATTRIROWS__": bloc_attribution(c),
        "__PROOFROWS__": bloc_preuves(),
        "__CONSOLE_DEMO__": console("demonstration"),
        "__CONSOLE_TOUT__": console("module-via-courtier"),
        "__HONESTY__": bloc_honesty(c),
        "__SUBLINKS__": bloc_liens(c),
        "__GAP_EN__": inline(c["submission"]["gap_en"]),
        "__CONTRIBUTION_EN__": inline(c["submission"]["contribution_en"]),
        "__TWIN_EN__": inline(c["submission"]["twin_en"]),
    }


def generer(cible: pathlib.Path) -> None:
    c = charge()
    idx = (cible / "template.html").read_text(encoding="utf-8")
    for jeton, valeur in jetons(c).items():
        if jeton not in idx:
            raise SystemExit(f"KO marqueur introuvable dans template.html : {jeton}")
        idx = idx.replace(jeton, valeur)
    (cible / "index.html").write_text(idx, encoding="utf-8")
    copie_artefacts(cible)


def main(argv: list) -> int:
    if "--out" in argv:
        out = pathlib.Path(argv[argv.index("--out") + 1])
        if out.exists():
            shutil.rmtree(out)
        shutil.copytree(SITE, out)
        generer(out)
        print(f"OK site régénéré dans {out}")
    else:
        generer(SITE)
        print("OK site/ régénéré depuis content.json et docs/preuves/")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
