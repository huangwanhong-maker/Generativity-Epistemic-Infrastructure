# v0.3/v0.4 matched benchmark analysis

**Class:** Informative engineering evidence  
**Status:** Completed local Windows x64 Release measurements, 2026-10-13 JST  
**Scope:** Bounded fixtures; no throughput SLA, confidence interval or production-scale claim

All comparisons use the preserved v0.3 SDK and the final v0.4 Release artifacts on the same machine/compiler/Python. Builds and tests were stopped during capture. Background load, power/frequency state and filesystem caches were not controlled experimentally. Fresh baseline timings differ from older captures, so the primary tables use fresh reruns, not historical medians.

[Primary commands](capture-v0.4-commands.json), [alternating native-repeat commands](capture-v0.4-native-repeats-commands.json), [fresh admission-baseline commands](v0.3-pre-v0.4/admission-rerun-commands.json), [build/package identities](v0.4-source-artifacts.json), and [protocols](../../benchmarks/README.md) preserve reproduction details. Ratios in timing tables are v0.4/v0.3; lower is faster. Throughput tables use accepted commits per second; higher is faster. Rounded values are presentation only.

## Unchanged common workloads

Five samples after one warmup; Python serialization, C transport and native work included. [Fresh v0.3](v0.3-pre-v0.4/benchmark-common-rerun.json), [v0.4](benchmark-v0.4.json), [comparison output](capture-v0.4-compare-common.txt).

| Workload | v0.3 ms | v0.4 ms | Ratio |
|---|---:|---:|---:|
| construct_100 | 6.128 | 6.223 | 1.02 |
| validate_300 | 11.295 | 12.268 | 1.09 |
| graph_300 | 15.488 | 14.159 | 0.91 |
| apply_100_of_300 | 29.120 | 28.089 | 0.96 |
| sha256_1MiB_cpu.sequential | 36.547 | 30.494 | 0.83 |
| sha256_1MiB_cpu.threaded | 28.644 | 28.169 | 0.98 |
| commit_100_memory | 14.461 | 11.359 | 0.79 |
| blob_4MiB_memory | 87.679 | 93.054 | 1.06 |
| commit_100_sqlite | 18.973 | 18.217 | 0.96 |
| blob_4MiB_sqlite | 95.843 | 98.010 | 1.02 |

All ten passed the deliberately coarse 3x alert. SQLite create/commit improved about 4% against the fresh v0.3 measurement; memory commit improved about 21%. Validation, construction and both base64-read medians increased. The earlier reported v0.2→v0.3 SQLite increase (15.324→18.796 ms) is **not fully recovered or causally explained** by this result. The old v0.2 measurement is not a fresh three-version comparison.

Source inspection found redundant canonicalization and snapshot-membership statement preparation; v0.4 removes that work while retaining integrity checks. The native profile below separates admission from physical commit, but does not isolate the precise contribution of each edit to this common Python workload. No attribution of the old 23% regression solely to history, locking or validation is justified.

## Fixed accepted-work concurrency

Each case has three fresh-database repetitions, the same seeded state, 32 accepted measured commits and 33 verified ledger entries/receipts. Untracked batches contain one/eight records; tracked batches four. [Fresh v0.3](v0.3-pre-v0.4/benchmark-transactions-rerun.json), [v0.4 optimistic](benchmark-v0.4-transactions-optimistic.json), [v0.4 serialized](benchmark-v0.4-transactions-serialized.json).

| History / batch | Writers | v0.3 optimistic commits/s | v0.4 optimistic commits/s | v0.4 serialized commits/s | Conflicts: old / optimistic / serialized |
|---|---:|---:|---:|---:|---|
| None / 1 | 1 | 308.39 | 313.50 | 367.22 | 0 / 0 / 0 |
| None / 1 | 2 | 280.06 | 326.65 | 317.40 | 4 / 5 / 0 |
| None / 1 | 4 | 272.11 | 205.06 | 352.38 | 7 / 7 / 0 |
| None / 8 | 1 | 53.17 | 69.92 | 60.57 | 0 / 0 / 0 |
| None / 8 | 2 | 52.76 | 57.28 | 82.29 | 19 / 13 / 0 |
| None / 8 | 4 | 53.02 | 77.04 | 75.77 | 29 / 26 / 0 |
| Tracked / 4 | 1 | 7.29 | 9.40 | 8.72 | 0 / 0 / 0 |
| Tracked / 4 | 2 | 7.11 | 8.13 | 9.87 | 26 / 27 / 0 |
| Tracked / 4 | 4 | 6.89 | 8.91 | 10.86 | 68 / 71 / 0 |

