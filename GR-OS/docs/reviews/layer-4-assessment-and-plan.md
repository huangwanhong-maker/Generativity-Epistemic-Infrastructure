# Layer 4 assessment and development plan

**Class:** Informative architecture assessment and implementation plan  
**Date:** 2026-10-13  
**Status:** Selected for the user-authorized runtime increment, before substantive implementation

## Assessment of the current foundation

The v0.2 source provides useful independent contracts: immutable record versions and qualified projections; exact operand/operation fingerprints; several physical providers; memory/SQLite atomic workspace publication; retry receipts; verified binary readers; a pure semantic checkpoint DAG; and installed C/Python consumers. A fresh baseline passes all 12 CTest suites. These guarantees are worth retaining because they are tested, not because compatibility is mandatory.

It is not yet a runtime. Every execution call resolves, copies and hashes registry descriptions again, and threaded calls create their own worker pools. Calling those backends concurrently from a new scheduler would multiply threads. Storage composition lives inside the C binding, so a new C++ runtime would otherwise duplicate semantic admission. There is no owned asynchronous plan/job lifetime, global admission budget, transaction session, unknown-outcome state, durable-history composition or command-line operating surface. The semantic DAG and physical ledger are correctly separate but not yet published together by a coordinator.

The 1 MiB canonical JSON value bound differs from the 32 MiB aggregate snapshot limit. Generic typed JSON operations must retain the former; transactional snapshot commands cannot silently inherit a smaller undocumented scope. Full checkpoint history and copied Python transport also limit scale. v0.2 benchmark results show that tiny operations can become slower with threading. Adding concurrency alone would not solve these issues.

Review inputs include actual kernel/storage/execution/SDK/build/test sources, current verification and decision registers, the Programme Brief, R8 runtime document 25, compiler document 19, instruction/registry documents 21–22 and boundary/interface/acceptance documents 28/31/32. Existing programme drafts remain proposals, not adopted Standards. This increment coordinates existing GR semantics; it does not redefine historical truth or collapse recording ancestry into represented occurrence.

## Architecture and alternatives

Select a **typed graph interpreter with prepared native execution and explicit transaction barriers**, exposed as an independently usable C++ library, C ABI/Python SDK and native CLI.

| Area | Selected now | Alternatives and reasons |
|---|---|---|
| Instruction representation | Small versioned typed DAG with literals, result references and control dependencies | Bytecode/stack VM adds an ISA without current need; MLIR is appropriate for future computational dialects but too large as mandatory metadata/transaction machinery |
| Planning | Validate complete graph, effects/types and exact contracts; choose compatible provider/backend; prepare immutable handles | Dynamic name lookup per invocation repeats work; speculative algebraic rewrites have no declared equivalence laws |
| Scheduling | Fixed worker pool, ready queues, bounded admission, priority aging and explicit serial mode | Lock-free work stealing complicates verification at present graph scale; established TBB/OpenMP can own internal parallelism under future resource leases |
| Transactions | Pinned workspace snapshot, staged semantic changes, optimistic whole-workspace CAS, short native store transaction | Long SQL transaction across computation increases contention; row-level protocols and distributed 2PC exceed current storage guarantees |
| Modules | Explicit trusted registration, exact dependencies and sealed catalogs held by plans | Dynamic shared-library loading/unloading requires an ABI and isolation model not currently available |
| Distribution | Local process first, explicit execution and storage ports | A daemon/cluster would require durable job intent, authentication, leases, placement and network failure semantics |

