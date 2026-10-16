# Bounded performance measurements

**Class:** Informative engineering evidence  
**Status:** Local Windows Release measurements; no throughput SLA or production-scale claim

## v0.7 module measurements

Run [the unchanged-workload driver](../tools/measure_v07.py) only after all builds,
tests and other benchmarks have stopped. It checks that the four common workload
sources match the preserved v0.6 archive, then runs native data in old/new and
new/old order, qualified history, runtime and Python/NumPy measurements. A new
`--label` is required for each capture; prior evidence is never overwritten.

```text
python -B tools/measure_v07.py --label final
python -B tools/measure_v07_modules.py --package-run build/m/ACCEPTED_RUN --label final
```

The independently installed analysis and visualization packages have opt-in native
benchmark targets (`GR_MODULE_BUILD_BENCHMARKS=ON`). Analysis separates tracked
historical acquisition, incidence computation, first indexing, repeated summaries
and comparisons, snapshot export/import and scheduled summary. Visualization
separates layout, SVG, copied interchange and a synthetic end-to-end publication/
selection/analysis/render workflow. Analysis sizes 64, 256 and 768 and visualization
sizes 32, 256 and 1,024 exercise repeated participant occurrences. Setup and cumulative peak RSS remain explicit.

`benchmark_analysis 3 FIXTURE_DIRECTORY` also exports qualified source fixtures
outside measured phases for the installed Python package runner
`packages/gr_relational_analysis/benchmark.py`. Its raw-byte import boundary and
portable export/revalidation costs are separate from persistent native queries.
Use the accepted installed wheel/library paths, not a different source build.
These portable projections omit source bodies but are not guaranteed to be smaller
than every input. Scene/SVG sizes and transport costs matter alongside CPU timings.

[Final captures and interpretation](../docs/validation/benchmark-v0.7-analysis.md)
record retained gains, slower cases, import/output costs and timing variability. Native byte equality, analytical equivalence and rendered-pixel
appearance are different checks; no timing threshold substitutes for conformance.

## v0.6 maturation measurements

[The v0.6 analysis](../docs/validation/benchmark-v0.6-analysis.md) compares the
preserved v0.5 executables and Python facade with unchanged-workload v0.6 runners.
The [serial driver](../tools/measure_v06.py) runs native data in old/new then new/old
order, qualified histories, the existing runtime and Python/NumPy transport. It
then measures access paths, validation/decode caches and the independently
installed package. Run it only after all build/test work has ended:

```text
python -B tools/measure_v06.py --package-run build/x/ACCEPTED_RUN --label final
```

The package directory must contain the successful independent-package harness's
`artifacts.json`. The driver uses that installed wheel/native library, records
commands/return codes/capture hashes and refuses to overwrite prior evidence.
The baseline comes from `build/baselines/v0.5/`. A subsequent run needs a new label.

Native and Python data cases use one warmup and five samples; qualified history
uses three including the first; the unchanged runtime uses seven. New language
profiles cover 8/4096/262144 elements with nullable/nonnullable and memo-off/on
cases. Access profiles include forward, reverse and zero strides; copy timings
include allocation/destruction. Relational profiles separate off/cold/warm contexts
at 64/256/768 relations, with decoder/validator/derivation/encoding counters and
timings. The package measures synchronous copied Python/C++ execution at
16/4096/65536 elements with an independent exact result oracle outside timing.

Instrumented phase timing is not sampled CPU attribution. Explicit copy-byte
counts, retained-buffer charges, serialized cache charges and cumulative RSS are
different measures. Fixed-order local experiments do not control host background
activity, affinity or power policy. Preserve regressions and variation; do not
infer universal speedups, acceleration or scalability from these bounded fixtures.

## v0.5 Layer 3 pilots

The [complete v0.5 phase analysis](../docs/validation/benchmark-v0.5-analysis.md)
records native nullable tables, independent CSR providers, qualified historical
transforms, Python/NumPy transport and unchanged-runtime comparisons. These
measurements establish a working extension foundation and expose substantial
generic validation, view-access and copying costs; they do not establish a
high-throughput dataframe engine.

Run Release workloads sequentially without concurrent builds/tests:

```text
benchmark_data OUTPUT.json
benchmark_data_relational
python -B benchmarks/data.py --output OUTPUT.json
benchmark_runtime OUTPUT.json
```

