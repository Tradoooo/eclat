#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Éclat — le site servi en local, avec la cible QNX en direct.

Un mini-serveur local (bibliothèque standard uniquement) qui, toutes les 2 secondes, interroge
la VM QNX par SSH (un battement du « pouls » : uptime + capteur lus VIA le courtier), garde un
historique, et sert **le site lui-même** — le même `site/index.html` que GitHub Pages publie.

Pourquoi le site est servi ici aussi : GitHub Pages est en HTTPS public, et un navigateur
moderne refuse qu'une origine publique interroge `127.0.0.1` (Local Network Access). La seule
façon d'avoir le projet ET la courbe vivante sur une seule page est donc de servir le site
depuis la machine qui porte la VM.

Usage :
    tools/eclat-live.py [--port 8791] [--host 127.0.0.1]

Puis ouvrir http://127.0.0.1:8791/ dans le navigateur.
Prérequis : la VM QNX lancée (tools/qnx-vm.sh start) et eclat-pouls déployé (tools/eclat-watch.sh).
"""
import argparse
import collections
import datetime
import json
import mimetypes
import pathlib
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

RACINE = pathlib.Path(__file__).resolve().parent.parent
SITE = RACINE / "site"

DEFAUT = {"ok": False, "uptime_ms": 0, "sensor": None, "ts": ""}
DERNIER = dict(DEFAUT)
HISTORIQUE = collections.deque(maxlen=400)
VERROU = threading.Lock()


def un_battement():
    """Un battement du pouls sur la cible, renvoyé en dict."""
    ssh = [
        "ssh", "-i", "~/.ssh/id_ed25519_qnx_eclat",
        "-p", "2280",
        "-o", "StrictHostKeyChecking=no", "-o", "UserKnownHostsFile=/dev/null",
        "-o", "ConnectTimeout=4",
        "root@127.0.0.1", "cd /tmp/eclat-broker && ./eclat-pouls json once",
    ]
    try:
        out = subprocess.run(ssh, capture_output=True, text=True, timeout=10).stdout.strip()
        for ligne in reversed(out.splitlines()):
            ligne = ligne.strip()
            if ligne.startswith("{"):
                d = json.loads(ligne)
                d["ts"] = datetime.datetime.now().strftime("%H:%M:%S")
                return d
    except Exception:
        pass
    return dict(DEFAUT, ts=datetime.datetime.now().strftime("%H:%M:%S"))


def sondeur():
    global DERNIER
    while True:
        b = un_battement()
        with VERROU:
            DERNIER = b
            if b.get("ok"):
                HISTORIQUE.append((b["ts"], b["sensor"]))
        time.sleep(2)


TYPE_MIME = {".html": "text/html; charset=utf-8", ".css": "text/css; charset=utf-8",
             ".js": "text/javascript; charset=utf-8", ".json": "application/json",
             ".txt": "text/plain; charset=utf-8", ".svg": "image/svg+xml"}


def fichier_statique(chemin: str):
    """Résout une URL vers un fichier de site/ — jamais hors de site/."""
    rel = chemin.lstrip("/") or "index.html"
    cible = (SITE / rel).resolve()
    if SITE.resolve() not in cible.parents and cible != SITE.resolve():
        return None
    if cible.is_dir():
        cible = cible / "index.html"
    return cible if cible.is_file() else None


class Requete(BaseHTTPRequestHandler):
    def _envoi(self, corps, ctype):
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        # Le site statique (https://tradoooo.github.io/eclat/) lit ce flux en direct :
        # CORS ouvert + Private Network Access (le navigateur exige cet en-tête quand
        # une page publique appelle un hôte du réseau privé).
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Private-Network", "true")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()
        self.wfile.write(corps)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Private-Network", "true")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Max-Age", "600")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self):
        if self.path == "/status.json":
            with VERROU:
                d = dict(DERNIER)
                d["history"] = list(HISTORIQUE)
            self._envoi(json.dumps(d).encode(), "application/json")
            return
        chemin = self.path.split("?", 1)[0]
        cible = fichier_statique(chemin)
        if cible is None:
            self.send_response(404)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.end_headers()
            self.wfile.write(b"404")
            return
        ctype = TYPE_MIME.get(cible.suffix, "application/octet-stream")
        self._envoi(cible.read_bytes(), ctype)

    def log_message(self, *_):
        pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8791)
    ap.add_argument("--host", default="127.0.0.1")
    args = ap.parse_args()

    global DERNIER
    b = un_battement()
    with VERROU:
        DERNIER = b
        if b.get("ok"):
            HISTORIQUE.append((b["ts"], b["sensor"]))

    threading.Thread(target=sondeur, daemon=True).start()
    serveur = ThreadingHTTPServer((args.host, args.port), Requete)
    print(f"Éclat en direct : http://{args.host}:{args.port}/  (Ctrl-C pour cesser)")
    print("Le site servi ici est le même que celui de GitHub Pages — avec la courbe vivante.")
    try:
        serveur.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()