<!-- SPDX-License-Identifier: Apache-2.0 -->
# Éclat — Contract Specification (English)

This file is the English rendering of [`CONTRACT.md`](../CONTRACT.md), which remains the
authoritative document: if the two ever disagree, `CONTRACT.md` wins. It is self-contained — an
English-speaking reviewer needs nothing else to understand the interface.

> ## Status — read this first (11 November 2026)
>
> **Implemented and tested**, on Linux only, with no QNX licence required:
>
> - `core/` — the capability engine in strict C11 (no clock, no file, no socket, no allocation, no
>   mutable global state): restricted-TOML policy parser and validator, the decision procedure of
>   §3, the sliding-window budget of §4, the decision-log formatter of §5. **161 checks** green,
>   **ASan/UBSan clean**.
> - `host/` — an independent Python twin of the decision engine, plus the differential harness of
>   §6: **28 Python tests**, and **10,000 decisions** compared C ↔ Python by seed, **all in
>   agreement**, with bisection to the first divergence.
>
> **Not implemented yet**:
>
> - the QNX platform layer, the broker as a QNX resource manager, the QNX security policy, and the
>   real binding to the QNX mechanisms (abilities, Pathtrust, HAM, `ionotify`, `qcrypto`);
> - the **QNX SDP 8.0 is not installed**, so nothing below has been executed on a QNX target. No
>   latency figure is published anywhere in this document — anything performance-related is
>   **to be measured**.
> - Éclat provides isolation and policy. It is **not** a qualification and makes no certification
>   claim (see [`docs/security-mapping.md`](security-mapping.md), §5).

---

## 0. What this contract covers

A third-party WebAssembly module runs inside a QNX process that holds no privileges. Every access
it makes to the outside world — sensor, actuator, key-value store, network channel — is an **import
call**, and every import call becomes a **synchronous message to a broker** (a QNX resource
manager) which decides according to a declarative policy. This document freezes the four things
that both sides must agree on:

1. the **import ABI** exposed to the module (§1);
2. the **policy format** the broker loads (§2);
3. the **decision procedure** and its codes (§3);
4. the **budget**, the **decision log**, and the **differential harness** that proves C and Python
   agree (§4–§6).

Anything not written here is not part of the contract.

---

## 1. Import ABI (frozen signatures)

The host exposes these imports to the WebAssembly module. Two rules govern all of them:

- **No NUL-terminated string is ever passed.** Every string is a `(pointer, length)` pair.
- **Every length is checked against the declared buffer size *before* any copy happens.** An
  import whose length exceeds the buffer is rejected — it never copies first and checks later.

A third rule governs resolution: **an unknown import is rejected at instantiation time, never at
execution time.** A module that asks for something the host does not provide cannot start.

| Import | Signature (wasm) | Semantics |
|---|---|---|
| `cap_time_ns` | `() -> i64` | Monotonic instant in nanoseconds, supplied by the host |
| `cap_log` | `(ptr:i32, len:i32, level:i32) -> i32` | Logging; returns an error code (`CAP_OK` on success) |
| `cap_read_sensor` | `(id:i32, out_ptr:i32, out_len:i32) -> i32` | Policy-checked read; fills a bounded buffer |
| `cap_write_actuator` | `(id:i32, value_milli:i32) -> i32` | Policy-checked write; integer value in milli-units |
| `cap_kv_get` / `cap_kv_put` | `(key_ptr:i32, key_len:i32, val_ptr:i32, val_len:i32) -> i32` | Bounded key-value storage |
| `cap_net_send` | `(chan:i32, ptr:i32, len:i32) -> i32` | Send on a declared channel, policy-checked |

Enumeration order (`eclat_cap_t`) — this order is part of the contract and must not be renumbered:

```
0 ECLAT_CAP_TIME_NS
1 ECLAT_CAP_LOG
2 ECLAT_CAP_READ_SENSOR
3 ECLAT_CAP_WRITE_ACTUATOR
4 ECLAT_CAP_KV_GET
5 ECLAT_CAP_KV_PUT
6 ECLAT_CAP_NET_SEND
7 ECLAT_CAP_COUNT
```

Textual names (used in TOML policies and in the decision log): `time_ns`, `log`, `read_sensor`,
`write_actuator`, `kv_get`, `kv_put`, `net_send`.

---

## 2. Policy format (restricted TOML)

