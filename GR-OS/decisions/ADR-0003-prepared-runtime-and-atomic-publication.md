# ADR-0003: prepared systems runtime and atomic semantic publication

**Class:** Implementation architecture decision  
**Status:** Implemented for user review within the authorized Layer 4 increment  
**Date:** 2026-10-13 JST  
**Scope:** GR-OS Layers 4, 5, 6S and 6E

## Issue and authority

The user authorized a comprehensive systems runtime design and substantial
integrated implementation after the v0.2 foundation. Layer 4 needs native owners
for planning, asynchronous execution, resources and semantic transactions. Existing
descriptor validation repeats per call; record admission is binding composition;
physical history and semantic checkpoint bundles are not atomically integrated.
The [assessment](../docs/reviews/layer-4-assessment-and-plan.md) preceded substantive
implementation. This decision refines ADR-0002 and supersedes its no-Layer-4 scope;
its semantic/physical distinctions and preserved v1 identities remain in force.

## Alternatives

1. A thin dispatcher retains binding-owned transaction semantics and provides no
   coherent job/resource/compiler lifecycle. Insufficient for the authorized scope.
2. A custom bytecode VM, generalized scheduler, distributed transaction layer and
   plugin ABI introduce obligations without current language/device workloads.
3. A lightweight typed DAG, immutable prepared providers, bounded native scheduler
   and transaction coordinator supply concrete extension boundaries now.

Select option 3. MLIR/LLVM remains a future compiler-provider option; neither is a
mandatory dependency. Fixed workers and mutex-protected ready queues are preferred
to custom lock-free work stealing until contention evidence justifies complexity.
Native SQLite transactions are preferred to a new logging/recovery engine.

For history, select complete verified bundles with an atomic technical manifest
over existing storage schema 1. Full branch-head database redesign or incremental
history storage would require independent retention, merge and concurrency work.
For modules, select explicit trusted registration and exact dependencies with
sealed catalogs. Dynamic discovery/unload remains deferred pending a real ABI and
trust model. For cross-store operations, design manifest/outbox reconciliation but
do not implement or imply distributed atomicity.

## Selected invariants

- L5 defines semantic types, operations, records and history. Providers pin exact
  operation descriptions; multiple physical providers remain independently extensible.
- L6E prepares a physical invocation and declares its target/capacity. L4 validates
  the complete graph, chooses compatible implementations and owns job scheduling.
- Catalogs seal before planning. Plans pin trusted callback/backend lifetime;
  registration does not grant authority or sandbox code.
- Graph execution currently permits pure exact host-value operations. Publication
  is an explicit barrier, outside freely reorderable computation.
- Admission bounds live jobs and retained logical payload; cancellation is
  cooperative, siblings are joined, shutdown joins all native workers.
- The native coordinator pins a workspace base, stages bounded changes, validates
  the complete candidate/artifact closure and publishes via one L6 atomic commit.
- Isolation is whole-workspace optimistic CAS. No silent rebase, weaker guarantee,
  arbitrary provider retry, nested transaction or cross-store ACID is introduced.
- Optional semantic-history bundles, manifests, records, blobs and receipt share
  one atomic unit. Semantic ancestry and physical publication order stay distinct.
- Unknown outcome is explicit. Receipt recovery and exact frozen retry do not
  re-execute the graph; acknowledged commit knowledge survives late cancellation.
- C++, C/Python and native CLI share these native semantics. ABI 1 is extended
  additively; this remains experimental software without a stable C++ binary ABI.

## Compatibility and limits

Canonical/record/history v1 formats, SQLite schema 1 and legacy direct convenience
entry points remain. `Storage.commit` now delegates to the native coordinator.
The C++ `Value` source API intentionally changes to immutable shared payloads and
accessors: direct mutable field access must migrate; no stable C++ layout was
promised. Public C ABI and Python typed values retain their transport meaning.

Trusted modules, CPU host values and local memory/SQLite adapters are implemented.
Device memory, numerical equivalence, dynamic plugins, compiler rewrites, distributed
scheduling, durable jobs, access-control services and legacy migration remain
unimplemented. Exact descriptor pins do not authenticate code. Memory durability
is volatile; SQLite process-crash behavior does not establish power-loss certification.

No GR conceptual term is redefined. The technical history manifest is an operational
runtime profile, not an additional ultimate/record ontology class. Programme
publication authority and semantic truth are not inferred from passing software tests.

## Affected contracts and validation

Affected: L5 prepared type/operation/value APIs, L6E prepared backend interface,
L4 catalog/planner/scheduler/transaction library, native C/Python bindings, CLI,
CMake/package exports, installed consumers, examples, contributor guidance and
planning agenda. L6S additionally exposes its existing canonical request digest
for exact recovery comparison; no new database journal is introduced.

Validation includes existing conformance regressions, provider/plan lifetime and
whole-graph rejection, concurrent scheduling and cancellation, coordinator conflicts
and uncertain acknowledgments, genuine child-process kill/restart tests, installed
consumers, native dispatch/scaling benchmarks and concurrent SQLite workloads.
Actual evidence and limits are in [verification](../docs/verification.md). Stop
after reporting readiness for review; no Layers 1–3 implementation is authorized
by this decision.
