# ADR-0002: independent contracts and foundation reconstruction

**Class:** Implementation architecture decision  
**Status:** Implemented for user review within the authorized second increment  
**Date:** 2026-10-13 JST  
**Scope:** GR-OS Layers 5, 6S and 6E

## Issue and alternatives

v0.1 has useful admission, transaction and execution invariants but combines meaning and callback in one operation registry. Projections do not consistently use caller registries; only physical linear history exists; binary reads materialize whole objects; SDK consumers depend on the source layout.

The user authorized assessment followed by substantive reconstruction. The [assessment and plan](../docs/reviews/architecture-v0.2-plan.md) preceded source edits. ADR-0001's layered direction remains selected. Its one-registry execution API, hardcoded temporal types and in-tree-only distribution are superseded here.

Options considered:

1. Extend v0.1 incrementally without changing registries: low immediate work, continued conflation of operation and implementation.
2. Implement the full proposed runtime/compiler/plugin architecture: broad future interfaces, but speculative obligations and premature Layer 4 work.
3. Introduce exact semantic contracts with separate physical providers, supported by bounded history, binary reading and SDK foundations.

Select option 3. Keep ordinary record/history functions direct; avoid placeholder opcodes and packages for trivial classes.

For history, compare event-log replay, checkpoint DAG and full branch-aware storage. Select a checkpoint DAG: inspectable reconstruction/conflicts without pretending to define replay semantics or transactional branches. Full-checkpoint costs remain explicit.

For storage, separate transactional workspace publication from immutable binary reading. Scientific files and object readers need not claim database guarantees.

## Decision

- Cohesive C++20 support, operand/operation, record/history, storage and execution libraries; a synchronous C/Python composition boundary.
- Independent immutable type/operation descriptors and provider registration. Operations pin every signature type's description; providers pin operation fingerprints. Unsupported effects, domains or determinism fail without fallback.
- Pure exact host-value execution initially, multiple arguments/results/providers, explicit selection, stable ordering, cooperative cancellation and joined workers. No callback sandbox claim.
- Record profiles separate from operand types, with explicit event-anchor/precedence capabilities passed consistently through projections.
- Pure attributed checkpoint DAG, reconstruction/diff and explicit-base three-way merge. Recording ancestry remains distinct from represented event order.
- Linear physical publication, typed capabilities/requirements, receipts and independent verified range readers. Preserve SQLite schema 1 and disclose receipt-attribution integrity limits.
- Preserve canonical/record v1 bytes; add strict duplicate-key parsing and golden vectors. Stricter new identity rules do not rewrite retained opaque wrappers.
- Relocatable native installation and separately packaged Python SDK, verified by external installed consumers.
- Known vectors, cross-provider/backend conformance, seeded independent models, failure tests and measured shared workloads.

## Semantic discipline and consequences

Current drafts and R8 proposals are informative, not adopted Standards. The assessment consults FM-007 on overlapping categories and unequal histories with equal endpoints, and FM-005 on qualified continuity and revisable interpretation. Checkpoint history is an operational software contract, not a resolution of GR historical identity.

Unknown required semantics protect source/candidate admission, including explicit merge bases. Projections declare scope/losses. Staged edits identify their base without claiming its retained checkpoint identity.

Canonical record bytes and SQLite schema/request/revision identities remain v1. C ABI 1 is extended additively; Python byte helpers map to fixed providers. The v0.1 C++ execution API is deliberately replaced; rebuild consumers. No legacy bytes, signatures, applications or data are migrated.

Descriptors cannot prove code correctness. Whole-checkpoint history scales poorly; verified opening reads the whole object; uploads/computation transport still copy bounded bytes. These are visible limits and follow-on priorities, not a completed heterogeneous runtime.

Affected: reorganized headers/sources, C/Python SDK, build/install configuration, tests/examples/benchmarks, README, contributor guidance, change record and [decision agenda](../docs/planning/open_questions.md). Parent publications, legacy repositories and original decisions are unchanged.
