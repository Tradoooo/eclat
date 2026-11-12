# Reproduction

*Exact commands to reproduce every claim. English. All commands come from the repository's
`Makefile` and `tools/*.sh` — nothing is paraphrased.*

Three parts:

- **(a) the host only** — no QNX licence, no QNX software, ~4 minutes;
- **(b) the QNX target** — requires a QNX SDP 8.0 installation;
- **(c) the proof run** — replays everything and regenerates `docs/preuves/ETAT.md`.

---

## (a) Host only — no QNX licence required

Prerequisites: a C11 compiler (`cc`), Python 3 (standard library only), `make`, `cmake`, and
network access for the first `make deps`. **No QNX software and no QNX licence are needed for
anything in this part** — this is the point of the "two platform layers, one contract"
architecture: all the logic lives in `core/`, which is pure C11 (no clock, no file, no socket, no
allocation).

```bash
git clone https://github.com/Tradoooo/eclat.git
cd eclat
```

### A.1 — Build and test the decision engine

```bash
make core            # libeclat.a, C11 strict -std=c11 -O2 -Wall -Wextra -Werror
make test            # 198 checks (budget 36, decision 45, log 24, policy 56, sha256 37)
make test-san        # the same under ASan/UBSan
```

Expected output of `make test`:

```
== build/bin/test_budget ==
OK 36 checks
== build/bin/test_decision ==
OK 45 checks
== build/bin/test_log ==
OK 24 checks
== build/bin/test_policy ==
OK 56 checks
== build/bin/test_sha256 ==
OK 37 checks
```

### A.2 — Python twin and the differential test

```bash
make python-tests    # 131 tests (twin, console, audit, signed manifest)
make differential    # 10 000 decisions compared C <-> Python
```

`make differential` runs `python3 host/diff.py --bin build/bin/eclat-decide --seed 1 --n 10000`;
expected last line:

```
DIFF OK 10000 decisions
```

### A.3 — Fuzz and mutants

```bash
make fuzz                             # default: 2000 mutated inputs
make fuzz FUZZ_ITERATIONS=3000        # the configuration of the archived proof
```

Expected:

```
FUZZ 3000 entrées : acceptées=412 refusées=2588 plantages=0
FUZZ OK : aucun plantage, aucune erreur ASan/UBSan
```

```bash
make mutants          # 22 injected regressions in core/, all must be killed
```

Expected last line:

```
mutants : 22 tués / 22 (survivants : 0, config cassée : 0)
MUTANTS OK : taux de détection 100 %
```

### A.4 — Real `.wasm` modules on WAMR (still no QNX)

```bash
make deps             # WAMR-2.4.5 (pinned) + wasi-sdk 34.0, sha256 verified -> .deps/
make wasm runtime     # 3 .wasm modules + the eclat-run loader
make test-wasm        # 12 integration checks: ECL-04 / 05 / 06 / 07
```

`make deps` runs `tools/fetch-deps.sh`: it clones WAMR at tag `WAMR-2.4.5` and downloads
`wasi-sdk-34.0`, verifying the tarball's SHA-256 **before** extracting it.

Expected tail of `make test-wasm`:

```
intégration wasm : 12 ok, 0 échec(s)
OK 12 vérifications
```

### A.5 — Optional extras

```bash
make bench            # latency bench: C engine vs Python twin (measured on Linux, NOT on QNX)
make size-check       # < 500 lines per file, < 50 lines per function
make spdx             # SPDX header on every source file
```

### A.6 — The third-party module SDK (optional demo)

```bash
python3 tools/eclat-new-module.py demo --dir /tmp && cd /tmp/demo && make test
```

Expected:

```
module demo — politique « demo »
  [ 1] read_sensor     id=1    valeur=0       -> CAP_OK
  [ 2] write_actuator  id=3    valeur=2100    -> CAP_DENIED_RANGE
  [ 3] net_send        id=9    valeur=0       -> CAP_DENIED_POLICY
```

---

## (b) The QNX 8.0 target

Prerequisites, in addition to part (a):

- **QNX SDP 8.0** installed (any path). `qcc` 12.2.0 in our runs.
- `qemu-system-x86_64` on the host.
- The upstream WAMR QNX port tooling (`altinayselman/wamr-qnx`, Apache-2.0) and the WAMR tree
  it fetches.

### B.1 — Point the shell at the SDP

```bash
source tools/setup-sdp.sh          # source it — it exports QNX_HOST and QNX_TARGET
```

It locates `qnxsdp-env.sh` (default candidates: `$HOME/qnx800`, `$HOME/qnxsdp`, `/opt/qnx800`,
`/opt/qnx`, `$HOME/sdp/qnx800`) and checks that `qcc` answers. Pass a path explicitly if your
SDP lives elsewhere: `source tools/setup-sdp.sh /path/to/sdp`.

Expected:

```
SDP trouvé : /home/<user>/qnx800/qnxsdp-env.sh
qcc        : qcc (QNX SDP 8.0.0 2026/06/05) 12.2.0
QNX_HOST   : /home/<user>/qnx800/host/x86_64
QNX_TARGET : /home/<user>/qnx800/target/x86_64
```

### B.2 — Tooling prerequisites for the cross build

`tools/build-qnx.sh` needs **cmake >= 3.31** (the host's cmake 3.30.5 has no `Platform/QNX`
module) and **ninja**. Install them in an isolated venv, without touching the system:

```bash
python3 -m venv .deps/cmake-venv
.deps/cmake-venv/bin/pip install cmake ninja
```

`cmake 4.4.4` and `ninja 1.13.2` are the versions used for the archived run. cmake 4.x rejects
WAMR's old `cmake_minimum_required`, so the build passes
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5`.

### B.3 — The upstream QNX port tooling (PR #5200's repository)

Our target run builds on the upstream QNX platform layer, which is **not ours** — it is
[PR #5200](https://github.com/bytecodealliance/wasm-micro-runtime/pull/5200) by Selman Altınay,
whose patches are published in the `wamr-qnx` tooling repository:

```bash
cd .deps
git clone https://github.com/altinayselman/wamr-qnx.git
cd ..
cd .deps/wamr-qnx
bash scripts/fetch-upstream.sh          # clones WAMR at WAMR-2.4.5 + applies the QNX patches
bash scripts/build-qnx.sh x86_64        # cmake + ninja -> build/x86_64/iwasm/libiwasm.a
cd ../..
```

`scripts/build-qnx.sh` accepts `x86_64` or `aarch64le` — it is the upstream script, so use the
**target directory name** (`aarch64le`), not Éclat's `QNX_ARCH=aarch64`. Its output is what
`make qnx-amont` consumes.

### B.4 — Build Éclat for QNX

```bash
make qnx-amont                    # eclat-run, eclat-broker, eclat-client, eclat-boucle for QNX 8.0
make qnx-amont QNX_ARCH=aarch64   # the aarch64 target
```

`make qnx-amont` sets `ECLAT_WAMR_SRC` and `ECLAT_WAMR_LIB` to the tree/library built in B.3 and
calls `tools/build-qnx.sh`, which compiles `core/`, `platform/qnx/`, `runtime/`, `target/` and
`broker/` with `qcc -Vgcc_ntox86_64` and links against the upstream `libiwasm.a`.

The variant `make qnx` (without `qnx-amont`) builds against **our own** two patches in
`upstream/wamr/patches/` instead of the upstream port. Both work; the archived target run used
`qnx-amont`.

The binaries land in `build/qnx-x86_64/`:

```
eclat-run      the module loader (WAMR runtime + decision engine)
eclat-broker   the capability broker (QNX message server)
eclat-client   the broker test client
eclat-boucle   the 1 kHz control loop
```

### B.5 — The QNX 8.0 image for QEMU

```bash
tools/qnx-vm.sh build            # mkqnximage --type=qemu -> .deps/qnx-vm/output/ifs.bin
tools/qnx-vm.sh start            # boots it, waits for sshd, prints the ssh command
```

`build` wraps the upstream `scripts/vm/make-image.sh`; `start` launches QEMU with KVM when
`/dev/kvm` is readable and writable, otherwise in software emulation, and publishes SSH on
`127.0.0.1:2280` (override with `QNX_SSH_PORT`). Other subcommands: `stop`, `status`, `console`.

Expected on a booted VM:

```
VM prête : ssh -i /home/<user>/.ssh/id_ed25519_qnx_eclat -p 2280 root@127.0.0.1
QNX qnxwamr 8.0.0 2026/06/05-16:26:03EDT x86pc x86_64
```

### B.6 — The four scenarios on the target

```bash
tools/demo-on-qnx.sh             # ECL-04 / 05 / 06 / 07 on QNX 8.0
```

It builds the `.wasm` modules if needed, writes one policy per module (with the module's real
SHA-256), transfers everything to `/tmp/eclat` on the target, runs the four scenarios and writes
the raw console to `docs/preuves/qnx800/<date>-demonstration.txt`.

Expected (excerpt):

```
════════ ECL-04 : module honnête (tout est autorisé) ════════
--- bilan ---
appels au courtier : 4
refus              : 0
code=0

════════ ECL-05 : module hostile (4 accès interdits) ════════
{"seq":1,"module":"hostile","cap":"net_send","decision":"CAP_DENIED_POLICY"}
{"seq":2,"module":"hostile","cap":"write_actuator","decision":"CAP_DENIED_RANGE"}
{"seq":3,"module":"hostile","cap":"read_sensor","decision":"CAP_DENIED_RANGE"}
{"seq":4,"module":"hostile","cap":"kv_put","decision":"CAP_DENIED_POLICY"}
refus              : 4

════════ ECL-06 : module gourmand (boucle sans fin, budget 300 ms) ════════
module tué par le superviseur : budget CPU de 300 ms dépassé
    1.33s real     1.30s user     0.00s system

