# Typed planning, catalogs and prepared execution

**Class:** Experimental implementation contract  
**Version:** GR-OS 0.5; not an adopted programme Standard

## Boundary

Layer 4 accepts an explicitly typed acyclic dataflow graph. Each node names a semantic operation, ordered literal/result operands, optional control predecessors, implementation preferences and payload limits. Graph outputs explicitly select result slots. Compilation verifies the complete graph: unused nodes are still checked and executed. Pure operations can fail observably, so purity alone does not justify dead-code elimination, fusion or common-subexpression replacement.

The planner checks node identities, arity, exact value types, available result slots, control/data edges, cycles, supported effects and compatible physical targets. It compiles only pure operations with exact determinism. Snapshot acquisition, mutation and publication belong to the transaction coordinator; a native callback cannot obtain those permissions by declaring an effect.

`PreparedPlan` has no mutable execution state. It retains immutable values, selected executors and all native validation/provider/backend lifetimes. A runtime supplies per-job results and invokes `assemble_inputs` and `execute_node`. Dependencies index nodes in declaration order; they do not imply represented occurrence order. Concurrent completion/error/trace ordering is distinct from exact output determinism.

## Catalog and selection

`Catalog` separately registers value types, semantic operations, physical implementations, execution backends and static module manifests. Registration occurs before `seal()`. Sealing checks exact module dependencies and cycles, and verifies the operation/type bindings. No mutable registry escapes. Module descriptors describe trusted statically linked code; they neither dynamically load libraries nor grant authority. A callback is not isolated from the process.

Default catalogs include CPU backends/providers and `gr.record.verify@1`, whose provider calls the actual Layer 5 record verifier. That operation accepts the bounded `gr.json@1` domain and returns `gr.record-verification@1`. It preserves the verifier's invalid/opaque reports and its explicit limits on truth, signatures and authority. It does not silently expand the canonical JSON limit or claim to cover larger snapshot/history APIs.

Selection first applies explicit provider/backend/device constraints, exact semantic/type fingerprints, declared determinism and compatible device/memory/binding capabilities. Caller preferences rank compatible providers; remaining ties use stable lexical provider/backend/device identity. No execution failure triggers an implicit provider retry. This is reproducible capability-based selection, not an empirical cost optimizer. Capabilities and selected metadata expose the actual choice.

Backends supply typed targets and a virtual prepared-execution hook. The planner contains no CPU dispatch branch. Current host preparations execute one invocation on the calling thread and declare one CPU slot, including preparations obtained from the threaded batch backend. Layer 4 owns its fixed worker pool and cooperative stop/deadline control. Standalone Layer 6 batch execution retains its own sequential/threaded behavior. There is no nested thread pool per runtime node.

## Preparation and ownership

Layer 5 prepared value/operation handles pin descriptions and native validators once. Dispatch still validates operand representations, declarative constraints and installed pre/postconditions. It avoids repeated registry locks and descriptor hashing for every batch item or runtime node execution.

The native `Value` payload is privately held through immutable shared ownership. Existing `Value::json`, `Value::bytes`, `as_json`, `as_bytes` and exact equality remain available. Raw public `Value::data` and aggregate initialization were replaced in v0.3. Copies of a value share payload storage; changing a copied type tag does not modify another value. Graph constants and producer results therefore remain valid without copying an entire binary/JSON payload on every edge. Borrowed references must still not be retained by native callbacks beyond their call.

v0.4 adds `Value::buffers` and `as_buffers`: a named collection of owned, checked host buffer views. It preserves shape, signed byte strides, physical encoding, byte order and immutable lifetime without defining a universal mathematical tensor. The host binding remains synchronous `gr.host-values/1`; device memory and transfer synchronization remain unsupported. The optional Python exporter implements its own retained read-only lease. See [buffer and numerical contracts](buffers.md).

