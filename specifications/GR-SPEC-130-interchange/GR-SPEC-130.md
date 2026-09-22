# GR-SPEC-130 — Project transactions, Git binding and interchange

**Document class:** Specification  
**Status:** EXPERIMENTAL technical specification; not an adopted Standard  
**Edition:** 0.2, 2026-09-22  
**Protocol:** `gsp-record-protocol/0.2`  
**General profile:** `gsp.general/0.2`  
**Storage binding:** `gsp.git/0.2`

## Foreword

This Specification defines a bounded project-transaction and interchange binding for the experimental Generativity workspace. It develops the infrastructure need identified in [the graph-workspace design](../../docs/planning/graph_workspace_design.md) and [ADR-0008](../../decisions/ADR-0008-record-graphs-transactions-and-modules.md). Those documents record the selected alternatives and limitations. They do not establish standards adoption, institutional authority or factual warrant.

The first implementation combines an independent protocol package, an authenticated HTTP adapter and a Git storage adapter. The components have different responsibilities. Availability of a shared package does not establish that two independent applications have successfully exchanged records. Adaptation of the academia application remains subsequent work.

The requirements below use **shall** for requirements, **should** for recommendations, **may** for permission and **can** for capability. They apply within the declared experimental binding. A claim about a tested component shall identify its scope rather than imply conformance of an institution, factual claim or whole GR standards family.

## 1. Scope

This Specification covers:

- coordinated changes to a project's records, declarative modules and retained material;
- base-version comparison, committed receipts and repeated requests;
- one-repository-per-project Git serialization;
- reads of complete historical project snapshots;
- current-snapshot JSON, selected-snapshot material packages and full-history bundles;
- adaptation and explicit migration of the initial `gsp-workspace/0.1` representation;
- bounded failure behavior and evidence needed to assess implementation claims.

It does not define the full semantics of the six operational recording roles, replace the information model, supply an access policy, implement a public federation protocol or establish a universally applicable retention policy. Authentication, project ownership and disclosure checks are supplied by the application. Independent signatures, external timestamps, semantic merging, concurrent branches, selective erasure and remote object storage are outside this edition.

## 2. References and authority

The record envelope and module contract are specified in [GR-SPEC-120](../GR-SPEC-120-information-model/GR-SPEC-120.md). The executable representation is distributed with [the independent protocol package](../../applicative_infrastructure/common/packages/gsp_record_protocol/). Both remain experimental.

This binding uses Git's documented object and reference operations. Conditional publication follows [Git update-ref](https://git-scm.com/docs/git-update-ref); blob construction and bounded object reading use [Git hash-object](https://git-scm.com/docs/git-hash-object) and [Git cat-file](https://git-scm.com/docs/git-cat-file). Full-history interchange uses [Git bundle](https://git-scm.com/docs/git-bundle).