Set `GR_OS_LIBRARY` to the Release native library and `PYTHONPATH` to
`bindings/python/src` for the Python runner. Redirect the relational runner's
stdout to its JSON capture. Native data and Python cases use one warmup and five
samples; relational cases use three samples including the first. The runtime
runner uses seven samples and excludes preparation/pool startup. Preserve all
samples and result checks. Two extra alternating-order old/new runtime pairs
investigate the first pair's scheduler variance; do not select the fastest pair.
See [data-value protocol](../docs/data-values.md) and
[relational protocol](../docs/relational-data.md) for exact fixture/phase scopes.

## v0.4 measured results

The [complete matched analysis](../docs/validation/benchmark-v0.4-analysis.md)
contains all common, contention, admission, history-position, dispatch and buffer
tables with raw captures. Fresh v0.3 reruns use the preserved SDK; earlier results
below remain historical evidence. Three extra alternating-order native pairs
retain and investigate the initial timing variance instead of replacing it.

| Observation | v0.3 | v0.4 |
|---|---:|---:|
| Tracked retained history blobs, 128 records / 33 publications | 37,150,477 bytes | 283,208 bytes |
| Tracked SQLite begin+stage+commit, 32 updates | 15.760 s | 6.697 s |
| SQLite selected older node, delta depth 15 | 261.260 ms | 116.597 ms |
| Four tracked writers, accepted commits/s | 6.89 optimistic | 8.91 optimistic / 10.86 serialized |
| Common SQLite create/commit, 100 records | 18.973 ms | 18.217 ms |
| Prepared calls, 4,096; pooled native samples | 1.425 ms | 1.521 ms |
| Runtime jobs, 256; pooled native samples | 13.353 ms | 15.274 ms |

Compact history is 99.24% smaller in retained blob bytes in this fixture, while
object count increases from 33 to 99. Small-dispatch overhead and some snapshot,
transport and contention cases regress. Common SQLite commit improves only about
4%; the earlier 23% increase is not fully recovered or causally isolated. The
analysis records queue-inclusive latency, checkpoint/deep-chain distinctions,
measurement variability and residual inventory/object-access costs. No blanket
speedup or production-scale conclusion follows.

## v0.4 comparison protocol

The pre-edit v0.3 installation, Python facade, old runners and source hashes are
preserved in `build/baselines/v0.3/` and [the evidence manifest](../docs/validation/v0.3-pre-v0.4/manifest.json).
Run Release benchmarks serially, with no concurrent build/test workload. The common
runner and native prepared/scheduler workload remain the comparison baseline.
Use the frozen old transaction runner/facade for v0.3; the v0.4 runner adds only an
explicit admission-policy selection to the same fixed accepted-work fixture.

`benchmarks/admission.cpp` compiles unchanged against both installed SDKs. For
MemoryStore and SQLite, with and without history, it seeds 128 records carrying
256-byte padding and performs 32 one-record changes. Timings separate begin,
stage and commit and wrap public storage calls. `open_blob` includes opening and
whole-object integrity verification; subsequent reader-copy calls remain in the
outer phase time. These observations localize costs to boundaries, not individual
CPU instructions. Retained history counts unique uploaded blob bytes, excluding
database pages, record versions, ledger and indexes. Final 128-record/33-publication
invariants are checked outside timings. Each invocation requires a new directory.
The unchanged adapter wrapper does not override v0.4's selective-record read or
coordination hint; default fallback may load a full snapshot. This comparison may
understate improvements of the direct SQLite path. It measures optimistic admission.

```text
benchmark_admission NEW_OUTPUT_DIRECTORY
python -B benchmarks/transactions.py --mode fixed --admission optimistic --output optimistic.json
python -B benchmarks/transactions.py --mode fixed --admission serialized --output serialized.json
python -B benchmarks/buffers.py --output buffers.json
python -B benchmarks/history_reads.py --output history-reads.json
```

Serialized admission explicitly selects the head after acquiring a process-local
writer lease. It changes scheduling policy, not accepted work, WAL/FULL or CAS.
Report its queue-inclusive latency and throughput separately from optimistic retry
counts. New compact histories have different physical receipt identities; compare
record results, retained semantic identities where attribution is matched, and
publication counts rather than requiring old/new physical revisions to match.

The buffer runner measures copy import, copy-out into a preallocated destination,
and 1,000 genuine read-only export/release pairs for 1 KiB, 1 MiB and 4 MiB payloads.
Checksums and post-close lifetime checks occur outside timing. This is a new API
without a v0.3 counterpart; it does not imply ecosystem-wide or GPU performance.

