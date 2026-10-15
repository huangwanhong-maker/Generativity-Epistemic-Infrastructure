# GR-OS v0.4 architectural review

**Class:** Informative implementation assessment. **Status:** Experimental, for architectural review. This is not an adopted Standard or a production qualification.

## Assessment and scope

The six-layer direction remains useful. Layers 4–6 now provide independent semantic contracts, prepared execution, scheduling and local atomic publication; they do not require a universal mathematical object model. The principal v0.3 deficiencies were representation and admission costs, plus the absence of owned typed memory. A radical rewrite would have discarded working recovery and extension boundaries without addressing those costs more directly.

The [pre-implementation assessment](v0.4-assessment-and-plan.md) and
[ADR-0004](../../decisions/ADR-0004-compact-history-admission-and-owned-buffers.md)
record alternatives and selected changes. Legacy infrastructure and application sources remain reference material. All implementation changes are under GR-OS. No user data was migrated and no programme Standard or GR concept was redefined.

## History representation

New automatic histories use immutable content-addressed headers, state payloads and catalogue links. Payloads choose a complete checkpoint or a smaller exact delta, with periodic checkpoints bounding reconstruction. Physical compression bases remain distinct from semantic parents. Current and archived inventories, attribution, parent order, explicit merge conflicts and v1 semantic node identities survive representation conversion.

An ordinary append normally creates three objects. A selected read follows its bounded reconstruction chain; an older revision may additionally require a catalogue walk. Explicit full export reconstructs and validates the complete retained DAG, including disconnected branches. Selected reads intentionally do not audit inactive branches. All new objects, material references, control manifest, records and receipt publish atomically. There is no implicit history deletion or garbage collection.

Existing v1 manifests retain their original append encoding unless explicitly upgraded. Fresh explicit v1 imports also remain v1 unless upgrade is requested. Upgrade is deliberate, non-destructive and atomic, bounded by the existing 128-object transaction limit. A large valid legacy history can therefore remain readable but be too large for a one-shot compact upgrade. The standalone upgrade helper preserves the selected head; an ordinary coordinator commit with `upgrade_history=true` also adds its normal attributed commit node. New compact physical receipts differ from v1 receipts; automatic nodes recording those different physical bases can also have different attribution-derived semantic identities. This is distinct from encoding an existing node, whose identity is preserved exactly.

Full checkpoints remain reasonable for dense change and interchange. Unbounded deltas create poor recovery latency. Persistent trees and indexed projections could further reduce inventory work but need more code, canonicalization rules and migration machinery. The hybrid is a smaller justified step. Full state-inventory hashing, candidate reference checks and SQLite snapshot membership remain O(snapshot); this is not an unlimited history engine. See [the exact storage contract](../history-storage.md).

## Transactions and ACID

The isolation contract remains a pinned workspace snapshot with whole-workspace compare-and-swap. Finer write-set or history-head concurrency would be unsound without complete read/predicate/dependency tracking. Optional process-local FIFO admission captures the current base after acquiring a logical writer lease, reducing speculative work for cooperating writers. It holds no SQLite transaction during caller computation. Other processes, aliases and direct backend writers still encounter physical CAS; no automatic rebase or weaker isolation was introduced.

Transaction-local validation sessions reuse verified immutable wrappers under a copied registry. Reuse compares wire structure with numeric-kind discipline, avoiding JSON's `1 == 1.0` equality trap. Source mandatory interpretations and candidate-wide references/participants/temporal dependencies remain checked. A stale preflight can reject before expensive preparation only when no receipt for that identity exists. Successful exact replay from an old base remains available.

| Guarantee | Boundary after v0.4 |
|---|---|
| Atomicity | Records, supplied/required objects, compact or legacy history manifest, receipt and head movement share one workspace/backend commit. Computation and external effects are outside it. |
| Consistency | Registered source/candidate semantics, reference availability, content identities and newly admitted artifact lengths/custody are checked. Hashes do not establish factual truth or actor authority. |
| Isolation | Whole-workspace CAS remains mandatory. Serialized admission changes local ordering, not the isolation scope. Read-only transactions retain their pinned view. |
| Durability | MemoryStore is volatile. SQLite retains WAL/FULL on local storage; guarantees depend on filesystem/device synchronization behavior. Process-crash tests do not qualify power-loss durability. |
| Recovery | Unknown backend outcomes retain the exact frozen request. Matching receipts establish acceptance; absence means not observed, not proof of rollback. Retry does not rerun providers or semantic interpretation. Acknowledged success survives later cancellation/unavailability. |

Queue waits honor transaction deadlines/cancellation. Scheduler submission also applies job deadlines and runtime shutdown to queued admission; leases can be released on a different thread. Cross-store transactions, group commit, durable jobs and external-effect rollback remain absent. See [the lifecycle contract](../runtime-transactions.md).

## Memory and numerical contracts

The new host-memory library owns aligned immutable allocations, optional allocation budgets, mutable construction leases, checked typed views, slices and named buffer sets. Views explicitly carry encoding, item size, alignment, endianness, shape, signed byte strides and offset. Negative/broadcast read-only layouts are supported; bounds and arithmetic are checked before execution. A view pins its complete allocation. Each BufferSet is charged the maximum of logical element bytes and unique retained allocation bytes, plus descriptors. Aliases across separate Values are conservatively charged again; this is not global allocation accounting.

Logical arrays, tensors, graphs and unfamiliar structures remain extensible semantic profiles over physical carriers. A buffer encoding does not assign units, missingness, mathematical equality or scientific meaning. Allocation budgets are opt-in and do not cover arbitrary provider temporaries or total process memory. Native code remains trusted; escaped mutable C++ pointers violate the borrow contract.

