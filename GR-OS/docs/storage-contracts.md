# L6S transactional storage and immutable readers

Class: implementation specification. Status: EXPERIMENTAL, GR-OS 0.3.

The physical schema and contracts below remain v1. The [Layer 4 transaction
coordinator](runtime-transactions.md) now provides native semantic admission,
optional atomic history-artifact custody and recovery over this storage boundary.
It does not change the physical ledger into a semantic DAG or introduce cross-store
atomicity.
Storage format and request identity: `gr.storage/1`; SQLite schema version: 1.
Canonical metadata: `gr.canonical-json/1`.

## Responsibilities and module boundaries

`storage/contracts.hpp` defines opaque workspace history, publication requests,
receipts, typed guarantees, and requirements. It depends on common metadata and
the independent `storage/blob_reader.hpp` object-reader contract. `memory_store.hpp`
and `sqlite_store.hpp` are concrete adapters. `gr/storage.hpp` remains an umbrella
include. Internal validation and hashing live in `src/storage/contracts.cpp`;
physical adapters live in `memory.cpp` and `sqlite.cpp`. SQLite and PicoSHA2 headers
are private implementation dependencies.

`IStore` is a transactional history port. It does not require every future
columnar, array, object, or scientific-file adapter to implement workspace commits.
An artifact adapter can implement `BlobReader` independently. A future transactional
adapter must satisfy the common publication contract and declare its guarantees.
Neither port decides payload semantics, operation support, authentication,
authorization, or the truth of attributed claims.

## Publication and receipts

A workspace starts at revision `0`. A commit checks an expected head and atomically
publishes record upserts, admitted binary objects, a complete snapshot, a transaction
receipt, and the new head. Omitted record identities remain present. Records have
immutable content versions; old snapshots and versions remain readable. Scope is
one workspace, with no cross-workspace transaction or object lookup.

The store computes the request digest, including actor, reason, expected head,
transaction ID, ordered record input metadata and verified payload digests, supplied
object digests and lengths, and the explicit required-object inventory. Retries
must preserve that request exactly. An existing transaction ID is checked before a
stale-head conflict: an identical request returns its original revision plus the
current head; changed content fails with `transaction_conflict`. Record input with
an omitted version and the same input with an explicit version are distinct requests.

`receipt(workspace, transaction_id)` returns the stored entry and the head observed
with it, or an empty optional for an unknown transaction. This lookup requires no
semantic revalidation and works on a read-only adapter. It is a receipt of storage
publication, not an externally signed receipt or an authorization decision.

Required objects must already exist in that workspace or be included in the same
commit. Each supplied object must be in the explicit inventory. Native admission
verifies every required object's complete SHA-256, including preexisting objects;
the SQLite check runs inside the publication transaction. Missing or corrupt
objects and late database errors roll back records, objects, receipt and head.
L5 derives the required inventory from its validated candidate; L6 does not inspect
opaque payloads to infer references. Whole uploads remain bounded vectors. Staging,
resumable upload, object-store coordination and distributed transactions are deferred.

## Capabilities and requirement checks

`describe()` returns `StorageCapabilities`. `require_support(capabilities,
requirements)` checks the already selected adapter and reports missing guarantees;
it does not select a backend, silently downgrade requirements, or fall back.
`capabilities()` provides the corresponding JSON projection for bindings.

| Guarantee | Memory | SQLite |
| --- | --- | --- |
| Persistence | Process lifetime | Local file, WAL, FULL synchronization |
| Publication scope | One workspace | One workspace |
| History | Linear, newest-first pagination | Linear, newest-first pagination |
| Atomic records, objects and receipt | Yes | Yes |
| Conditional head update | Serialized store calls | Independent connections, one writer |
| Snapshot consistency | Immutable in-memory snapshot | SQLite read transaction |
| Reader lifetime | Shared immutable bytes | Independent pinned read transaction |
| Range integrity | Whole object verified before exposure | Whole object verified before exposure |
| Cancellation | Between bounded chunks | Between bounded chunks |

The current contract includes no branches, merge bases, multiple parents, deletion,
garbage collection, remote replication, or physical disk quota. Requirements for
DAG history fail explicitly. SQLite's local durability depends on SQLite and the
underlying filesystem/device honoring synchronization; it is not distributed durability.

## Immutable object reads

`blob_info()` reports a bounded byte length and digest with integrity `unverified`.
It provides cheap metadata for length/admission planning; it does not hash bytes.

`open_blob()` pins the object and verifies its entire SHA-256 before returning a
reader with integrity `sha256-verified`. Verification uses at most a 64 KiB scratch
buffer and respects the requested maximum chunk size, which is between 1 byte and
1 MiB. Opening costs O(object size), including for a subsequent small range. A plain
SHA-256 identifier cannot independently authenticate an arbitrary partial range.
Merkle/chunk proofs and authenticated remote-range protocols require a future
explicit contract rather than a weaker interpretation of this guarantee.

