# v0.6 measured performance and remaining costs

Class: local engineering evidence. Date: 2026-10-15 JST. Windows x64 Release,
MSVC 19.44.35209, Intel Core i7-11370H, eight reported logical threads,
Python 3.12.10 and NumPy 2.5.0. These are bounded synthetic workloads, not SLAs.
All result checks passed. Runs were serial after build/test work ended; affinity,
background activity and power policy were uncontrolled.

## Comparison method and evidence

The [serial command transcript](benchmark-v0.6-commands-final.json) records all
14 successful processes, executable/script hashes and capture hashes. The
[driver](../../tools/measure_v06.py) requires actual NumPy and verifies the accepted
independent-package binary/wheel hashes. The frozen v0.5 source archive and five
native binaries passed a [fresh hash check](v0.6-baseline-reverification.json).
`data.cpp`, `data_relational.cpp`, `runtime.cpp` and `data.py` are byte-identical
between versions. Compiler/build metadata match. MSVC's legacy `__cplusplus`
macro in the capture is not the selected language flag: CMake builds as C++20.

Only the native table/CSR runner uses two alternating pairs: old/new, then new/old.
History, runtime and Python comparisons each use one old/new sequence. Native and
Python data phases use one warmup and five samples; history uses three including
the first; runtime uses one warmup and seven samples. Seven-sample nearest-rank
p95 is just the maximum. There is no significance test or automatic timing gate.

The unchanged Python runner labels its workload protocol `version=0.5.0` in both
captures. Actual engines are identified by their native paths/hashes and transcript.
The relational runner's older text saying there is no cross-version baseline is
also preserved raw protocol text; the matched comparison below supplies one.
Earlier [old](benchmark-v0.5-data-for-v0.6-initial.json),
[new](benchmark-v0.6-data-initial.json) and
[language-profile](benchmark-v0.6-language-profile-initial.json) captures remain
available; they preceded the final small cache-accounting refinement. No sample
was discarded because it was slow.

## Matched nullable table and sparse workloads

[Pair 1 old](benchmark-v0.5-data-for-v0.6-pair1-final.json),
[pair 1 new](benchmark-v0.6-data-pair1-final.json),
[pair 2 old](benchmark-v0.5-data-for-v0.6-pair2-final.json),
[pair 2 new](benchmark-v0.6-data-pair2-final.json),
[all 48 comparisons](benchmark-v0.6-native-comparison.json).
Workload objects and every reported physical output digest match. Entries below
are **pair 1 / pair 2 medians in milliseconds**, not pooled observations.

| Phase | v0.5 | v0.6 |
|---|---:|---:|
| 65,536-row table construction | 12.975 / 21.615 | 2.598 / 1.417 |
| 65,536-row prepared serial pipeline | 149.804 / 151.593 | 5.755 / 6.615 |
| 262,144-row construction | 120.387 / 71.606 | 5.956 / 7.156 |
| 262,144-row inference over literals | 0.0858 / 0.0890 | 0.1986 / 0.1117 |
| 262,144-row compilation, warm input digests | 232.580 / 188.720 | 7.246 / 7.230 |
| 262,144-row prepared serial pipeline | 854.357 / 933.164 | 24.138 / 23.518 |
| 262,144-row runtime, one worker | 1184.719 / 864.666 | 23.634 / 24.444 |
| 262,144-row runtime, four workers | 819.779 / 1085.811 | 20.478 / 20.550 |
| CSR 4,096 rows, direct prepared | 3.668 / 4.122 | 0.255 / 0.269 |
| CSR 16,384 rows, construction | 3.602 / 4.190 | 1.943 / 1.939 |
| CSR 16,384 rows, direct prepared | 11.691 / 9.398 | 0.921 / 1.994 |
| CSR 16,384 rows, decoded prepared | 6.493 / 8.104 | 1.170 / 1.079 |

The six-node large table pipeline improves 35.4× and 39.7× in the final pairs.
The initial pair was 565.740→23.114 ms, about 24.5×; substantial old-run variation
precludes presenting a universal 40× gain. CSR direct execution improves 12.7× and
4.7× in the final pairs. The much narrower fused host reference remains roughly
1.1–1.2 ms in final v0.6; it lacks full validation, lineage, intermediate values
and runtime contracts and is not an interchangeable implementation.

Large-input inference regresses **132% and 26%**, albeit by 0.113/0.023 ms. The
initial CSR construction capture regressed 1.837→1.960 ms, while both final pairs
improve; this is not a stable directional conclusion. Four workers expose only
two overlapping graph slots and offer modest final v0.6 benefit. Worker count
alone is not useful parallelism. Batch access, validation reuse and specialization
reuse changed together; the whole-pipeline comparison does not allocate gains
among them.

## Access, copying and validation profiles

