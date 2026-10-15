# GR-OS open architectural decisions

**Class:** Informative decision agenda  
**Status:** v0.6 complete; stopped for review before broad Layer 2 or migration
**Date:** 2026-10-15 JST

The initial [assessment](../../ARCHITECTURE_REVIEW.md) remains historical discussion. The user selected explicit R8 layering, C++ with Python access, and implementation of Layers 5 and 6 followed by review. [ADR-0001](../../decisions/ADR-0001-layered-development-and-layers-5-6.md) records this disposition; [the implementation contract](../layers-5-6.md) specifies the first bounded version. Detailed v0.1 choices remain reviewable and do not imply approval of later layers.

The v0.1/v0.2 dispositions below are historical context. The user subsequently authorized Layer 4 assessment and implementation; [ADR-0003](../../decisions/ADR-0003-prepared-runtime-and-atomic-publication.md) records that stage, which excluded Layers 1–3. OS-01 gained a native CLI; OS-06/09 gained runtime coordination and atomic semantic-history publication; OS-10 gained prepared plans and asynchronous scheduling. The v0.4 authorization and current review agenda follow. OS-05 canonical bytes, OS-07 temporal distinctions and OS-11 C++/C/Python language direction remain unchanged.

## v0.6 dispositions and next review decisions

[ADR-0006](../../decisions/ADR-0006-layer3-maturation.md) records the selected
increment. The earlier agendas below remain historical. The next roadmap is:

1. **PROVISIONAL: selected independent Layer 2 modules.** The external diagonal
   package composes custom structures with standard reduction and the existing
   runtime. Select one real analysis/modeling/visualization consumer and its
   acceptance workload after architectural review. The package is an engineering
   boundary test, not approval to build the whole ecosystem.
2. **OPEN: numerical reference and approximation contracts.** Ordered finite
   binary64/complex machine arithmetic and checked integers are implemented.
   Choose reference computations, error acceptance and nonfinite policies before
   approximate solvers, parallel reductions, fusion, float32 or GPU providers.
3. **PROVISIONAL: package distribution.** Independent exact-version C++ source
   builds and a narrow package-owned Python facade are demonstrated. Decide
   supported compiler/platform matrix, signing/trust, compatible-version policy
   and shared Python registration before a public package ecosystem. Native code
   is trusted; manifests do not grant record/storage authority.
4. **OPEN: sustained workload and storage scale.** Weak validation certificates,
   owning strided access and bounded parsed-source reuse reduce measured work.
   Qualified results still embed source bytes; every new owner is checked. Select
   real history sizes, custody policies and memory/latency targets before shared
   persistent source objects, indexes, chunk streams or durable query caches.
   Small/cheap validation and several Python import/export phases regressed in
   the local captures; choose cache policy and transport improvements using the
   next consumer's workload, not a universal speedup assumption.
5. **OPEN: runtime parameters and cancellation.** Function composition reuses
   immutable specialization, but binding still constructs a concrete pinned plan.
   Define reusable runtime parameters and stream/backpressure only when required.
   Trusted validation/inference callbacks and serialized providers remain
   cooperative; no preemptive wall-clock guarantee is claimed.
6. **OPEN: interoperability expansion.** NumPy ownership and numeric copy/view
   paths work for their declared domains. Arrow, SciPy and DLPack need explicit
   validity, duplicate, device, lifetime and qualification-loss contracts.
7. **OPEN: release and authority.** Windows x64 remains the tested platform.
   Linux/macOS, TSan, device execution, process isolation, authenticated access,
   durable executable provenance and legacy GSP/GRRP migration remain separate
   decisions. No software test confers standards or institutional authority.

v0.6 verification is complete and work stops for review. Broad Layer 2, application migration
and programme-source changes remain outside this increment.

## v0.5 dispositions and next review decisions (historical)

The user authorized [v0.5](../reviews/v0.5-assessment-and-plan.md) after the v0.4 milestone. [ADR-0005](../../decisions/ADR-0005-extensible-computational-language.md) selects a native typed expression language over existing prepared runtime plans. The v0.4 and earlier agendas below remain historical records, not current prohibitions on this implementation.