Python imports copy to establish ownership before asynchronous use. Optional CPython 3.11+ read-only export uses a genuine buffer-protocol owner that pins native memory beyond handle close; portable copying/readinto remains available. Native-only builds do not require Python. NumPy, Arrow and DLPack informed the interface, but v0.4 does not claim complete adapters or device synchronization.

Optional operation numerical descriptors pin storage/accumulation formats, overflow, rounding, nonfinite handling, reduction order, reassociation and explicit error references/tolerances. Accuracy and repeatability are separate. Descriptor pins identify declarations; they neither prove provider correctness nor automatically execute tolerance tests. Existing absent-numerical descriptors retain their fingerprints. Execution remains pure/exact; approximate acceptance and floating reduction equivalence require a later deliberate policy. Checked int64 sum preserves left-to-right overflow behavior, including the distinction illustrated by `[MAX, 1, -1]`. See [buffer and numerical details](../buffers.md).

## Experimental Layer 3 consumer

The isolated `gr.experimental.data@1` module selects a named record and explicit payload pointer at a historical semantic/physical revision. It constructs one owned little-endian signed-int64 column (at most 1,048,576 elements, without units or missing values), transforms two ordered slices through runtime providers, concatenates in source order, and performs one checked ordered reduction. Source JSON and transformation parameters retain canonical metadata integer/size limits, although binary values can represent full int64. Serial and threaded scheduling use the same contracts. Results expose owning buffers, selected record/version and physical/semantic provenance, transformation recipe, provider plan and execution diagnostics through C++ and Python; the CLI exercises the same native path.

This demonstrates that a new logical type and operations can be registered without changing the kernel's fundamental architecture. It also makes costs visible: source metadata is decoded into typed memory; transform/concatenation currently allocate intermediate outputs; reduction order limits reassociation; arbitrary native temporary allocations are not centrally capped. It is a composability experiment, not a dataframe language, universal tensor model, query optimizer or academic-domain ontology. Typed results are not implicitly archived without an explicit serialization/publication contract.

## Verification and performance evidence

The preserved pre-edit v0.3 suite passed all 20 cases. v0.4 Release passed all 28 CTest cases, including all previous suites, installed C/C++ consumers, Python wheel/exporter, native CLI, history corruption/branch/merge cases, memo adversaries, writer concurrency, buffer lifetimes and process-crash recovery. Detailed final sanitizer coverage, package identities, commands and benchmark comparisons are recorded in [verification](../verification.md).

The portable native admission profile separates begin/stage/commit and storage-call costs on the same record/update workload against preserved v0.3 libraries. The historical-selection runner measures checkpoint and delta positions through each version's available SDK path; it is explicitly end-to-end rather than a pure codec comparison. Existing prepared-dispatch, scheduling, parallel hash and common SQLite workloads remain separate. Buffer interoperability is new and has no claimed v0.3 equivalent. Raw samples and methodology are retained; improvements are not extrapolated into production-scale throughput or a latency SLA.

All 28 cases also passed under AddressSanitizer across corrected runs, including the native bridge, read-only Python exporter, CLI and relocated package. Initial DLL-loader failures and the large Python suite's watchdog timeout remain recorded; the unchanged suite ultimately passed with a longer outer watchdog. No sanitizer diagnostic was reported. This is not one clean 28-case invocation, ThreadSanitizer coverage or power-loss qualification.

The [matched measurements](../validation/benchmark-v0.4-analysis.md) show retained history blobs decreasing from 37.15 MB to 0.283 MB (99.24%) in the 128-record/33-publication fixture. Tracked SQLite admission/publication took 15.760→6.697 s (2.35x faster). An older selected SQLite state at delta depth 15 took 261.260→116.597 ms (2.24x faster), while the latest checkpoint improved more. Four tracked writers delivered 6.89→8.91 commits/s optimistically and 10.86 with explicit serialization; the latter had zero retries in this fixture.

Performance remains mixed. Common SQLite commit improved about 4%, without fully recovering the earlier reported 23% increase. Pooled native prepared and tiny-job timings regressed about 6.7% and 14.4%; serial-hash results varied substantially across four pairs and retained a pooled regression. Some untracked snapshot and contention cases also regressed. Compact SQLite object reads trade smaller bytes for more verified opens. These are concrete reasons to choose operation granularity, checkpoint policy and indexing against real Layer 3 workloads, rather than claim every path is faster.

## Readiness and decisions for the next increment

The foundation is suitable for a bounded, explicitly selected Layer 3 workload after review. A general-purpose data language or heterogeneous/distributed computation system would still be premature. The extension and ownership seams are usable; their policies and scale limits remain deliberate engineering choices.

The next decisions are:

1. Choose a real Layer 3 workload, data/history sizes and measurable correctness/performance targets.
2. Choose a small logical type/expression model and serialization/provenance recipes; do not turn physical buffers into a universal mathematical ontology.
3. Define approximate/floating acceptance, reduction order and numerical reference semantics before compiler rewrites or accelerated providers.
4. Select concrete ecosystem adapters and device completion/transfer contracts; add backends against evidence, not capability labels alone.
5. Decide history indexing, large atomic import and archival/retention policy before long-lived scale claims.
6. Decide durable job intent, versioned validator/provider replay, multi-user authority and supported release platforms separately from local computational correctness.

Broad Layer 3 development stops here pending architectural discussion. The [decision agenda](../planning/open_questions.md) retains unresolved semantic, institutional and engineering questions.