JSON/octet payload limits retain their old meaning. Buffer values charge the greater of logical element bytes and distinct retained owner bytes within the value, plus layout/metadata. A tiny slice therefore cannot hide a large retained allocation, nor can broadcast strides hide logical volume. Independent `AllocationBudget` leases track owned native payload until the last alias dies. Provider allocations outside that allocator, container overhead and external library threads remain outside these guarantees.

## Identity, caching and bounds

Logical graph identity hashes bounded node descriptions containing literal content hashes, explicit control dependencies and resource/selection requests, followed by ordered output references. Physical plan identity also includes exact operation descriptions and selected execution descriptions. Buffer literal identity includes layout and owned content, never process addresses. Existing JSON/octet literal hashes remain unchanged. It is separate from record/content/recording-history identity. Plan hashes identify descriptions, not machine-code correctness or authorization.

v0.5 adds optional `Graph.context`, a canonical bounded metadata object included in graph/cache/plan identity and exposed in the prepared description only when nonempty. Empty context preserves earlier identities and descriptions exactly. Layer 3 uses this to bind its recipe identity: equal computational bytes accompanied by different provenance must not permit a result from one recipe to be attributed to the other. Context adds no execution node or scheduler work. It is neither authenticated source evidence nor an operation permission grant. Its serialized size counts against the existing combined plan budget.

The planner has configurable node, edge, literal-payload and combined plan-payload limits. It does not pass an arbitrarily large complete graph through the one-MiB metadata canonicalizer: it hashes bounded components compositionally. The initial cache holds at most 16 plans and 64 MiB of accounted literal/serialized-plan payload, with FIFO eviction. These are logical payload bounds rather than exact allocator usage. A live plan survives eviction and catalog destruction. Cache lookup belongs to one immutable sealed catalog and never imports executable plans from arbitrary serialized metadata.

Compilation still validates and hashes supplied constants when looking up a cached graph. Cache reuse saves physical preparation and graph analysis; it is not a promise of constant-time compilation for large inputs. Runtime users should retain an existing prepared plan for repeated execution.

## Compiler evolution

The present compiler performs validation and native physical binding, with no speculative algebraic transformations. R8 documents 19, 21, 25 and 31 support this separation of semantic graph, optional lowering and physical plan; they do not mandate bytecode or a particular instruction list.

MLIR provides extensible multi-level representation and lowering mechanisms. A later compiler module could lower suitable computational subgraphs through its dialect/conversion infrastructure when concrete operators and rewrite laws justify that dependency. LLVM ORC supplies JIT infrastructure; it does not supply GR authority, historical semantics or transaction guarantees. Those runtime boundaries must survive any later lowering. See the [MLIR rationale](https://mlir.llvm.org/docs/Rationale/Rationale/), [dialect conversion framework](https://mlir.llvm.org/docs/DialectConversion/) and [LLVM ORC design](https://llvm.org/docs/ORCv2.html).

Future compiler passes need explicit applicability, equivalence, effect-ordering and resource contracts, with independent conformance. No arbitrary graph-rewrite callback is currently treated as a certified optimizer. v0.4 numerical descriptors can pin overflow, precision, rounding, reduction order and reference tolerances; they do not authorize approximate execution or reassociation. Device transfers, numerical acceptance policies, heterogeneous/distributed placements and dynamic module loading remain separate extensions.

## Verification

`tests/unit/planner.cpp` checks typed data/control composition, graph rejection, exact selection, module dependencies, resource bounds, cache eviction, shared payload ownership, retained plans and actual record-verification parity. `tests/conformance/prepared.cpp` compares direct/prepared execution against an independent SHA-256 vector, verifies lifetime and value/cancellation/byte-limit checks. The existing execution/conformance suites continue to test lying providers, incompatible contracts, worker joining and direct batch behavior. Scheduler and transactional failure/recovery evidence is recorded separately; these planner tests do not establish durability or universal semantic equivalence.
