# Layers 4–6 verification record

**Class:** Implementation test evidence  
**Date:** 2026-10-13 JST  
**Scope:** GR-OS v0.3 runtime, native foundation, C/Python/CLI, packaging and bounded benchmarks

## Build and acceptance

Windows x64 Release built with Visual Studio 2022 / MSVC 19.44, CMake 3.31.6 and Python 3.12.10 on an Intel Core i7-11370H (eight reported logical processors). The final Release build emitted no compiler diagnostics. All **20 CTest suites passed in 35.05 seconds**. A separate native AddressSanitizer build passed all **14 selected suites in 42.77 seconds**, with no sanitizer findings.

| Suite / boundary | Observed evidence |
|---|---|
| records / history | 56 record checks; 63 history checks including 36 generated branching revisions and independent reconstruction expectations |
| canonical | 18 golden vectors and 200 seeded fixed points; strict duplicate-key/number/UTF-8 failures |
| storage / SQLite | Shared memory/SQLite conformance, seeded models, CAS/replay, pinned readers, corruption and sequence/head guards |
| execution / operations | 48 execution and 74 operation checks: contract pins, provider parity, invalid providers, limits, cancellation and serialization |
| planner / prepared | 47 planner and 10 prepared checks: whole-graph rejection, typed dependencies, selection, module/backend lifetime, bounded cache, shared payloads and actual L5 verifier parity |
| transactions | Both adapters: staging, conflicts, rollback, requirements, history custody, reserved identities, byte accounting, uncertain outcomes and frozen request recovery |
| runtime | 109 checks including 80 jobs from concurrent submitters; real callback overlap, dependency/serial order, quotas/leases, cancellation/deadlines, joined failures/shutdown, priority aging and policy-controlled artifact publication |
| runtime recovery | Acknowledgment loss recovered by receipt or exact retry; absence stays uncertain; synchronous recovery survives runtime destruction and restores outputs without provider reexecution |
| runtime crash | Separate processes terminated before publication, inside SQLite before head update, and after commit before acknowledgment; fresh-process integrity/inventory/history checks and exact retry |
| Python | 16 existing integration tests plus 17 runtime tests; the latter include 13 native/CLI cases and four focused wire/ownership test doubles |
| examples / vendor | External C++ provider, existing Python storage example, integrated artifact/history example; all seven pinned dependency artifacts verified |
| installed SDK | Independent C/C++ consumers, Python wheel imported in isolation, and native CLI against a relocated installation |

The original 12-suite v0.2 baseline passed before runtime edits. Early integrated failures are retained: one test assumed immediate worker-reference reclamation after completion; another build combined a new transaction test with an older object while sources were being edited. The lease assertion now allows bounded internal cleanup. Final acceptance used stable sources and a forced scheduler recompile. No failing tests were disabled.

## Evidence and reproduction

- [Release build](validation/build-v0.3.txt), [CTest summary](validation/ctest-v0.3.txt), [full test/consumer output](validation/ctest-v0.3-details.txt)
- [ASan build](validation/build-v0.3-asan.txt), [ASan summary](validation/ctest-v0.3-asan.txt), [ASan detailed output](validation/ctest-v0.3-asan-details.txt)
- [Exact build, packaging, ASan and benchmark commands](validation/sdk-v0.3-commands.md)
- [Pre-runtime baseline](validation/ctest-v0.2-pre-runtime.txt), [historical v0.2 verification](validation/verification-v0.2.md), [historical v0.1 verification](validation/verification-v0.1.md)
- [Benchmark protocols and observations](../benchmarks/README.md)

The installed-package test built `gr_os-0.3.0-py3-none-any.whl` offline using the preexisting local setuptools 80.9.0 / wheel 0.45.1 environment. SHA-256: `17c66c9618299bb782d393369420d6e24e195f5540ade12d5874567faa099503`. The tested relocated prefix was `build/p/5d93f436/relocated`; the wheel is under its sibling `wheels` directory. The facade and native library are installed separately. No global package installation or new dependency download occurred in this increment.

