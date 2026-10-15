# v0.4 foundational assessment and implementation plan

Class: implementation engineering report. Status: implementation plan, recorded before substantive v0.4 changes. This is not an adopted Standard. Scope: Layers 4–6 and one isolated experimental Layer 3 consumer.

## Assessment

The current separation is useful: Layer 5 defines valid records and semantic operations; Layer 4 plans, schedules and coordinates publication; Layer 6E implements execution; Layer 6S provides physical custody and atomic publication. C++, C, Python and CLI share native admission. Prepared provider fingerprints, immutable operands, workspace CAS, durable receipts and exact frozen retries are strengths to preserve. No evidence justifies replacing SQLite, introducing a VM/compiler, or merging semantic interpretation with physical storage.

The v0.3 source inspection identifies repeated full-history decode, DAG validation, serialization and retention on each tracked commit. Record envelopes and custom validators are repeatedly visited during begin, stage, source admission and candidate admission. Contending writers can perform these expensive steps before discovering a stale head. SQLite additionally stores complete physical snapshot membership. The missing general computation foundation is an owned physical buffer carrier, not a universal mathematical tensor class. Numerical accuracy, repeatability and reduction order need separate declarations.

These are bounded representation/admission deficiencies. Whole-workspace conflict scope is a deliberate safety property, not automatically a defect. General predicate/read-set tracking, streaming uploads, device synchronization, distributed transactions and scalable physical snapshot indexing can be deferred with explicit limits. Current native plugins are trusted code, not a security sandbox.

Before edits, the unchanged Release suite passed all 20 tests (47.79 seconds). Source hashes, installed SDK, Python facade, runner inputs and previous benchmark artifacts are preserved under `docs/validation/v0.3-pre-v0.4/` and build-local `build/baselines/v0.3/`. New comparisons will distinguish measured improvement from anticipated benefit.

## Selected history direction and alternatives

Keep v1 semantic node descriptors, versions, attribution, parent order and normalized reconstructed snapshots. Introduce a separate compact physical representation: immutable content-addressed node headers, checkpoint/delta payloads and append-only catalogue links. Compression bases are distinct from semantic parents. A bounded delta chain periodically checkpoints, or checkpoints early when a delta would be larger. A selected head requires only its bounded reconstruction chain; ancestry uses small verified headers. Explicit full export/audit reconstructs the complete retained DAG, including disconnected branches. Every fetched object is integrity checked; there is no process-global cache hiding changed storage bytes.

Full checkpoints remain simple and useful for interchange and dense changes. Unbounded deltas save space but give unacceptable reconstruction chains. Persistent trees can reduce inventory and indexing costs further, but add balancing, canonicalization and migration complexity without current workload evidence. The selected hybrid removes cumulative full-bundle duplication while preserving a simple v1 export oracle. It does not remove the O(snapshot identities) v1 state digest, whole-candidate reference checks, or SQLite membership writes. Catalogue lookup of an arbitrary old node remains linear in catalogue history; materialized indexes and garbage collection are deferred.

New histories use the compact profile. Existing v1 histories remain readable and retain their existing append encoding unless explicitly upgraded, protecting restart request identities. Import/upgrade is atomic and bounded by existing blob/transaction limits; no partial upgrade. Explicit v1 imports remain supported. All new history objects, record changes, artifacts, manifest and receipt enter one physical commit. No historical object is deleted by v0.4.

## Selected transaction direction and alternatives

Preserve workspace snapshot CAS and SQLite WAL/FULL. Add a transaction-local validation session that owns a copied registry, verifies immutable wrappers once, checks exact wrapper equality before memo reuse, and always repeats candidate-wide dependency and required-interpretation checks. Pure stable validator callbacks remain a documented trust requirement. No public unchecked-admission flag and no cross-transaction mutable database cache.

Offer explicit process-local serialized writer admission before capturing the current base. A non-thread-affine lease coordinates cooperating writers and supports cancellation/deadlines while queued. It does not hold a SQLite transaction during user computation. Optimistic admission remains available; external writers are still protected by physical CAS. Explicit expected revisions are not silently rebased. Release the lease after a publication attempt; unknown outcomes retain exact frozen recovery. A stale-head preflight may avoid expensive preparation only when it cannot suppress an existing receipt replay.

Finer-grained write-set OCC would permit write skew without complete read/predicate/dependency tracking. History-level OCC cannot alone protect shared current records. Those alternatives are deferred. Group commit and relaxed durability are not introduced. Safe local reductions of redundant canonicalization and statement preparation will be considered with profiling evidence.

## Buffer and numerical direction

Add an independently usable host-memory library: immutable shared aligned allocation; budget charged until the final owner dies; mutable construction with scoped tracked borrows; checked typed views with shape, signed byte strides, offset, alignment and byte order; named collections of views with bounded metadata. Physical encoding IDs remain separate from logical operand types. Arrays, graphs and unfamiliar structures can use different logical profiles over these carriers.

Execution accounting reports logical bytes and unique retained allocation bytes and conservatively covers both. Importing arbitrary Python memory copies before asynchronous use. Optional native read-only Python buffer export pins the native allocation and exposes no writable ctypes backing alias. No claim of Arrow or DLPack compatibility is made without implementing their full ownership/device contracts. Their formats and the Python/NumPy interfaces inform the design without becoming mandatory dependencies.

Optional numerical descriptors pin storage/accumulation formats, overflow, rounding, nonfinite behavior, reduction order, reassociation and explicit error reference/tolerances. Absent numerical contracts preserve existing operation fingerprints. The first experiment uses checked int64 transformation and ordered reduction: regrouping `[MAX, 1, -1]` must not silently change failure behavior.

## Implementation and verification sequence

1. Preserve v0.3 binaries and benchmark inputs; record this plan and update workspace scope.
2. Implement compact history codec/store helpers and differential, branching, merge, corruption and atomic-publication tests.
3. Implement validation reuse, writer admission and phase measurements without changing frozen request/recovery guarantees.
4. Implement native buffer/numerical contracts and invalid-layout, alias lifetime, resource, cancellation and asynchronous retention tests.
5. Add a separate experimental Layer 3 consumer selecting a historical dataset, constructing typed data, transforming/reducing through runtime providers and returning provenance and diagnostics through C++ and Python.
6. Run native/conformance/integration/process-crash tests, installed C/C++/Python/CLI consumers and ASan. Benchmark identical Release workloads against preserved v0.3, including contention, history growth/reconstruction, dispatch, parallel execution and interop costs. Do not infer power-loss durability from process termination.
7. Update ADRs, architecture, verification, limits and roadmap, then stop for architectural review before broader Layer 3 work.

The scope deliberately excludes legacy migration, programme publication changes, broad Layer 3 language development, JIT/MLIR, GPU/MPI/OpenMP implementations and a custom database engine.
