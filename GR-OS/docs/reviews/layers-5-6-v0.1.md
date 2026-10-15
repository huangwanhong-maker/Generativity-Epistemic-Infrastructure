# Layers 5 and 6: v0.1 contract and review guide

**Class:** Experimental implementation specification  
**Status:** Implemented for review; no standards adoption or legacy compatibility claim

## Responsibility and dependency direction

```text
Python convenience API -> versioned C ABI composition adapter
                            |             |             |
                        L5 kernel      L6S storage   L6E execution
                            |             |             |
                            +---- shared primitives ----+
```

The adapter validates a complete candidate with L5 before publishing through L6S. It is a local synchronous library interface, not a job scheduler, permission engine or application runtime. Native users can use any library independently. Raw `IStore` intentionally stores opaque records: its integrity guarantees do not imply L5 semantic admission. Storage and execution do not depend on L5 code.

## Layer 5: records and values

A record is `{ "document": {...}, "version": "sha256:<hex>" }`. The version hashes the exact `gr.canonical-json/1` encoding of the document, excluding the wrapper. Construction never invents an actor, date or scientific interpretation.

Minimum document:

```json
{
  "format": "gr.record/1",
  "id": "observation:1",
  "type": "gr.assertion@1",
  "payload": {"statement": "The observer reported a change."},
  "attribution": {"agent": "observer:alice", "method": "reported"}
}
```

Required fields are `format`, `id`, `type`, object-valued `payload`, and explicit `attribution.agent`/`method`. Optional fields are `qualifiers`, `roles`, `references`, `participants`, `order`, `temporal`, `artifacts`, `extensions` and `required`. Unrecognized envelope fields are rejected; domain data belongs in `payload` or declared extensions. Attribution/qualifier payloads are not an authentication or evidential-warrant system.

Built-in versioned types are `gr.entity@1`, `gr.state@1`, `gr.event@1`, `gr.process@1`, `gr.relation@1`, `gr.property@1`, `gr.assertion@1` and `gr.order@1`. The first six are convenient operational record profiles, not a closed ontology. `roles` may overlap. Built-ins validate the envelope and the stated structural conditions; they do not validate the truth or every possible domain meaning of payload text. A relation requires at least one participant; an order requires its two event IDs.

`TypeRegistry::register_type` adds a trusted document validator. `register_extension` independently adds a validator for extension `data`; registering a record type cannot accidentally certify arbitrary extension data. Register before sharing a registry across threads; concurrent registry mutation is not supported. Validator callbacks are trusted, pure in-process functions and must not mutate external state. Dynamic loading and callback sandboxing are absent.

An extension has `{ "type": "lab.units@1", "required": true, "data": {...} }`. Unknown optional types/extensions are preserved and produce `semantic_complete: false`. Unknown required semantics remain readable as retained content, but prevent whole-snapshot mutation. This check applies to both the source and candidate, so replacing away an unknown constraint is not a bypass. An intentional translation needs a future separate migration contract.

### Canonical metadata

`gr.canonical-json/1` is pinned to the documented nlohmann JSON encoding used by this implementation: object keys sorted by UTF-8 byte order, compact UTF-8 output, strict UTF-8 validity, no Unicode normalization, and library JSON string escaping. It is not RFC 8785. Supported numbers are integers in `[-9007199254740991, 9007199254740991]`; floating-point values, nonfinite values, binary JSON and larger integers are rejected. Negative integer zero canonicalizes to zero. Large integers and decimals require explicit domain-typed strings or an artifact encoding. A digest over bytes does not establish numerical equivalence between encodings.

Metadata values are limited to depth 64, 100000 values and 1 MiB canonical encoding. Snapshot plus supplied archive documents are limited to 32 MiB; at most 10000 current records, 10000 supplied archive versions and 1000 updates per candidate. IDs are opaque nonempty UTF-8 strings of at most 512 bytes, without NUL. No filesystem path is derived from an ID.

### Identity, references and candidate admission

