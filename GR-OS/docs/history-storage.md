# Compact semantic-history storage

Class: implementation contract and engineering guidance. Status: experimental v0.4 implementation; not an adopted Standard. The semantic model remains [`gr.history/1`](layers-5-6.md). This document describes its separate physical representation and runtime publication.

## Identity and preservation

A semantic node retains its exact v1 descriptor: format, ordered semantic parents, state digest, and attribution. Its semantic revision remains the SHA-256 digest of that descriptor under the existing metadata canonicalization contract. A physical node reference carries both this semantic revision and the digest of a storage object. Changing physical checkpoint policy does not change semantic identity.

The reconstructed state preserves both normalized inventories: current records sorted by identity, and archived versions sorted by identity/version with current duplicates removed. Deltas explicitly represent current upserts/removals and archived additions/removals. They do not silently inherit archival entries that a v1 checkpoint did not contain. No ordering of represented events, truth, endorsement, or institutional authority follows from recording ancestry or successful integrity verification.

Existing v1 manifests (`gr.runtime.history-manifest@1`) remain readable. Ordinary append to an existing v1 history retains the original bundle encoding and manifest construction. Explicit v1 imports also retain that encoding unless the caller requests upgrade or is extending an already compact history. This compatibility boundary preserves the inputs needed to reproduce existing physical transaction requests. A frozen request is retried verbatim, independently of later representation defaults.

New automatically tracked histories use `gr.runtime.history-manifest@2`. An explicit, bounded upgrade preserves every semantic node and the original legacy artifact bytes. It publishes all new objects and the replacement manifest in one ordinary workspace commit. There is no destructive migration, garbage collection, or partial upgrade.

## Object layout

| Object | Fields and role |
|---|---|
| `gr.history-object-node/1` | Exact v1 `version` and `document`, ordered physical `parents`, `state` payload digest, `delta_depth` |
| `gr.history-checkpoint/1` | Complete normalized `snapshot` |
| `gr.history-delta/1` | Physical `base` reference, `records_upsert`, `records_remove`, `archive_add`, `archive_remove` |
| `gr.history-catalogue/1` | Prior catalogue digest or null, `added` node references, selected `head`, cumulative `node_count` |
| Manifest `@2` | Reserved record `gr:runtime:history`; payload pins catalogue digest, selected head reference and node count; artifact descriptor supplies catalogue byte length |

Objects use strict UTF-8 JSON serialization and SHA-256 content addressing. Complete containers can exceed the 1 MiB metadata-value limit; their record documents and semantic descriptors still obey that contract. Object parsing separately rejects duplicate keys and excessive nesting/value counts.

Compression ancestry and semantic ancestry are separate. A delta base is a physical reconstruction choice; it does not add a semantic parent. The encoder uses a checkpoint when a delta would be no smaller, after the checkpoint interval (default 16), or before exceeding the configured delta-depth limit (default 32). The reader independently enforces a maximum delta depth of 64 and verifies the declared depth at each edge.

An ordinary append prepares at most one state payload, one node header and one catalogue link. Identical byte objects are deduplicated within the operation. Catalogue links retain branches without rewriting a full node index. An ordinary current-head read proves membership in the newest catalogue page and follows its direct node reference. A head selected from an older page, or an arbitrary older semantic revision, requires a catalogue walk. A materialized index is deferred.

## Admission, custody and atomicity

`HistoryStore` is a preparation/read helper, not a database transaction manager. Its append/import/upgrade methods return a manifest, bounded immutable blobs, required digests, selected snapshot and declared source-artifact lengths. They perform no writes. The runtime coordinator admits the candidate, verifies artifact lengths against staged or existing workspace bytes, and combines these results with records and the durable receipt in one Layer 6S commit.

Every newly introduced source-artifact digest is included in required custody. Imports inspect artifacts across every supplied node, including unselected branches and archived wrappers. The physical store inherits the prior revision's retained blob inventory, so older branch objects remain in workspace custody without rereading every inactive branch on every append. The store verifies required blob content during publication. No history reference grants access to another workspace's objects.

Appending checks the selected parent's mutation requirements and the candidate's complete dependency constraints. Imported required profiles may remain readable without their implementation; mutation of a selected state requiring an unavailable profile remains rejected. Compact import retains every existing semantic node unchanged; exporting and the existing v1 merge operation remain the complete-DAG semantic oracle. Merge-base rules and explicit conflict resolution are unchanged.

## Read verification and its scope