The identical historical-selection runner creates 32 records with 256-byte padding,
then a seed and 32 single-record updates. It measures nodes 17, 24, 32 and 33, whose
v0.4 compact delta depths are 0, 7, 15 and 0. Each node is selected both from its own
physical publication and by semantic identity from the final publication. Five
samples follow one warmup per query; selected identity and record contents are
checked outside timing. v0.3 reads/transfers its complete bundle and invokes public
v1 reconstruction; v0.4 uses native selected reconstruction. This is an end-to-end
SDK comparison, not a pure codec benchmark. Automatic physical-base attribution
differs across encodings, so new semantic identities are checked within each run,
while record inventories and generations remain matched across versions. The
33-node final read in the separate admission fixture is a checkpoint; do not use
that result to represent worst-case delta reconstruction.

## Existing common protocol

Run from GR-OS with the desired DLL and facade selected:

```powershell
$env:GR_OS_LIBRARY = (Resolve-Path build/Release/gr_native.dll).Path
$env:PYTHONPATH = (Resolve-Path bindings/python/src).Path
python -B benchmarks/run.py --output build/benchmark.json --label local --repeats 5
python -B benchmarks/compare.py docs/validation/benchmark-v0.2.json build/benchmark.json --check
python -B benchmarks/run_v0_2.py --output build/new-contracts.json --repeats 5
```

The common suite uses unchanged workloads across v0.1 and v0.2. Its recorded script SHA-256 is `7efaa3105914435aa4dc647a1138c79c4e4af404fcfa54bf0ad21454cbce5a1d`. Changing fixtures or timed work requires a new suite identity or fresh comparable baselines. The additional v0.2 suite has no v0.1 comparison.

Each case performs one warmup and five samples by default, retaining every sample and median/min/max. It includes Python serialization, C ABI transport, native validation and computation. Fixtures/database setup are outside timings except explicitly measured workspace creation/commit. SQLite commit repetitions use fresh workspaces in one growing warmed database, not a reset database for each sample. Execution/storage cases assert expected outputs; separate conformance tests validate kernel behavior. Run those tests before interpreting timings.

Measurements include platform, architecture, Python version, logical CPU count and DLL hash. Use the same machine, compiler/configuration, Python and workloads without concurrent builds/tests. Compiler/CPU model are not automatically recorded by the runner; this capture used MSVC 19.44 x64 Release and an Intel Core i7-11370H at nominal 3.30 GHz. Record those separately when comparing another host. Filesystem cache, power state and background work can affect results. Five samples do not establish a confidence interval.

The optional comparison gate rejects incompatible environment/method/workload inventories and nonfinite/nonpositive medians. Its default 3x threshold is an intentionally coarse regression alert, not an accepted performance budget. Use `--max-ratio` only with an agreed target and controlled environment. It is not part of default CTest. Semantic conformance remains independent of speed.

## Layer 4 protocols

The [native runtime protocol](runtime-protocol.md) separates prepared invocation
from scheduler submission and measures the same 4 MiB fork/join hash graph in
serial mode and with one, two and four workers. It records seven samples per
case, checked outputs, selected execution descriptions, observed peak slots and
retained logical payload. Planning, pool creation and language transport are
outside those measurements; they are not end-to-end Python latency.

The concurrent transaction runner uses the installed/public Python interfaces:

```powershell
python -B benchmarks/transactions.py --mode fixed --output docs/validation/benchmark-v0.3-transactions.json
```

It performs one global warmup, then three fresh-database repetitions per case:
one/two/four independent SQLite connections, one or eight records per untracked
commit, and four records per history-tracked commit. Fixed mode measures exactly
32 accepted commits for every writer count, after an identical untimed seed of
four record groups. A single writer rotates through all groups; two writers each
own two groups; four writers each own one. The initial state, final state and
total accepted work therefore stay constant. Each final ledger contains the seed
plus 32 measured commits. Native semantic admission remains active throughout.

WAL/FULL is unchanged. Connection opening is excluded; throughput begins when all
writers reach a barrier. Final connection close/checkpoint work is included.
Per-commit latency includes record construction, JSON transport, staging,
publication and conflict reconciliation. The final read-only snapshot, complete
ledger and every accepted receipt/digest, including the seed, are checked outside
the timed interval. The seed request/revision is checked identical across writer
counts for each batch/history condition.