1. **PROVISIONAL: language foundation.** Exact versioned structural families, separate representations, generic inference/specialization and explicit shared lazy expressions provide a working extension path. Resolve executable module distribution/version provenance and cross-library interoperability before a third-party ecosystem. Metadata cannot install or sandbox code.
2. **OPEN: numerical domains and equivalence.** Decide floating/complex arithmetic, nonfinite/missing policies, tolerances and reference computations before approximate providers, fusion or reassociation. Physical payload hashes are not mathematical equality across layouts.
3. **OPEN: measured compilation and validation cost.** Fresh sealed specialization catalogs preserve existing invariants. Use pilot evidence to choose reusable specialization caches, validation certificates or coarser operators; require exact invalidation and semantic conditions.
4. **PROVISIONAL: bounded chunking.** Named expression inputs can bind successive immutable chunks; each binding produces a concrete plan. Define external parameters, streams/backpressure, checkpoint/restart and data-source pinning before claiming a streaming engine.
5. **OPEN: qualified source custody and export.** Retained wrappers, direct archived referents, source revisions and projection losses are explicit. Decide access/retention boundaries, portable recipe/source packages and indirect reference closure before broad derived-dataset sharing.
6. **PROVISIONAL: host interoperability.** Concrete NumPy ownership tests establish their tested scope. Arrow validity/schema callbacks, SciPy duplicate/traversal policies, DLPack device/stream lifetimes and graph projection policies require separate adapters and conformance.
7. **OPEN: Layer 2 readiness.** Select representative reusable tools and scientific contracts after reviewing v0.5. No application, scientific model, legal authority or adopted standards status follows from the computation language.
8. **OPEN: platform and scale.** Windows-native evidence, bounded host memory, cooperative cancellation and one-process trusted extensions remain the qualification scope. Large histories, external stores, GPU/JIT/MPI and Linux/TSan need explicit workloads and verification.

Stop after this implementation for architectural review. Layers 1–2 and legacy migration remain outside the authorized scope.

## v0.4 dispositions and next review decisions (historical)

The user subsequently authorized the [v0.4 refinement](../reviews/v0.4-assessment-and-plan.md)
and one isolated experimental Layer 3 consumer. [ADR-0004](../../decisions/ADR-0004-compact-history-admission-and-owned-buffers.md)
supersedes earlier no-Layer-3 scope only for that experiment. Historical tables below
retain their original stage-specific reasoning.

1. **OPEN: first real Layer 3 workload and measurable acceptance.** Choose dataset shapes,
   history depth/branching, artifact sizes, latency/throughput targets and supported platforms
   before a general dataframe/expression language or standard library. The
   [v0.4 measurements](../validation/benchmark-v0.4-analysis.md) identify tiny-dispatch
   overhead, full inventory work and many small SQLite history reads as concrete targets.
2. **PROVISIONAL: compact physical history.** Exact v1 semantics and bounded reconstruction
   are implemented. Catalogue lookup remains linear, full physical snapshots remain stored,
   and upgrade is bounded by 128 uploaded objects. Decide indexing, archive/GC custody and
   policy before claiming long-lived retention at scale.
3. **PROVISIONAL: writer admission.** Optional process-local serialization avoids speculative
   contention while physical workspace CAS remains. Read/predicate sets, multi-process
   coordination and alternative databases need representative evidence before finer isolation.
4. **PROVISIONAL: memory interoperability.** Owned host buffers and optional Python read-only
   export are implemented. Choose specific Arrow/NumPy/DLPack adapters and device/stream
   synchronization contracts; layout resemblance alone is not compatibility.
5. **OPEN: approximate computation.** Numerical declarations distinguish ordering, overflow,
   rounding and repeatability. Execution remains pure/exact. Define reference computations,
   acceptance policies and adversarial conformance before floating reductions or rewrites.
6. **OPEN: durable computational intent and authority.** Live frozen retries and receipts do
   not persist a language/runtime job or authenticate attribution. Validator/provider version
   replay, authorization, disclosure, signed warrants and external effects remain distinct.
