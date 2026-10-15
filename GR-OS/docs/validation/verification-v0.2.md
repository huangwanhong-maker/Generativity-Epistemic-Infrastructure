# Layers 5–6 verification record

**Class:** Implementation test evidence  
**Date:** 2026-10-13 JST  
**Scope:** GR-OS v0.2 native foundation, SDK, packaging and bounded benchmarks

## Build and behavioral results

Windows x64 Release build succeeded with Visual Studio 2022 / MSVC 19.44 and Python 3.12.10 on an Intel Core i7-11370H. The final build emitted no compiler diagnostics. All **12 CTest suites passed**, total 38.80 seconds.

| Suite | Observed evidence |
|---|---|
| records | 56 checks: retained v1 integrity/admission/relations/time cases, immutable records, strict new IDs with readable old opaque IDs, caller profile capabilities |
| history | 63 checks, including 36 generated branch revisions and independent reconstruction model, conflicts, explicit resolution, unavailable base semantics and staged provenance |
| canonical | 18 golden vectors and 200 seeded canonical fixed points; duplicate keys, unsafe numbers and UTF-8 failures |
| storage | Shared memory/SQLite contract plus 72-step seeded model per backend, old snapshots/retries, pagination, capability rejection, ranges, cancellation and reader lifetime |
| sqlite | Independent writers/retries, late rollback, read-only reopen, bounds, payload/membership/blob/ledger corruption, sequence/head guards and pinned reader view |
| execution | 48 checks: ordering, ownership, limits, worker joining, cancellation/deadlines and cross-caller serialization |
| operations | 74 checks: type/operation pins, independent extension, multiple providers/results, incompatible/lying providers, independent expected SHA vectors |
| python_binding | 16 integration tests, including binary reads and lifetime/cancellation, typed dispatch, history merge, 10000-current-record replacement, C transport and input validation |
| vendor_integrity | All 7 pinned artifact hashes match |
| example_extension | External C++ type/operation/provider registration and dispatch |
| example_python | Synthetic temporary SQLite record/artifact example and CPU computation |
| installed_sdk | Relocated native exports consumed by independent C/C++ builds; Python wheel built offline, installed and imported in isolation |

A failed early canonical fixture had an incorrectly encoded expected Unicode character; a reader fixture omitted the required-object inventory; the installed test exceeded Windows path length. These test/harness issues were corrected before the final complete passing run. No tests were disabled to obtain the result.

## Evidence and reproduction

- [Final build output](build-v0.2.txt)
- [Final complete CTest output](ctest-v0.2.txt)
- [Before-reconstruction test baseline](v0.1-baseline.txt)
- [Historical v0.1 verification](verification-v0.1.md)
- [Benchmark procedure and tables](../../benchmarks/README.md)
- [Saved comparison](benchmark-comparison-v0.2.txt)

Follow the [README](../../README.md) and [SDK guide](../sdk.md) to prepare the build-local packaging environment, build and run CTest. CMake requires Python tests by default when bindings are enabled. The installed-package test performs no network downloads. Vendor sources are pinned; packaging tools are setuptools 80.9.0 and wheel 0.45.1, installed only under the local build tree.

The tested wheel is `gr_os-0.2.0-py3-none-any.whl`, SHA-256 `46da1d36f0ae86f5ed0be310ed7ee438b14274316847788eb536937582a8317a`. It contains the Python facade, with a separately installed native library. The verified disposable installed prefix is `build/p/4ef2a649/relocated`; package artifacts are under that sibling directory. Generated build artifacts are ignored, while sources and saved evidence are retained.

## Performance observations

Ten common before/after workloads were captured with one warmup/five samples, using the same host and Release configuration. The final uncontended v0.2 medians are 0.74–1.23x the v0.1 medians; the configured 3x coarse regression check passes. Timings include Python/C transport and do not isolate kernel speed.

New binary verified-open plus full 4 MiB drain measured 27.554 ms in memory and 38.577 ms in SQLite. Already pinned reads were cheaper but exclude initial whole-object verification. Tiny typed CPU calls were slower with threading (92.767 versus 64.249 ms for 1024 one-byte calls). Full-bundle 24-checkpoint reconstruction measured 36.048 ms. These are bounded local observations, not a large-scale throughput claim. Full samples, workload details and qualifications are in the benchmark files.

## What remains unqualified

This establishes tested local contracts, installed-consumer usability and measured bounded workloads. It does not establish exhaustive semantic equivalence, GSP/GRRP migration, signature preservation, authenticated provenance, hostile-code containment, hard real-time deadlines, power-loss certification, distributed durability, GPU execution or production-scale performance.

Both SHA providers share PicoSHA2 despite separate provider paths. Descriptor hashes do not prove native code. SQLite v1 receipts cannot independently authenticate all attribution fields. Semantic histories use bounded complete checkpoints; physical stores remain linear. Full Linux/macOS packages have not been qualified. Uploads and computation transport remain copied JSON/base64, and numerical arrays lack a new equivalence contract.

Final status inspection matches the starting state outside GR-OS: parent unrelated changes remain, legacy infrastructure has its pre-existing untracked R8 archive, and generalized/academia repositories are clean. No legacy migration, global package installation, commit, push or higher-layer implementation occurred.