════════ ECL-07 : module falsifié (1 octet modifié) ════════
REFUSÉ (-2) : empreinte du module différente de la politique (32e7e8db3f1118cf…)
code=1
```

### B.7 — The broker, reached over the IPC boundary

```bash
tools/demo-broker-on-qnx.sh      # the broker alone + its test client
tools/demo-module-via-broker.sh  # the module's imports going through the broker
```

The second one runs `eclat-run --broker cap`, so **every import of the module becomes a
`MsgSend` to the broker in another process**; the counters printed by the loader are the
broker's own counters.

Expected:

```
════════ ECL-04 via courtier : module honnête ════════
eclat: décisions prises par le courtier « cap » (autre processus)
appels au courtier : 4
refus              : 0
```

### B.8 — The 1 kHz control loop and the kill

```bash
tools/demo-boucle.sh             # baseline / during the kill / after
```

It starts the broker, then measures the 1 kHz loop (a pulse-driven QNX task at priority 21 that
reads the sensor **through the broker**) three times: idle, while a greedy module is loaded and
killed by the budget supervisor, and afterwards.

Expected (excerpt of `docs/preuves/qnx800/<date>-boucle-1khz.txt`):

```
════════ 1. baseline (boucle seule) ════════
boucle 1 kHz : 3000 tics, 2999 mesures de gigue
════════ 2. pendant la mise à mort du module gourmand ════════
boucle 1 kHz : 3000 tics, 2999 mesures de gigue
════════ 3. après ════════
boucle 1 kHz : 3000 tics, 2999 mesures de gigue
```

**Read the numbers honestly:** the absolute jitter is dominated by the QEMU emulation (no real
timer device, no vCPU isolation). What this proves is that the loop **keeps running, tick count
intact, while an enemy module is killed underneath it** — not real-time performance. Exact jitter
figures require a physical target. The artifact says so in its own header.

### B.9 — Optional: the feasibility probe and the port recipe

```bash
make e2                          # E2 probe: does WAMR's POSIX layer compile with qcc?
make e2 E2_ARGS=--patched        # same, with our two patches applied
```

And, to re-test the upstream port recipe of PR #337 in the official QNX ports tree (SDP 8.0 and
its `mk` framework required):

```bash
git clone https://github.com/qnx-ports/build-files.git
JLEVEL=$(nproc) QNX_PROJECT_ROOT="<WAMR tree that contains the QNX platform support>" \
  make -C build-files/ports/wamr install
```

Both targets (`nto x86_64`, `nto aarch64le`) build and install without error.

---

## (c) The proof run

```bash
make preuves                      # replays everything, regenerates docs/preuves/ETAT.md
tools/preuves.sh --rapide         # same, minus the slow proofs (mutants, long fuzz)
```

`tools/preuves.sh` writes one dated raw artifact per proof under `docs/preuves/hote/` and
`docs/preuves/qnx800/`, then regenerates `docs/preuves/ETAT.md` with one line per proof: name,
artifact, verdict, duration. It exits non-zero if any executed proof failed.

The QNX proofs run only if the VM is up (`tools/qnx-vm.sh status` must report "en marche");
otherwise they are recorded as "VM arrêtée" and the host proofs still run.

Latest recorded state (`docs/preuves/ETAT.md`, generated 2026-11-12T21:29:19+01:00) — **11 proof
lines, all green**:

| Proof | Artifact | Verdict | Duration |
|---|---|---|---|
| C tests of the core (ECL-01/02/05/06/07) | `docs/preuves/hote/2026-11-12-tests-core.txt` | green | 1s |
| C tests under ASan/UBSan | `docs/preuves/hote/2026-11-12-tests-core-asan.txt` | green | 0s |
| Python tests | `docs/preuves/hote/2026-11-12-tests-python.txt` | green | 8s |
| Differential, 10 000 decisions | `docs/preuves/hote/2026-11-12-differentiel.txt` | green | 1s |
| Fuzz, 3 000 inputs (ASan/UBSan) | `docs/preuves/hote/2026-11-12-fuzz.txt` | green | 115s |
| Mutants (100 % detection rate) | `docs/preuves/hote/2026-11-12-mutants.txt` | green | 66s |
| `.wasm` integration on the host | `docs/preuves/hote/2026-11-12-integration-wasm.txt` | green | 2s |
| Demo on QNX 8.0 (ECL-04/05/06/07) | `docs/preuves/qnx800/2026-11-12-demonstration.txt` | green | — |
| Broker on `/cap` (MsgSend/MsgReply) | `docs/preuves/qnx800/2026-11-12-courtier.txt` | green | — |
| Module imports via the broker | `docs/preuves/qnx800/2026-11-12-module-via-courtier.txt` | green | — |
| 1 kHz loop (alive during the kill) | `docs/preuves/qnx800/2026-11-12-boucle-1khz.txt` | green | — |

The archives are raw, dated command output — untouched. A failure is archived as a failure: a
red artifact is worth more than a missing line.
