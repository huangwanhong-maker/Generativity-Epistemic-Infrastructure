# Record, projection and historical contracts

**Class:** Experimental implementation specification  
**Version:** GR-OS 0.2; record format 1 and new history format 1

## Record values and profiles

A wrapper is exactly `{document, version}`; the version is `sha256:` plus the digest of canonical document bytes. `core::Record` owns its document/version and exposes const access. `from_document` constructs a new value; `from_wire` verifies retained content. JSON convenience functions use the same code.

Required document fields are `format: "gr.record/1"`, nonempty `id`, versioned `type`, object `payload`, and object `attribution` with explicit `agent` and `method`. Optional fields are `qualifiers`, `roles`, `references`, `participants`, `order`, `temporal`, `artifacts`, `extensions` and Boolean `required`. Unknown envelope fields fail; domain content belongs in payload/extensions.

Record identity, content version, subject reference, recording revision and physical location are distinct. Attribution is neither authentication nor truth certification. Qualifications remain part of hashed content. Built-in entity/state/event/process/relation/property profiles are operational conveniences; roles can overlap. They do not exhaust GR theory.

Identifiers are nonempty UTF-8 strings of at most 512 bytes without NUL. New type/extension registrations and construction additionally use `kernel::validate_qualified_id`. Retained v1 wrappers retain the original permissive namespace/version-marker checks. Historical opaque identifiers are not rewritten. Stricter new construction and old-byte verification intentionally differ.

`core::TypeRegistry` separates record validators from extension-data validators. Register before concurrent reads; concurrent mutation is unsupported. Validators are trusted pure functions. The default registry is shared and immutable. Record descriptors include identity, description and understood capabilities: currently `event-anchor` and `precedence`. A precedence profile requires the order structure; unsupported capability strings fail registration. Projections take the caller's registry consistently.

An extension is `{type, required, data}`. Unknown optional content remains readable and yields `semantic_complete: false`. Unknown required current semantics block mutation, including replacement that would remove the unknown constraint. Archived records are verified without rebinding all their historical dependencies to the current snapshot. Computational operand registration is a separate concern; see [execution contracts](execution-contracts.md).

## Canonicalization and evolution

`gr.canonical-json/1` retains the pinned nlohmann JSON encoding: sorted UTF-8 byte keys, compact UTF-8 output, valid strings, no Unicode normalization and library escaping. It is not RFC 8785. Numbers are exact integers in [-9007199254740991, 9007199254740991]; floats, nonfinite values, binary JSON and larger integers fail. Negative integer zero becomes zero. Domain decimals and arrays need explicit encodings.

Metadata is limited to depth 64, 100000 values and 1 MiB canonical bytes. Strict `parse_metadata` rejects duplicate decoded keys, including escaped spellings of the same key. A Json object already parsed elsewhere cannot recover discarded duplicate-key evidence.

[Golden vectors](../tests/conformance/canonical-v1.json) pin bytes/hashes. A changed encoding needs a new identifier and explicit translation. Fingerprints cover declarations, not validator machine code or factual accuracy.

## Snapshot admission and staging

A snapshot has `revision`, current `records` and optional retained `archive`. Bounds are 10000 current records, 10000 archive versions, 1000 updates per apply and 32 MiB total canonical document bytes. Each current ID is unique.

A reference has `id`, record/subject `scope`, and optional pinned `version`. Unpinned record references resolve current IDs; pinned ones resolve exact ID/version in current/archive. Subject references do not promise local existence and cannot pin record versions. Current references and event dependencies must resolve.

`apply(snapshot, updates, registry)` validates the source, upserts each update ID once, retains replaced versions in a deduplicated archive, then validates the whole candidate. Forward references within the batch work. Failure leaves inputs unchanged.

Effective edits remove stale reconstructed history metadata and add `staging.base_revision`. The retained revision denotes the base, not a new publication. Projections report `source_revision: null`, `source_state: "staged"` and `base_revision`. Repeated staging preserves the original base; a no-op preserves source provenance. History append or physical publication establishes a new identity.

## Relations and represented time

