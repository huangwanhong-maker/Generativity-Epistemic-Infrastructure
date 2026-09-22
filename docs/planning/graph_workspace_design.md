# Graph workspace and shared recording protocol

**Class:** Informative implementation design and delivery plan  
**Status:** PROVISIONAL design selected for experimental implementation; 2026-09-22  
**Application:** Generalized workspace first; academia adaptation is a subsequent project  
**Protocol identifier:** `gsp-record-protocol/0.2`  
**Profile:** `gsp.general/0.2`

## 1. Purpose, source precedence, and scope

The user requested a carefully designed graph workspace, visual creation of records and connections, modular record panels and file storage, with a protocol that can support a later independent academia application. The user explicitly prefers the current GR manuscripts where the older academia project conflicts, and authorizes revisions to our specifications and drafts when a concrete deficiency is identified.

The [source review](graph_semantics_review.md) grounds graph semantics in actual foundation passages and current GR-100/110/200 requirements. The [academia comparison](../reviews/academia_application_reference.md) identifies useful interaction patterns and differences between its design and implementation. The current programme remains the conceptual source; academia is a reference implementation of an older, narrower scholarly model. Its eight epistemic acts, append-only signed transition format, disclosure restrictions and prohibitions on some node kinds do not become universal GR rules.

The present implementation covers a general record graph, a reusable protocol package, atomic Git transactions, record modules, bounded retained attachments, consistent historical views and a migration path from the first workspace. It does not modify the academia repository, rewrite its signed records, or claim cross-application conformance. A later adapter needs independently exercised mapping and preservation fixtures.

## 2. Three distinct structures

| Structure | Meaning | Constraints and authority |
|---|---|---|
| Record graph | Attributed representations and relationships, including relations whose participants are other relations | Cycles, parallel relations, repeated participants and several roles are possible; no Git DAG constraint is imposed |
| Revision history | Accepted changes to a project's stored representations and material | The initial Git binding publishes one parented project commit per accepted transaction on `main`; parentage is recording provenance, not causality or occurrence chronology |
| Display projection | A selected rendering of a particular project revision | Layout, zoom and selection do not change the records; filters and unsupported material remain visible limitations |

A record identifier identifies a representation retained by this system. It does not establish the identity of the represented target. An attachment digest identifies bytes, not their independence, truth or evidential sufficiency. A transaction is a storage operation, not automatically a recorded Event or a substantive GRRP transition.

## 3. Record envelope and overlapping roles

The general profile retains the current descriptive fields so existing accounts remain legible: title, content, primary knowledge basis, modality, owner-assigned status, attributed source, method, evidence text, uncertainty, alternatives, conditions, consequences and described time. Server-assigned IDs, creator/updater and recording timestamps retain their distinct meanings.

`record_type` remains the primary presentation role. `record_roles` contains one or more of Entity, State, Event, Process, Relation and Property and includes that primary role. Selecting multiple roles does not create additional targets or occurrences. The model does not introduce an ontology class for every user-interface module.

`related_records` remains a set of neutral navigation references. Its preservation is especially important during migration: an earlier untyped link cannot become a causal, evidential or social relation merely because the new interface draws edges.

Records declare `schema_version: gsp-workspace/0.2` and carry a namespaced `modules` mapping. Core descriptions and modules belong to one record revision; a module is not an independently trusted authority. The existing single primary epistemic mode remains limited, with mixed grounds stated in method and qualification text. A richer epistemic representation remains a separate work item.

## 4. Relations as records

A structured Relation is an ordinary record whose roles include Relation and whose `gsp.relation` module has version `1`. It can carry the same notes, files, attribution and history as other records. The module has:

- `predicate`: an attributed local relationship label; optional `predicate_definition` states its intended meaning;
- `participants`: one to 32 incidence entries, each with its own UUID `id`, `record_id`, descriptive `role`, `reference_scope` and `orientation`;
- `participants_complete`: an assertion about the listed scope, with `participant_limitations` required when false;
- `context`, `identity_criterion` and `temporal_scope`: descriptive text; empty text means not recorded, not false, timeless or resolved.

