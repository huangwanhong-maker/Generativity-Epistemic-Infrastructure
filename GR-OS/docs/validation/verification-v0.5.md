# GR-OS v0.5 verification record

**Class:** Implementation test and performance evidence  
**Date:** 2026-10-15 JST; work began 2026-10-14  
**Scope:** Bounded Layer 3 computational language and existing Layers 4–6 regression  
**Status:** Completed; stopped for architectural review

## Environment and preserved baseline

Windows x64, Visual Studio 2022 / MSVC 19.44, CMake 3.31.6, Python 3.12.10,
NumPy 2.5.0, Intel Core i7-11370H and eight reported logical processors.
Native dependencies remain pinned in-tree. Packaging uses the existing local
setuptools/wheel environment offline. No global installation was performed.

Before implementation, unchanged v0.4 passed **28/28 cases in 46.18 s**.
Its source archive, installed SDK, Python facade and benchmark executables remain
under `build/baselines/v0.4/`; 161 source/document hashes were checked unchanged.
The [baseline manifest](v0.4-pre-v0.5/manifest.json) has SHA-256
`35c7ed9dcc0a0b93d5b4e8c93fd71e4fe5d1ce2271aee1f1f2a55c7697ef39d7`.
The [v0.4 verification report](verification-v0.4.md) is preserved.

## Final Release acceptance

After all implementation and review fixes, the complete Release suite passed
**35/35 CTest cases in 93.11 s**. No existing case was disabled. The final
[build log](build-v0.5-verified.txt),
[CTest summary](ctest-v0.5-verified.txt) and
[detailed output](ctest-v0.5-verified-details.txt) identify this run.
Earlier captures remain historical evidence; their elapsed times are not the
final run and test timings are not performance benchmarks.

| Boundary | Observed coverage |
|---|---|
| Language core | 51 checks: extension registration, logical/physical distinctions, inference, selected roots, named inputs, sharing, provenance identity and limits |
| Standard module | 40 checks: heterogeneous tables, nullable columns, checked int64 operations, layouts, invalid schemas and explicit missingness |
| Independent CSR module | 108 checks: installation, canonical CSR/shape validation, two providers, independent dense expectations and overflow |
| Qualified history | 38 checks: revisions, pinned referents, participant occurrences, roles, temporal qualifications, exclusions and derived counts |
| Python data | 12 tests including actual NumPy, endian/stride behavior, ownership, async lifetime, CLI, historical selection and partial-result cleanup |
| Existing foundation | All previous kernel, execution, history, storage, transaction/recovery, scheduler, buffer and Python suites; process-termination tests retained |
| Installation | Relocated external C/C++ consumers, offline-built wheel, native CLI and existing/new examples; installed SDK case passed in 60.88 s |

Final Release native DLL SHA-256:
`7ba373639d3027708e69ad5d53b748fcd2a0d11efa438e8f126f959e5b0dbd80`.
The tested package is `build/p/ccf4fa6d/wheels/gr_os-0.5.0-py3-none-any.whl`,
SHA-256 `a56031172d6d7029f343773c3b8b97a46f0b460c435619331b51e771ddfe08b8`.
Its sibling `relocated/` contains the separately installed native SDK.
This validates Windows/Python 3.12.10, not every platform or Python version
supported by packaging declarations.

## AddressSanitizer acceptance and limits

The final instrumented build passed **34/34 selected CTest cases in 274.14 s**,
including all new native Layer 3 suites, Python data, CLI/examples, installed
consumers and foundation regression cases. No sanitizer diagnostic was reported.
See [configure](configure-v0.5-asan.txt),
[build](build-v0.5-asan.txt), [summary](ctest-v0.5-asan.txt)
and [details](ctest-v0.5-asan-details.txt).

