# GR-OS v0.4 verification record

**Class:** Implementation test and performance evidence  
**Date:** 2026-10-13 JST  
**Scope:** Layers 4–6 refinement and the isolated experimental Layer 3 consumer  
**Status:** Completed; stopped for architectural review

## Environment and preserved baseline

Windows x64 Release uses Visual Studio 2022 / MSVC 19.44, CMake 3.31.6 and
Python 3.12.10 on an Intel Core i7-11370H (eight reported logical processors).
Native dependencies remain pinned in-tree. Python packaging uses the existing
local setuptools 80.9.0 / wheel 0.45.1 environment offline; no global installation
or dependency download was required.

Before substantive edits the unchanged v0.3 Release suite passed 20/20 in 47.79 s.
All 128 source hashes and 20 prior validation artifacts were retained, alongside
a frozen installed native SDK, Python facade and runners. The
[baseline manifest](validation/v0.3-pre-v0.4/manifest.json),
[installed SDK manifest](validation/v0.3-pre-v0.4/installed-sdk-manifest.json) and
[pre-edit test log](validation/v0.3-pre-v0.4/ctest-baseline.txt) retain the evidence.
The previous [v0.3 verification report](validation/verification-v0.3.md) is
preserved as historical context, including its narrower sanitizer coverage.

## Release acceptance

The integrated Release build passed **28/28 CTest cases in 38.93 s**, without
compiler diagnostics. No previous suite was disabled.
[Build log](validation/build-v0.4.txt),
[CTest summary](validation/ctest-v0.4.txt) and
[detailed test and installed-consumer output](validation/ctest-v0.4-details.txt)
record the actual run.

A final Python-only compatibility guard refuses the optional exporter on Python
below 3.11, retaining the copy/readinto fallback. After that change and sanitizer
test-deployment adjustments, all six affected Release Python/example/package cases
passed again in 27.87 s ([summary](validation/ctest-v0.4-sdk-final.txt),
[details](validation/ctest-v0.4-sdk-final-details.txt)). Native implementation
artifacts were unchanged by those refinements.

| Boundary | Observed coverage |
|---|---|
| Compact history | 156 checks, including 28 differential branching states and a merge against v1, archive changes, semantic identities, checkpoint/delta bounds and malformed objects/catalogues |
| History storage | 81 checks across memory/SQLite: import, explicit upgrade, retained branches, corruption, custody and failed atomic publication |
| Admission | Exact memo reuse and adversarial numeric-kind changes; required source interpretations and candidate references; four cooperating writers with six dependent updates each; independent SQLite connections, pinned readers, cross-thread lease release, deadlines, cancellation, scheduler shutdown and noncooperating CAS |
| Buffers | 51 checks for alignment, mutable leases, ownership, negative/broadcast/empty/scalar layouts, safe slices/copies, overflow, conservative accounting and allocation budgets |
| Numerical contracts | 65,557 checks, including independently evaluated bounded multiplication inputs, int64 boundaries, malformed tolerance declarations and reduction-order constraints |
| Buffer execution | 19 checks for invalid types/limits, direct/prepared/threaded parity, asynchronous retention, cancellation joining and final-owner budget release |
| Experimental consumer | Selected historical source/version/provenance, owned int64 data, checked serial/threaded transformation and ordered reduction |
| Existing kernel/execution | 56 record, 63 history, 48 execution, 74 operation, 47 planner and 10 prepared checks; 18 canonical vectors and 200 seeded fixed points |
| Runtime | 109 checks including 80 jobs from concurrent submitters, quotas, actual callback overlap, dependency order, cancellation, shutdown, provider failure and publication policy |
| Storage/transactions/recovery | Both adapters, rollback/reopen, CAS and exact replay, immutable ranges, corruption guards, unknown outcomes, retained frozen requests and successful receipt recovery without provider reexecution |
| Process termination | Separate processes terminated before publication, within SQLite before head update, and after commit before acknowledgement; fresh-process integrity/inventory/history checks and exact retry |
| Python | 16 existing, 17 runtime and 9 buffer/experiment tests; some wire/ownership tests use explicit test doubles, while native and CLI paths are independently exercised |
| Installation | Relocated external C/C++ consumers; isolated offline-built Python wheel with native read-only exporter; native CLI historical-data session |
| Examples/vendor | Existing extension/Python/runtime examples and all seven pinned dependency artifacts |

The Python exporter targets CPython's 3.11+ stable ABI. The actually exercised
platform here is Windows/Python 3.12.10; targeting that ABI does not qualify every
Python version or operating system. Python 3.10 retains the copy/readinto path.
Native-only builds have no mandatory Python runtime dependency.

## Sanitizer and final package evidence

All **28 cases passed under the instrumented configuration across corrected runs**:
22 cases in the [initial run](validation/ctest-v0.4-asan-initial.txt), five affected
SDK/example/package cases in the [loader-corrected rerun](validation/ctest-v0.4-asan-sdk-timeout.txt),
and the final legacy Python suite in [474.26 s](validation/ctest-v0.4-asan-python-final.txt).
No sanitizer diagnostic was reported. This is combined coverage, not one clean
28-case invocation. The [instrumented build](validation/build-v0.4-asan.txt) emitted
no compiler diagnostics after sanitizer flag correction.

