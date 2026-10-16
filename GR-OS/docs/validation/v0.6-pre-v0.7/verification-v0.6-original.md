# GR-OS v0.6 verification record

**Class:** Implementation test and performance evidence  
**Date:** 2026-10-15 JST  
**Scope:** Layer 3 maturation and existing Layers 4?6 regression  
**Status:** Completed; stopped for architectural review

## Environment and preserved baseline

Windows x64, Visual Studio 2022 / MSVC 19.44, CMake 3.31.6, Python 3.12.10,
NumPy 2.5.0, Intel Core i7-11370H and eight reported logical processors.
Dependencies remain pinned in-tree; packaging used the existing build-local
setuptools/wheel environment offline. No global package installation was performed.
Other operating systems, compilers and devices have not been qualified by this run.

Before edits, all 247 v0.5 source/artifact manifest entries matched and the unchanged
Release suite passed **35/35 in 67.87 s**. The 189-file source/document archive,
installed SDK, facade, runners and benchmark executables were preserved.
[Baseline manifest](validation/v0.5-pre-v0.6/manifest.json),
[baseline CTest](validation/v0.5-pre-v0.6/ctest-baseline.txt) and
[previous verification](validation/verification-v0.5.md) retain that evidence.
A final [baseline recheck](validation/v0.6-baseline-reverification.json) verified
all 189 archived entries and five preserved native binaries against accepted
v0.5 hashes. The four matched benchmark sources remain byte-identical to v0.5.

## Release acceptance

The final full suite passed **38/38 in 106.64 s**, after package-contract and
numerical-profile review corrections. See [configure](validation/configure-v0.6-release-final.txt),
[build](validation/build-v0.6-release-final.txt),
[summary](validation/ctest-v0.6-release-final.txt) and
[details](validation/ctest-v0.6-release-detail.txt).
The earlier combined 38/38 run is retained as initial evidence.

| Area | Accepted coverage |
|---|---|
| Computational language | 83 checks: typed expressions, exact provenance/context, weak-owner cache isolation/eviction/failure/concurrency, Function sharing/substitution and specialization reuse |
| Numeric extension | 89 checks: checked unsigned boundaries, independent expected floating/complex bits, scalar/batch agreement, missingness, bounds, hostile floating environment restoration, cancellation and allocation rollback |
| Host access | 58 checks: owning ranges, little-endian loads, reversed/zero strides, empty/extreme geometry and lifetime |
| Standard and CSR | 42 and 108 checks; nullable access, shape, overflow, alternative providers and independent expectations |
| Qualified histories | 61 checks: revisions/referents/occurrences/temporal qualifications, hostile provenance/new owners, exact cache-on/off bytes, bounded session reuse and concurrency |
| Python | 17 data cases plus legacy 16, runtime 17 and buffer 9; actual NumPy uint64/complex copies and read-only owner lifetime, Function ownership and partial-wrapper fault injection |
| Existing foundation | Kernel, execution, recording histories, SQLite/memory transactions, frozen recovery, scheduler, crash/process-termination, buffers and CLI cases retained |
| Installation | Relocated C/C++ consumers, offline GR wheel and examples; separate installed source package, dependency rejection, relocated C++ Function consumer and package wheel/C facade |

A subsequent sanitizer run exposed a **test-harness startup defect**, described
below. Its correction stages the required runtime earlier and consistently uses
the build-local packaging environment. The affected Release installed-SDK test
was rerun and passed **1/1 in 21.62 s**; production libraries were unchanged.
[Corrected Release summary](validation/ctest-v0.6-release-sdk-rerun.txt),
[details](validation/ctest-v0.6-release-sdk-detail.txt).

Final Release native DLL SHA-256:
`bb2f6850187ef760e423db207bdc8d2920404c5fe6db1ca50ceb0ea37e0fb77a`.
The corrected installed SDK prefix is `build/p/59eb42e4`; its wheel hash is
`f31e7565b8f3ad6e90c799942bec51c2e0950dcc46ebc325d7d71a147b5ddbb0`.
The earlier accepted full-suite prefix `build/p/85df9ba2` is also retained.
Wheel timestamps may differ between builds; byte-identical archives are not claimed.

## AddressSanitizer and the corrected failure

MSVC AddressSanitizer Release instrumented native C/C++, including SQLite and
external consumers. The global interpreter made actual NumPy tests available;
Python and NumPy themselves were not sanitizer builds. Outer watchdogs used
multiplier eight without changing operation deadlines.

