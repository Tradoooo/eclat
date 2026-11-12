# Éclat — video script (3 minutes, English)

Target: **exactly 180 seconds**. Every number on screen is taken verbatim from the raw console
artefacts in `docs/preuves/qnx800/` — nothing is invented. A short French recap follows the script.

Legend: **SCREEN** = what is filmed (real command / real console capture). **VO** = voice-over.
**SRC** = the raw artefact the shot must match, in `docs/preuves/qnx800/` — nothing is retyped
by hand, nothing is invented.

| Act | Time | Length | Beats |
|---|---|---|---|
| 1 — the problem, in one image | 0:00–0:20 | 20 s | 0:00–0:10 · 0:10–0:20 |
| 2 — the contribution: the broker | 0:20–0:50 | 30 s | 0:20–0:35 · 0:35–0:50 |
| 3 — the hostile module, under WAMR | 0:50–1:35 | 45 s | 0:50–1:05 · 1:05–1:20 · 1:20–1:35 |
| 4 — the 1 kHz loop keeps running | 1:35–2:10 | 35 s | 1:35–1:50 · 1:50–2:10 |
| 5 — verified, not just demoed | 2:10–3:00 | 50 s | 2:10–2:30 · 2:30–2:50 · 2:50–3:00 |
| **Total** | | **180 s** | 12 beats, each 10–20 s |

---

## Act 1 — the problem, in one image (0:00–0:20)

### 0:00–0:10
**SCREEN** — title card: "Éclat — a capability broker for WebAssembly modules on QNX 8.0".
Under it, one line: "runs a module · decides every access · the control loop never blinks".
**VO** — "You want to run untrusted code on a safety-critical QNX device. A partner's module,
a vendor's algorithm. Today that costs months of certification. This is our answer."

### 0:10–0:20
**SCREEN** — the official QNX ports index (`qnx-ports/build-files/ports`): the list scrolls, no
`wasm`, no `wamr`, no `wasi`. Then the curl result: `GET /repos/qnx-ports/wamr → 404`.
**SRC** — `docs/E5-preuves-absence.md` (rows 1–3 of the table: index read, 404, zero search hits).
**VO** — "When we started, no WebAssembly runtime existed in the official QNX ports index. And in
full honesty: someone has just proposed one upstream, pull request 5200. So our contribution is
not the port. It is the layer on top — the broker."

---

## Act 2 — the contribution: the broker (0:20–0:50)

### 0:20–0:35
**SCREEN** — terminal on the target. Prompt runs `./eclat-broker pol-broker.toml`, then shows:
`courtier: politique « broker-demo » chargée (2 règles allow, 0 règles deny)` and
`courtier: à l'écoute sur cap (canal 1)`.
**SRC** — `docs/preuves/qnx800/2026-11-12-courtier.txt`, section « Démarrage du courtier ».
**VO** — "Meet `eclat-broker`. It loads a policy — two allow rules, zero deny — and it listens
as a first-class QNX resource manager, reachable by name."

### 0:35–0:50
**SCREEN** — `./eclat-client` runs. Lines appear one by one: `cap_read_sensor(id=1) t0 → CAP_OK
(payload=2257)` … `t0+250ms → 3673` … `t0+500ms → 3732` … `t0+750ms → 2317`. Then the denials:
`net_send → CAP_DENIED_POLICY`, `write_actuator(99999) → CAP_DENIED_RANGE`,
`read_sensor(id=42) → CAP_DENIED_RANGE`, `kv_put → CAP_DENIED_POLICY`.
**SRC** — `docs/preuves/qnx800/2026-11-12-courtier.txt`, section « Le client interroge le courtier ».
**VO** — "The client asks, the broker decides. The authorized sensor reads are granted — and
notice the values move, that's a real live signal. Everything else is refused, by policy or by
range. The default is: deny."

---

## Act 3 — the hostile module, under WAMR (0:50–1:35)

### 0:50–1:05
**SCREEN** — `eclat-run` under WAMR. First the honest module (`honnete.wasm`): `appels au courtier :
4 · refus : 0 · code=0`. Then `eclat-run hostile.wasm --broker cap`, the hostile module, and four
JSON lines scroll: `{"seq":1,"module":"via-broker","cap":"net_send","decision":"CAP_DENIED_POLICY"}`,
then `write_actuator → CAP_DENIED_RANGE`, `read_sensor → CAP_DENIED_RANGE`,
`kv_put → CAP_DENIED_POLICY`. Bilan: `appels au courtier : 6 · refus : 4`, `code=0`.
**SRC** — `docs/preuves/qnx800/2026-11-12-module-via-courtier.txt` (the refusals are decided by
the broker, in **another process**).
**VO** — "Now the real thing: a hostile Wasm module, executed inside WAMR. It tries four
forbidden things. Every single one is refused, in JSON, by the broker — and the module survives,
it simply gets nothing. Not a mock-up: this is the QNX console."

### 1:05–1:20
**SCREEN** — `eclat-run` on `falsifie.wasm`. One line: `REFUSÉ (-2) : empreinte du module
différente de la politique (32e7e8db3f1118cf… )`, then `code du module : -1`.
**SRC** — `docs/preuves/qnx800/2026-11-12-demonstration.txt`, section « ECL-07 ».
**VO** — "And if someone smuggles a different module? One byte changed is a different digest, and
the loader refuses to run it. Policy and hash must match, or nothing loads."