Concurrent writers own disjoint synthetic record groups. A conflict explicitly starts a new
logical transaction with a new identity and refreshed base. This is a deliberate
benchmark application policy, not a runtime automatic rebase or exactly-once
retry. v0.3 history tracking retains complete bundles/checkpoints; v0.4 fresh
automatic histories use compact objects. Both retain semantic history and its
artifact custody; their representation cost remains visible. Cases retain raw latency
samples, throughput, p95 (nearest rank), conflict counts and environment identity.

On Windows, the transaction runner also samples whole-process working set and
the cumulative process peak. These include Python, native libraries, allocators
and SQLite; a later case inherits the earlier high-water mark. They are useful
host observations, not isolated per-job memory measurements or runtime quotas.
The original [growing-work run](../docs/validation/benchmark-v0.3-transactions-growing.json)
is preserved separately: it measured eight commits per writer, so total work,
current records and history depth grew with writer count. Its throughput decline
cannot be attributed solely to contention. `--mode growing` retains that workload;
the controlled suite is identified as `gr.runtime.transactions/2`.

All cases are small local workloads; neither three repetitions nor the p95 of
those short runs establishes production tail behavior or general scaling.

## v0.3 native runtime observations

[Raw native data](../docs/validation/benchmark-runtime-v0.3.json), Windows x64
Release, Intel Core i7-11370H, MSVC 19.44, seven measured repetitions:

| Workload | Median total ms | Per-call / interpretation |
|---|---:|---|
| 4,096 prepared one-byte identity invocations | 1.387 | 0.339 microseconds/invocation; no job lifecycle |
| 256 one-node runtime identity jobs | 12.104 | 47.282 microseconds/job; existing pool and plan |
| 4 MiB fork/join hashing, serial job, four-worker pool | 26.614 | One active node at a time |
| Same graph, one worker | 22.960 | Parallel scheduling enabled, capacity one |
| Same graph, two workers | 14.260 | 1.61x versus one worker |
| Same graph, four workers | 8.884 | 2.58x versus one worker |

Job submission is much more expensive than prepared direct invocation for tiny
operations; use the prepared fast path or batch meaningful work. The serial
four-worker condition was slower than the one-worker pool on this run. These
measurements include synchronization, result checks and stated diagnostics, but
exclude planning/pool startup/language transport. They do not establish a general
speedup or predict a different workload. Seven samples provide limited precision.

The graph reserved 2,097,152 logical bytes and recorded 1,049,100 retained payload
bytes in every condition, with observed peak slots 1/1/2/4. Shared aliases are
conservatively counted per retained value; this is not physical RSS or allocator
instrumentation. Cumulative process memory is reported separately by the
transaction runner.

## v0.2/v0.3 unchanged common workloads

[Preserved v0.2 data](../docs/validation/benchmark-v0.2-pre-runtime-preserved.json),
[v0.3 data](../docs/validation/benchmark-v0.3.json) and
[comparison gate output](../docs/validation/benchmark-v0.3-comparison.txt):

| Workload | v0.2 median ms | v0.3 median ms | Ratio |
|---|---:|---:|---:|
| Construct 100 records | 4.705 | 4.648 | 0.99 |
| Validate 300 records | 14.177 | 9.803 | 0.69 |
| Graph 300 records | 15.723 | 11.218 | 0.71 |
| Replace 100 of 300 records | 34.650 | 27.193 | 0.78 |
| SHA-256 1 MiB, sequential | 33.686 | 30.473 | 0.90 |
| SHA-256 1 MiB, threaded | 28.960 | 26.445 | 0.91 |
| Create/commit 100, memory | 18.326 | 14.335 | 0.78 |
| Create/commit 100, SQLite | 15.324 | 18.796 | 1.23 |
| Base64 blob read 4 MiB, memory | 110.175 | 82.619 | 0.75 |
| Base64 blob read 4 MiB, SQLite | 96.545 | 90.593 | 0.94 |

All ten cases passed the unchanged 3x coarse alert. SQLite commit is a measured
23% regression in this capture; native coordination adds validation/ownership work,
but these measurements do not isolate causation. Several other paths improved.
Environment noise, warm caches and transport remain part of the observation;
there is no blanket performance claim or statistical confidence interval.

## v0.3 concurrent transactions with fixed accepted work

[Controlled raw data](../docs/validation/benchmark-v0.3-transactions.json) and
[runner output](../docs/validation/benchmark-v0.3-transactions-fixed.log) retain
three samples for each condition. All 27 measured samples verified the identical
seed per batch/history mode, 32 measured accepted commits, 33 physical ledger
entries, exact receipts and final generations. Each row holds accepted work and
initial/final record inventory constant across writer counts.

