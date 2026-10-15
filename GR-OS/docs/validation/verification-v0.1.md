# Layers 5–6 verification record

**Class:** Implementation test evidence  
**Date:** 2026-10-13 JST  
**Scope:** GR-OS v0.1 local native libraries and Python binding; no migration or higher layers

## Observed result

Windows x64 Release build succeeded using Visual Studio 2022 / MSVC 19.44 and Python 3.12.10. The final build emitted no compiler diagnostics. All **4 CTest suites passed**:

| Suite | Result | Main evidence |
|---|---|---|
| Native core | 47 checks passed | Canonical/hash vectors; unsafe numeric/UTF-8 rejection; type and extension registration; unknown-required protection; atomic pure candidate validation; forward/pinned references; n-ary incidence; conflicting accounts; incomplete temporal interpretations and interval cuts |
| Native storage | Passed | Shared memory/SQLite contract; immutable snapshots and versions; compare-and-swap races across separate SQLite connections; duplicate transaction replay; cross-workspace separation; binary integrity/limits; injected late transaction abort; reopen/read-only; corrupted bytes and missing snapshot membership |
| Native execution | Passed | Serial/threaded byte equality and stable order; callback extension and safety declaration; device/version rejection; payload/item/thread limits; cooperative caller cancellation and deadline; exception propagation and joined workers |
| Python/C ABI integration | 10 tests passed | Native semantic calls, record/blob custody, invalid admission atomicity, historical hydration, SQLite reopen/retry/concurrency, CPU execution/failure limits, malformed transport and allocated error responses |

The tested synthetic example completed: it wrote and read an observation plus artifact in a temporary SQLite database, verified its graph projection and computed the expected SHA-256 through the threaded CPU backend. All **7 vendored artifact SHA-256 checks** matched `third_party/MANIFEST.json`.

The native execution agent also compiled and ran its isolated component under a C++20 GCC environment. That is a component check; it does not establish a fully qualified Linux or macOS distribution.

## Reproduction and evidence

From `GR-OS`, run the README's `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\bootstrap.ps1` command to discover the installed Windows toolchain, build and run CTest. This exact helper invocation was verified after observing that direct script invocation is disabled by the local default execution policy. The flag is process-local; no system policy was changed. Python integration requires CMake to find Python. The README gives the environment and command for the synthetic example. Dependency custody verification is documented in `third_party/README.md`.

- [Saved CTest output](ctest-2026-10-13.txt)
- Latest generated log: `build/Testing/Temporary/LastTest.log`
- Native suites: `tests/test_core.cpp`, `tests/test_storage.cpp`, `tests/test_execution.cpp`
- Binding suite: `tests/test_python.py`

The observed test run took under one second; this is test-run duration, not a throughput/latency benchmark. The build and temporary runtime artifacts are ignored locally, while test sources and this evidence remain reviewable.

## What the result establishes

The tested implementation is operational at its declared local library boundaries. It retains and validates the tested semantic/history cases, publishes records and bytes atomically in the SQLite adapter, and executes bounded synchronous CPU batches through C++ and Python access.

It does not establish exhaustive theoretical adequacy, GSP/GRRP wire equivalence, legacy signature preservation, full crash/power-loss qualification, a security sandbox, hostile-native-code containment, authenticated multi-user access, GPU support, large-scale performance, or production readiness. Higher layers, retention/erasure, migration, zero-copy analytical arrays and additional backends are explicit follow-on decisions.

Existing infrastructure source and repository configuration were left unchanged; its pre-existing untracked R8 archive and the parent repository's unrelated changes remain present. No data migration, release publication, commit or push was performed.
