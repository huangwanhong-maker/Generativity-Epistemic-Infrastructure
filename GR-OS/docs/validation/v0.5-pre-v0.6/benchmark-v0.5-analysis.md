# v0.5 measured performance and next priorities

Class: local engineering evidence. Date: 2026-10-15 JST. Windows x64 Release, Intel Core i7-11370H, eight reported logical processors, MSVC 19.44, Python 3.12.10 and NumPy 2.5.0. Benchmarks ran serially after the final Release regression, without concurrent build/test work. Host background activity, power state and affinity were not controlled. These are bounded synthetic workloads, not SLAs or production-scale qualification.

The new Layer 3 APIs have no v0.4 counterpart. Their phase measurements must not be represented as a cross-version speedup. The preserved v0.4 native runtime executable supplies a separate unchanged-workload comparison. Correctness checks, result hashes and independent numerical expectations accompany every pilot.

## Numerical/tabular pilot

[Native raw capture](benchmark-v0.5-data-native.json), [console output](benchmark-v0.5-data-native.txt), [runner](../../benchmarks/data.cpp). One warmup and five samples per case; medians in milliseconds. Construction, expression inference, fresh compilation, prepared execution and runtime execution are separated. Output checking/hashing and destruction are outside these measured intervals.

The six-node pipeline selects a table field, applies checked affine arithmetic, constructs a nullable predicate, filters a two-column table and transformed numerical column, then computes an ordered checked sum. The 262,144-row fixture selects 102,317 rows and returns sum 5,729,719, with identical output hashes in serial and runtime variants.

| Phase | 65,536 rows | 262,144 rows |
|---|---:|---:|
| Construct typed nullable table | 22.755 | 95.293 |
| Infer six calls over existing literals | 0.327 | 0.142 |
| Fresh catalog/plan, warm input owner hashes | 56.820 | 190.583 |
| Prepared serial execution, no scheduler/materialization | 274.120 | 1,114.721 |
| Runtime, one worker | 269.123 | 1,223.247 |
| Runtime, four workers | 209.419 | 994.764 |
| Specialized known-range fused host loop | 0.452 | 2.048 |

The fused loop is a deliberately narrower reference: it exploits the fixture's known integer range and produces ordinary vectors. It does not implement generic type/layout validation, checked arbitrary-input arithmetic, buffer contracts, provenance or runtime dispatch. Its large advantage is still a useful warning: the current generic path is not a high-throughput dataframe engine. Repeated validation, generic per-element view access, separate allocations and passes are concrete profiling/optimization targets. The measurements do not isolate their individual shares.

Four workers used at most two slots in this dependency graph. At the larger size, runtime execution was about 19% faster than one worker, not fourfold. Tracked peak retained runtime payload was 12,436,621 bytes; selected outputs account for 1,023,179 logical/retained bytes. These counters exclude callback temporaries, metadata and process overhead, and may charge aliases conservatively. They are not RSS bounds.

## Separately installed CSR extension

The same native capture tests an independently registered matrix family, canonical tridiagonal CSR layout and two exact providers. Each provider uses checked multiplication and ordered accumulation; independent stencil expectations verify results.

| Phase | 4,096 rows | 16,384 rows |
|---|---:|---:|
| Construct CSR | 1.035 | 3.049 |
| Compile direct-view provider | 5.703 | 6.955 |
| Execute direct-view provider | 2.697 | 11.057 |
| Compile decoded-vector provider | 5.569 | 6.289 |
| Execute decoded-vector provider | 2.128 | 9.180 |

The decoded provider was about 17% faster at 16,384 rows despite allocating temporary native vectors. Those vectors are included in elapsed time but outside the owned-buffer budget. Both providers produced the same physical result digest. This supports keeping physical implementations selectable and measuring them rather than assuming a nominally lower-copy representation is faster. No internally parallel, SIMD or accelerator result is claimed.

## Qualified relational-historical pilot

[Raw capture](benchmark-v0.5-data-relational.json), [runner and methodology](../../benchmarks/data_relational.cpp), [semantic contract](../relational-data.md). Three repetitions include the first run. Each case has 16 entities, the stated relation count, three participations per relation and two recording revisions. It selects the older semantic snapshot through the newer physical manifest, projects incidences, selects actor roles and counts their occurrences. Source construction/publication is excluded; actual read, validation and copying costs remain included in their phases.

| Phase / size | 64 relations | 256 relations |
|---|---:|---:|
| Source bytes | 38,889 | 139,197 |
| Qualified result bytes | 115,638 | 437,062 |
| Eager pinned history selection, ms | 61.587 | 189.739 |
| Literal validation and identity, ms | 15.341 | 56.549 |
| Expression construction and compilation, ms | 26.760 | 65.832 |
| Prepared runtime execution, ms | 236.565 | 927.894 |
| Typed materialization and full inspection, ms | 47.968 | 204.715 |