ASan instrumented C/C++ support/kernel/storage/execution/runtime, native tests and the crash worker. It excluded the C binding, CLI and Python package. MSVC emitted C5072/LNK4302 warnings because that Release build omitted debug symbols; instrumentation was active but diagnostic symbol quality is limited. This is not ThreadSanitizer, a leak audit or exhaustive race/memory-safety proof.

## Performance interpretation

Measurements run sequentially after builds/tests finish against Release binaries. Native planning and pool creation are outside prepared/scheduler timings. Prepared identity measured 1.387 ms per 4,096 calls (0.339 microseconds/invocation); one-node scheduling measured 12.104 ms per 256 jobs (47.282 microseconds/job). Scheduling is not a free replacement for a tiny direct call; batching or prepared invocation remains useful.

The same 4 MiB fork/join hash graph measured 22.960 ms with one worker, 14.260 ms with two and 8.884 ms with four: observed 2.58x one-to-four-worker scaling. Serial mode on a four-worker pool measured 26.614 ms. The graph reported 1,049,100 retained logical bytes under a 2 MiB reservation. These are not process-memory measurements; callbacks, metadata, snapshots and caller copies have separate costs.

All ten unchanged common workloads passed the configured coarse 3x regression alert. Medians were 0.69–1.23x the preserved v0.2 measurements. SQLite create/commit of 100 records increased from 15.324 to 18.796 ms (1.23x). Coordination adds validation/ownership work, but these Python/C transport measurements cannot isolate each cause without profiling. This is an observed regression, not evidence that every path improved.

The benchmark register also records concurrent SQLite batches with and without complete history tracking. Growing-work and fixed-total-work runs distinguish changing history size from concurrency effects. With identical seeded state and 32 measured commits, untracked batch-one throughput fell from 389.84 to 291.19 commits/s between one and four writers; tracked batch-four fell from 13.47 to 8.96. The latter recorded median conflicts of 0 versus 75. Complete-history admission, retries, scheduling and Python transport remain material costs; the data do not isolate SQLite locking alone. The process peak reached 48.76 MiB, a cumulative whole-process observation. Raw samples and limits are in the [benchmark tables](../benchmarks/README.md).

## Qualification limits and readiness

These results establish bounded local contracts and installed-consumer usability. They do not establish standards adoption, universal semantic equivalence, GSP/GRRP migration, authenticated provenance, hostile-code containment, hard real-time deadlines, production-scale performance or complete platform portability.

SQLite termination tests exercise genuine restart recovery without C++ stack unwinding. They do not simulate torn sectors, power/controller-cache loss, network filesystems or actual filesystem-full conditions. Resource exhaustion tests cover configured admission/staging/backend quotas; adapter-failure tests cover explicit uncertain outcomes and unavailable acknowledgments. Full machine OOM survival is not claimed. Memory storage is volatile; SQLite durability depends on the filesystem/device contract. Atomicity is one workspace/backend, not cross-store.

Semantic histories retain complete checkpoints and successive immutable complete bundles. Accumulated cost and quotas make large long-lived histories a remaining engineering issue. Physical history stays linear; schema-v1 receipt attribution limitations remain. Descriptor pins do not authenticate native code. JSON/base64 computation transport copies data and lacks array/device-memory contracts. Linux/macOS packaging and TSan remain unqualified; GPU, JIT, dynamic plugin isolation and distributed runtime are unimplemented.

The result is suitable for review and a bounded Layer 3 consumer prototype over exact host values. A broader HPC language or production multi-user platform needs the decisions in [the roadmap](planning/open_questions.md). No Layers 1–3 implementation or legacy migration occurred. Parent status matches the starting unrelated edits, legacy infrastructure retains only its preexisting untracked R8 archive, and generalized/academia application worktrees remain clean. No commit or push was made. Stop here for review.