MLIR supports extensible multi-level representations and lowering; it remains a future compiler-provider choice rather than a mandatory runtime dependency. See the [MLIR rationale](https://mlir.llvm.org/docs/Rationale/Rationale/) and [dialect conversion contract](https://mlir.llvm.org/docs/DialectConversion/). A future lowering must preserve typed results, effects, qualifiers, authorization boundaries and numerical guarantees, not merely produce faster code.

## Subsystems, interfaces and invariants

### Prepared engine and catalog

The existing separate type, operation and implementation registries remain semantic authorities. Add immutable validation/execution handles so preparation captures exact descriptions and trusted callbacks once. Runtime catalogs own registered backend instances, module manifests and exact dependencies. Sealing prevents later mutation; plans retain code/object lifetime. Registering a module grants no store access or external effects.

A graph node identifies an operation, typed literal/result operands, explicit control predecessors, selection requirements and input/output budgets. Validate every node, including unreferenced nodes; exceptions are observable, so unproven dead-code elimination is prohibited. Reject missing references, arity/type mismatches, cycles, unavailable providers, unsupported effects and insufficient resources before any callback.

A prepared plan pins type/operation/provider/backend/device identities and a content-derived plan digest. Selection is capability-filtered and deterministic, with explicit preferences/pins and no silent semantic downgrade. A bounded cache belongs to the sealed catalog/planner context. Compiler transformation providers remain a designed extension point; no unchecked user rewrite is presented as verified optimization.

Initial executable graph effects are pure/exact host-value operations. Kernel record verification is a real semantic operation under its bounded JSON contract. Snapshot mutation/publication belongs to the explicit transaction coordinator, outside arbitrary provider effects. Future snapshot-read, approximate/device and external-effect bindings require their own supported contracts.

### Scheduling and resources

The runtime owns persistent worker threads and one deadline/control monitor. Prepared node dispatch does not create nested worker pools. Workers take dependency-ready instructions; explicit edges are barriers. Concurrent jobs share compute capacity. Priority aging prevents indefinitely bypassing ready low-priority work, provided callbacks eventually return. Serial reproducibility mode preserves stable node order within a job; concurrent trace timing/first failures are not claimed deterministic.

Admission bounds live jobs and reserved retained payload, including completed job results until handles release them. Per-job bounds cover literals plus retained outputs; per-node limits remain active. These are logical payload reservations, not an OS heap sandbox: descriptors, containers, call stacks and arbitrary trusted callback allocations are separate overhead. Plans/cache also have separate bounds. No promise of NUMA placement, accelerator residency or zero-copy device interchange is made.

Cancellation is a request propagated to descendants and active callbacks. Failure stops unscheduled work, requests cancellation, and waits for active sibling calls before finalization. Deadline is measured from admission and is cooperative. Shutdown rejects new work, cancels outstanding jobs and joins all workers. Job results remain inspectable independently of the runtime object's lifetime. Nested synchronous waits on the same saturated runtime are forbidden to avoid pool deadlock.

### Transaction coordination and historical integrity

Move whole-candidate admission and artifact closure out of the C adapter into one native coordinator used by C++, Python and legacy convenience entry points. Begin captures an immutable workspace base and required storage guarantees. Stage collects a bounded atomic group; full reference admission occurs before commit. Rollback discards unpublished staging and performs no speculative persistent writes. No SQL transaction spans computation.

A job may bind a transaction at submission, execute a pure graph, materialize selected byte outputs as explicitly attributed records/artifacts, then cross one publication barrier after all nodes succeed. It does not treat computation itself as an ACID transaction. The caller supplies workspace/actor/reason/transaction identity, and runtime policy must explicitly permit publication to that workspace.

Optional tracked history uses an explicit reserved manifest record plus a verified history-bundle artifact. Publish candidate records, manifest, bundle, other artifacts and receipt atomically in the same store transaction. Existing tracked histories append on later coordinated writes; explicit imported branch/merge bundles must match the final current inventory excluding the manifest and retain required artifact custody. The physical ledger remains linear. Checkpoint duplication and storage quotas remain visible costs.

Freeze the exact prepared storage request before publication. A successful native commit wins over cancellation arriving afterward. An unexpected error after entering commit may mean **outcome unknown**. Receipt recovery and identical frozen-request retry are separate from rollback; receipt absence alone does not prove no concurrent publication. Conflict never silently rebases or reruns a semantic decision.

### ACID scope

| Property | Promised scope |
|---|---|
| Atomicity | One workspace on one selected transactional backend: records, required blobs, history manifest/snapshot membership and receipt |
| Consistency | L5 validates the complete candidate and history/artifact closure; L6 verifies immutable content and physical inventory |
| Isolation | Pinned immutable reads plus whole-workspace optimistic CAS; successful writes serialize against the captured base; unrelated writes can conflict |
| Durability | Memory is volatile. SQLite uses its native WAL/FULL synchronization and recovery, subject to filesystem/device guarantees |

SQLite serializes writers and WAL allows readers to retain earlier snapshots; this does not make an arbitrary sequence of separate runtime calls one SQL transaction. See [SQLite isolation](https://www.sqlite.org/isolation.html). WAL transactions spanning attached databases are not atomic as a set; our scope is narrower and excludes cross-store atomicity. See [SQLite WAL](https://www.sqlite.org/wal.html).

Cross-store artifacts, caches and external services require staged immutable objects plus a transactionally published manifest/outbox and idempotent reconciliation. Such a future protocol provides its declared convergence/recovery guarantees, not automatic distributed ACID. Derived indexes can be rebuilt; authoritative records cannot depend on an uncommitted external cache. Email/publication cannot be rolled back by deleting a local intent. No external-effect worker or distributed transaction protocol is implemented in this increment.

### Public interfaces and diagnostics

C++ plans/jobs/transactions own or share resources explicitly. The C ABI remains additive; Python owns opaque process-local handles with bounded polling, cancellation and cleanup. Runtime close cannot join workers while holding the global handle registry lock. Store leases survive closing the caller's wrapper. Completed jobs retain bounded result leases until release.

The native CLI offers machine-readable JSON and a bounded session mode when persistent handles are needed. One-shot invocation cannot promise that asynchronous handles survive process exit. CLI/Python reuse native semantics. Traces record plan/implementation identity, queue/start/finish/commit events, errors and measured durations; they are operational evidence, not authenticated scientific provenance.

## Implementation and acceptance sequence

1. Record this assessment and update local scope instructions.
2. Implement prepared contracts/catalog/planner, scheduler/resource/job lifetime, and transaction coordination in parallel with agreed headers.
3. Integrate SDK/CLI, atomic graph-result publication and optional history custody.
4. Test whole-graph rejection, provider selection/parity, dependency order, fairness, caps, cancellation/deadlines, joined failure/shutdown and explicit policy denial.
5. Test shared memory/SQLite transaction behavior, conflicts, frozen retries, uncertain outcomes, missing/corrupt artifacts and history admission.
6. Use child-process termination/restart for precommit, in-transaction and postcommit/preconfirmation windows; distinguish these tests from exceptions and from physical power-loss testing.
7. Run complete regression suites, installed C++/Python/CLI consumers and examples. Investigate available sanitizer/stress options without claiming unavailable platform coverage.
8. Benchmark prepared dispatch, graph scheduling/scaling, payload reservations and concurrent SQLite transactions; compare unchanged v0.2 workloads and report regressions.
9. Document implemented versus designed boundaries, readiness for a bounded Layer 3 consumer, and remaining Layer 4 work. Do not implement Layers 1–3 or migrate legacy data.