The initial six instrumented Python/package failures occurred before GR calls:
Python's restricted Windows DLL search did not locate the ASan runtime through
PATH. The corrected setup stages the toolchain runtime beside test DLLs, including
the relocated test prefix, without modifying normal packages or global installations.
The legacy 10,000-record capacity/replacement suite then exceeded its ordinary
120 s CTest watchdog. It passed all 16 unchanged tests with a 480 s
watchdog (`GR_TEST_TIMEOUT_MULTIPLIER=4`); runtime operation deadlines and test
inputs were not relaxed. This large instrumented workload has little timeout
margin on this host; allow a larger outer watchdog for future full ASan runs.
Normal Release configuration retains multiplier 1.

Detailed evidence: [initial run](validation/ctest-v0.4-asan-initial-details.txt),
[loader-corrected run and watchdog failure](validation/ctest-v0.4-asan-sdk-timeout-details.txt),
[final Python success](validation/ctest-v0.4-asan-python-final-details.txt).

Instrumented C and C++ use MSVC AddressSanitizer with debug information; C++
exception handling remains enabled. Executable, shared-library and module links
use debug information and disable incremental linking. The normal Python
interpreter itself is not instrumented. This is not ThreadSanitizer, a leak audit
or exhaustive race/memory-safety proof.

The final tested Release prefix is `build/p/2a41e3b0/relocated`. Its sibling
`wheels/gr_os-0.4.0-py3-none-any.whl` has SHA-256
`b57ad94277f208baf9feb07269d65cd294ffed5d5c5556d66a6eb5b0837a8add`.
The Release native DLL SHA-256 is
`0ee1ba62b827cd55dbfad0fd31027c168b70777735bcf9cd517b0f71bc59a0d1`.
The Python facade and native library/exporter are separately installed artifacts.
The historical Python example also verified its expected checked sum of 26.

[Exact build, SDK, sanitizer and static-check commands](validation/v0.4-build-sdk-commands.md)
record reproducible configurations and the actual corrections. The
[source/artifact manifest](validation/v0.4-source-artifacts.json) identifies 130
source/build/vendor/SDK inputs and 23 Release/ASan/package artifacts. The
[raw-evidence manifest](validation/v0.4-evidence-manifest.json) hashes 63 logs,
captures and command records, including preliminary failures. Living review
documents are deliberately outside those frozen evidence manifests.
The final independent check matched all 216 recorded source/artifact/evidence
entries; 23 current Markdown documents also passed local-link checks.

## Performance protocol

All timed workloads run sequentially after builds/tests, using Release binaries on
the same host. Fresh v0.3 reruns use the preserved installation/facade/runners.
Individual samples, environment and binary identities are retained. Small local
runs and medians do not establish confidence intervals, production tails or an SLA.

The unchanged common suite measures the same Python/C/native paths. Native
prepared/scheduler measurements exclude planning and pool startup. Fixed-work
transactions hold initial/final record state and 32 accepted commits constant
across one/two/four writers; serialized admission additionally includes queueing
before base capture. It remains a different explicit ordering policy, not weaker
CAS or durability.

The unchanged native admission runner seeds 128 records with 256-byte padding and
applies 32 one-record changes. It separately measures begin, stage, commit and
public storage-call time, and counts unique retained blob bytes rather than
SQLite file pages. Its unchanged wrapper uses v0.4's default selective-record
fallback, so it may understate direct SQLite-path improvement. Boundary timings
are not sampled CPU profiles; reader-copy work remains outside open/verification
timing. Its final node 33 is a checkpoint.

The separate historical-selection runner tests nodes 17/24/32/33 at compact depths
0/7/15/0, both at their original physical publication and by semantic selection
from the final publication. Each version uses its available public SDK path:
v0.3 transfers/reconstructs a complete bundle; v0.4 uses the native selected helper.
This measures end-to-end usability, not pure codec speed. The coordinator also
checks the pinned current state before reconstructing a different selected state.

New buffer measurements report copy import, copy-out to a preallocated destination
and genuine read-only export/release. No fictitious v0.3 buffer equivalent is used.
See the [protocol register](../benchmarks/README.md).

## Measured comparisons

The [complete comparison tables and raw links](validation/benchmark-v0.4-analysis.md)
retain every condition, preliminary timing capture and alternating-order native
repeat. Values below are bounded local observations, not universal speedups.