### 1:20–1:35
**SCREEN** — diagram: three stacked layers — Wasm sandbox / a QNX process with no abilities / the
kernel policy — with the broker as the single exit point on the side.
**VO** — "That is the argument: three independent layers. The WebAssembly sandbox, a QNX process
with no abilities, and one choke point — the broker. Break one layer and the next still holds."

---

## Act 4 — the 1 kHz loop keeps running (1:35–2:10)

### 1:35–1:50
**SCREEN** — `./eclat-boucle 3000` runs. Output: `boucle 1 kHz : 3000 tics, 2999 mesures de
gigue`. The sensor reads keep changing (a 1 Hz sinus around 3000, amplitude 1000).
**SRC** — `docs/preuves/qnx800/2026-11-12-boucle-1khz.txt`, section « 1. baseline ».
**VO** — "A control loop ticks at one kilohertz and reads a sensor that is actually moving. Three
thousand ticks, measured. This loop is the stand-in for your real-time controller."

### 1:50–2:10
**SCREEN** — the greedy module is launched mid-measure, then killed: `module tué par le
superviseur : budget CPU de 300 ms dépassé`. Three jitter columns stay side by side: baseline,
during the kill, after — all show `3000 tics`. A caption states the loops never stopped.
**SRC** — `docs/preuves/qnx800/2026-11-12-boucle-1khz.txt`, sections 2 and 3 (the three
`boucle 1 kHz : 3000 tics` lines); the kill line itself comes from
`docs/preuves/qnx800/2026-11-12-demonstration.txt`, section « ECL-06 ».
**VO** — "Now we load a greedy module whose budget is three hundred milliseconds. The supervisor
kills it — and the loop never stops ticking. To be honest: absolute jitter here is dominated by
QEMU. What we prove is not raw performance — it is that the loop stays alive."

---

## Act 5 — verified, not just demoed (2:10–3:00)

### 2:10–2:30
**SCREEN** — proof cards: `DIFF OK 10000 decisions` · `mutants : 22 tués / 22 — taux de détection
100 %` · `FUZZ 3000 entrées … plantages=0`.
**SRC** — `docs/preuves/hote/2026-11-12-differentiel.txt`, `…-mutants.txt`, `…-fuzz.txt`.
**VO** — "But is the broker's decision engine itself trustworthy? We wrote a Python twin and ran
ten thousand decisions — they agree. We injected twenty-two mutations, and all twenty-two were
caught. We fuzzed three thousand inputs, zero crashes."

### 2:30–2:50
**SCREEN** — more proof cards: `OK 198 checks` (C core), `Ran 131 tests` (Python),
`OK 12 vérifications` (Wasm integration). Then the closing line in large text: "One policy. One
choke point. Nothing else to trust."
**SRC** — `docs/preuves/hote/2026-11-12-tests-core.txt` (5 binaries, 198 checks),
`…-tests-python.txt`, `…-integration-wasm.txt`.
**VO** — "Hundreds of unit checks — a hundred and ninety-eight in the C core alone — a differential
twin, fuzzing, mutants. But the real strength is an idea: the policy is enforced in exactly one
place. Every sensor read, every actuator write, every network byte passes through the broker —
and nowhere else."

### 2:50–3:00
**SCREEN** — repo title and the three layers again, with a single caption: "Éclat — the port is
someone else's. The broker is ours."
**VO** — "The port was someone else's. The capability broker is ours. Éclat — untrusted code,
that can only do what you allowed. Thank you."

---

## Version française abrégée (récapitulatif)

| Temps | À l'écran | Voix off |
|---|---|---|
| 0:00–0:20 | Index officiel des ports QNX sans trace de `wasm` ; 404 sur `qnx-ports/wamr` | Aucun runtime WebAssembly dans l'index. Et honnêtement : quelqu'un vient d'en proposer un en amont (#5200). Notre apport n'est pas le portage, c'est le courtier. |
| 0:20–0:50 | `eclat-broker` charge sa politique ; `eclat-client` reçoit `CAP_OK` pour le capteur autorisé, `CAP_DENIED_POLICY`/`CAP_DENIED_RANGE` pour le reste | Un courtier, processus séparé, décide de chaque accès. Le capteur autorisé passe ; tout le reste est refusé. Le défaut : refuser. |
| 0:50–1:35 | Le module hostile sous WAMR : 4 refus en JSON ; le module falsifié refusé à l'empreinte | Quatre tentatives, quatre refus. Un octet changé, empreinte différente, module refusé. |
| 1:35–2:10 | La boucle 1 kHz lit un capteur vivant (sinusoïde) ; le gourmand est chargé puis tué (budget 300 ms) pendant que la boucle continue | La boucle ne s'arrête pas pendant la mise à mort. On filme la vivacité, pas la performance — la gigue absolue est dominée par QEMU. |
| 2:10–3:00 | Les preuves : 10 000 décisions concordantes, 22 mutants tués, fuzz sans plantage, 198 vérifications | Le moteur est un programme vérifié, pas une simple démo. Et la politique n'est appliquée que d'un seul côté : le courtier. |