7. **OPEN: release/platform qualification.** Windows evidence does not qualify Linux/TSan,
   GPU/MPI, packaging distribution, binary compatibility or production operation.

The v0.4 stop point was superseded by the subsequent v0.5 authorization above.

## v0.3 dispositions (historical)

The [runtime architecture](../runtime.md), [transaction contract](../runtime-transactions.md)
and [verification](../verification.md) distinguish implemented behavior from future design.

| Priority | Decision before broader implementation | Current disposition / evidence needed |
|---|---|---|
| 1 | Layer 3 pilot scope and performance targets | Bounded exact host-value DAGs are usable; choose a real workload, data/history sizes, latency and throughput targets before general language work |
| 2 | Array/buffer ownership and numerical contracts | Immutable host bytes/JSON only; specify shape/layout, zero-copy lifetime, tolerances, transfer and device completion before GPU/SIMD integration |
| 3 | Long-lived semantic-history representation | Complete bundles/checkpoints and artifact closure are implemented; accumulated immutable bundles hit finite storage limits. Decide incremental custody, indexing and explicit retention before claiming scale |
| 4 | Transaction concurrency granularity | Pinned view plus whole-workspace CAS is implemented and conservative; measure conflicts before introducing read/write sets, row/predicate coordination or PostgreSQL |
| 5 | Durable recovery intent | Live frozen retries and persisted receipt lookup exist; define serializable intent, validator/provider version replay and durable job lifecycle for restartable execution |
| 6 | Multi-user authority and extension trust | Local operation/workspace policy is implemented; principal authentication, disclosure/retention enforcement and plugin isolation remain OPEN |
| 7 | Compiler/backend growth | Prepared plan/provider boundary exists; choose measured compiler passes or internally parallel/device backends only with explicit semantic and resource obligations |
| 8 | Platform and release scope | Windows x64 validation, installed C++/Python/CLI and optional ASan evidence; Linux/TSan, packaging distribution, ABI/release ownership and licence choices require deliberate qualification |

Unsupported isolation/durability requests fail; no cross-store ACID, silent rebase,
external-effect rollback, runtime daemon or distributed scheduling is implemented.
Outbox/immutable staging and reconciliation are architectural directions only.
Signed receipt attribution, target continuity, source warrants and authenticated
historical provenance remain separate unresolved concerns.

## Earlier decision agenda (retained context)

| ID | Decision | Proposed starting direction | Evidence needed for disposition |
|---|---|---|---|
| OS-01 | First operating context and scale | Independently usable local CLI/library, with hosted service boundary designed explicitly | Required platforms, offline/browser needs, actors, record/history/artifact sizes, concurrency and latency targets |
| OS-02 | Minimum semantic contract | Small core with mandatory qualified-history semantics and domain profiles | Worked contested claim, evolving n-ary relation, category revision, and opaque scientific artifact |
| OS-03 | Compatibility obligations | Original-byte preservation and versioned legacy readers; distinct identity for translations | Per-family GSP/GRRP matrix: exact, qualified, lossy, unsupported, unresolved; active-data and cutover needs |
| OS-04 | Actor, signer and custody | Explicit account, acting party, signer, registrar, delegation, and host identities | Two-party workflows, signature scopes, key ownership/custody and portability requirements |
| OS-05 | Canonical value model | Versioned bounded metadata encoding plus typed external artifacts | Numeric/Unicode/adversarial vectors; legacy Python/TS numeric discrepancies; arbitrary precision and unknown-type requirements |
| OS-06 | Admission and history boundary | Atomic publication of versions and receipt under explicit concurrency preconditions | Concurrent writers, forward references, restart/retry, blob staging and backup/restore closure |
| OS-07 | Temporal interpretation | Query selects source revision, account set/policy, and temporal cut | Competing individually coherent chronologies, contradictory combined view, retrospective evidence and source exclusions |
| OS-08 | Authorization and retention | Current access enforcement across reads, caches, workers and export; explicit restriction/erasure model | Revocation during a job, partial histories, disclosure closure, retained originals, hashes and backup policy |
| OS-09 | Persistence authority | Evaluate transactional SQL versions/catalog plus artifacts | Recovery tests and workload measurements; deliberate disposition of GRRP C11 plain-file authority; first local/hosted choice |
| OS-10 | Runtime and execution | Direct dispatch and bounded coarse jobs initially | First long-running analytical task, timeout/cancellation, provider uncertain success and idempotency behavior |
| OS-11 | Language and public interfaces | Selected: C++20 shared semantic implementation, versioned C ABI, Python facade; TypeScript for future higher clients | Review transport copy costs, packaging, array interface and additional platform qualification |
| OS-12 | Extensions and analytical representation | Explicit package registration and typed projection APIs | New domain, unknown typed artifact, reusable method and second implementation without unnecessary core edits |
| OS-13 | Repository and release structure | One new implementation workspace with preserved source provenance | Separate GR-OS Git repository versus parent tracking, release/version ownership, license/attribution import strategy |
| OS-14 | First acceptance slice | CLI/API record, relation, contested history, two actors, import, protected export and recovery | Agreed observable behavior and meaningful compatibility/negative fixtures before implementation |

