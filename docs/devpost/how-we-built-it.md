# How we built it

*Technical narrative for the Devpost "How we built it" field. English. Every number is taken
from a raw artifact under `docs/preuves/` or from the repository README.*

---

## The architecture in one picture

```
        ┌────────────────────────────────────────────────────────────┐
        │  eclat-run (loader + WAMR runtime, QNX process)           │
        │                                                            │
        │  .wasm module ── WAMR interp ── 7 native imports          │
        │       (sandboxed)    │    (cap_*, registered once)         │
        │                     ▼                                      │
        │              transport (decide)                            │
        └───────────────────┬────────────────────────────────────────┘
                            │  MsgSend / MsgReply  (« cap » name,
                            │   /dev/cap published via pathmgr_link)
                            ▼
        ┌────────────────────────────────────────────────────────────┐
        │  eclat-broker (capability broker, ANOTHER QNX process)     │
        │  resource-manager-style message loop                      │
        │  policy (TOML) · budget · audit (chained hash) · decision │
        └────────────────────────────────────────────────────────────┘
```

The pipeline is: **module → WAMR → transport → broker → policy**. The loader's ability checks
(fingerprint, budget admission) happen at load time; the per-call **decision** happens inside the
broker, in a separate process.

Three layers of isolation, deliberately independent:

1. the **WebAssembly sandbox** (bounded linear memory, no direct syscall, imports explicitly
   declared);
2. the **QNX process without abilities** (the broker, whose abilities can be stripped);
3. the **deny-by-default security policy** the broker enforces per call.

## Two platform layers, one contract

The decisive architectural move was to split the world in two layers that share **one frozen
interface contract** (`platform/include/eclat_platform.h`):

```
platform/include/eclat_platform.h   ── ONE declaration of the interface
platform/posix/eclat_platform_posix.c ── Linux implementation (monotonic clock, thread CPU time)
platform/qnx/eclat_platform_qnx.c     ── QNX implementation (clock_gettime, CLOCK_THREAD_CPUTIME_ID)
```

`core/` (the decision engine, policy parser, budget, audit log) is **pure C11**: no clock, no
file, no socket, no allocation, no mutable global state. Everything it needs (time, CPU time,
log emission) is injected through the platform contract. The runtime and the broker only know
that contract.

The consequence is the reason the project could be delivered at all before the SDP licence
arrived: **every logical property was testable on Linux, with no QNX licence**, and only the thin
platform layer and the IPC transport differ on target. The two platform layers are never copies
of each other — they implement one contract, each in its native idiom.

The same idea applies to the decision path: a `transport` interface
(`runtime/eclat_transport.h`) has two implementations —

- `local` — the decision is made in-process by `eclat_decide` (used on the host, and as a
  fallback);
- `qnx` — the decision is made by the broker in **another process**, over `MsgSend`/`MsgReply`
  (selected with `eclat-run --broker cap`).

With the local transport the boundary is a convention; with the QNX transport it is a process —
and the module's import counters are then the broker's own counters. That is the property the
project promises, demonstrated in `docs/preuves/qnx800/2026-11-12-module-via-courtier.txt`.

## The Python twin and the differential test

A second, independent implementation of `eclat_decide` lives in `host/` (Python 3, standard
library only). A harness (`tools/eclat-decide`) exposes the C engine on stdin/stdout; the Python
twin generates `(policy, requests)` pairs and compares the two verdicts. On the first
disagreement it bisects the request list to isolate the exact diverging case.

This one test **caught a real bug**. The first version of the allow-rule used `id_min == -1` as a
sentinel for "no id constraint". But `-1` is also a perfectly legitimate id (`ids = [-1, 5]`).
With the sentinel, `ids = [-1, 5]` was read as "no constraint" — and on `ids = -1` with a query
`id = 10`, the C engine answered `CAP_OK` while the Python twin (which already distinguished
`None` from a tuple) answered `CAP_DENIED_RANGE`. The fix replaced magic values with explicit
`has_ids` / `has_value_range` flags. Without a second implementation, that bug would have
shipped; the twin's verdict over 10,000 differential decisions is what made the engine
trustworthy.

## The four QNX pitfalls we paid for (and wrote down)

Each of these cost hours; they recur in every QNX project, so recording them is part of the
contribution (`docs/DECISIONS.md`):

1. **`name_attach()` accepts only a NAME, not a path.** A string with a `/` is rejected
   (`EINVAL`). Publishing `/dev/cap` therefore requires `resmgr_attach()` plus the full
   `iofunc` set; until then the broker registers under the plain name `cap`.