Graph projection preserves each participant as a separate ordinal incidence with relation ID/version and role/reference data. Higher arity and repeated roles survive. It does not reduce relations to binary edges. Payloads, attribution, qualifications, artifacts and time fields remain in source records and are declared projection losses.

Match supports exact ID/type, role membership and exact top-level payload filters. It is not an inference engine.

Temporal projection selects order-record accounts and optionally derives a downward-closed event cut. It reports included/excluded claims, unsupported semantics, cycles and indeterminate intervals. Conflicting accounts remain records; incomparability does not imply simultaneity. Required unknown semantics prevent affected interpretation, while an explicitly selected supported subset can still be interpreted with scope visible. Event, unknown and unbounded endpoints remain distinct. Neither commit sequence nor recording parents supplies occurrence order.

## Recording-history DAG

An empty bundle is `{"format":"gr.history/1","nodes":[]}`. Each node contains exactly `version`, `document` and normalized checkpoint `snapshot`. The descriptor has:

```json
{
  "format": "gr.history-node/1",
  "parents": ["sha256:<earlier-node-digest>"],
  "state_digest": "sha256:<checkpoint-digest>",
  "attribution": {"agent":"recorder:A","method":"declared checkpoint","reason":"revised account"}
}
```

Node identity hashes its canonical descriptor. Parent order affects identity without granting authority/precedence. Parents must be present, distinct and acyclic. Bundle serialization order is irrelevant. Attribution requires agent/method/reason and may retain additional canonical metadata.

A normalized checkpoint has revision "0", current records sorted by ID and archive sorted/deduplicated by ID/version, excluding versions identical to current records. Its digest hashes ASCII `gr.history-state/1\nrecords\n`, then each current entry's 64-hex SHA-256 of canonical `{id,version}` followed by newline, then `archive\n` and the same framed archive entries. The outer hash is prefixed `sha256:`. Every record wrapper is independently verified. Physical revisions and presentation metadata are excluded.

| Function | Behavior |
|---|---|
| validate | Report envelope/hash/normalization checks, checkpoint structural validity and complete acyclic parent closure |
| append | Check immediate parents and candidate for mutation, then return a new bundle; identical repeat is idempotent |
| reconstruct | Return verified checkpoint with node revision, parents and attribution |
| diff | Report added, removed and changed current record versions |
| merge | Explicit-base record-version three-way merge: conflicts or a new parented node |

The merge base must be an ancestor of both tips. Base and both tips require available mandatory semantics. Equal left/right versions are selected directly; a change on one side while the other equals base selects the changed version. Other differences are conflicts. Each conflict needs a complete validated record or null for absence. Extra resolutions, mismatched IDs and invalid final dependencies fail. Unresolved conflicts return `merged: false` without mutation. Success retains current/archive versions from base and both tips, and records both parents (one if tips coincide).

This is declared checkpoint ancestry, not proof of replay from primitive instructions. Append accepts independently supplied checkpoints and does not claim how they arose. Parent nodes remain retained; append does not automatically copy their archive into the new checkpoint. Explicit merge absence is not physical erasure or factual adjudication.

## Ownership, cost and limits

These are synchronous value operations with borrowed const inputs and caller-owned results. Contract failures throw `gr::Error`; validation functions return reports. Malformed native Json or throwing trusted validators may propagate standard exceptions from other entry points. The C boundary catches exceptions.

History bounds: 1024 nodes, 32 immediate parents, 64 MiB aggregate canonical record documents plus descriptors, and all per-snapshot limits. Container/copy overhead is additional. Each operation validates the whole bundle and checkpoints; work scales with retained content and includes duplicated checkpoints. This is a bounded foundation, not an indexed persistent history engine.

Indexed histories, replay semantics, restricted-history export, signatures, target-continuity decisions and physical branch transactions remain open. [History conformance](../tests/conformance/history.cpp) covers seeded branch reconstruction, conflicts, unavailable constraints, identity distinctions and tampering. [Record tests](../tests/unit/records.cpp) cover relations, competing accounts, temporal uncertainty and opaque extensions.
