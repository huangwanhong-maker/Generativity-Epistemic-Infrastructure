# Native Layer 4 measurement protocol

Status: bounded implementation measurements, not a throughput or tail-latency guarantee.

Configure with `GR_BUILD_BENCHMARKS=ON`, build Release, and finish tests/packaging before measuring. From `GR-OS`, on an otherwise idle host:

```powershell
& .\build\Release\benchmark_runtime.exe .\docs\validation\benchmark-runtime-v0.3.json
```

Each workload performs one warmup and seven measured repetitions. The artifact retains every steady-clock sample, median and nearest-rank p95. With seven samples p95 is simply the maximum; this is not a statistically qualified tail estimate. It records the compiler, build assertion mode, reported logical threads and executable SHA-256. Record CPU model, compiler/build configuration, OS and background activity alongside the capture when comparing hosts.

The prepared path performs 4,096 single-byte identity invocations without job submission. The one-node runtime path submits and completes 256 single-byte jobs using an existing one-worker pool with tracing disabled. Reported times are totals for each named workload, not per-call numbers. Registration, plan compilation, pool construction and Python/C transport are excluded; output assertions and diagnostic collection are included. Comparing these cases directly requires dividing by their different invocation counts.

The fork/join graph copies one 512-KiB seed, runs eight independent SHA-256 nodes over that immutable shared result, and joins through explicit control dependencies. It exports all eight digests and the join result. It performs 4 MiB of hashing per graph; shared inputs avoid an extra full-buffer copy on each edge. Cases use serial job execution with four available workers, then parallel scheduling with one, two and four workers. All use the same prepared graph, provider, payload, correctness checks and 128-event trace cap. Expected hashes are calculated outside timings through the existing common SHA implementation; independent cryptographic expected vectors remain the responsibility of the conformance suite.

Diagnostics show observed peak reserved compute slots and retained logical payload bytes. Peak slot counters include warmup and earlier repetitions on the same pool. They are not process memory, total machine threads or internal callback allocation measurements. No NUMA, accelerator, cross-process, publication or SQLite claim follows from this graph benchmark; transaction benchmarks cover their own workloads.

Run benchmarks sequentially after all builds/tests stop. Do not run this executable under CTest. Compare only matching suite/workload definitions and build environments; retain regressions and raw samples. A thread-count increase is an experimental condition, not an assumed improvement.