The initial selected run passed **36/37 in 357.95 s**. `installed_sdk` failed
before its C consumer started, with Windows status 3221225781 (missing DLL).
The harness had staged the ASan runtime only after its first native consumer.
It now stages the DLL beside both consumer executables and in the relocated
native-library directory before use; all copies/environment changes remain local.
This was a setup failure, not a reported memory-safety finding.
[Initial summary](validation/ctest-v0.6-asan.txt) and
[initial details](validation/ctest-v0.6-asan-initial-detail.txt) preserve the failure.

The corrected affected test passed **1/1 in 25.12 s**:
[summary](validation/ctest-v0.6-asan-sdk-rerun.txt),
[details](validation/ctest-v0.6-asan-sdk-detail.txt).
Thus **all 37 selected tests have passing evidence across these runs**, rather
than a falsely clean first full run. Actual NumPy and the independent package
passed in the initial run; package installation used `build-asan/x/ee5e8379`.
[Configure](validation/configure-v0.6-asan.txt) and
[build](validation/build-v0.6-asan.txt) retain compiler evidence.

Only the unchanged legacy `python_binding` CTest was excluded from ASan: its
10,000-record capacity fixture has extreme instrumented allocation overhead.
It passed in full Release. No optional NumPy subtest was skipped in this v0.6
sanitizer selection. No TSan, non-Windows, power-loss or arbitrary-device
qualification is claimed. Tests cannot establish absence of all defects.

## Independent distribution and compatibility

`packages/gr_diagonal` was copied, configured and installed independently against
a relocated installed GR 0.6.0 SDK. It supplies a custom type, representation,
operator and CPU implementation, then composes with standard reduction through
Function and the existing Runtime. Missing SDK and actual v0.5 dependency probes
fail for the intended reason; native/Python tests check independent results,
overflow, mismatch, bounded inputs, deadline and diagnostics.

The accepted Release run is `build/x/b775460c`; its `commands.json` and
`artifacts.json` retain exact subprocesses, negative diagnostics, manifest and
hashes. Package native DLL SHA-256:
`c0e156372f906e42695168509d3665d9b0ee06e06dff87d769f16fc827cdd746`.
Its offline Python wheel SHA-256:
`db52271c84b66281ef5d8235d4945b96c3c4cb733e97aaa79c8fd797feb03de8`.
This proves the tested **source/API build boundary**, with exact SDK and matching
compiler/runtime/configuration. The Python facade is a copied synchronous package
boundary, not shared opaque-handle registration or a portable native plugin ABI.

The explicit new default-plan fixture in
[`capture_default_plan.py`](../tools/capture_default_plan.py) gives identical full
plan objects for [frozen v0.5](validation/plan-v0.5-for-v0.6.json) and
[v0.6](validation/plan-v0.6-default.json), including ID
`sha256:c521b4aab5d85c0c8b768f68d7dff4e2bab23743335c9e145ec359bd2c683bac`.
The earlier plan capture omitted its payload; this new fixture does not claim to
reproduce that undocumented input. Existing Layer 4/runtime and storage source
files are unchanged; kernel Value gains only an additive weak ownership token.

## Measurements, evidence and stop point

[Benchmark analysis](validation/benchmark-v0.6-analysis.md) distinguishes matched
whole workloads, current cache/access profiling, transport costs, regressions and
host variability. Timings are local synthetic evidence without controlled affinity,
power state or background activity. Counters and timed phases are not sampled CPU
or hardware-counter attribution. Cache/owner byte charges are not process RSS.

[Source/artifact manifest](validation/v0.6-source-artifacts.json) and
[evidence manifest](validation/v0.6-evidence-manifest.json) identify accepted inputs,
binaries, logs and captures. [Build commands](validation/v0.6-build-sdk-commands.md),
[exact SDK/package commands and outcomes](validation/v0.6-sdk-commands.md), and
[benchmark driver](../tools/measure_v06.py) support reproduction. Historical failed
and preliminary runs are retained; living review documents remain outside frozen
source/artifact identities.

All **172 source-input, 245 artifact and 66 evidence entries** were independently
rehashed successfully. The 22 current/referenced reports checked here contain
210 valid local links. Parent repository status matches the pre-existing unrelated
changes recorded at task start; this increment adds no programme-source edits.

See the [architectural review](reviews/v0.6-review.md) and
[next decisions](planning/open_questions.md). Selected independent host CPU modules
can use the implemented extension contracts. Approximate computation, device and
streaming contracts, wider platform qualification and broader Python registration
remain explicit future work. All task changes stay within GR-OS; legacy repositories,
programme sources and applications were not migrated. Stop for architectural review
before broad Layer 2 development.