`reference_scope` is `represented_target` or `record`: relating two organizations differs from relating two records about them. `orientation` is `in`, `out` or `undirected` and controls the declared incidence orientation; it does not by itself mean cause, influence, superiority or evidential strength. Role and predicate interpretation remain inspectable.

Repeated references to one record are allowed as different incidences; incidence UUIDs distinguish them. Unary, self-referential, cyclic, parallel and higher-order relations are permitted. Zero listed incidences are excluded as an explicit first-profile limit. Unknown participants are expressed through incomplete-incidence qualifications; the software does not create fictional participant records to complete a diagram. The first binding permits participant references within one project only. Cross-project protected references need a further design.

No uniqueness constraint on predicate plus endpoints is imposed: two agreements between the same parties can be different historical relations. An existing unstructured Relation record remains valid as an unstructured account and is identified accordingly.

## 5. Modules and authority boundaries

The first registry implements three persisted record modules:

| Module | Meaning | Editing behavior |
|---|---|---|
| `gsp.relation`, version `1` | Structured relation account | Available when Relation is among the record roles; complete module replacement validates participant references against the resulting snapshot |
| `gsp.notes`, version `1` | Retained supplementary notes, `data.text` | Explicit save with a revision reason; not a private scratchpad or independent assessment |
| `gsp.files`, version `1` | Retained material descriptors, `data.items` | Changes only through file operations so descriptors and bytes cannot diverge |

Module envelopes contain `version`, `required` and object `data`. An unfamiliar optional module remains intact during unrelated edits and is displayed as unsupported, read-only data. An unfamiliar required module prevents project mutation because its necessary constraints cannot be evaluated. Known modules at unsupported versions are also not silently normalized. No stored module installs code, runs scripts or selects arbitrary server paths.

An enabled empty files/notes module is distinguishable from an absent module. Module removal is an explicit transaction; removal of a files module with linked material is refused until the links are explicitly detached. Relation-module removal does not erase prior descriptions or the relation record's identity.

Graph layout is local view state, keyed by application, account and project. No credentials or record text are stored there. Record attachments differ from academia's mutable scratch workspace: saving an attachment deliberately retains its bytes in project history. Scratch folders, scheduling and domain-specific module registries can be added later with their own persistence and disclosure boundaries.

## 6. Shared package and application separation

An independent Python package under `applicative_infrastructure/common/packages/gsp_record_protocol/` provides constants, validation, schema resources, graph projection, transaction preparation and a command-line snapshot validator. It does not import Flask, SQLite or Git. JSON schemas and deterministic fixtures make the contract inspectable without a particular web client. ADR-0009 relocates the original root package and extracts the Git store into the adjacent `gsp_git_store` package without changing their record semantics.

The generalized application supplies authentication, authorization, server attribution, the Git adapter and its own interface. The academia application can later consume the same storage and exchange concepts through a profile adapter while preserving its original native envelope and signatures. UUIDs in the general workspace are not silently substituted for GRRP content identifiers. Shared infrastructure does not imply equal semantic types, equal mutation policy, or shared accounts.

Separate applications retain separate entry points, data roots and session namespaces. Different ports alone do not isolate browser cookies, so an eventual combined launcher needs explicit cookie naming and origin rules as well as port selection.

## 7. Transaction envelope

The experimental binding uses `POST /api/projects/{id}/transactions`. The logical request contains:

```json
{
  "protocol_version": "gsp-record-protocol/0.2",
  "transaction_id": "client-generated UUID",
  "expected_head": "full project commit identifier",
  "reason": "Why these representations or materials are being changed",
  "change_categories": ["description"],
  "operations": []
}
```

Categories are `description`, `represented_change`, `evidence_or_interpretation`, `classification`, `maintenance` and `migration`. They describe the recorder's account of the change, not a verified event classification. Multiple categories can apply. `migration` is reserved to the migration operation.

Supported operations:

| Operation | Input and effect |
|---|---|
| `record.create` | A caller-chosen record UUID and editable record fields; server adds recording attribution and time |
| `record.update` | `record_id` and partial `changes`; omitted fields/modules survive unchanged |
| `module.set` | `record_id`, `module_id`, complete module envelope; files content cannot be forged through this route |
| `module.remove` | `record_id`, `module_id`; explicit guarded removal |
| `file.attach` | New `file_id`, owning `record_id`, named binary `part`, filename, declared media type, SHA-256 and byte length |
| `file.replace` | Existing file descriptor ID with new material and metadata, preserving the descriptor's identity and previous historical versions |
| `file.detach` | Remove the current descriptor link; no claim of physical erasure |
| `project.migrate` | Sole operation of a legacy-project migration; explicit mapping and limitations recorded |

One request can create several records and a relation among them. Validation examines the final candidate snapshot, so operation order does not manufacture missing participants. Repeated core mutations of one record or repeated mutations of one module/file descriptor are rejected rather than interpreted as last-write-wins. Creating a record and subsequently adding a distinct module or attachment is permitted.

Ordinary commands use JSON. File commands use multipart with one textual `transaction` JSON part and declared binary part names. Duplicate JSON members, duplicate multipart names, extra or missing binary parts, nonfinite numbers, malformed Unicode, unrecognized operation fields and invalid declared hashes are refused.

## 8. Idempotency and publication

The server authorizes the project before inspecting any receipt. It computes a request digest using RFC 8785 canonical JSON over the logical envelope and verified file identities, excluding multipart boundaries and server-generated attribution/time. A vetted canonicalization library implements the encoding; ordinary Python JSON sorting is not presented as JCS.

The receipt lives in the same tree as accepted changes at `.gsp/transactions/{transaction_id}.json`. It records protocol version, transaction ID, original expected head, authenticated actor, server recording time, reason, change categories, request digest, changed-resource summaries and validation limits. The resulting commit identifier is not embedded in its own tree; the adapter obtains it from the receipt path's introducing commit.

The adapter checks a committed receipt before rejecting an old expected head. Identical actor/request replay returns the original result and `replayed: true`, even after other changes advance the project. Reusing an ID for a different logical request is a conflict. An identical concurrent request rechecks the receipt after a failed publication attempt. A revised request after reconciliation receives a new transaction ID.

After validation, the adapter writes blobs and complete changed subtrees, creates one parented commit, and conditionally updates `refs/heads/main`. The single reference is the publication boundary. Failure before publication leaves no partial accepted graph. Unreachable prepared objects may remain and are not accepted revisions. An uncertain response near publication is recovered through the transaction receipt, not by claiming that no commit occurred.

## 9. File custody and budgets

Files are stored as Git blobs at `assets/{sha256}`. Filenames are descriptive metadata and never filesystem paths. A descriptor retains `file_id`, filename, media type, SHA-256, byte length, upload actor and upload time. New bytes and the owning record's descriptor publish together. Replacements and detached links remain reconstructable through older commits.

The initial binding limits a file to 10 MiB, combined binary parts to 20 MiB per transaction, binary parts to eight, operations to 100, participant incidences to 32, active records to 1,000, module payloads to bounded JSON and retained asset bytes reachable from the current project tree to 128 MiB. Asset entries are retained in the project tree for this first custody policy; physical unreachable objects and Git metadata are not covered by that logical byte budget. Operational filesystem quotas and governed garbage collection remain hosting work.

Uploads are spooled and hashed with bounded reads. Downloads and bundles use bounded streaming or temporary files. Arbitrary content is offered as an attachment with `nosniff`, not rendered as active HTML. A digest is not an authorization token: the requested record, descriptor and chosen project revision determine access.

Preservation of a file does not make it assessed evidence. Identical bytes may have different acquisition histories; distinct descriptor IDs preserve those accounts while storage can reuse a blob. No antivirus, media interpretation, legal hold, selective erasure or remote object-store guarantee is implied.

## 10. Snapshot-consistent reads and exports