For four tracked writers, throughput rose from 6.89 to 8.91 with the new optimistic path and to 10.86 with explicit serialized admission. The latter removes speculative conflicts in this cooperating-process fixture. This does not mean more concurrent database writers or finer isolation. The four-writer untracked batch-one optimistic case regressed from 272.11 to 205.06 commits/s. Serialization is a policy choice, not a universal speedup.

Queue-inclusive tracked latency illustrates the trade-off. Entries are medians across three runs; each run's p95 is nearest-rank over only 32 accepted commits and is not a production-tail estimate.

| Writers | v0.3 median / p95 ms | v0.4 optimistic median / p95 ms | v0.4 serialized median / p95 ms |
|---|---:|---:|---:|
| 1 | 130.12 / 268.07 | 100.30 / 186.82 | 100.55 / 227.29 |
| 2 | 184.20 / 597.22 | 154.32 / 750.41 | 181.82 / 354.44 |
| 4 | 216.88 / 1436.86 | 179.94 / 1072.57 | 348.08 / 570.53 |

The four-writer serialized median includes waiting and is higher than the old optimistic median; its measured tail is lower and total accepted throughput is higher. These observations do not guarantee fairness timing or latency bounds. Physical workspace CAS, WAL/FULL and exact-replay rules are unchanged.

## Native admission and retained history

The same C++ source is compiled against both SDKs. Three fresh captures per version use 128 records, 256-byte padding and 32 single-record updates after the seed. The baseline reruns [1](v0.3-pre-v0.4/admission-rerun-1.json), [2](v0.3-pre-v0.4/admission-rerun-2.json), [3](v0.3-pre-v0.4/admission-rerun-3.json) supplement and preserve the original pre-edit profiles. v0.4 captures: [1](benchmark-v0.4-admission-1.json), [2](benchmark-v0.4-admission-2.json), [3](benchmark-v0.4-admission-3.json).

Each phase cell is its own median in milliseconds. Total is the median of each run's summed begin+stage+commit time; it need not equal the sum of independently computed phase medians.

| Backend / history | Begin old→new | Stage old→new | Commit old→new | Total old→new | Total ratio |
|---|---:|---:|---:|---:|---:|
| memory / none | 270.388→272.220 | 1.625→1.734 | 443.945→210.172 | 715.958→484.127 | 0.68 |
| memory / tracked | 6793.473→2029.763 | 2.400→2.854 | 13907.067→3031.360 | 20702.941→5063.976 | 0.24 |
| sqlite / none | 291.067→332.002 | 1.540→1.746 | 649.152→413.803 | 941.759→747.551 | 0.79 |
| sqlite / tracked | 5384.374→2821.012 | 2.156→2.569 | 10373.534→3873.815 | 15760.064→6697.397 | 0.42 |

| Backend / history | Physical commit boundary old→new ms | Final snapshot old→new ms |
|---|---:|---:|
| memory / none | 123.057→101.198 | 7.297→12.788 |
| memory / tracked | 886.936→134.856 | 280.334→20.501 |
| sqlite / none | 338.044→297.332 | 8.495→9.876 |
| sqlite / tracked | 1169.522→416.642 | 298.861→29.837 |

Tracked retained blob bytes fell identically in both backends from **37,150,477 to 283,208** (about **99.24% less**, 131.18x smaller). Unique object count increased from **33 to 99**, reflecting three small objects per compact publication. This counts history blobs, not SQLite pages, record versions, indexes or total process memory. Final record/publication inventories are checked outside timing.

Tracked SQLite total time fell from 15.760 s to 6.697 s (**2.35x faster**) and tracked memory from 20.703 s to 5.064 s (**4.09x faster**). The untracked current-snapshot read medians increased; the table retains those regressions. The tracked final read is node 33, a checkpoint, and does not represent deep delta reconstruction.