[Access capture](benchmark-v0.6-access-final.json) compares identical endian loads
and checked sums using repeated scalar lookup/geometry, an owning batch range, and
explicit contiguous copying followed by batch iteration. Medians are milliseconds
per scan; tiny cases average 64 scans. Copy timing includes allocation/destruction.

| Elements / layout | Scalar | Batch | Copy then batch |
|---|---:|---:|---:|
| 256, nullable contiguous | 0.01903 | 0.00345 | 0.04338 |
| 16,384, nullable reversed | 0.6197 | 0.1285 | 0.6924 |
| 262,144, nonnullable contiguous | 8.4951 | 2.5088 | 3.2384 |
| 262,144, nullable contiguous | 11.3376 | 2.6645 | 3.8847 |
| 262,144, nullable reversed | 10.9596 | 2.2782 | 10.3549 |
| 262,144, nullable zero-stride broadcast | 11.1279 | 2.3198 | 10.1439 |

Batch is 2.21–7.21× faster across all 18 conditions and copies no payload. Copying
first costs 1.29–17.94× the direct batch time, and is slower than scalar access in
nine cases. Large nullable copying transfers 2,359,296 bytes per scan; tracked
allocation peaks at 4,718,592 bytes including retained input. That peak accumulates
through the layout group and excludes metadata, temporary vectors and RSS.

[Language capture](benchmark-v0.6-language-profile-final.json) compares the same
current implementation with its generic memo disabled/enabled, using explicit
Function binding and a two-node computation. Medians below are **off→on**.

| Rows / nullable | Repeated validation, ms | Prepared runtime + materialization, ms |
|---|---:|---:|
| 8 / no | 0.0104→0.0059 | 0.2985→0.5578 |
| 8 / yes | 0.0105→0.0155 | 0.3226→0.5844 |
| 4,096 / no | 0.0097→0.0051 | 0.4397→0.6414 |
| 4,096 / yes | 0.0116→0.0087 | 0.7901→0.6607 |
| 262,144 / no | 0.0062→0.0076 | 11.0869→13.0964 |
| 262,144 / yes | 0.5489→0.0053 | 14.2173→11.8738 |

For the large nullable case, repeated validation changes from six misses to six
hits, with instrumented validation work 4.2181→0 ms across warmup plus five calls.
Prepared execution changes from 66 misses to 54 hits/12 misses, reducing measured
validation work 14.3292→2.9644 ms and whole phase median by 16.5%. Binding plus
execution/materialization improves 13.2%. The final 4,719,453-byte cache charge is
conservative accounting; the weak memo does not retain that payload.

Nonnullable int64 validation already avoids scanning values. Memo bookkeeping
provides little benefit there, and small/cheap cases regress. Caches are bounded
and can be disabled; a universal adaptive threshold is not established by these
fixtures. Cold-certificate profiles clear entries and always miss: their timing
differences cannot be credited to hits. Five samples, fixed mode order and overlap
(large nullable enabled prepared samples span 11.016–14.862 ms) constrain causal
interpretation. Instrumentation is timed phase/counter observation, not sampled
CPU-stack or hardware-counter profiling.

## Qualified history and retained source state

[Matched old](benchmark-v0.5-relational-for-v0.6-final.json),
[matched new](benchmark-v0.6-relational-final.json). Fixtures retain 16 entities,
three participant occurrences per relation and two recording revisions, selecting
the older semantic snapshot through the newer physical manifest. Medians in ms:

| Phase | 64 relations: old→new | 256 relations: old→new |
|---|---:|---:|
| Historical selection | 37.322→32.088 | 132.410→125.849 |
| Literal validation | 8.263→8.160 | 63.487→37.124 |
| Expression construction + compile | 13.047→4.933 | 61.870→6.614 |
| Prepared transforms | 140.653→26.774 | 983.246→114.291 |
| Materialize + uncached inspect | 37.607→15.295 | 152.389→55.820 |
| Median total measured phases per repeat | 231.401→97.318 | 1342.656→338.224 |

The prepared gains are 5.25×/8.60×, and totals 2.38×/3.97×. Totals are medians of
per-repeat phase sums, not sums of phase medians. All recorded semantic/physical
pins, selected counts, source/result lengths and count checksums match exactly.
The captures do not hash the complete qualified output bytes; separate conformance
tests establish exact cache-on/off output bytes. No phase median regresses here.

Memory does increase: cumulative process peaks are **11.574→12.855 MiB** and
**25.500→30.273 MiB** (+11.1%/+18.7%). These peaks include setup and earlier cases;
they do not isolate cache allocations. Retaining parsed state trades bounded memory
for less repeated validation; serialized cache charges exclude JSON overhead.

[Current-build relational profile](benchmark-v0.6-relational-profile-final.json)
uses a different one-revision fixture and separately measures memo off/cold/warm:

| Relations | Prepared off, ms | Cold, ms | Warm, ms |
|---:|---:|---:|---:|
| 64 | 156.588 | 25.408 | 28.538 |
| 256 | 883.550 | 198.458 | 192.985 |
| 768 | 2610.654 | 576.099 | 618.379 |

Per prepared run, off performs 12 full/source checks and 12 derivations; cold/warm
perform three full new-output checks, three owner hits, three exact-source hits,
zero source checks and six derivations. At 256 relations decoded bytes fall
4,024,513→1,229,527 and serialized work 6,907,952→2,872,532. Remaining cold prepared
phase medians include decode 90.546 ms, derive 63.661 and encode 27.568; these
separately computed medians must not be added as same-sample attribution. Generic
validation timing contains session work, so the two instrumentation totals overlap.

Warm input reuse reduces literal work, but prepared warm is 12.3% slower than cold
at 64 relations and 7.3% slower at 768 despite identical required work. Fixed mode
order and sample variability do not support a universal warm-versus-cold latency
claim. New outputs always remain fully checked. Scoped inspection reuses a verdict
but copies JSON; independent free inspection still performs full validation.

Enabled session peak charges are 0.685/2.609/7.742 MiB for the three sizes, below
the 16 MiB bound, with no eviction or validation failures in these fixtures.
They finish with two parsed entries and one source slot. The mode-by-mode RSS
observations are cumulative in one fixed-order process, so they cannot isolate
independent mode peaks or establish a leak. Wire charges exclude JSON/tree/index
allocator overhead.

## Python, NumPy and independent package costs

[Old Python](benchmark-v0.5-python-for-v0.6-final.json),
[new Python](benchmark-v0.6-python-final.json), identical runner/source hash and
interpreter/NumPy. Checks and cleanup are outside timing. Execution includes native
scheduling/materialization and handle transport. Medians in ms:

| Phase | 65,536 elements: old→new | 262,144 elements: old→new |
|---|---:|---:|
| Nullable Python list import | 23.393→29.743 | 107.331→106.552 |
| Compile warm literals | 3.511→3.532 | 12.016→3.126 |
| Prepared run/materialize | 9.892→2.494 | 62.502→6.480 |
| JSON items → Python list | 25.057→14.456 | 203.651→61.351 |
| NumPy copy import | 2.310→1.615 | 7.933→8.259 |
| Read-only NumPy view export | 0.1705→0.2200 | 0.2869→0.6165 |
| Writable NumPy copy export | 0.4803→0.4575 | 2.4366→3.9999 |

Regressions include 27% small list import, 4% large NumPy import, 115% large
read-only export (+0.330 ms) and 64% writable export (+1.563 ms). Read-only samples
vary considerably; writable sample ranges do not overlap (old 2.152–3.098,
new 3.203–4.974 ms). The cause is not isolated by one old/new sequence. Large list
import remains about 107 ms and dominates the new 6.48 ms prepared execution path.
These cases exercise standard nullable int64, not new complex arithmetic.

The [installed diagonal package capture](benchmark-v0.6-independent-package-final.json)
has no v0.5 counterpart. Medians are **4.646 / 12.628 / 68.681 ms** for
16/4,096/65,536 elements. This includes copied Python inputs, C transport, fresh
native construction/compilation, one-worker runtime, materialization and output
copying. Independent exact multiplication checks are outside timing. It measures
the complete simple facade; it is not isolated multiplication throughput or the
same workload as the standard reduction. Reusing a persistent facade runtime is
a possible later optimization if a real consumer needs it.

## Unchanged runtime and engineering priorities

[Old runtime](benchmark-v0.5-runtime-for-v0.6-final.json),
[new runtime](benchmark-v0.6-runtime-final.json). Workload objects and graph IDs
match. Planning, registration and pool creation are excluded; output checks and
diagnostics are included. Hash cases process 4 MiB per graph.

| Workload total | v0.5 ms | v0.6 ms |
|---|---:|---:|
| 4,096 prepared identity calls | 1.6781 | 1.4838 |
| 256 scheduled identity jobs | 12.1579 | 12.4268 |
| Eight hashes, serial | 32.8115 | 27.0061 |
| Eight hashes, one worker | 26.2223 | 26.2281 |
| Eight hashes, two workers | 12.7782 | 13.1860 |
| Eight hashes, four workers | 9.8472 | 9.3654 |

All old/new sample ranges overlap. The 2.2% scheduled-job and 3.2% two-worker median
increases do not establish systematic runtime regressions, nor do other medians
prove a runtime redesign benefit: runtime source is unchanged.

The strongest next targets are Python list/JSON transport, new-owner qualified
source decode/derivation/embedding, cheap-operation memo policy and concrete plan
construction for small jobs. Coarse providers and persistent explicitly owned
consumer contexts should be evaluated before fusion, global caches or another
scheduler. Profiling and these whole-workload results justify the implemented
changes; they do not establish production scale, arbitrary numerical performance,
GPU interoperability or automatic parallel speedup.