A snapshot has `revision`, current `records`, and optional `archive` containing retained record wrappers. The revision is storage identity, not represented time. Each current ID appears once. Multiple historical versions may coexist in the archive.

References have `id`, `scope` (`record` or `subject`) and optional pinned `version`. A `record` reference resolves inside current records, or by `(id, version)` against current/archive content when pinned. A `subject` reference does not claim a locally stored record and cannot pin a content version. Unresolved required record references reject the candidate. Archive documents receive structural/hash verification; their historical dependencies are not re-resolved against the current snapshot. Therefore a supplied archive is a targeted version inventory, not a full archival export closure.

`apply(snapshot, updates)` is pure: it verifies the source, replaces/upserts each updated identity once, retains replaced versions in its returned archive, and validates the complete candidate. Forward references inside a batch work. Inputs are unchanged on error. It neither publishes nor increments a storage revision. Existing identities cannot be deleted through this v0.1 API. Later retention or restriction policies need an explicit contract.

### Graph and temporal projections

`graph` returns current nodes, ordered participant incidences and neutral references. Each participant has a role and a `reference`; optional orientation is `in`, `out` or `undirected`. Repeated roles/participants and n-ary relations survive projection. The projection discloses omitted payload/qualification fields. A pinned incidence must resolve using `(id, version)`; it must not be silently joined to the current same-ID node. Subject references need not have a current graph node.

`match` supports equality filters on ID/type, membership of an explicit role, and immediate payload-key equality. It does not introduce a query language or infer ontology subsumption.

An order document declares `order: {before: "event:a", after: "event:b"}`; anchors must currently have type `gr.event@1`. These are strict precedence claims. Temporal selection takes `order_ids` and optional `after` event IDs. Omitted `order_ids` selects all supported order claims; an explicit empty selection selects none. Selected versions and excluded claims are returned. Contradictory claims are preserved but cycles produce no temporal cut. Unsupported order semantics, including unknown required extensions, prevent an unqualified default cut. An explicit supported account can exclude other claims with those exclusions disclosed.

The `after` set must be closed under selected known predecessors. A record's `temporal` has both `start` and `end`, each `{kind: "event", id: ...}`, `{kind: "unknown"}` or `{kind: "unbounded"}`. Start is inclusive and end exclusive. Two event bounds require established strict start-before-end in the selected account; equal, reversed or incomparable bounds are indeterminate with diagnostics. Unknown required interpretation of a scoped record/anchor also prevents a definitive presence classification. Results distinguish `active`, `inactive`, `indeterminate` and `unscoped`. Optional unknown descriptive extensions are preserved; these projections interpret the declared envelope only. No recording-order, simultaneity, causation, truth or legal inference is introduced.

## Layer 6S: storage

`IStore` exposes capability discovery, workspace creation, head, pinned snapshot, immutable record version, commit, paged history and exact blob reads. `MemoryStore` is volatile. `SQLiteStore` opens an explicit operator-supplied local path; its read-only mode rejects mutation and unknown database formats. A revision is workspace-scoped; `0` is the empty initial revision. Workspace IDs may contain path-like text without affecting physical paths.

Commit inputs include workspace, expected revision, transaction ID, actor attribution, reason, upserts, binary blobs and required blob inventory. A commit succeeds atomically or publishes nothing. Only one concurrent writer at an expected head succeeds. Retrying the same transaction with the same actor and request returns the original receipt even if the current head has advanced; changing the request or actor under that transaction ID fails. The request digest covers ordered record/blob inputs and metadata. It is not a claim that differently ordered requests are equivalent.

Both backends retain old versions and snapshots. SQLite uses a single transaction for records, snapshot membership, artifact bytes, receipt and head advancement. Per-record and blob hashes plus a snapshot membership digest detect tested corruption/omission cases. An administrator who can rewrite data and corresponding digests is outside this local integrity model; signatures and custody are separate work.