Old tracked SQLite physical commit accounted for about 1.170 s of 15.760 s; substantial cost lay outside the physical publication boundary. v0.4 still spends most time outside it. Its many small verified object opens totalled 958.84 ms versus 625.15 ms previously, despite much smaller retained bytes. Checkpoint/delta tuning, transaction-local history reuse or batched object access may help future workloads. The unchanged wrapper does not override v0.4's selective-record API, so this fixture may understate direct SQLite improvements. Open timing includes integrity verification; later reader copies remain in outer phases. These are boundary measurements, not a pure I/O profile or CPU attribution.

## Historical selection across checkpoint and delta positions

The identical runner builds 32 records with 256-byte padding and 33 publications. Five samples follow a query warmup. v0.3 transfers its complete bundle and invokes public reconstruction; v0.4 uses native selected reconstruction. Output generations and identities are checked within each version; new automatic physical-base attribution differs across encodings. This is an end-to-end SDK comparison, not codec-only timing or a branch/merge throughput benchmark.

[Fresh v0.3](v0.3-pre-v0.4/benchmark-history-reads.json), [v0.4](benchmark-v0.4-history-reads.json).

| Backend | Selected node | Pinned publication | Query | Delta depth | v0.3 ms | v0.4 ms | Ratio |
|---|---:|---:|---|---:|---:|---:|---:|
| memory | 17 | 17 | selected head | 0 | 128.935 | 3.850 | 0.030 |
| memory | 17 | 33 | explicit semantic ID | 0 | 254.405 | 7.929 | 0.031 |
| memory | 24 | 24 | selected head | 7 | 162.550 | 11.622 | 0.071 |
| memory | 24 | 33 | explicit semantic ID | 7 | 265.389 | 18.903 | 0.071 |
| memory | 32 | 32 | selected head | 15 | 236.672 | 28.694 | 0.121 |
| memory | 32 | 33 | explicit semantic ID | 15 | 235.102 | 31.358 | 0.133 |
| memory | 33 | 33 | selected head | 0 | 260.026 | 3.458 | 0.013 |
| memory | 33 | 33 | explicit semantic ID | 0 | 248.892 | 3.481 | 0.014 |
| sqlite | 17 | 17 | selected head | 0 | 135.399 | 9.451 | 0.070 |
| sqlite | 17 | 33 | explicit semantic ID | 0 | 276.174 | 36.518 | 0.132 |
| sqlite | 24 | 24 | selected head | 7 | 201.769 | 34.793 | 0.172 |
| sqlite | 24 | 33 | explicit semantic ID | 7 | 298.257 | 69.093 | 0.232 |
| sqlite | 32 | 32 | selected head | 15 | 251.107 | 63.385 | 0.252 |
| sqlite | 32 | 33 | explicit semantic ID | 15 | 261.260 | 116.597 | 0.446 |
| sqlite | 33 | 33 | selected head | 0 | 220.611 | 9.638 | 0.044 |
| sqlite | 33 | 33 | explicit semantic ID | 0 | 323.494 | 13.788 | 0.043 |

The SQLite node-32 lookup from publication 33, with 15 deltas plus catalogue lookup, improved from 261.260 to 116.597 ms (**2.24x**); the latest checkpoint query improved from 220.611 to 9.638 ms (**22.89x**). Quoting only the latter would hide the chain/query trade-off. Separate node-33 queries target the same state and show run variability. The coordinator verifies the pinned current physical state before a different requested semantic state; selected reconstruction diagnostics do not measure every outer API cost.

## Prepared execution and scheduling

The initial matched capture showed a 55% serial-hash median increase, prompting three additional pairs with alternating order. All four captures remain included. Each run has one warmup and seven samples. The table uses descriptive pooled medians of **28 samples per version**; repeated samples are not independent statistical evidence. Planning, pool construction and language transport are excluded; result/diagnostic checks remain timed.

[Initial v0.3](v0.3-pre-v0.4/benchmark-runtime-rerun.json), [initial v0.4](benchmark-runtime-v0.4.json); additional v0.3 [1](v0.3-pre-v0.4/benchmark-runtime-repeat-1.json), [2](v0.3-pre-v0.4/benchmark-runtime-repeat-2.json), [3](v0.3-pre-v0.4/benchmark-runtime-repeat-3.json); v0.4 [1](benchmark-runtime-v0.4-repeat-1.json), [2](benchmark-runtime-v0.4-repeat-2.json), [3](benchmark-runtime-v0.4-repeat-3.json).