`read(offset, destination, stop_token)` writes at most the destination size, the
configured chunk size, and the remaining bytes. It returns zero at EOF or for an
empty destination. An offset beyond EOF fails with `blob_range`. Reads may be
short. A pre-cancelled call fails with `storage_cancelled` without touching the
destination. The opening cancellation token is retained and checked on later reads;
per-call cancellation is also supported. Cancellation is cooperative between chunks,
not an interrupt of an operating-system read already in progress. No exception
guarantee promises an unchanged destination after an underlying I/O failure.

Readers are caller-owned, support repeated arbitrary offsets, and remain usable
after their originating store closes. A reader is not concurrently callable;
separate readers may be used independently. Store methods are serialized within
each adapter instance. Closing an object while another thread uses it remains the
caller's lifetime responsibility.

Memory readers share the admitted immutable byte allocation. SQLite readers open a
separate read-only connection, begin a read transaction, and use an incremental
SQLite BLOB handle. That transaction pins the verified view across later commits
or even external object-row changes. A later reader sees and rejects corruption.
The old reader continues serving exactly the bytes it verified. Reader destruction
closes the BLOB handle and transaction. Long-lived readers can retain WAL pages and
delay checkpoint reclamation, so callers should close them promptly.

`blob()` remains a bounded convenience wrapper which verifies through a reader and
allocates the complete return vector. Bindings can use raw `readinto` instead of
representing binary bytes as JSON integers.

## Limits, integrity and compatibility

Defaults retain at most 10,000 current records per workspace, 1,000 record upserts
per commit, 1 MiB canonical bytes per record, 32 MiB per snapshot, 16 MiB per object,
128 supplied objects per commit, 64 MiB supplied object bytes per commit, and
100 MiB distinct retained object bytes per workspace. History pages are 1–1,000
entries. History and old record versions are retained without an aggregate disk
quota; snapshot limits do not bound accumulated history or WAL size.

Constructors reject configured metadata limits exceeding the canonical contract.
SQLite object sizes are limited to the incremental BLOB API's signed integer range.
Stored text lengths are checked before making an application string copy, and
stored object lengths before reader exposure. These bounds do not claim to prevent
all allocations inside SQLite itself or to bound memory used by caller-owned input.

Every record read rechecks its canonical content digest. Snapshot reads verify the
complete sorted identity/version inventory, not just individual payloads. Blob
readers check the complete content digest. SQLite ledger reads check revision
identity against workspace, parent and request digest, positive bounded sequence,
immediate predecessor linkage, and the head against the latest sequence. Sequence exhaustion fails before arithmetic can
overflow. These are corruption checks, not evidence that an adversary with direct
file access cannot rewrite a consistent database.

Schema v1 stores actor, reason and transaction ID alongside a request digest but
does not retain the canonical request summary needed to reconstruct that digest
from a receipt alone. Therefore read-time revision-link checks do **not** authenticate
those attribution fields or independently reconstruct the original complete request.
Changing actor/reason directly in SQLite can escape read-time detection. The original
request supplied during retry is rehashed and checked against the stored digest;
the actor is also compared. A future stronger receipt format needs an explicit
schema/version decision, retained request summary and, if required, external signing
or anchoring. This increment does not silently migrate existing databases.

Existing schema-v1 files, canonical record versions, request digests and revision
IDs are preserved. Foreign databases, unsupported versions and incompatible
canonicalization declarations are rejected. SQLite paths are operator-supplied
local paths; workspace and record identifiers never become paths. URI and
`:memory:` SQLite paths are rejected because this adapter promises persistence and
uses separate connections for readers; use `MemoryStore` for volatile storage.

## Conformance evidence

`tests/conformance/storage.cpp` runs the same publication, rollback, version,
pagination, capability, receipt, range, cancellation and lifetime assertions against
both adapters. Its fixed-seed model generates repeated edits and checks current and
random historical snapshots against saved user values, old retries, failed CAS
receipts, and complete paginated history.

`tests/integration/sqlite.cpp` checks independent competing writers and identical
retries, late-trigger rollback, read-only reopening, bounded reads of existing
databases, payload/membership/object/ledger corruption, sequence guards, verified
reader pinning across later writes and store destruction, and ranges across large
object/chunk boundaries. The v1 attribution limitation is asserted explicitly so
future changes cannot accidentally claim stronger integrity than they provide.

These tests exercise process-level behavior; they are not power-loss simulation,
hostile-filesystem certification, remote-backend conformance or a performance claim.
