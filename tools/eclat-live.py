#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Éclat — tableau de bord temps réel : la cible QNX vue dans le navigateur.

Un mini-serveur local (bibliothèque standard uniquement) qui, toutes les 2 secondes, interroge
la VM QNX par SSH (un battement du « pouls » : uptime + capteur lus VIA le courtier), garde un
historique, et sert une page web qui se rafraîchit seule.

Usage :
    tools/eclat-live.py [--port 8787] [--host 127.0.0.1]

Puis ouvrir http://127.0.0.1:8787/ dans le navigateur.
Prérequis : la VM QNX lancée (tools/qnx-vm.sh start) et eclat-pouls déployé (tools/eclat-watch.sh).
"""
import argparse
import collections
import datetime
import json
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

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


PAGE = """<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<title>Éclat — la cible en direct</title>
<style>
 body{font-family:system-ui,sans-serif;background:#0d1117;color:#e6edf3;margin:0;padding:2rem;}
 h1{font-size:1.5rem;margin:0 0 .2rem;} .sub{color:#8b949e;margin-bottom:1.5rem;line-height:1.4;}
 .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:1rem;}
 .card{background:#161b22;border:1px solid #30363d;border-radius:10px;padding:1rem;}
 .card .k{color:#8b949e;font-size:.75rem;text-transform:uppercase;letter-spacing:.05em;}
 .card .v{font-size:1.7rem;font-weight:600;margin-top:.25rem;}
 .ok{color:#3fb950;} .ko{color:#f85149;}
 canvas{width:100%;height:190px;background:#161b22;border:1px solid #30363d;border-radius:10px;margin-top:1rem;}
 table{width:100%;margin-top:1rem;border-collapse:collapse;font-size:.85rem;}
 td,th{padding:.3rem .5rem;text-align:left;border-bottom:1px solid #30363d;}
 th{color:#8b949e;}
 .note{color:#8b949e;font-size:.85rem;margin-top:1.2rem;line-height:1.6;}
 code{background:#161b22;padding:.1rem .4rem;border-radius:4px;}
</style></head><body>
<h1>Éclat — la cible QNX en direct</h1>
<div class="sub">Un battement toutes les 2 s : l'uptime du noyau et le capteur, lus <b>via le courtier</b>
(dans un autre processus).</div>
<div class="grid">
 <div class="card"><div class="k">État</div><div class="v" id="etat">…</div></div>
 <div class="card"><div class="k">Capteur (milli-unités)</div><div class="v" id="capteur">…</div></div>
 <div class="card"><div class="k">Uptime noyau</div><div class="v" id="uptime">…</div></div>
 <div class="card"><div class="k">Dernier battement</div><div class="v" id="ts">…</div></div>
</div>
<canvas id="courbe" width="1200" height="190"></canvas>
<table id="recents"><tr><th>Heure</th><th>Capteur</th></tr></table>
<div class="note">
 Chaque point de la courbe est une lecture réelle du capteur à travers la frontière de capacité
 (sinusoïde de 1 Hz — la valeur évolue). Pour la <b>console graphique</b> de l'émulateur :
 <code>vncviewer 127.0.0.1:5901</code>.
</div>
<script>
function rendu(d){
  const etat = document.getElementById("etat");
  etat.textContent = d.ok ? "EN LIGNE" : "VM arrêtée";
  etat.style.color = d.ok ? "#3fb950" : "#f85149";
  document.getElementById("capteur").textContent = (d.sensor === null) ? "—" : d.sensor;
  document.getElementById("uptime").textContent = (d.uptime_ms/1000).toFixed(1) + " s";
  document.getElementById("ts").textContent = d.ts;
  const hist = d.history || [];
  const cv = document.getElementById("courbe"), x = cv.getContext("2d");
  x.clearRect(0,0,cv.width,cv.height);
  if (hist.length > 1) {
    x.strokeStyle = "#3fb950"; x.lineWidth = 2; x.beginPath();
    const min = 2000, max = 4000;
    hist.forEach((h,i)=>{
      const X = i/(hist.length-1)*cv.width;
      const Y = cv.height - (h[1]-min)/(max-min)*cv.height;
      i ? x.lineTo(X,Y) : x.moveTo(X,Y);
    });
    x.stroke();
  }
  let rows = "<tr><th>Heure</th><th>Capteur</th></tr>";
  hist.slice(-12).reverse().forEach(h => { rows += "<tr><td>"+h[0]+"</td><td>"+h[1]+"</td></tr>"; });
  document.getElementById("recents").innerHTML = rows;
}
async function maj(){ try { const r = await fetch("/status.json",{cache:"no-store"}); rendu(await r.json()); } catch(e){} }
maj(); setInterval(maj, 2000);
</script></body></html>"""


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
        elif self.path in ("/", "/index.html"):
            self._envoi(PAGE.encode(), "text/html; charset=utf-8")
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"404")

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
    try:
        serveur.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()