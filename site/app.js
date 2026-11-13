"use strict";
/* SPDX-License-Identifier: Apache-2.0 */
/* Éclat — site : onglets + branchement au dashboard local (la cible QNX en direct). */

(function () {
  const $ = (id) => document.getElementById(id);

  /* ——— onglets ——— */
  const nav = $("nav");
  if (nav) {
    nav.addEventListener("click", (e) => {
      const b = e.target.closest("button[data-tab]");
      if (!b) return;
      nav.querySelectorAll("button").forEach((x) => x.classList.toggle("active", x === b));
      document.querySelectorAll("main .tab").forEach((s) => {
        s.classList.toggle("active", s.id === b.dataset.tab);
      });
    });
  }

  /* ——— la cible en direct (dashboard local, http://127.0.0.1:8791) ——— */
  const LIVE = "http://127.0.0.1:8791/status.json";
  const courbe = $("courbe");
  const note = $("liveNote");
  if (!courbe) return;

  function rendu(d) {
    const etat = $("etat");
    if (etat) {
      etat.textContent = d.ok ? "EN LIGNE" : "VM arrêtée";
      etat.className = "v " + (d.ok ? "ok" : "ko");
    }
    if ($("capteur")) $("capteur").textContent = d.sensor === null ? "—" : d.sensor;
    if ($("uptime")) $("uptime").textContent = (d.uptime_ms / 1000).toFixed(1) + " s";
    if ($("ts")) $("ts").textContent = d.ts || "—";

    const hist = d.history || [];
    const cv = courbe, x = cv.getContext("2d");
    const W = cv.clientWidth || cv.width;
    const H = cv.clientHeight || cv.height;
    if (cv.width !== W) cv.width = W;
    if (cv.height !== H) cv.height = H;
    x.clearRect(0, 0, cv.width, cv.height);
    if (hist.length > 1) {
      x.strokeStyle = "#3fb950"; x.lineWidth = 2; x.beginPath();
      const min = 2000, max = 4000;
      hist.forEach((h, i) => {
        const X = (i / (hist.length - 1)) * cv.width;
        const Y = cv.height - ((h[1] - min) / (max - min)) * cv.height;
        i ? x.lineTo(X, Y) : x.moveTo(X, Y);
      });
      x.stroke();
      if (note) {
        note.className = "note ok";
        note.textContent = "Branchement actif : chaque point est une lecture réelle du capteur " +
          "via le courtier (sinusoïde 1 Hz). Console graphique : vncviewer 127.0.0.1:5901.";
      }
    }
  }

  async function maj() {
    try {
      const r = await fetch(LIVE, { cache: "no-store", mode: "cors" });
      if (!r.ok) throw new Error("statut " + r.status);
      rendu(await r.json());
    } catch (e) {
      /* hors ligne : on laisse le panneau d'aide initial visible */
    }
  }
  maj();
  setInterval(maj, 2000);
})();