Each case preserves its source revisions, verifies unchanged storage head, groups into 16 exact record-reference groups and conserves 128/512 selected occurrences, with 64/256 excluded occurrences retained explicitly. Output count checksums remain stable across repetitions. Cumulative process peak working set was 12,926,976 / 26,918,912 bytes; the second includes earlier work and neither is a per-phase allocation measurement.

These small workloads expose substantial reconstruction/validation/copying cost. A future immutable shared source representation and exact validation reuse should be evaluated before scaling graph analytics. These timings exclude temporal queries, SDK transport and SQLite I/O; they cannot characterize those workloads.

## Python/C and actual NumPy boundary

[Raw capture](benchmark-v0.5-data-python.json), [runner](../../benchmarks/data.py). One warmup and five samples; validation/cleanup outside timing. This Python workload reduces one nullable column, so its runtime numbers are not comparable to the six-node native table pipeline above.

| End-to-end phase, ms | 65,536 elements | 262,144 elements |
|---|---:|---:|
| Python iterable → owned nullable column | 47.362 | 180.599 |
| Native inference request | 0.181 | 0.202 |
| Compile existing sum expression | 7.286 | 10.455 |
| Run prepared sum and materialize typed result | 14.398 | 48.474 |
| C JSON items → Python list | 33.951 | 135.782 |
| NumPy → owned native copy | 2.228 | 9.150 |
| Native → readonly NumPy view | 0.342 | 0.400 |
| Native → writable NumPy copy | 0.660 | 3.000 |

Readonly export retains immutable native ownership and includes authoritative specification lookup and SDK handle exchange. It does not copy or compute over the array. The mutable NumPy source is copied on import; array layout/byte order normalization is explicit. Nullable columns and noncontiguous zero-copy exports require explicit policies and are refused by the convenience API; strided copying is supported. These results do not establish Arrow, SciPy or DLPack adapters.

## Existing runtime comparison

[Preserved v0.4 executable capture](benchmark-v0.4-runtime-for-v0.5.json) and [v0.5 capture](benchmark-v0.5-runtime.json) run the unchanged benchmark source and seven samples per case, excluding catalog/planning/pool creation. The default plan objects and IDs remain equal in [v0.4](plan-v0.4-default.json) and [v0.5](plan-v0.5-default.json); outer SDK-version fields differ.

The initial one-worker hash median increased from 65.436 to 103.209 ms. Two additional alternating-order pairs investigate that variation, after sanitizer work finished and without concurrent build/test work: [old pair 2](benchmark-v0.4-runtime-for-v0.5-pair2.json), [new pair 2](benchmark-v0.5-runtime-pair2.json), [old pair 3](benchmark-v0.4-runtime-for-v0.5-pair3.json), [new pair 3](benchmark-v0.5-runtime-pair3.json). All captures remain included. The following descriptive medians pool 21 samples per version; they are not independent-host confidence intervals.

| Unchanged runtime workload | v0.4 pooled ms | v0.5 pooled ms | Change |
|---|---:|---:|---:|
| Prepared identity, 4,096 calls | 2.694 | 2.449 | -9.1% |
| Runtime identity, 256 jobs | 17.652 | 19.769 | +12.0% |
| Eight 512 KiB hashes, prepared serial | 31.615 | 32.509 | +2.8% |
| Same hashes, one worker | 33.067 | 32.536 | -1.6% |
| Same hashes, two workers | 17.843 | 17.421 | -2.4% |
| Same hashes, four workers | 10.951 | 12.629 | +15.3% |

Per-run medians vary substantially: prepared identity spans 1.856–3.203 ms old and 1.643–3.050 ms new; one-worker hashing spans 28.645–65.436 ms old and 29.980–103.209 ms new. The initial one-worker magnitude did not repeat. Four-worker hashing was slower in all three pairs, and runtime identity was slower in two, so these remain regression-monitoring priorities. No individual source-level cause or universal improvement is established. Timing is not a behavioral conformance gate.

## Engineering decision

The implementation demonstrates extension and semantic contracts; it does not yet meet an unspecified high-performance target. Before the broader Layer 2 ecosystem, select realistic sizes and latency targets, profile the repeated validation/view-access paths, and implement measured validated fast paths, reusable specialization and shared qualified-source/lineage ownership. Fusion or parallel reduction must preserve null handling, checked prefixes, effect boundaries and declared numerical equivalence. Retain the current providers as conformance references when introducing faster ones.
