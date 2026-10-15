# ADR-0001: Layered development and the first Layers 5–6 implementation

**Class:** Implementation architecture decision; no Standard adoption  
**Date:** 2026-10-13 JST  
**Status:** Layered development and Layers 5–6 scope selected by the user; detailed v0.1 contracts implemented for user review

## Issue and disposition

**v0.2 disposition:** The layered direction remains selected. [ADR-0002](ADR-0002-independent-contracts-and-foundation-reconstruction.md) supersedes the conflated execution registry, hardcoded temporal profile recognition, in-tree-only SDK and execution dependency statement below. Layer 6E now consumes Layer 5 operand/operation contracts; Layer 6S remains independent of semantic code. This document otherwise preserves the original v0.1 reasoning.

The initial review proposed reducing the number of mandatory subsystems. The user prefers the explicit R8 layered architecture because its responsibilities and development sequence are clearer. The user subsequently authorized implementation of Layers 5 and 6, including actual storage/device interaction, followed by a stop for review. This supersedes the assessment-only stage and the earlier suggestion to replace the layer organization. It does not authorize applications, a runtime, data migration, or changes to the existing infrastructure.

The architecture keeps these logical layers:

| Layer | Responsibility | This stage |
|---|---|---|
| 1 | Applications, including academic applications | Deferred |
| 2 | Reusable domain tools and modules | Deferred |
| 3 | Optional data/computation language framework | Deferred |
| 4 | System coordination, policy, jobs, external effects | Deferred |
| 5 | Record/value contracts, validation, pure admission, projections | Implemented for review |
| 6S | Storage, physical integrity, atomic publication | Implemented for review |
| 6E | Device capabilities and physical execution | Implemented for review |

Layer numbers identify responsibility. They do not mandate a separate process or a runtime dispatch hop for every call. L6S and L6E are peers; storage does not execute analytical callbacks. A local binding adapter composes L5 validation with L6 storage, and exposes L6 execution directly. L5 does not contain a database driver, and L6 does not depend on the semantic kernel. A shared primitive library supplies bounded JSON, hashes and errors.

## Alternatives considered

- **Python foundation with later native optimization:** easiest initial iteration, but leaves a later semantic rewrite or pervasive native boundary. The user favors C++ for lower layers.
- **Rust foundation:** a reasonable ownership-safe alternative from the review; not selected for this implementation language direction.
- **One monolithic native library:** fewer targets initially, but obscures independently testable storage/execution contracts and their dependency direction.
- **A framework/compiler/runtime before the kernel:** delays direct testing and adds an unnecessary prerequisite. Layers 5–6 remain usable as libraries.
- **External object files plus a SQL catalogue immediately:** scales large artifacts independently, but introduces publication, orphan recovery and backup closure protocols. This version stores bounded binary artifacts transactionally in SQLite itself.
- **pybind11 initially:** ergonomic typed bindings, but adds Python build/packaging dependencies. A versioned, exception-safe C ABI and Python `ctypes` facade are sufficient for bounded metadata and initial binary batches. Native C++ APIs avoid JSON transport for large data. Future array interfaces need a separately specified lifetime/ownership contract.

## Selected v0.1 contracts

1. **One C++20 implementation.** Python accesses native behavior; it does not duplicate admission or temporal semantics. TypeScript remains a future application/client choice.
2. **Explicit, versioned record envelopes.** Content versions hash the canonical document. Record identity, represented subject references, type identity, actor attribution, content digest, storage revision and physical path are separate. Six familiar record roles may overlap; they are not ultimate classes of reality.
3. **Bounded canonical metadata.** `gr.canonical-json/1` uses UTF-8 JSON, sorted object keys and exact safe integers; floating metadata is rejected. Scientific numbers can use explicitly typed strings or binary artifacts. This is a new local contract, not a claim of RFC 8785, GSP or GRRP compatibility. No legacy bytes/signatures are translated.
4. **Pure whole-candidate validation.** A batch can contain forward references. An invalid batch produces no candidate. Mutation requires registered interpretation of types/extensions explicitly marked required. Unknown optional content remains intact, with incomplete interpretation reported.
5. **Scoped temporal projections.** Explicitly selected order claims create a strict partial order. Cycles remain recordable evidence but block a coherent cut. Cuts must contain known predecessors; start is inclusive, end exclusive. Unknown bounds remain indeterminate. Unselected orders are disclosed. Recording order never creates occurrence order.
6. **Local atomic storage.** One memory adapter and one SQLite adapter implement the same snapshot, immutable-version, blob, compare-and-swap and transaction-retry contract. SQLite publishes versions, artifact bytes, snapshot membership and receipt in a single transaction. Transaction identity covers actor and the exact request digest; a successful retry is recognized before checking a stale base.
7. **Explicit bounded CPU execution.** Callers select sequential or threaded execution. Versioned trusted operations declare whether concurrent invocation is allowed. Input/output byte limits, stable batch ordering, stop tokens and cooperative deadlines are explicit. Discovery reports a host CPU only. No GPU, distributed scheduler or sandbox is claimed.
8. **Scoped trust.** These are trusted local libraries. Supplied attribution is not authentication. Hash checking is not a signature, truth, legal authority or authorization decision. Native callbacks can allocate memory and have effects; the API cannot forcibly undo or preempt them.

## Semantic sources and deliberate narrowing

The source-grounded assessment and [review evidence](../REVIEW_EVIDENCE.md) distinguish the deep metamodel from operational record classes, preserve competing histories and relation roles, and identify canonicalization defects in the legacy implementations. This decision operationalizes a small software contract. It does not redefine GR theory or promote any draft Specification to a Standard.

An event anchor in this first temporal profile must have the explicit `gr.event@1` type. Other record roles can overlap, but automatically recognizing an arbitrary extension as an event requires a future declared capability contract. Typed values, validators and computational operations are independently extensible through trusted registration; no universal plugin loader is introduced.

## Consequences and remaining work

Layer separation is testable through build dependencies and common backend fixtures. The first temporal and record profiles are deliberately narrower than the complete theoretical corpus. SQLite stores complete snapshot membership per commit; efficient delta/checkpoint histories and large external objects remain workload-driven extensions. The C ABI uses JSON and encoded bytes, not zero-copy arrays. Deadlines and output caps cannot sandbox trusted native code.

No compatibility migration, external service, credential store, signature/custody implementation, permission model, retention/erasure policy, production durability qualification, distributed commit, device accelerator or scientific algorithm package is included. These are recorded boundaries, not implemented stubs reported as working features. Follow-on layer work waits for user review.

Affected files: local `AGENTS.md`, `README.md`, `CHANGELOG.md`, `docs/planning/open_questions.md`, `docs/layers-5-6.md`, native headers/sources/tests, build/dependency manifest, Python adapter and example. The original assessment remains historical discussion with a disposition note.
