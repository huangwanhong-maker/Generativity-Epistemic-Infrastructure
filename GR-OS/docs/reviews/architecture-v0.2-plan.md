# Assessment and plan for the second architectural increment

**Class:** Informative engineering assessment and implementation plan  
**Date:** 2026-10-13 JST  
**Status:** Assessment completed; next increment authorized by the user's follow-up request

## Assessment of v0.1

The first implementation is a working foundation, not a mature realization of R8. Its useful invariants should survive reconstruction: pure whole-candidate validation, distinct record/content/revision identities, optional-content preservation, required-interpretation checks, attributable competing temporal claims, n-ary incidences, atomic SQLite record/blob/receipt publication, CAS and retry, and CPU cancellation/worker joining. These have executable failure tests rather than only design descriptions.

The largest architectural gap is that `execution::OperationDefinition` combines semantic operation and physical callback, and `OperationRegistry` permits only one implementation per operation/version. There is no independently identified typed signature, effect/determinism contract, compatibility negotiation or semantic conformance record. This directly misses the central L5–6 boundary described by R8 documents 21, 22, 28 and 31.

Other observed limitations:

- `core.cpp`, `storage.cpp`, `execution.cpp` and `c_api.cpp` group too many distinct responsibilities. The build separates libraries, but public headers and source organization do not yet explain the same boundaries.
- Record validation uses anonymous callbacks, loosely checked type identifiers and a closed event/order type check. Projection calls create their own default registry, so caller-defined semantics cannot participate consistently. Equal retained states have no independent recording-DAG model; the only durable commit history is a physical linear ledger.
- The semantic envelope preserves provenance text but does not establish authentication, evidence warrant or factual truth. A richer generic historical structure must retain these qualifications without baking academic or institutional classes into the kernel.
- Canonical encoding is bounded and rejects ambiguous numeric values, which is appropriate for metadata, but needs stable golden vectors and explicit schema-evolution rules. It must not be silently replaced with another encoding or mistaken for a scientific array representation.
- Storage `IStore` is a transactional workspace history contract, not a universal storage abstraction. HDF5/Parquet/object readers should not inherit a false CAS/transaction promise. Binary reads currently materialize whole objects; effective limits, capabilities and ownership need typed contracts.
- SQLite reads allocate text before enforcing its configured size. Receipt identity fields are not rechecked against the revision digest on reads. These are concrete integrity/resource issues, separate from future backend work.
- C ABI archive hydration incorrectly counts current records plus replacement inputs against the current-record limit. A legal full snapshot can therefore reject a replacement. Artifact descriptors also trigger redundant full blob reads.
- Public distribution consists of an in-tree CMake build and `PYTHONPATH`. There is no installed consumer test, relocatable CMake package or Python packaging metadata. Python tests can disappear when an interpreter is missing.
- Existing deterministic example tests are useful but do not establish cross-provider semantic equivalence, randomized history conformance, streaming lifetime guarantees, packaging correctness or measured performance.

## Source basis and authority

Review covers the current workspace's public interfaces, implementations, build, SDK, examples and tests, with parallel component audits. The original assessment/evidence remain relevant to the unchanged legacy repositories. The R8 handoff is preliminary: README/AGENTS, read-first, boundary sketches, acceptance scenarios, registries and decision register guide this increment, not a required opcode list.

The current Programme Brief and parent contributor rules remain applicable. GR-120 WD 0.1, GR-SPEC-120/130 experimental 0.2 and GR-SPEC-121 experimental 0.1 are reference proposals, not adopted Standards. Their distinctions among record identity, target identity, occurrence order, recording ancestry and competing accounts inform the tests. FM-007 (PDF pp. 2, 15, 79) preserves overlapping categories, distinct histories despite equal endpoints and declared-regime limits. FM-005 (PDF pp. 114, 168–169) grounds continuity qualifications and revisable historical interpretation. No universal ontology, epistemic truth test or new standards authority follows from this software increment.

## Selected increment

Implement **explicit semantic contracts and replaceable physical providers**, alongside the historical, storage and distribution foundations needed to test that boundary.

1. Reorganize into cohesive support, kernel, storage, execution and binding components. Split interfaces from concrete adapters; retain small compatibility umbrella headers. Avoid a package or translation unit per trivial class.
2. Add immutable, versioned value-type and semantic-operation descriptors, independently registered from physical implementations. Specify signatures, constraints, effects, exactness and failure behavior. Use explicit exact compatibility initially; no implicit coercion, subtype inference or scheduler.
3. Replace the conflated execution registry with a physical implementation registry. Resolve explicit implementation/backend choices, validate their declared contract, and expose capabilities. Demonstrate multiple implementations of the same meaning with independently specified conformance cases, including a deliberately incorrect provider. Retain the proven CPU resource/failure machinery.
4. Refine record extension/projection contracts and add a pure, content-addressed recording-history DAG with explicit parentage, reconstruction, differences and conflict-reporting merge support. Preserve represented-time projections as separate semantics. The current physical store remains a clearly advertised linear publication ledger; semantic DAG objects do not secretly turn it into distributed branch storage.
5. Separate transactional storage from immutable binary reading. Implement typed capabilities/requirements and bounded verified range readers in memory and SQLite, independent reader lifetime, receipt lookup and identified integrity fixes. Retain bounded atomic uploads; defer multipart/external-object custody until its publication/recovery contract is designed.
6. Provide the same usable contracts through the public C/Python boundary, including binary reads without JSON/base64 copies. Add relocatable native installation and Python SDK packaging, tested from outside the source tree.
7. Add unit, integration, seeded property/conformance and failure tests. Add reproducible, bounded benchmarks with machine/build/workload metadata and a comparison tool; do not turn noisy host timings into unsupported universal performance claims.

## Alternatives and deliberate limits

Keeping the one-provider registry would be simpler but fails the requested independent extension boundary. A compiler, bytecode VM, planner, general effect system, remote scheduler or universal device buffer would be premature. Exact descriptor compatibility plus explicit selection is a smaller useful contract. New host execution supports pure operations with exact guarantees; declarations of other effects/guarantees must be rejected when unsupported.

An entirely new wire format would force gratuitous data churn. Preserve existing v0.1 record bytes/versions where valid; document any tightened interpretation and additive new history/descriptor formats. C++ source APIs may change deliberately at v0.2. Keep existing Python byte convenience operations as documented fixed selections of new providers, not a second semantic implementation.

Transactional history and binary/range access are separate capabilities. Additional SQL stores, Git, HDF5, Zarr, Parquet and object stores can implement the contracts they actually support. This increment does not implement that backend matrix, GPU/distributed execution, authentication, retention/erasure, legacy migration or Layers 1–4.

## Acceptance and execution order

Record this assessment before edits. Then implement contracts and module boundaries, migrate adapters/SDK, and run the complete affected build/tests. Verify independent extension without core edits, provider mismatch rejection before callbacks, exact cross-provider results, nonlinear semantic-history reconstruction/conflicts, storage conformance and pinned-reader lifetime. Run installed native/Python consumers and synthetic examples. Capture benchmark evidence, public API/ownership/error documentation, compatibility disposition and remaining priorities. Stop after reporting this increment; later runtime/application development remains separate work.
