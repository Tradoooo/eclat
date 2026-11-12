# Budget and links

*One place for every URL, what is ours and what is not, the key files, and the budget line.
English. Intended to fill the Devpost "Links" / "Resources" fields and to answer the "did anything
cost money?" question — nothing did.*

---

## Repository and license

| Field | Value |
|---|---|
| Submission repository | **https://github.com/Tradoooo/eclat** |
| Previous name (redirects) | `https://github.com/BaptisteFaisy/eclat` (the account was renamed on GitHub; both URLs resolve to the same public repository) |
| Visibility | PUBLIC |
| Created during the submission window | 2026-11-11 (submission deadline 2026-12-15) |
| Licence | **Apache-2.0** (compatible with WAMR, which is Apache-2.0 **WITH LLVM-exception**) |
| Fork? | **No.** The upstream WAMR history is not copied; WAMR/wasi-sdk live in `.deps/`, never committed. |

---

## Upstream project (the thing we port / build on)

| Field | Value |
|---|---|
| Project | **WAMR** — WebAssembly Micro Runtime |
| URL | **https://github.com/wasm-micro-runtime/wasm-micro-runtime** |
| Note on the URL | the previously-used `bytecodealliance/wasm-micro-runtime` also resolves (account renamed); the canonical URL above is what GitHub returns now |
| Pinned version | `WAMR-2.4.5`, commit `25bd7eb63e828e4bd242cc9b38d260b4b31c6605` — see [`VERSIONS`](../VERSIONS) |
| Upstream licence | Apache-2.0 **WITH LLVM-exception** (checked line 204 of the upstream `LICENSE`) |

---

## The two pull requests — clearly separated

### Ours: the qnx-ports recipe — PR #337

| Field | Value |
|---|---|
| URL | **https://github.com/qnx-ports/build-files/pull/337** |
| Title | « Add wamr port: WAMR 2.4.5 for QNX 8.0 » |
| Opened | 2026-11-12 |
| State | OPEN / MERGEABLE |
| What it adds | `ports/wamr/` in the official QNX ports tree: the port recipe + `qnx.nto.toolchain.cmake` + README + a CI workflow |
| Tested before opening | Yes — both targets build and install without error (`libiwasm.a` 1 348 796 B / 1 371 320 B; `iwasm` 715 024 B / 744 464 B for `nto x86_64` / `nto aarch64le`) |
| **Ownership** | **This is OUR upstream contribution.** |

### Not ours: the WAMR QNX platform layer — PR #5200

| Field | Value |
|---|---|
| URL | **https://github.com/wasm-micro-runtime/wasm-micro-runtime/pull/5200** |
| Title | « Add QNX 8.0 platform support » |
| Author | **Selman Altınay** (`altinayselman`) — **NOT us** |
| Opened | 2026-11-04 |
| State | OPEN / MERGEABLE |
| What it adds | `core/shared/platform/qnx/` and `product-mini/platforms/qnx/` |
| Its own author ran it | yes — on QNX 8.0 under QEMU (133/137 upstream unit tests pass) |
| **Ownership** | **NOT our work.** We build on it and attribute it — see [`NOTICE`](../NOTICE). |

The tooling repository where #5200's patches are published (and which drives the QEMU image and
the cross build we use): **https://github.com/altinayselman/wamr-qnx** (Apache-2.0).

**Why we opened no PR on the porting work:** #5200 already solves the two gaps we had found
(`MAP_32BIT` → QNX's `MAP_BELOW`, kept the optimization; `-ldl`/`-lpthread` → a real
`product-mini/platforms/qnx/` instead of a special case), with better solutions. A third duplicate
PR would have been noise. Our two patches remain in `upstream/wamr/patches/` as the trace of the
independent verification that let us compile and link WAMR for QNX on our machine — **not
submitted**.

---

## Key files in the repository

| Path | What it is |
|---|---|
| `core/` | the C11 decision engine: policy parser, decision, budget, chained-hash audit log, SHA-256 — pure, no platform, no allocation |
| `platform/include/eclat_platform.h` | the single interface contract shared by the two platform layers |
| `platform/posix/` | Linux implementation (clock, CPU time) |
| `platform/qnx/` | QNX implementation (`clock_gettime`, `CLOCK_THREAD_CPUTIME_ID`) |
| `runtime/` | WAMR integration: instantiation, the 7 native imports, the transport interface |
| `broker/` | the capability broker (QNX message server) + its test client + the 1 kHz loop |
| `target/eclat_run.c` | `eclat-run`, the loader/supervisor (fingerprint admission, budget kill) |
| `upstream/qnx-ports/` | the port recipe submitted as PR #337 |
| `upstream/wamr/` | `patches/` (our, not submitted), `PORTING-NOTES.md`, `build-instructions.md` |
| `host/` | the Python twin + `diff.py` (differential test) + `eclatctl` + manifest/audit tools |
| `tools/` | `preuves.sh`, `demo-*.sh`, `qnx-vm.sh`, `setup-sdp.sh`, `build-qnx.sh`, `fetch-deps.sh`, fuzz/mutants harnesses |
| `docs/preuves/` | the raw dated proof artifacts (host + QNX target) and `ETAT.md` |
| `CONTRACT.md` | the frozen contract: ABI, policy format, decision codes, budget, differential harness |
| `NOTICE` | attribution of every third-party work we build on |
| `VERSIONS` | pinned versions + sha256 of wasi-sdk |

---

## The budget line: nothing paid

| Tool | Cost | Note |
|---|---|---|
| **QNX SDP 8.0** | **Free** | *Non-Commercial / Academic* licence (series 911285-10384620). No SDP binary, header or image is redistributed: it stays on the dev machine, never in the repo, a Docker image or CI. Target artifacts in `docs/preuves/qnx800/` are transcripts of our **own** binary's console output, not SDP extracts. |
| **wasi-sdk** | **Free** | Apache-2.0 WITH LLVM-exception. Pinned `wasi-sdk-34` (34.0), sha256 verified (`b761e3a0…`). |
| **WAMR** | **Free** | Apache-2.0 WITH LLVM-exception. Cloned into `.deps/`, never committed. |
| **QEMU / KVM** | **Free** | GPL. The QNX 8.0 image is built with `mkqnximage --type=qemu`; QEMU runs the target. |
| **CI** | **Free** | GitHub Actions public runners (see the `qnx-ports` workflow in PR #337). |

No paid service, no paid licence, no paid compute anywhere in the project.