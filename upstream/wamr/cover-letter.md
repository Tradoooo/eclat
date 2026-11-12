# Cover letter — add a QNX 8.0 platform layer to WAMR

> **Statut de cette lettre : brouillon.** Elle n'a pas été soumise. La couche `platform/qnx/`
> n'existe pas encore dans ce dépôt parce que le SDP QNX 8.0 n'est pas installé ; nous
> n'ouvrirons pas de PR contenant du code non compilé. Le texte ci-dessous est ce qui
> accompagnera la PR une fois la compilation effective obtenue.

---

**Subject:** [RFC] platform/qnx: add a QNX 8.0 OS abstraction layer

Hello,

## The gap

QNX 8.0 runs on safety-critical controllers, and WAMR has no platform layer for it. As of
2026-11-12 we verified that:

- `core/shared/platform/` ships 15 OS layers (`alios`, `android`, `common`, `cosmopolitan`,
  `darwin`, `ego`, `esp-idf`, `freebsd`, `linux-sgx`, `linux`, `nuttx`, `riot`, `rt-thread`,
  `vxworks`, `windows`, `zephyr`) and **no `qnx`**;
- `product-mini/platforms/` ships 17 targets including `posix`, and **no `qnx`**;
- the official QNX ports index (`qnx-ports/build-files`, ~150 ports) contains **no** WebAssembly
  runtime of any kind, and `github.com/qnx-ports/wamr` is a **404**.

WAMR already ports well to small real-time kernels — `nuttx`, `zephyr`, `rt-thread` and `riot`
are the precedent we intend to follow. QNX is POSIX-conformant and its `qcc` compiler is a GCC
front end, so we expect the delta to be small.

## Proposed approach

Derive `core/shared/platform/qnx/` from `core/shared/platform/linux`, reusing
`core/shared/platform/common` and implementing the interface declared in
`core/shared/platform/include`, then add `product-mini/platforms/qnx/`. The rationale: QNX
provides `clock_gettime(CLOCK_MONOTONIC)`, POSIX threads, and an aligned-allocation-compatible
libc, so most of the Linux layer should carry over. Where it does not, we will document each
deviation — see `PORTING-NOTES.md` in this directory for the list we expect to work through.

Our contribution is deliberately **narrow**: an OS abstraction layer and the product entry point,
nothing else. No changes to the interpreter, the AOT compiler, or the WASI implementation.

## Why we are doing this

We are building a capability broker for third-party WebAssembly modules on QNX 8.0: every import
a module makes becomes a synchronous message to a resource manager that decides against a
declarative policy, default-deny. The interesting property is that three independent isolation
layers stack — the Wasm sandbox, an abilities-stripped QNX process, and a kernel-enforced security
policy — and that a 1 kHz control loop can be shown, by measurement, not to move while a hostile
module saturates its budget and gets killed.

The decision engine behind that broker is already written, tested and green **on Linux**, with no
QNX dependency:

- C11 strict (`-std=c11 -O2 -Wall -Wextra -Werror`), pure core: no clock, no file, no socket, no
  allocation, no mutable global state — **161 assertions**;
- clean under **ASan/UBSan**;
- policy parser **fuzzed**: 3,000 mutated inputs, 0 crashes;
- **22 mutants** injected into the core, **22 killed** (100% detection);
- an **independent Python twin** of the decision engine, compared by a seeded differential test:
  **10,000 decisions per seed**, in agreement (seeds 1, 2, 3, 5, 7).

That differential harness is what caught a real bug before it shipped: a `-1` sentinel used both
for "no constraint" and for a legitimate `ids = -1` policy value. Two independent implementations
of the same written contract disagreed, and the disagreement was the bug. We mention it because it
is the reason we trust the engine enough to port a runtime on top of it.

## What we are asking for

1. **Direction, before we write code.** Is deriving from `platform/linux` acceptable, or would
   you rather we build on `platform/common` plus a thin QNX layer? Is there an in-flight QNX
   effort we have not found?
2. **Naming and layout.** `core/shared/platform/qnx/` + `product-mini/platforms/qnx/`, following
   the `nuttx` precedent — please tell us if that is wrong.
3. **Review bandwidth** once the layer actually compiles with `qcc` and runs a module end to end.

We will not open a PR until the code compiles for a real QNX 8.0 target and passes the same test
suite it passes on Linux. If it turns out the platform layer cannot be made to work in the time we
have, we will say so here rather than submit something half-finished.

Happy to be told this is out of scope, or that it belongs in a separate repository.

Thanks for WAMR — it is the reason this project is feasible at all.