| Workload | v0.3 pooled ms | v0.4 pooled ms | Change |
|---|---:|---:|---:|
| prepared_identity_4096 | 1.42530 | 1.52110 | 6.7% |
| runtime_identity_256 | 13.35310 | 15.27390 | 14.4% |
| fork_join_hash_8x512KiB_serial | 26.45700 | 29.82630 | 12.7% |
| fork_join_hash_8x512KiB_workers1 | 29.44045 | 26.52335 | -9.9% |
| fork_join_hash_8x512KiB_workers2 | 17.16340 | 13.49480 | -21.4% |
| fork_join_hash_8x512KiB_workers4 | 9.98675 | 9.46065 | -5.3% |

| Workload | v0.3 run medians, ms (initial, repeats 1–3) | v0.4 run medians, ms |
|---|---|---|
| prepared_identity_4096 | 1.3716, 1.4271, 1.4235, 1.4432 | 1.4791, 1.5040, 1.4734, 2.4656 |
| runtime_identity_256 | 13.9141, 12.4385, 12.7815, 14.8569 | 16.2528, 18.5099, 12.7996, 12.7602 |
| fork_join_hash_8x512KiB_serial | 28.5716, 27.0044, 25.7304, 22.7612 | 44.3391, 24.8701, 29.0397, 31.2725 |
| fork_join_hash_8x512KiB_workers1 | 26.8313, 29.2950, 26.3907, 36.6434 | 29.5289, 25.5021, 26.4115, 26.8131 |
| fork_join_hash_8x512KiB_workers2 | 17.0400, 17.4921, 14.8664, 20.1389 | 15.3480, 14.0899, 12.6865, 12.1231 |
| fork_join_hash_8x512KiB_workers4 | 9.6781, 10.6608, 9.2155, 12.0199 | 9.3198, 9.3023, 9.4020, 10.0876 |

Prepared calls were slower in every pair, with variable magnitude; scheduler-job differences changed direction. The serial-hash initial 55% magnitude did not consistently repeat (paired ratios 1.552, 0.921, 1.129, 1.374). The pooled result still shows a regression. No particular code edit is established as its cause, and the repeat measurements do not erase the initial capture.

Pooled prepared cost is 0.348→0.371 microseconds/call; scheduled cost is 52.161→59.664 microseconds/job. Batch meaningful work and reuse prepared plans rather than scheduling every tiny operation. The v0.4 hash graph observed about 2.80x one-to-four-worker scaling. Its 1,049,100 retained logical bytes under a 2,097,152-byte reservation and peak slots 1/1/2/4 are stable diagnostics, not RSS or allocation profiles.

## New buffer interoperability

[Raw v0.4 data](benchmark-v0.4-buffers.json). Seven samples after one warmup. Copy import includes Python staging, native allocation and handle close. Readinto uses a preallocated destination. Export measures 1,000 acquire/release pairs with no payload computation; checksums and post-close ownership checks occur outside timing.

| Payload | Copy import ms | Readinto ms | 1,000 read-only exports ms |
|---|---:|---:|---:|
| 1 KiB | 0.0583 | 0.0094 | 3.4026 |
| 1 MiB | 0.8580 | 0.0694 | 5.7671 |
| 4 MiB | 3.0939 | 0.3307 | 4.4941 |

Export measured roughly 3.4–5.8 microseconds per acquire/release pair without monotonic payload-size growth. It does not measure reading, hashing or computing over the payload. This is a new API with no v0.3 counterpart, not a NumPy/Arrow/DLPack or GPU throughput claim.

## Interpretation and remaining work

Compact representation substantially reduces retained history bytes and measured tracked-publication/selection costs. Optional admission removes wasted retries in the cooperating-writer fixture. Neither change makes arbitrary concurrency scale or eliminates current-inventory work. Common commit recovery is modest, and tiny dispatch plus some snapshot/transport paths regress.

The next performance targets should come from a real Layer 3 workload: inventory size, branching/read position, object access latency, data layout and operation granularity. Fixed CPU/power settings, randomized run order, more independent captures and targeted profiling are needed before small percentage differences become release budgets. No correctness check, isolation condition or durability setting was relaxed for these measurements.
