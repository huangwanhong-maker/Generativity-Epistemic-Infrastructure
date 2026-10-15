# ADR-0004: compact history, transaction admission and owned buffers

**Class:** Implementation architecture decision  
**Status:** Implemented in the authorized v0.4 increment; architectural review pending  
**Date:** 2026-10-13 JST  
**Scope:** Layers 4–6 and an isolated experimental Layer 3 consumer

## Issue and authority

The user requested assessment followed by substantive foundational refinement and a small Layer 3 composability experiment. The [assessment and plan](../docs/reviews/v0.4-assessment-and-plan.md) preceded source changes. Preserved v0.3 profiling attributes most tracked-publication latency to work outside physical commit calls. Complete history bundles repeatedly duplicate, decode, validate and serialize old states. General computation also needs physical buffer ownership and numerical declarations.

This decision refines ADR-0003's history representation and operand carrier. It preserves semantic/physical separation, pure prepared execution, workspace CAS and exact frozen recovery. Its prohibition on any Layer 3 code is superseded only for the authorized isolated experiment. No GR concept or programme publication status changes.

## Alternatives and selection

| Issue | Alternatives | Selected direction and reason |
|---|---|---|
| History | Full bundles; unbounded deltas; persistent trees; bounded hybrid | Content-addressed headers/state payloads, bounded checkpoint/delta chains and incremental catalogue. Preserve exact v1 semantic nodes as compatibility oracle; defer persistent trees until inventory costs justify complexity. |
| Writer concurrency | Workspace OCC; row/write-set OCC; history-head OCC; serialized admission; group commit | Preserve workspace CAS and add optional process-local FIFO admission before base capture. Incomplete predicate/dependency tracking cannot justify finer isolation. Avoid wasted work without claiming parallel SQLite writers. |
| Validation | Global cache; unchecked flags; local memo | Bounded session with copied registry and exact wrapper equality. Always check complete candidate dependencies and required source interpretations. |
| Memory | Universal tensor; borrowed pointers; named owned views | Independent aligned host buffers and checked views. Logical types remain registry extensions; async inputs own their backing. Account for both logical bytes and retained allocations. |
| Numerics | Single determinism enum; implicit epsilon; orthogonal declarations | Optional fingerprinted numerical contract separates accuracy, ordering, overflow and repeatability. No implicit arithmetic relaxation. |
| Layer 3 validation | Full language; JSON-only example; isolated typed consumer | Experimental module selects historical data and runs checked integer transformation/reduction through existing plans/providers, shared by C++ and Python. |

## Contracts and consequences

New automatic histories use manifest profile 2. Existing v1 manifests remain readable and preserve append encoding unless explicitly upgraded. Fresh explicit v1 imports retain profile 1 unless upgrade is requested. Compact encoding preserves existing semantic versions, attribution, parent order and merge decisions. An ordinary upgrading coordinator commit also appends its normal attributed node; the standalone conversion helper preserves the selected head. Compression bases do not add semantic ancestry. Atomic import/upgrade respects existing transaction limits and never publishes partial conversion. Unselected branches remain retained; no deletion or garbage collection is introduced.

Readers verify fetched object hashes and reconstructed identities. Selected reconstruction uses a bounded chain; arbitrary older-revision lookup traverses catalogue metadata. Full export/audit deliberately validates the complete DAG. Ordinary reads do not audit every disconnected historical branch. Full v1 state-inventory hashing, candidate dependency checks and SQLite snapshot membership remain O(snapshot). Long-term indexing, archival custody and retention policy remain open.

Serialized admission covers cooperating callers within one process/workspace. SQLite's resolved filename is only a coordination hint: aliases, other processes and direct backend writers may bypass it but cannot bypass physical CAS. Wrappers may forward the hint; otherwise coordination covers the same store object. A non-thread-affine lease releases after a publication attempt, rollback or destruction. Queue waits support deadlines/cancellation. Holding a transaction indefinitely can delay other cooperating writers; no SQL transaction spans user computation.

Serialized admission explicitly requires an empty expected revision and captures the head after admission. Optimistic remains default. A stale preflight without an existing receipt is a definite conflict before a frozen request exists. Existing receipt replay still prepares and compares the exact request. Unknown outcomes preserve frozen requests and matching-receipt recovery; acknowledged success wins late cancellation and survives subsequent backend unavailability.

Validation reuse assumes trusted pure callbacks remain stable. Memo entries retain exact wrappers and a copied registry, are bounded and transaction-local, and never replace full dependency checks. There is no public unchecked-admission flag or database cache hiding later corruption.

Views pin immutable host ownership. Python imports copy; optional read-only buffer export pins native ownership through the real buffer protocol. Shape/strides alone do not establish Arrow or DLPack conformance. Device synchronization requires future adapters. Numerical descriptors constrain declarations but do not prove implementations correct; the experiment uses checked integer arithmetic and fixed reduction order.

## Compatibility and verification

Canonical JSON/record v1, semantic-history v1 and SQLite schema 1 remain. Untracked request identities and v1 tracked append identities remain stable. Compact publications deliberately have different physical request/revision identities. C ABI 1 additions are additive; C++ consumers rebuild because no stable binary layout is promised. Optional CPython export does not add a Python dependency to native-only builds.

Affected: record admission, buffer/value/numerical descriptors, planner accounting, history codec/store helper, transaction coordinator, C/Python access, experimental module, packaging, tests and documentation. Validation includes differential history export, branches/merges/corruption, adversarial memo entries, concurrent admission, exact replay and uncertain outcomes, process termination, buffer layouts/lifetimes, async cancellation, installed consumers, ASan and matched Release benchmarks. Actual results and residual limits belong in [verification](../docs/verification.md). Process-crash tests do not certify power-loss survival.

Stop after v0.4 results and readiness assessment. Broader Layer 3 requires architectural review and an explicitly selected workload.