The Python/C adapter also checks L5 candidate admission, gathers directly pinned historical targets, derives artifacts from the whole current candidate, and verifies descriptor lengths. An artifact descriptor is `{sha256: "sha256:<hex>", byte_length: 42, media_type: "application/octet-stream"}`. Record validation alone cannot verify unavailable bytes; the composed storage commit does. Raw L6 callers provide their own semantic policy.

Default storage limits are 10000 current records, 1000 upserts per commit, 1 MiB per record, 32 MiB current snapshot, 16 MiB per blob, 128 blobs/64 MiB blob bytes per commit and 100 MiB unique workspace blob bytes. Native callers can provide `StorageLimits`, within shared canonical restrictions. These are bounded initial defaults, not performance claims. There is no record-history garbage collector, pruning or total retained-history quota; disk/memory capacity remains the local operator's responsibility. Complete snapshot membership is materialized per revision, which favors simplicity over very large-history efficiency.

SQLite is a local transactional adapter with one active writer, not a distributed database. Tests cover reopen, independent-connection races, injected rollback and corruption detection. They do not establish behavior under every power-loss/filesystem failure. Backup/export closure and online migration remain outside this version. Retry through the semantic adapter assumes the same versioned validator contract; future validator upgrades must preserve accepted retry identity explicitly.

## Layer 6E: execution

`discover_devices()` reports the host CPU and available logical-thread information. `SequentialBackend` and `ThreadedCpuBackend` implement a synchronous batch interface. The caller selects the backend/device; there is no job scheduler or automatic backend selection. Unsupported device and operation versions fail explicitly.

An `OperationDefinition` declares operation/version, implementation identity/version, thread safety and a trusted callback from borrowed read-only bytes plus `stop_token` to owned result bytes. Registration is explicit; duplicates are rejected. A registry selects one implementation per operation version. Separate registries can select alternatives. Built-ins are `identity@1` and `sha256@1`; the latter returns lowercase ASCII hex. Tests register additional operations without modifying backend code.

Outputs preserve input order. Errors return no partial batch; every started worker is joined. Threaded invocation requires a thread-safe callback. Limits cover compute workers, batch-item count (65536 by default), total input/output payload bytes (64 MiB each by default), cancellation and a cooperative deadline. A deadline may add one monitor thread beyond the compute-worker budget. Outputs are measured after each callback returns. Callback internal allocations/effects cannot be bounded or rolled back; callbacks that ignore cancellation cannot be forcibly interrupted.

Live stop tokens and custom registration are native C++ features. The JSON/Python interface exposes built-ins, an initial `cancelled` flag and `deadline_ms`. GPU, remote devices, device buffers, streaming, array-layout negotiation and zero-copy Python arrays are not implemented.

## C ABI and Python

`gr_abi_version`, `gr_call_json` and `gr_free` form C ABI version 1. JSON requests declare `abi: 1` and `op`; responses contain `ok` with a result or structured error. Native exceptions do not cross the ABI. Requests reject duplicate object keys and exceedance of depth/16 MiB transport bounds. Responses are limited to 64 MiB. Binary bytes use canonical padded base64, so transport limits are tighter than some native storage limits. Python uses only its standard library and the compiled shared library.

Storage handles are process-local resources, not principals or authorization tokens. Explicit context-managed closing releases them; an operation already in flight retains its resource safely. A `Native` instance uses an explicit library path or `GR_OS_LIBRARY`. The adapter does not install a daemon, configure credentials or access devices beyond the selected local CPU and database file.

## Verification and review boundaries

Native suites test the semantic contract, common storage behavior across both adapters, SQLite concurrency/reopen/rollback/corruption, and execution cancellation/resource/error behavior. Python integration tests exercise the same native DLL through the C ABI, including complete record/blob admission and persistence. The synthetic example writes only a temporary database.

This stage stops after Layers 5–6. Review priorities are the record envelope, required-extension rules, temporal cut behavior, trusted-library boundary, native versus Python transport limits, and first backend choices. The full R8 ecosystem, higher layers, scientific application packages, legacy readers and compatibility migrations remain subsequent decisions.
