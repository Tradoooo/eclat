# Éclat — capability broker for WebAssembly modules on QNX 8.0

*Devpost "About the project" text. English. Every figure below is taken from a raw artifact
stored in `docs/preuves/` or from the repository README — nothing is estimated.*

---

## The gap

WebAssembly is a natural fit for running untrusted third-party logic on a safety-critical
controller: a bounded linear memory, an explicitly declared import list, no direct system
calls. QNX 8.0 has all the primitives needed to make that safe — synchronous message passing
(`MsgSend`/`MsgReply`), resource managers, process abilities, security policies, `qcrypto` — and
almost everything else is already ported to QNX (`rust`, `lua`, `perl5`, `protobuf`, `grpc`,
`ros2`, `tensorflow`, `pytorch`, `qt`, `gtk`, `sqlite`, `curl`, `zlib`, …).

**WebAssembly is not.**

Verified on 2026-11-12, reading the official port index directly:

- `github.com/qnx-ports/build-files/ports` — about 150 entries read, **no** `wasm`, `wamr`,
  `wasi`, `wasmtime` or `wasmer` entry;
- `github.com/qnx-ports/wamr` → **404**;
- GitHub search `org:qnx-ports wasm` → **0 repositories**.

*A stated limitation:* the `oss.qnx.com` dashboard advertises ~2,500 ports and cannot be queried
without a JavaScript-capable browser. The absence is certain inside `qnx-ports`, and **not**
absolutely certain across the whole QNX ecosystem. We say so rather than claim more.

A Wasm runtime did appear upstream in parallel: **PR #5200 "Add QNX 8.0 platform support"**
(`bytecodealliance/wasm-micro-runtime`), opened 2026-11-04 by Selman Altınay, currently OPEN and
MERGEABLE, welcomed by a WAMR maintainer. **That port is not ours and we do not claim it.** It
adds `core/shared/platform/qnx/` and `product-mini/platforms/qnx/` and was itself run on QNX 8.0
under QEMU (133/137 upstream unit tests pass).

So the porting layer is (being) contributed by someone else. **What nobody had proposed is the
layer above it: a capability boundary that decides, per call, what a module is allowed to do.**
That is what Éclat is.

## The contribution: a capability broker

Éclat turns every outbound access of a WebAssembly module into a **synchronous message to a
resource manager that decides**. The decision is taken on **one side only** — inside the broker,
in a process whose QNX abilities can be stripped away.

```
  .wasm module  ──import──▶  WAMR runtime  ──transport──▶  broker (separate process)
   (sandboxed)                                            policy + budget + audit
                                                            │
                                    decision ◀────────────┘
```

Four mechanisms, all implemented and all measured:

1. **Declarative, deny-by-default policy (TOML).** A module is admitted only with an explicit
   `sha256` of its bytes, an optional CPU budget and call quota, and a list of `allow` rules
   carrying identifier ranges and value ranges per capability. Anything not explicitly allowed is
   denied. The decision order is frozen into a contract and mirrored exactly by an independent
   implementation.
2. **CPU budget with process kill.** A module that loops forever is executed in a child process
   and killed when the budget is exceeded (`fork` + `pipe` + `poll` + `SIGKILL`, which work
   unmodified on QNX).
3. **Fingerprint verification before load.** A single flipped byte in the `.wasm` makes the
   loader refuse the module and print the expected digest.
4. **Tamper-evident audit log.** One JSON line per refusal or budget overrun, in a chained-hash
   journal with an independent verifier.

The decision engine is **portable C11** (`core/`): no clock, no file, no socket, no allocation.
It is validated by:

- **198 assertions** in the C test suite, clean under **ASan/UBSan**;
- a **Python twin** — a second, independent implementation — compared against the C engine over
  **10,000 differential decisions** (identical verdicts, per-seed);
- **3,000 mutated inputs** fuzzed through the policy parser under ASan/UBSan (0 crashes, 0
  sanitizer reports);
- **22 injected mutants** of the decision engine (rule order, default flipped, range comparison,
  budget counter, log formatting, …): **22/22 killed, 100 % detection rate**.

The Python twin earned its keep: it caught a real bug — a `-1` sentinel used to mean "no id
constraint", which silently collided with the legitimate id `-1`. The C engine answered
`CAP_OK` where the twin answered `CAP_DENIED_RANGE`. The fix was explicit `has_ids` /
`has_value_range` flags instead of magic values. Without a second implementation, that bug would
have shipped.