| Workload | 1 writer commits/s | 2 writers commits/s | 4 writers commits/s | Median conflicts, 1 / 2 / 4 writers |
|---|---:|---:|---:|---|
| No history, one record/commit | 389.84 | 325.87 | 291.19 | 0 / 3 / 6 |
| No history, eight records/commit | 116.26 | 97.93 | 90.56 | 0 / 14 / 19 |
| Tracked history, four records/commit | 13.47 | 11.25 | 8.96 | 0 / 27 / 75 |

All rates are medians. More writers did not improve any tested condition, despite
disjoint writer record groups. Whole-workspace CAS causes conservative conflicts;
retries repeat native admission and, when enabled, complete history validation.
Thread scheduling and Python/JSON transport also contribute. These measurements
cannot attribute the entire decline solely to SQLite locking. Different rows have
different batch sizes, so they are not a matched isolation of history overhead.
The tracked workload establishes a concrete bounded result, not scalable long-lived
history. Per-record conflict protocols or incremental history need another design
increment; correctness was not relaxed to improve throughput.

Post-verification working-set observations ranged from 29.81 to 43.93 MiB; the
cumulative whole-process peak reached 48.76 MiB. This includes Python/native/SQLite
and earlier cases, excludes any claim of per-job allocator attribution, and does
not establish leak freedom. Native job retained-payload diagnostics have a separate
scope described above.

For comparison, the preserved growing-work capture measured tracked batch-four
throughput of 54.25 / 26.78 / 11.89 commits/s with one/two/four writers. Its accepted
work and history depth also grew from 8 to 32 commits; those values should not be
used as a pure concurrency scaling curve. Both captures are retained deliberately.

## Historical v0.1/v0.2 common workloads

Milliseconds, medians; [v0.1 raw data](../docs/validation/benchmark-v0.1.json), [v0.2 raw data](../docs/validation/benchmark-v0.2.json).

| Workload | v0.1 | v0.2 | Ratio |
|---|---:|---:|---:|
| Construct 100 records | 6.318 | 4.705 | 0.74 |
| Validate 300 records | 12.643 | 14.177 | 1.12 |
| Graph 300 records | 13.032 | 15.723 | 1.21 |
| Replace 100 of 300 records | 31.329 | 34.650 | 1.11 |
| SHA-256 1 MiB, sequential | 30.494 | 33.686 | 1.10 |
| SHA-256 1 MiB, threaded | 29.452 | 28.960 | 0.98 |
| Create/commit 100, memory | 14.956 | 18.326 | 1.23 |
| Create/commit 100, SQLite | 14.193 | 15.324 | 1.08 |
| Base64 blob read 4 MiB, memory | 103.307 | 110.175 | 1.07 |
| Base64 blob read 4 MiB, SQLite | 108.136 | 96.545 | 0.89 |

No common workload exceeded the configured 3x alert. Several medians increased; the table is not evidence of a general speedup. Both versions use the same byte convenience path, including base64, so the comparison does not silently credit the new API with an incompatible transport baseline.

## New contract paths

[Raw measurements](../docs/validation/benchmark-v0.2-new-contracts.json):

| Workload | Median ms |
|---|---:|
| Verify/open 4 MiB, memory | 26.857 |
| Verify/open and drain binary reader, memory | 27.554 |
| Drain already pinned reader, memory | 2.098 |
| Verify/open 4 MiB, SQLite | 35.032 |
| Verify/open and drain binary reader, SQLite | 38.577 |
| Drain already pinned reader, SQLite | 6.775 |
| Validate 24 checkpoints, 16 current records each | 22.294 |
| Reconstruct from that full bundle | 36.048 |
| 1024 typed one-byte identity calls, sequential | 64.249 |
| Same typed calls, threaded | 92.767 |

Reader drains use 64 KiB buffers and check every chunk. Opening verifies the full object; pinned-read results exclude that cost and must not be quoted as end-to-end authenticated read latency. The base64 and binary paths are separately named; their difference includes transport/allocation work.

Tiny typed operations were slower when threaded. Repeated descriptor validation/copying and transport are candidates for profiling and prepared immutable validation handles, not a reason to claim automatic parallel speedup. Whole-bundle history work includes validation of all checkpoints. Indexed/delta history and array interfaces should follow representative workload targets.