| Matched observation | v0.3 | v0.4 | Interpretation |
|---|---:|---:|---|
| Retained tracked-history blobs, 128 records / 33 publications | 37,150,477 bytes | 283,208 bytes | 99.24% less; object count 33→99; excludes database pages/record versions |
| Tracked SQLite begin+stage+commit, 32 updates | 15.760 s | 6.697 s | 2.35x faster; three fresh runs per version |
| Tracked memory begin+stage+commit, same fixture | 20.703 s | 5.064 s | 4.09x faster |
| SQLite older selected node, delta depth 15 | 261.260 ms | 116.597 ms | 2.24x faster via each SDK's available end-to-end path |
| SQLite latest selected checkpoint | 220.611 ms | 9.638 ms | 22.89x faster; not worst-case reconstruction |
| Four tracked SQLite writers, accepted commits/s | 6.89 optimistic | 8.91 optimistic; 10.86 serialized | Medians over three fixed-work runs; conflicts 68→71/0 |
| Common SQLite create/commit of 100 records | 18.973 ms | 18.217 ms | About 4% faster; earlier reported 23% regression is not fully recovered |
| Prepared identity, 4,096 calls | 1.425 ms | 1.521 ms | Pooled 28-sample medians; 6.7% slower |
| One-node runtime, 256 jobs | 13.353 ms | 15.274 ms | Pooled 28-sample medians; 14.4% slower |

The common suite passed its coarse 3x alert; several medians nevertheless rose.
The four-writer untracked batch-one optimistic case fell from 272.11 to 205.06
commits/s. Serialized tracked latency includes queueing: with four writers its
median rose to 348.08 ms, while measured p95 fell to 570.53 ms versus the old
216.88/1436.86 ms. These short-run percentiles do not establish production tails.

The native serial-hash first capture regressed 55%, prompting three additional
alternating-order pairs. All four retained captures yield a descriptive pooled
12.7% increase; per-pair differences vary markedly. Four-worker hashing improved
about 5.3% pooled, with about 2.80x observed one-to-four-worker scaling in v0.4.
Neither the initial magnitude nor any specific source-level cause is established
as generally reproducible. Prepared calls were slower in every pair; batching
meaningful work and reusing plans remain important.

The new 4 MiB buffer path measured 3.094 ms copy import, 0.331 ms readinto and
4.494 ms for 1,000 read-only export/release pairs (about 4.49 microseconds each).
Export acquires ownership/metadata without computing over the payload. It has no
v0.3 counterpart. Checksums and post-close lifetime checks were outside timing.

The admission profile places most tracked latency outside physical commit:
old SQLite commit-boundary time was 1.170 s of 15.760 s. Source inspection and
the unchanged fixtures support eliminating repeated history/validation work,
not attributing the old common 23% regression entirely to one cause. Many small
compact-object opens increased measured SQLite open/verification time despite
lower total latency. Current inventory and object-access costs remain targets.
No correctness, isolation or durability check was relaxed.

## Guarantees and qualification limits

Records, admitted binary/history objects, manifest, receipt and head movement
publish atomically within one workspace/backend. Layer 5/candidate validation
remains active; immutable memo reuse does not skip candidate-wide constraints.
Pinned snapshots plus whole-workspace CAS remain the isolation boundary.
Optional FIFO admission coordinates cooperating writers in one process; direct
writers, aliases and other processes still face physical CAS.

MemoryStore is volatile. SQLite remains WAL/FULL on local storage, with durability
dependent on filesystem/device synchronization. Process termination tests do not
simulate torn sectors, controller-cache or power loss, actual filesystem-full
conditions, or network filesystems. Configured quota and injected-adapter failures
are tested; whole-machine OOM survival is not claimed.

Unknown outcomes preserve the exact frozen request. A matching receipt establishes
acceptance; absence means not observed, not proof of rollback. Exact retry does not
rerun computations or semantic interpretation. An acknowledged successful commit
remains known after later cancellation or backend unavailability. Runtime job
intent itself is not durably persisted across process restart. External effects
and cross-store transactions remain outside these guarantees.

Compact history preserves existing v1 semantic nodes and explicit merge decisions,
but ordinary selected reads are not full-DAG audits. Catalogue lookup is linear;
state hashing, candidate dependency checks and physical snapshot membership remain
O(snapshot). Histories retain the 1,024-node limit, per-session reconstruction
budgets and workspace/object bounds. Large imports/upgrades can fail the 128-object
atomic limit earlier. No garbage collection or automatic archival policy is added.

Owned typed buffers support host memory. Allocation budgets are opt-in and exclude
arbitrary provider temporaries/process memory. Native plugins remain trusted;
cancellation is cooperative. No complete Arrow/DLPack adapter, device synchronization,
floating acceptance engine, GPU/MPI backend, JIT or distributed scheduler is claimed.
Numerical fingerprints identify declared contracts, not universal correctness proofs.

## Readiness and scope

The [architectural assessment](reviews/v0.4-review.md) finds the foundation ready
for a bounded Layer 3 workload after review. A broad general-purpose language,
production multi-user service or heterogeneous HPC system still requires explicit
workloads, logical-type/serialization policies, numerical acceptance rules,
ecosystem/device adapters, durable intent/authority and platform qualification.
Linux/macOS, ThreadSanitizer, power-loss qualification and general race/leak freedom
are not established by this Windows run.

All implementation changes belong to GR-OS. Legacy repositories and unrelated
programme sources were not modified by this increment. No migration, commit or
push was performed. Broader Layer 3 implementation stops for architectural review.