2. **`name_attach()` CREATES the channel and hands it back in `name_attach_t.chid` — the server
   must read on *that* channel.** Creating your own `ChannelCreate()` and reading there makes
   the client block forever inside `name_open()`: the path manager sends the connection handle
   to the manager's channel, not yours. This is the classic mistake, and it only shows up as a
   silent hang.
3. **The server must handle the `_IO_CONNECT` message** (reply `MsgReply(rcvid, EOK, NULL, 0)`)
   and the disconnect pulse (`_PULSE_CODE_DISCONNECT` → `ConnectDetach`) **before** any
   application message.
4. **An application message must carry a type ABOVE `_IO_MAX` (0x1ff)** or the kernel/path
   manager interprets it. Our type is `0xec01`. Bonus detail: on QNX 8.0, `MsgReply()` takes the
   **status as its second argument** — `MsgReply(rcvid, 0, &rep, n)`.

Two tooling details that saved us after the fact: QNX has no `pkill`, it has `slay`; and with
`set -o pipefail`, a `grep -q` that finds its match closes the pipe early, the upstream process
gets SIGPIPE and the pipeline reports failure — a "is the VM alive?" test answered "no" while the
VM was running fine.

Other recorded porting gaps (now all solved upstream, not by us): `MAP_32BIT` does not exist on
QNX (the upstream port uses `MAP_BELOW` instead, preserving the low-2 GB AOT region); QNX has no
`libdl`/`libpthread` (they live in libc — hence `-lsocket -lm` and no `-ldl -lpthread`); and in
the qnx-ports recipe, target matching must key on **`CPUVARDIR`**, not `CPU` (`aarch64` vs
`aarch64le` — the same offset as `ntoaarch64-ar` and `-Vgcc_ntoaarch64le`).

## What is ours and what is not

The honest split, which we treat as a strength and state plainly:

- **The QNX 8.0 port of WAMR is NOT ours.** It is PR #5200
  (`bytecodealliance/wasm-micro-runtime`, author Selman Altınay, opened 2026-11-04). Our two
  patches in `upstream/wamr/patches/` (feature-test-gated `MAP_32BIT`, `-ldl`/`-lpthread`
  removal) are kept only as the trace of the independent verification that let us compile and
  link WAMR for QNX on this machine; they are **not submitted**, because #5200 solves the same
  gaps better. Building on an existing, superior port for days of work saved is the point — not
  a source of embarrassment.
- **Our upstream contribution is the qnx-ports recipe, PR #337** (`qnx-ports/build-files`,
  "Add wamr port: WAMR 2.4.5 for QNX 8.0", opened 2026-11-12, OPEN/MERGEABLE): `ports/wamr/`
  plus a `qnx.nto.toolchain.cmake`, README, and a CI workflow. It was **executed before it was
  opened** using the SDP 8.0 `mk` framework — both targets (`nto x86_64`, `nto aarch64le`)
  build and install without error. The recipe openly depends on #5200 (WAMR has no QNX platform
  layer upstream yet) and says it proposes to switch to the upstream tag once #5200 merges.
- **The capability broker and the policy engine are ours** — the layer nobody had proposed.

## The demo, on the target

On `QNX qnxwamr 8.0.0 2026/06/05 x86pc x86_64` (QEMU/KVM, image from `mkqnximage --type=qemu`):

- the honest module runs (4 broker calls, 0 denials);
- the hostile module gets 4 denials, logged as JSON by the broker in another process;
- the greedy module looping forever is killed at 1.30 s for a 300 ms budget (`fork`/`pipe`/`poll`/
  `SIGKILL` work unmodified on QNX — the choice made after the watchdog-thread approach proved
  unable to stop a compute loop, because `wasm_runtime_terminate()` only sets an exception the
  interpreter never consults during a tight loop);
- the tampered module (one byte flipped) is refused with its digest;
- and a 1 kHz control loop that reads a 1 Hz sinusoid sensor **through the broker** keeps its
  tick count intact across all three phases (idle / during the kill / after): 3000 ticks fired
  and 2999/2999 jitter measurements completed in each phase, with no failed sensor read.

The sensor value is a 1 Hz sine (centered on 3000 milli-units, amplitude 1000) read at request
time via `ClockTime` — so four spaced reads return four different values, proving the broker is a
living signal source, not a constant. The loop itself runs at priority 21, woken by a
`timer_create` + `SIGEV_PULSE` + `MsgReceive`.

**Honesty on the loop:** measured under QEMU/KVM the absolute jitter is dominated by the
emulator (no real timer device, no vCPU isolation); p50 lands in the hundreds of µs to ~1 ms and
p99 in the tens of ms. What is proven is **liveness** — the loop continues, tick count intact,
while an enemy module is killed underneath it. Exact jitter requires a physical target, and we
say so in the artifact header and in the README.