## v0.2 dispositions and immediate priorities

[ADR-0002](../../decisions/ADR-0002-independent-contracts-and-foundation-reconstruction.md) records the authorized second increment. The earlier table and v0.1 discussion remain historical context; the following dispositions now refine them:

- OS-02/07: pure checkpoint DAG with reconstruction/explicit-base merge, independent represented-time projections, and declared event-anchor/precedence profile capabilities are implemented. Replay semantics, target continuity and restricted-history export remain OPEN.
- OS-05/11: canonical bytes remain v1; strict parsing/golden vectors, relocatable native exports, separately packaged Python SDK and raw binary reads are implemented. Arrays, numerical equivalence and complete additional-platform qualification remain OPEN.
- OS-06/09: typed transactional capabilities, receipt lookup and verified immutable readers are implemented. Physical stores advertise linear history; branch-aware publication, resumable uploads, power-loss recovery qualification and accumulated-history quotas remain OPEN.
- OS-10/12: exact pinned type/operation/provider contracts and explicit host CPU selection are implemented. A second SHA provider and external multi-input/result extension demonstrate independent registration. Approximate execution, device memory, async completion and runtime selection remain OPEN.

Resolve these priorities before the next substantial increment:

1. Select representative scale/workloads and supported platforms. Whole-checkpoint history and Python JSON transport need measured targets before indexing, delta storage or array APIs are designed.
2. Decide whether the next slice needs integrated durable semantic branches or linear publication of retained history artifacts. Define branch-head concurrency, merge-base selection, closure and recovery before changing storage formats.
3. Choose the next operand representation and equivalence contract: exact domain values, numerical arrays with declared tolerances, or a real device workload. Do not broaden effects without an authority/transaction boundary.
4. Decide extension distribution/trust and validator version provenance. Descriptors pin declared meaning, not installed machine code; Python callback/plugin loading is not implemented.
5. Decide whether schema-v1 receipt attribution needs a v2 canonical request summary/signature contract. Current link checks cannot independently authenticate actor/reason/transaction fields from receipts alone.
6. Revisit target continuity, provenance warrant, signer/custody, disclosure and retention alongside an explicitly authorized Layer 4 design. None follows from a checksum or this local SDK.

Additional review questions retained from the first implementation:

- Should future mutation preserve unknown required constraints through a separately authorized translation path, or require the original type package in every case?
- RESOLVED for v0.2: projections accept explicitly registered event-anchor/precedence profiles; more general temporal semantics still require a new declared contract.
- What workload justifies zero-copy Python arrays, external large-object storage, history quotas, incremental snapshots or another execution device?
- How should versioned validator changes interact with already accepted transaction retries and historical read interpretation?
- Which authentication, actor/signer distinctions, restriction/retention requirements and external-effect policies belong in the first Layer 4 contract?

Programme-level conceptual and standards questions remain in [the parent register](../../../docs/planning/open_questions.md). No parent decisions or publication statuses are changed by this agenda.