Each operation owns its reader and caches. Every fetched object is hashed; node headers also verify the v1 descriptor hash, parent identities and ordering, and payload reference. Reconstruction verifies complete record wrappers, dependencies, normalized inventories and the v1 state digest. A transaction-local `ValidationSession` reuses exact immutable wrapper checks along a reconstruction chain while retaining candidate-wide dependency checks. There is no process-global cache that could hide altered storage bytes from a later operation.

Selected reads validate the selected reconstruction path. They do not audit every inactive branch or prove the availability of every semantic ancestor of a checkpoint. `export_v1` explicitly walks the complete catalogue, reconstructs every retained node, and applies the existing v1 complete-DAG validation, including parent closure. Missing or corrupt inactive history can therefore remain undiscovered by a selected read and be detected by export/audit. This selective verification scope is deliberate and is distinct from the complete custody established when a valid history is admitted.

`CompactReader` and the preparation helpers are synchronous, caller-owned and not concurrently callable. Loader callbacks are scoped to one operation and honor the requested byte bound before copying. The storage-backed loader uses an independently verified blob reader; it checks the declared root-artifact length as well as content identity. A new read verifies bytes again. The API does not expose a global unchecked read path.

Direct native calls with malformed JSON can still expose a JSON-library exception from some low-level encoding/export boundaries; callers should catch `std::exception` in addition to inspecting `gr::Error` codes. Runtime import validates its v1 input and the C ABI normalizes exceptions. Uniform low-level error classification is deferred; malformed objects are not accepted as valid history.

Runtime selected-history responses additionally identify the workspace, pinned physical revision, selected semantic revision and manifest version. The coordinator first visits the complete pinned physical snapshot and reconstructs the manifest's current head to check their agreement. A different selected semantic revision then requires another reconstruction. A corrupt current head can therefore block selection of an otherwise intact older node through this API. Efficient single-record lookup for this public selection path is deferred; standalone selected reconstruction has the narrower path-verification scope described above and does not read the complete history DAG.

## Bounds and remaining costs

| Limit | Current bound |
|---|---:|
| Semantic nodes | 1,024, preserving the v1 limit |
| Immediate semantic parents | 32 |
| One encoded object | 16 MiB |
| Fetched bytes per loading/verification session | 64 MiB |
| Reader object count | 8,192 |
| Physical delta depth | 64 maximum |
| Reconstructed-state cache | 64 MiB conservative logical document/framing accounting |
| New blobs in one physical commit | 128 |
| Total new blob bytes in one commit | 64 MiB |
| Retained blob bytes per workspace | Existing 100 MiB storage limit |

These are logical limits, not a guarantee of peak resident memory: parsed JSON, retained wrappers, output bundles and transient copies add overhead. The immutable object cache and reconstructed-state cache belong to a loading/verification session. An import can first export the existing history and then encode it in a separate session; the 64 MiB read bound is not a total API-call or process-memory cap. Dense snapshots must still fit a checkpoint object. An upgrade/import can exceed the transaction's object-count limit before reaching the semantic node limit; it then fails before publication. Larger conversion needs a separately designed staging/import contract and is not silently split into non-atomic commits.

The compact representation removes repeated retention and serialization of cumulative full bundles. It retains O(current inventory) semantic state hashing and candidate dependency checks; reconstructing a delta chain also visits intermediate inventories. SQLite still writes complete physical snapshot membership. Old-revision lookup is O(catalogue length), full export remains proportional to all reconstructed history, and the complete retained workspace remains bounded. These limits should guide benchmarks rather than claims of unlimited scalability.

## Interfaces and evidence

The Layer 5 codec is in `include/gr/kernel/compact_history.hpp` and `src/kernel/compact_history.cpp`; shared v1 identity primitives remain beside `src/kernel/history.cpp`. Runtime representation/custody preparation is in `include/gr/runtime/history_store.hpp` and `src/runtime/history_store.cpp`. Storage contracts and schema are unchanged.

`tests/conformance/compact_history.cpp` compares seeded branching checkpoints, deltas and a two-parent merge with the v1 oracle, including archive changes, exact descriptor identity, periodic checkpoints, malformed/corrupt data, missing ancestry and read budgets. `tests/integration/history_store.cpp` exercises both memory and SQLite adapters, explicit upgrade, retained branches, failed atomic publication, workspace isolation and close/reopen. Coordinator, recovery and process-termination suites separately cover publication outcomes; exception tests alone do not establish crash recovery or power-loss durability. Final executed results and measurements belong in the validation report.