Request-digest canonicalization uses [RFC 8785, JSON Canonicalization Scheme](https://www.rfc-editor.org/rfc/rfc8785.html). HTTP terminology follows [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html). The body field `expected_head` is an application-level precondition; this edition does not redefine HTTP entity tags or `If-Match` semantics.

## 3. Terms and identifiers

**Project snapshot:** the project manifest and retained record representations obtained from one accepted project revision. A snapshot can refer to retained material without embedding its bytes in JSON.

**Transaction:** one proposed coordinated mutation, identified by a caller-supplied UUID, evaluated against a declared base revision and accepted through one publication operation. A storage transaction is not automatically a represented Event, institutional act or epistemic judgment.

**Receipt:** structured information retained in the same accepted snapshot as the transaction's effects. It identifies the request, recorder, base, reason, changes and bounded validation result.

**Revision:** in this Git binding, a full lowercase 40-character SHA-1 Git commit identifier. A revision identifies retained project content within the binding; it does not authenticate the represented target or establish independent time.

**Asset:** retained binary material addressed by its lowercase 64-character SHA-256 content digest. Its storage digest identifies bytes. A file descriptor supplies a separately identifiable acquisition and descriptive context.

**Publication:** successful advancement of `refs/heads/main` to the candidate commit, conditional on its preceding value. Prepared but unreachable objects are not accepted project revisions.

**X130-001.** Project, record, transaction and file descriptor identifiers shall use canonical UUID strings in this profile. Applications shall not derive arbitrary filesystem paths or Git revision expressions from supplied identifiers.

**X130-002.** A revision requested through this binding shall be a complete commit identifier reachable from the selected project's accepted `main` history. A shortened identifier, branch expression, foreign-project commit or dangling prepared commit shall not be treated as an accepted revision.

## 4. Transaction representation

A transaction contains `protocol_version`, `transaction_id`, `expected_head`, `reason`, `change_categories` and `operations`. Operation names and their field constraints are defined by GR-SPEC-120 and the package schemas. This edition supports record creation/update, module setting/removal, file attachment/replacement/detachment and an explicit project-migration operation.

**X130-003.** A receiver shall validate the supported protocol/profile, operation envelopes, identifiers, declared types and resource limits before accepting a mutation. Duplicate JSON members, malformed Unicode, nonfinite numbers and unrecognized operation fields shall be rejected rather than silently normalized.

**X130-004.** Transaction validation shall examine the complete resulting candidate snapshot. References between resources created in the same transaction shall be evaluated against that candidate, not against an arbitrarily ordered series of partially accepted resources.

**X130-005.** A transaction shall not publish a valid subset of its operations when another operation fails. Repeated mutations of one guarded resource shall follow the information-model rules rather than an implicit last-write-wins convention.

**X130-006.** Server-managed recording attribution and time shall be assigned by the receiving application. They shall remain distinguishable from supplied source attribution, represented occurrence time, assessments and acquisition descriptions.

**X130-007.** An implementation shall preserve unmodified optional modules it cannot interpret. It shall refuse a mutation when an unsupported required module prevents evaluation of the project's necessary constraints. Unsupported data shall not install executable code or determine server paths.

The shared package prepares a complete `{project, records}` candidate and a receipt without accessing Flask, SQLite, a repository or a filesystem. The application verifies the caller and actual uploaded material. The Git adapter enforces storage identities, resource bounds, material closure and conditional publication. Passing one layer's checks does not substitute for the others.

## 5. File transfer and material closure

An ordinary transaction can be sent as JSON. A transaction carrying files uses multipart encoding with one textual `transaction` JSON field and explicitly named binary parts. A file operation declares its part name, file descriptor ID, owning record, filename, media type, SHA-256 and byte length.

**X130-008.** The received binary part names shall correspond exactly to the declared attachment/replacement operations. Duplicate, missing and undeclared parts shall be rejected. Multipart boundary strings and transmission ordering shall not determine logical transaction identity.

**X130-009.** The receiving implementation shall compute the actual byte length and SHA-256 of each supplied file using bounded reads. A mismatch with its declared identity shall prevent publication.

**X130-010.** The bytes subsequently written to Git shall be the same bounded bytes whose digest was checked. An implementation shall not validate one path state and later publish different bytes through an unchecked reread.

**X130-011.** For every understood `gsp.files` version `1` descriptor in a candidate, the candidate tree shall contain a matching asset with the declared byte length. The adapter shall reject supplied assets that have no current understood descriptor in the accepted candidate.

**X130-012.** Filenames and media types shall remain descriptive metadata. Filenames shall not be used as arbitrary repository or filesystem paths, and media-type declarations shall not cause active content to execute.

**X130-013.** File detachment and replacement shall not be described as erasure of earlier bytes. This edition retains existing asset entries in subsequent project trees. Unsupported optional modules shall not trigger collection of material that a reader cannot interpret.

Files are initially offered as downloads with `nosniff`. This edition includes no antivirus finding, media authenticity finding or evidence-sufficiency determination. Identical retained bytes can serve different descriptors and acquisition accounts without becoming independent evidence merely because there are several references.

## 6. Request identity and committed receipts

The request digest is lowercase SHA-256 over RFC 8785 canonical JSON of:

```json
{"transaction": "the logical transaction object", "files": "verified part-name metadata mapping"}
```

The displayed strings above denote objects, not literal serialization values. Each entry in `files` has its verified `sha256` and `byte_length`. Ordinary JSON requests use an empty mapping. The logical transaction includes its original base and operation ordering. Server-assigned actor/time and multipart framing are excluded from the digest; actor identity is checked separately.

**X130-014.** A committed receipt shall include protocol version, transaction ID, original `expected_head`, authenticated actor, recording time, reason, change categories, request digest, changed-resource summaries and the scope/limitations of structural validation.

**X130-015.** The receipt shall be retained at `.gsp/transactions/{transaction_id}.json` in the same tree as the accepted effects. A published receipt path shall not subsequently be overwritten through this binding.

**X130-016.** The resulting commit identifier shall not be embedded in the receipt whose containing tree would determine that identifier. A receipt response shall resolve the commit that introduced the immutable receipt path and distinguish it from the currently observed head.

**X130-017.** The application shall check a committed receipt before rejecting a request solely because its base has become old. A request with the same transaction ID, actor and logical digest shall return the original result without publishing another mutation. Reuse for a different request or actor shall be reported as a conflict.

**X130-018.** After a failed publication race, an application shall check whether an identical transaction was committed before reporting the request as an unrelated conflict. A client retry after an uncertain response shall retain its transaction ID and logical request.

**X130-019.** A receiver shall not claim that a timed-out or interrupted response proves that nothing was published. Receipt lookup or retry shall be available to recover the accepted result. An absent receipt during an ongoing race is an observation at that read, not a promise about subsequent completion.

Requests rejected before publication do not acquire a committed receipt. A materially reconciled draft uses a new transaction ID. These rules constrain accepted project effects; they do not promise exactly-once external notifications or institutional action.

## 7. Git repository binding

Each project uses an application-managed bare repository with one accepted branch. The retained tree has this structure:

```text
project.json
records/<record-uuid>.json
assets/<sha256>
.gsp/protocol.json
.gsp/transactions/<transaction-uuid>.json
```

The protocol manifest identifies protocol, profile, binding, object format and record schema. New `gsp-workspace/0.2` projects initialize the manifest and empty records/assets/transactions trees. Older project manifests remain readable without being rewritten on access.

**X130-020.** The adapter shall prepare complete affected trees and a single commit before publishing any change. It shall advance `refs/heads/main` only if its current value equals the supplied expected parent.

**X130-021.** Publication failure shall not expose a partial accepted project. Unreachable prepared objects may remain in repository storage, but ordinary reads and exports of accepted history shall not treat them as published revisions.

**X130-022.** Repository operations shall avoid dependence on a shared working tree or index. Untrusted host Git configuration, hooks, replacement objects, transport settings and inherited `GIT_*` redirection shall not silently control the binding's record operations.

**X130-023.** A full-candidate storage write shall not silently omit existing records. The first profile represents withdrawal or revision through retained records rather than current-tree deletion. Existing asset entries and receipt paths shall survive subsequent accepted writes.

**X130-024.** Receipt resolution shall use one pinned accepted head and establish that the receipt path was introduced once without later mutation. An inconsistent or modified receipt history shall produce an error rather than a false original-result attribution.

**X130-043.** Before mutation, an existing `.gsp/protocol.json` shall exactly match the supported manifest's protocol version, profile, binding, object format and record schema, with no unrecognized fields. A different or extended manifest shall be preserved and mutation refused; the adapter shall not silently replace a future binding declaration with its own defaults. Read-only inspection of otherwise readable stored data does not imply support for mutation under that declaration.

The present adapter uses Git's SHA-1 repository object format and independent SHA-256 asset digests. Neither substitutes for an authenticated signature or external preservation guarantee. An authorized server administrator can rewrite references or remove repositories; this binding does not claim immutability against that administrator.

## 8. Limits, buffering and capability disclosure

The following are initial implementation limits, not requirements imposed by GR theory. A lower application transport limit also applies where declared.

| Limit | Initial value | Scope |
|---|---:|---|
| Transaction operations | 100 | Protocol envelope |
| Records | 1,000 | Retained project snapshot |
| Binary parts | 8 | HTTP transaction |
| Individual file bytes | 10 MiB | Input and retained asset |
| Combined incoming file bytes | 20 MiB | Transaction, before deduplication in HTTP adaptation |
| Retained unique asset bytes | 128 MiB | Asset blobs reachable from the current asset tree |
| JSON resource bytes | 2 MiB | One stored serialized JSON resource |
| Project plus record JSON bytes | 32 MiB | Combined stored serialized snapshot |
| Protocol transaction JSON | 2 MiB | Shared package bound |
| HTTP transaction JSON | 1 MiB | Initial HTTP binding |
| Module JSON | 256 KiB | Shared profile |
| Returned history entries | 100 | One history response |

The package's internal snapshot validation bound is 64 MiB. This Git binding's 32 MiB serialized bound is narrower. Storage JSON includes its chosen whitespace and therefore is not measured identically to the JCS request digest. Capabilities disclose the layer-specific limits.

**X130-025.** An implementation shall disclose its applicable bounds and shall fail before publication when a proposed change exceeds them. It shall not truncate records, modules or material to make a request fit.

**X130-026.** Snapshot reads shall inspect object kinds and sizes before materializing their bodies. Implementations shall bound per-resource and aggregate JSON reads and reject malformed or incomplete object streams.

**X130-027.** Attachment, download and bundle handling shall avoid requiring the complete accumulated history or bundle to reside in application memory. Temporary storage shall be closed and released on success, failure and client disconnection according to the host's supported lifecycle.

The retained-asset limit does not bound unreachable prepared objects, Git metadata, transaction-ledger size, operating-system caches or backups. Filesystem quotas, monitoring, governed collection and recovery remain hosting responsibilities. Increasing a declared bound requires capacity assessment and corresponding tests.

## 9. Snapshot-consistent reading and presentation

**X130-028.** A project read shall resolve its accepted revision once and obtain the manifest and all selected records from that tree. Participant names, relation modules, notes, related-record labels and file descriptors in a historical presentation shall not silently come from today's project.

**X130-029.** An application shall distinguish a requested historical revision from its currently observed project head. Historical views shall not silently become current editable views.

**X130-030.** Historical content, history, receipt and file operations shall pass the application's applicable project authorization checks. A digest or revision identifier shall not serve as a permission token. File delivery shall require a descriptor in the authorized record at the selected revision, not merely the presence of a matching retained blob.

**X130-031.** A bounded history listing shall disclose its limit. The Git recording sequence shall not be presented as the occurrence chronology or causal ordering of represented targets.

The storage adapter returns caller-owned binary temporary files positioned at byte zero for assets and bundles. The HTTP adapter closes these handles when responses finish or fail. The legacy `bundle()` helper remains available for small compatibility tests, but HTTP downloads use `bundle_file()`.

## 10. Export classes

Three distinct exports are supported:

| Export | Included content | Excluded content |
|---|---|---|
| Snapshot JSON | Manifest, current-at-selected-revision record representations and source revision | File bytes, earlier versions, receipt ledger, credentials |
| Snapshot ZIP | Export manifest, project manifest, records and their understood referenced material at the selected revision | Earlier versions, accumulating receipts, detached/unreferenced assets, credentials |
| Git bundle | Accepted reference and its reachable commit/object history, including retained receipts and material | Application account/session database, authorization configuration, unreachable failed candidates |

**X130-032.** An exporter shall identify the source project revision, declared export scope and known exclusions. It shall preserve exact stored representations rather than silently replacing them with a display adaptation.

**X130-033.** A selected-snapshot material package shall include the referenced bytes of understood supported file descriptors and preserve their digest-based filenames. It shall not include every retained asset or the accumulating transaction ledger merely because they occur in the current Git tree.

**X130-034.** A material package shall disclose unsupported modules and the limits of reconstructing material described only inside them. An exporter shall not assert complete recovery of an opaque module's dependencies.

**X130-035.** A full-history bundle shall preserve the accepted reachable history, including earlier replaced or detached material. Its presentation shall disclose that these bytes and earlier reasons can remain present even when a current snapshot no longer shows them.

**X130-036.** An export shall not be described as a complete running-service backup when it omits accounts, access configuration or other required service state. Importing a repository copy shall not itself assign ownership or authenticate its recording actors in a new service.

Current JSON and ZIP are experimental interchange representations. This edition specifies no automatic package-import endpoint. Independent import, collision handling, protected subsets, additional hash bindings and academia-profile mapping require further executable fixtures before an interoperability claim.

## 11. Legacy adaptation and migration

**X130-037.** Reading a `gsp-workspace/0.1` project shall not mutate its repository. Any presentation adaptation shall identify the legacy source and preserve its identifiers and original retained history.

**X130-038.** Migration shall be an explicit, attributed transaction with the old revision as its base and a recorded mapping/limitations account. Original commits shall remain available. A stale migration shall fail through the same publication precondition as another mutation.

**X130-039.** Migration shall not infer structured Relation participants, predicates, evidence judgments, acquisition records or attachment bytes from legacy navigation links and free text. An unstructured Relation account shall remain distinguishable from a structured relation.

Compatibility record endpoints can translate bounded requests into the shared transaction preparation path. Such endpoints do not provide the full caller-controlled retry guarantee when the server generates a new transaction ID for each request. They do not authorize bypassing unsupported-module or material-closure checks.

## 12. HTTP operation mapping and errors

The first application exposes these operations:

| Method and path | Result |
|---|---|
| `GET /api/protocol` | Supported protocol/profile/modules and layered limits |
| `GET /api/projects/{project}` with optional `revision` | Snapshot and distinction between selected/current revision |
| `GET /api/projects/{project}/graph` with optional `revision` | Projection of that selected snapshot |
| `POST /api/projects/{project}/transactions` | Accepted or replayed transaction result |
| `GET /api/projects/{project}/transactions/{transaction}` | Committed receipt, original result revision, observed head |
| `GET /api/projects/{project}/history` | Up to 100 project commits |
| `GET /api/projects/{project}/records/{record}/history` with optional `revision` | Bounded record history at the selected revision |
| `GET /api/projects/{project}/records/{record}/files/{file}` with optional `revision` | Authorized descriptor's retained bytes |
| `GET /api/projects/{project}/migration` | Informative preview without mutation |
| `GET /api/projects/{project}/export` with optional `revision` | Snapshot JSON |
| `GET /api/projects/{project}/package` with optional `revision` | Snapshot ZIP |
| `GET /api/projects/{project}/bundle` | Complete accepted reachable-history bundle |

**X130-040.** State-changing HTTP requests shall require application authorization and its anti-forgery protection. A protected project or resource shall not become discoverable solely through a guessed identifier.

**X130-041.** The application shall distinguish malformed/unsupported input, denied access, missing authorized resources, base or request-ID conflicts, resource limits and storage unavailability. A failed request shall not discard the client's unsaved graph or imply a substantive judgment about its claims.

The application uses `201` for a new accepted transaction, `200` for a recognized replay, `400` for validation failures, `401`/`403` for applicable authentication/anti-forgery failures, `404` for unavailable authorized resources, `409` for conflicts, `413` for binding size limits, `415` for unsupported media types and `503` for unavailable storage. Error classification does not establish whether an uncertain publication occurred; the receipt resolves that separate question.

## 13. Assessment evidence

**X130-042.** An implementation assessment shall identify the components, profile, limits, environment and scenarios actually exercised. Structural validation shall not be labelled factual verification, evidence sufficiency, institutional adoption or full GR conformance.

The first test set covers coordinated record/relation/material publication; no partial publication on failure; competing writers; stale bases; persistent receipt lookup after restart and advancing heads; immutable receipt paths; historical labels and file replacement/detachment; bounded snapshot batching; digest/length/material-closure failures; retained-byte budgets and deduplication; bundle reconstruction; and migration preserving legacy history without manufacturing structure.

HTTP tests additionally need same-request replay, changed-request rejection, malformed multipart and unsupported module behavior, authorization across current/historical resources, and snapshot export selection. Browser assessment needs pinned graph/inspector navigation, unsaved-draft preservation and a nonspatial interaction path. A passing storage test does not establish these additional behaviors.

Relevant implementation artifacts are [Git adapter](../../applicative_infrastructure/common/packages/gsp_git_store/src/gsp_git_store/store.py), [transaction storage tests](../../applicative_infrastructure/common/packages/gsp_git_store/tests/test_transactions_store.py), [legacy storage tests](../../applicative_infrastructure/common/packages/gsp_git_store/tests/test_git_store.py), [HTTP binding](../../applicative_infrastructure/gr_generalized_application/web_application/generative_app/protocol_api.py) and [protocol package](../../applicative_infrastructure/common/packages/gsp_record_protocol/).
