# Layers 5 and 6: architecture and review guide

**Class:** Experimental implementation specification  
**Version:** 0.4 foundation boundary; experimental, without standards adoption

The foundation is composed by [Layer 4](runtime.md). v0.4 adds owned typed buffers,
numerical declarations, compact physical history and transaction admission reuse.
Canonical records, semantic-history identities and physical database schema remain v1.

## Responsibility and dependency direction

| Target | Depends on | Owns |
|---|---|---|
| GR::common | Pinned JSON/hash primitives | Bounded canonical metadata and structured errors |
| GR::buffer | common | Owned host allocations, budgets, checked views and named buffer sets |
| GR::semantics | common, buffer | Operand types, pinned semantic/numerical descriptions, validation |
| GR::core | semantics | Record admission, qualified projections, recording-history DAG |
| GR::storage | common; private SQLite | Physical integrity, publication, retry, binary-reader lifetime |
| GR::execution | semantics | Provider compatibility, CPU invocation and guarantees |
| GR::runtime | core, storage, execution | Catalog/planning, scheduling/resources and semantic transactions |
| GR::native | runtime | C transport and process-local resource ownership |

Python uses the C ABI. Public libraries never depend on the binding. Storage has no semantic dependency; execution consumes operation semantics without depending on record projections or storage.

The native Layer 4 transaction coordinator validates a complete candidate, derives its artifact inventory and requests atomic publication. Raw native storage intentionally accepts opaque records: physical integrity cannot establish semantic admission. C/Python/CLI delegate to the coordinator; Layer 4 separately owns jobs and execution policy.

## Independent extensions

Operand types, semantic operations and physical implementations have separate registries. Operations pin the descriptions of all signature types. Providers pin an exact operation fingerprint and supported backend/device/memory domains. Dispatch checks those choices before callbacks. Matching descriptions are a claim that requires conformance evidence, not a proof of machine-code behavior.

Direct Layer 6 execution uses synchronous host values, pure exact operations, multiple arguments/results and explicit provider selection. Layer 4 adds compatible provider planning and asynchronous jobs over prepared physical invocation. Unsupported effects, guarantees and device domains fail without fallback. Owned typed host buffers and optional numerical declarations now separate layout, accuracy, repeatability and reduction order. Device buffers, completion and approximate acceptance still need concrete contracts. A future GPU adapter need not pretend device memory is a host byte vector.

Record profiles remain distinct from operand types: archival interpretation and computational representation have different responsibilities. Profiles can explicitly declare event-anchor and precedence capabilities understood by projections. There is no general subtype inference or universal schema language. See [execution contracts](execution-contracts.md) and the [extension example](../examples/cpp/extension.cpp).

## Different histories

The semantic DAG contains explicitly attributed, content-addressed checkpoints. It supports reconstruction, differences and explicit-base three-way record-version merges. Equal endpoints do not imply equal recording histories. Merge choices do not certify truth.

The physical ledger is linear: it publishes workspace state under an expected-head precondition and returns a receipt. It does not implement physical branches or merges. The runtime atomically publishes semantic history objects, a selected-head manifest, user records, artifacts and receipt. Compact manifests coexist with legacy bundles; see [history storage](history-storage.md).

Represented event precedence is separate again: it comes from selected qualified order records, never from ancestry or commit sequence. [Record/history contracts](kernel-history.md) define these distinctions; the compact physical representation preserves their v1 semantic identities.

## Physical capabilities

IStore promises transactional workspace history. BlobReader independently promises pinned verified immutable reading. An HDF5, Zarr, Parquet or object adapter can provide reading without claiming CAS/atomic publication. Future transactional backends must satisfy the full publication contract and declare actual guarantees.

Memory and SQLite implement the shared contract with different durability. Opening a reader verifies the entire object's SHA-256 in bounded chunks; ranges then refer to that pinned view. Opening costs O(object size). See [storage contracts](storage-contracts.md).

## Compatibility disposition

| Surface | Decision |
|---|---|
| Canonical v1 and retained record v1 bytes/hashes | Preserved; golden vectors and strict duplicate-key parsing added |
| New type/extension identities | Qualified namespace/version grammar enforced |
| Older permissive identities in retained wrappers | Original v1 envelope remains readable and opaque; hashes unchanged |
| SQLite schema 1 and request/revision identities | Preserved; integrity checks do not migrate files |
| C ABI | Version 1, additive operations and binary read function |
| Python identity/SHA helpers | Preserved as fixed contract/provider selections |
| C++ execution API | Deliberately replaced by independent registries and typed invocation; consumers rebuild |
| C++ storage implementers | New typed descriptions, receipt/metadata/open-reader methods; blob is a convenience wrapper; implementers update and rebuild |
| Staged projections | Changed candidates report no current published revision; base identity remains explicit |
| Legacy GSP/GRRP | No migration or wire/signature/API equivalence claim |

The [v0.1 guide](reviews/layers-5-6-v0.1.md) remains a historical snapshot. [ADR-0001](../decisions/ADR-0001-layered-development-and-layers-5-6.md) records the initial slice; [ADR-0002](../decisions/ADR-0002-independent-contracts-and-foundation-reconstruction.md) identifies superseded implementation choices.

## Deliberate limits

Legacy history functions validate bounded whole bundles. Compact selected reconstruction is bounded by a checkpoint/delta chain, while arbitrary catalogue lookup remains linear and full audit deliberately reads the DAG. Uploads remain bounded copies; owned host views and optional Python read-only export avoid selected computation copies. Native callbacks are trusted; cooperative deadlines and output caps cannot bound arbitrary native effects or allocation.

Layer 4 planning/scheduling is implemented, with an isolated experimental Layer 3 consumer. A full language, VM, dynamic plugin loader, authorization service, distributed transaction, retention policy and Layers 1–2 remain outside this increment. [Verification](verification.md) separates tested behavior and measurements from unqualified platforms and workloads. The [decision agenda](planning/open_questions.md) records follow-on discussion.