`GET /api/projects/{id}?revision={commit}` resolves one authorized reachable project revision and reads all records from that tree. A graph response and its inspector use the same revision. Participant labels, related-record labels, relation structure, notes and downloads do not silently resolve through today's head when viewing history. Historical screens are read-only; returning to the current project is explicit.

The graph projection exposes every retained record as a selectable node. Structured relations appear as relation nodes with incidence spokes; neutral references have a distinct dashed style. Predicate/role labels and reference scope remain inspectable. A selected relation has the same module panel as another record. A future collapsed binary view is optional and cannot discard relation identity, parallel edges or higher arity.

Filtering discloses that it is a selected view and reports hidden nodes/incidences. Layout and distance are not scores, time or causality. A list/form alternative provides creation, selection and connection without requiring precise spatial gestures. The server supplies warnings for unsupported/unstructured modules rather than silently presenting a complete semantic graph.

Current snapshot JSON identifies its source revision and excludes file bytes. A current snapshot ZIP includes only selected current records and their referenced material plus an export manifest. It omits the accumulating receipt ledger and detached assets; otherwise a supposedly current-only export would leak historical reasons and content. A full Git bundle deliberately carries all reachable history, receipts and retained assets. Neither export contains account credentials or restores ownership by itself.

Batch Git object reads replace one-process-per-record loading. Any derived cache is keyed by exact commit and is disposable; it cannot become a competing source of record truth.

## 11. Explicit legacy migration

Reads never migrate a project. Old commits are interpreted through a declared read adaptation: the primary role becomes the sole displayed role, missing modules are absent, and old navigation references remain neutral. IDs, original bytes and historical Git commits are retained.

An owner can preview and request migration of the current project through a dedicated transaction. The new commit upgrades project/record schema declarations, adds role arrays and module containers, and records the source revision and mapping limitations. It does not infer relation predicates, participant roles, new epistemic grounds or nonexistent attachment bytes. Existing unstructured Relation accounts remain unstructured.

New projects start with the new protocol. Compatibility record endpoints can translate their existing bounded commands into transactions while preserving extension modules; they do not bypass validation or promise the full retry contract of a caller-supplied transaction ID.

## 12. Academia migration boundary

The later academia upgrade begins with fixtures preserving original GRRP bytes, identifiers, signed-covered fields, registrations, disclosure sidecars and referenced material. The current manuscripts govern new conceptual choices, but old records are not rewritten to pretend they always used the new model. A correspondence explicitly distinguishes exact preservation, partial mapping and unresolved differences.

The reference review identifies concrete repair work: bind web sessions to acting identities; preserve absent references; validate imports for path, collision, size and partial failure; clarify mutable workspace versus retained material; separate administrative operations from substantive graph projections. These are queued migration concerns, not changes made to the academia application by this delivery.

## 13. Sequence and acceptance evidence

1. Complete source/design comparison and record architectural decisions.
2. Write experimental GR-SPEC-120/130 details and executable schema/fixture contracts. Develop bounded implementation-neutral requirements in the currently preliminary GR-120/130/140/160 projects where needed; leave adequate 0.3 clauses intact.
3. Implement and test the independent validator/projector, then atomic Git binding and HTTP adaptation.
4. Implement graph creation/connection, conditional modules, historical navigation and migration UI.
5. Exercise failure cases and browser journeys, inspect desktop/mobile views, document limits and restart the local application only after tests pass.

Required evidence includes: atomic node-plus-relation-plus-file creation; rollback on a bad final operation; same-batch cyclic/higher-order participants; parallel and unary relations; role overlap; invalid/cross-project references; identical/different replay across restart and advancing heads; racing writers; missing, extra, corrupt and oversized file parts; historic file replacement/detachment; legacy migration without invented semantics; opaque module preservation; current export selection; full bundle reconstruction; pinned historical graph/inspector; visual and nonspatial connection creation; draft preservation after conflict; and no semantic commit for node dragging.

No numerical test count substitutes for inspecting these distinctions. Independent academia interoperability, public deployment, institutional contestation and full GR conformance remain outside this release's evidence.