## The proof, on a real QNX 8.0 kernel

Target: **`QNX qnxwamr 8.0.0 2026/06/05 x86pc x86_64`**, image built with `mkqnximage
--type=qemu`, run under QEMU/KVM. Raw target console archived in `docs/preuves/qnx800/`.

| Requirement | Result on QNX 8.0 |
|---|---|
| **ECL-04** — honest module runs | 4 broker calls, **0 denials**, exit code 0 |
| **ECL-05** — hostile module is refused | **4 denials**: `CAP_DENIED_POLICY` (`net_send`), `CAP_DENIED_RANGE` (actuator 99999, outside ±1000), `CAP_DENIED_RANGE` (sensor 42), `CAP_DENIED_POLICY` (`kv_put`) — logged as JSON |
| **ECL-06** — greedy module, 300 ms budget | **killed by the supervisor** at 1.30 s user / 1.33 s real |
| **ECL-07** — tampered module (1 byte) | **refused**, digest cited, exit code 1 |

And the part that matters most: **the module's imports really do cross the IPC boundary.** With
`eclat-run --broker cap`, every import becomes a `MsgSend` to the broker in another process; the
counters printed by the loader are the broker's own counters. The hostile module's four JSON
denials were produced *on the other side of the message boundary*.

A 1 kHz control loop reads its sensor **through the broker** and keeps running while a greedy
module is loaded and killed underneath it: in each of the three phases (idle / during the kill /
after) the loop fired **3000 ticks and completed 2999/2999 jitter measurements** (the first tick
has no predecessor to measure against) — no tick lost, no failed sensor read.

## Honesty about what the numbers do and do not prove

The jitter figures of that 1 kHz loop are **dominated by the emulator**, not by the module:
under QEMU/KVM there is no real timer device and no vCPU isolation, so the measured p50 lands in
the hundreds of microseconds to ~1 ms and p99 in the tens of milliseconds. The "during the kill"
phase was actually *cleaner* than the idle baseline — proof that the noise comes from QEMU, not
from the module under test.

**What is proven:** the loop stays alive and keeps its tick count while an enemy module is killed
underneath it. **What is not proven:** real-time performance. Exact jitter figures require a
**physical hardware target**, and we do not claim them. The archived artifact says so in its own
header.

Also not claimed: any certification-grade security statement. Éclat provides isolation and a
policy; it does not provide a qualification.

## Upstream contribution, honestly attributed

| What | Who | Where |
|---|---|---|
| QNX 8.0 platform layer for WAMR | **Selman Altınay** (not us) | [bytecodealliance/wasm-micro-runtime#5200](https://github.com/bytecodealliance/wasm-micro-runtime/pull/5200) |
| **WAMR port recipe for the official QNX ports tree** | **Us (Éclat)** | [qnx-ports/build-files#337](https://github.com/qnx-ports/build-files/pull/337) |
| **Capability broker, policy engine, audit chain** | **Us (Éclat)** | this repository |

We opened **no** pull request against WAMR for the porting work: it would have been a lower-quality
duplicate of #5200, which solves the same two gaps we had found with better answers (`MAP_BELOW`
instead of giving up on `MAP_32BIT`, and a real `product-mini/platforms/qnx/` instead of a
special case in the Linux CMakeLists). Our own two patches remain in `upstream/wamr/patches/` as
a trace of the independent verification that let us compile and link WAMR for QNX on our
machine; they are **not submitted**.

The recipe in PR #337 was **tested before it was opened**, with the SDP 8.0 `mk` framework:
both targets (`nto x86_64`, `nto aarch64le`) build and install without error
(`libiwasm.a` 1 348 796 B / 1 371 320 B, `iwasm` 715 024 B / 744 464 B). A recipe that was
never executed would only have been a guess.

## Why three isolation layers, not one

1. the **WebAssembly sandbox** — bounded linear memory, no direct system call;
2. the **QNX process without abilities**;
3. the **security policy applied by the kernel**.

On a monolithic kernel only the first exists. It is their superposition that is the argument —
and the broker is what makes layer 3 expressible per capability instead of per process.

## Where to look next

- `docs/preuves/` — every claim, with its raw dated artifact;
- `upstream/README.md` — what is ours and what is not, line by line;
- `NOTICE` — attribution of every third-party work we build on;
- `reproduction.md` — how to re-run all of it, host-only and on target.