The C parser accepts a **subset** of TOML, documented below. The Python twin uses `tomllib` from
the standard library and restricts the same fields: both implementations must therefore accept
exactly the same documents within this subset.

```toml
[module]
name = "diag-partner"        # module name (≤ 64 characters)
sha256 = "…"                 # mandatory fingerprint of the .wasm (64 hex)
cpu_budget_ms_per_s = 50     # CPU budget per sliding second
mem_max_kib = 2048
calls_per_s = 200

[[allow]]
cap = "read_sensor"
ids = [1, 2, 7]              # or a single integer, or absent (no id constraint)

[[allow]]
cap = "write_actuator"
ids = [3]
value_milli_range = [-1000, 1000]

[[deny]]
cap = "net_send"
```

This is **standard TOML**: the Python twin reads it with `tomllib` (standard library), and the C
parser accepts the same subset.

### 2.1 Subset rules accepted by the C parser

- Sections: `[module]` (exactly one, mandatory) and arrays of tables `[[allow]]` / `[[deny]]`
  (zero or more).
- Module keys: `name` (string, mandatory, ≤ 64 characters), `sha256` (string, 64 hex characters,
  mandatory), `cpu_budget_ms_per_s`, `mem_max_kib`, `calls_per_s` (unsigned integers).
- `cap`: a string, one of the seven textual names. An unknown capability identifier is a
  **validation error**.
- `ids`: a single integer, or an array of integers (stored as an inclusive `[min, max]` range).
- `value_milli_range`: an array of **exactly two** integers, minimum then maximum.
- Comments `#`; strings in straight double quotes `"…"` (escapes `\"`, `\\`, `\n`, `\t`);
  decimal integers, possibly negative (`-1000`).
- An **unknown key is ignored** (tolerance), but a **malformed value is an error**.

### 2.2 Mandatory fields and defaults

`name` and `sha256` are mandatory. The three budgets (`cpu_budget_ms_per_s`, `mem_max_kib`,
`calls_per_s`) default to `0` when absent, and `0` means **unlimited**. An absent or malformed
`sha256` (anything that is not exactly 32 bytes ↔ 64 hex characters) is a **fatal validation
error**.

### 2.3 Deny by default

An invalid policy is rejected with both the offending **field** and the **cause**. The default is
**always** applied: **a capability that is not listed under `allow` is denied.** There is no
implicit grant anywhere in this contract.

---

## 3. Decision codes

```
CAP_OK = 0
CAP_DENIED_POLICY
CAP_DENIED_RANGE
CAP_DENIED_BUDGET
CAP_DENIED_UNKNOWN
CAP_BAD_ARG
CAP_KILLED_CPU
CAP_KILLED_MEM
```

`eclat_decide` (see `core/include/eclat/cap_decision.h`) produces **only** `CAP_OK`,
`CAP_DENIED_POLICY`, `CAP_DENIED_RANGE` and `CAP_DENIED_UNKNOWN`. It is a pure function of
`(policy, cap, id, value_milli)`.

### 3.1 The seven-step decision procedure, in order

1. `cap` outside `[0, ECLAT_CAP_COUNT)` → `CAP_DENIED_UNKNOWN`.
2. `cap` present in `policy->denies` → `CAP_DENIED_POLICY`.
3. `cap` equal to `TIME_NS` or `LOG` → `CAP_OK` (host facilities, not subject to the `allow` list).
4. No `allow` rule with `rule.cap == cap` → `CAP_DENIED_POLICY`.
5. The rule carries an id constraint (`has_ids`) and `id` is outside `[id_min, id_max]` →
   `CAP_DENIED_RANGE`.
6. `cap == WRITE_ACTUATOR`, the rule carries a value constraint (`has_value_range`) and
   `value_milli` is outside `[val_min, val_max]` → `CAP_DENIED_RANGE`.
7. Otherwise → `CAP_OK`.

The order is strict, from the highest priority (step 1) to the lowest (step 7). The Python twin
implements **exactly** these seven steps in this order — that agreement is what §6 checks.

Constraints are explicit flags, not sentinels: an `ids` or `value_milli_range` constraint applies
if and only if the corresponding flag (`has_ids`, `has_value_range`) is set. `-1` is therefore a
legitimate id, never a marker for "no constraint".

### 3.2 Codes produced by the host, not by `eclat_decide`

`CAP_DENIED_BUDGET`, `CAP_KILLED_CPU`, `CAP_KILLED_MEM` and `CAP_BAD_ARG` are produced by the
**host**:

- the budget is checked **around** the call (before and after), not inside the decision procedure;
- the ABI arguments (a length that does not fit the declared buffer) are checked **at copy time**.

---

## 4. Budget (one-second sliding window)

- `cpu_budget_ms_per_s`: maximum CPU per window. `cpu_ns_used` is the sum of CPU time accounted in
  the current window. `budget = cpu_budget_ms_per_s * 1 000 000` nanoseconds.
- `calls_per_s`: maximum number of calls per window. `0` (or `cpu_budget_ms_per_s == 0`) means
  unlimited.
- The window lasts `1 000 000 000` ns. When `now_ns - win_start_ns >= 1e9`, the window restarts:
  `win_start_ns = now_ns`, counters reset to zero, excess flags reset to zero.
- `eclat_budget_charge` (see `core/include/eclat/cap_budget.h`) accounts the time, then checks the
  limits: it returns `0` when OK, `1` when the CPU budget is exceeded, `2` when the call quota is
  exceeded. The `cpu_exceeded` / `calls_exceeded` flags stay raised until the end of the window.

Because `core/` has no clock, the current instant is **injected by the caller**: the budget logic
is testable without a target, and deterministic under test.

---

## 5. Decision log

One line per **denial or excess** (granted decisions are logged only in audit mode). Target format:
single-line JSON.

```json
{"seq":12,"module":"diag-partner","cap":"write_actuator","decision":"CAP_DENIED_RANGE"}
```

`core/log.c` provides `eclat_log_format_decision`, which writes that line into a **caller-supplied
buffer** — no allocation, no global state. Actually emitting the line (to a file, to a socket) is
the host's job, never `core/`'s.

---

## 6. Differential harness (`tools/eclat-decide`)

The `eclat-decide` binary is the interface between the C engine and the Python twin. Its contract
is frozen:

```
Usage: eclat-decide <policy.toml> <requests.txt>
policy.toml  : a policy in the format of §2 (restricted TOML).
requests.txt : one request per line, 3 space-separated fields:
    <cap_name> <id> <value_milli>
    cap_name ∈ {time_ns, log, read_sensor, write_actuator, kv_get, kv_put, net_send}
    id         : decimal integer, or "-" if not applicable (→ -1)
    value_milli: decimal integer, or "-" if not applicable (→ 0)
Output: one line per request, the name of the decision code
    (CAP_OK / CAP_DENIED_POLICY / CAP_DENIED_RANGE / CAP_DENIED_UNKNOWN).
Invalid policy: stderr "ERROR:<field>:<message>" and exit code 2.
```

The Python twin (`host/diff.py`) generates `(policy, requests)` pairs, runs this binary, and
compares its output against its own reference implementation of `eclat_decide` (same algorithm,
§3.1). On any disagreement it **bisects the request list** to isolate the first divergence.

This is the verification that matters for the contract: two independent implementations, one in C
and one in Python, must agree decision for decision — **10,000 compared decisions, all in
agreement** at the time of writing.

---

## 7. Portability rules (hard constraints)

- `core/` is **pure C11**: no clock, no file, no socket, no allocation, no mutable global state.
  All inputs and outputs (time, buffers) are injected by the caller.
- `platform/posix/` and `platform/qnx/` share **the same interface contract**: one interface
  declaration, two implementations. Code is never copied from one layer to the other.
- Under 500 lines per file, under 50 lines per function.

This is the architectural point that makes the project deliverable without a QNX licence: the
decision logic is tested on Linux through `platform/posix/`, and the QNX-specific code stays
confined to `platform/qnx/`, `broker/*_qnx.c` and `target/`.

---

## 8. Where this contract sits

- The three isolation layers (Wasm sandbox · QNX process without abilities · security policy
  enforced by the kernel) and the five QNX mechanisms are described in
  [`docs/qnx-five-technologies.md`](qnx-five-technologies.md) and drawn in
  [`docs/qnx-five-technologies.svg`](qnx-five-technologies.svg).
- The mapping of these rules to ISO 26262 (parts 6 and 9) and IEC 62304, and the explicit
  statement that Éclat claims **no certification**, are in
  [`docs/security-mapping.md`](security-mapping.md).
- The 22 project requirements (`ECL-01`..`ECL-22`) and the claim status are in
  [`docs/cahier-des-charges.md`](cahier-des-charges.md) (§6 and §14).