The sole excluded CTest case was unchanged `python_binding`, whose 10,000-record
capacity workload took 474.26 s under the previous v0.4 sanitizer run. It passed
this final Release suite; its earlier sanitizer evidence remains in the preserved
v0.4 report. This is a selected 34-case run, not a fresh 35-case ASan pass.
Within `python_data`, the optional NumPy test was skipped because NumPy is absent
from the isolated sanitizer Python environment. Actual NumPy 2.5.0 behavior
passed in Release and was separately benchmarked.

C and C++ use MSVC AddressSanitizer and debug information; C++ retains exception
handling. Links disable incremental linking. The MSVC sanitizer runtime is staged
beside the test DLL and into the isolated installed-consumer prefix. The outer
test watchdog multiplier is eight; workload inputs and runtime deadlines are
unchanged. The normal Python interpreter is not instrumented. This is neither a
ThreadSanitizer run nor a leak audit or proof of memory/race safety.

## Review fixes and compatibility

Review corrected generated operation identifiers, strict JSON object attributes,
opaque physical-revision handling, native schema validation before indexing,
stride range checks and malformed byte imports. Python logical descriptors are
recursively frozen, native specifications remain authoritative for NumPy, and
partial multi-output wrapping releases all returned handles.

The substantive cross-layer fix adds optional bounded `Graph.context`: the
Layer 3 recipe participates in plan/cache identity so equal payload bytes
cannot silently substitute different source lineage. Empty context preserves
the prior plan object and identity, verified against the preserved v0.4 SDK:
[old capture](plan-v0.4-default.json),
[new capture](plan-v0.5-default.json). Their outer SDK versions differ;
the plan objects match, including ID
`sha256:990f899549868326fe2d3d15648089274acdc38817f830469f1ce1a6891ee5aa`.

Compilation preflights the complete result provenance envelope and caps aggregate
serialized output lineage against the plan metadata budget. These are bounded
metadata contracts, not process-RSS limits. Existing C ABI 1 remains additive;
native C++ consumers must rebuild for changed public structures. Historical
storage formats, semantic identities and explicit publication remain intact.

## Benchmarks and architectural interpretation

The [measurement analysis](benchmark-v0.5-analysis.md) provides phase
tables, raw samples, correctness checks and limitations. Captures run serially
without concurrent builds/tests; background host activity and power policy are
uncontrolled. New native numerical/CSR and Python cases have one warmup and five
samples. Qualified-history cases have three samples including the first run.
The unchanged runtime runner supplies a separate v0.4 comparison, with additional
alternating-order pairs retained to investigate initial timing variance.

The language validates all three contrasting pilots and is sufficiently expressive
for further Layer 3 extension. It is not yet a high-throughput dataframe or
historical-query engine. At 262,144 rows the six-node table pipeline takes about
1,115 ms prepared serially and 995 ms with four runtime workers; only two graph
slots overlap. Generic validation, view access, multiple passes and copies are
visible targets, without a sampled profile assigning their individual costs.
The narrower fused host reference is not a semantically equivalent replacement.

The qualified 256-relation pilot takes about 190 ms for acquisition and 928 ms for
prepared transforms. Retaining immutable source objects and exact validation
reuse merits investigation before scale-up. NumPy import explicitly copies;
read-only contiguous export retains native ownership. There are no implemented
Arrow, SciPy, DLPack, GPU, JIT or distributed adapters.

## Evidence and review stop

The [source/artifact manifest](v0.5-source-artifacts.json) identifies
implementation inputs and tested binaries/packages; the
[evidence manifest](v0.5-evidence-manifest.json) identifies raw logs,
captures and reproducibility records. All 150 source-input, 97 artifact and 37
evidence entries were independently rehashed successfully; 18 current documents
passed local-link checks. Living assessment/review documents remain outside the
frozen manifests. [Build and SDK reproduction commands](v0.5-build-sdk-commands.md)
record the tested configurations. See the [benchmark protocol](../../benchmarks/README.md)
and [architectural review](../reviews/v0.5-review.md) for scope and next decisions.

All task changes are confined to GR-OS. Legacy repositories, programme sources
and application data were not migrated or modified by this increment. Stop for
architectural review before further Layer 3 scope or Layers 1–2 work.
