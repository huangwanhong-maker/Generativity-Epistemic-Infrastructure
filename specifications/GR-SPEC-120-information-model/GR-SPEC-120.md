# GR-SPEC-120 — Experimental record graph and module model

**Publication class:** Specification  
**Maturity:** EXPERIMENTAL implementation draft; not an adopted Standard  
**Version:** 0.2  
**Date:** 2026-09-22  
**Protocol identifier:** `gsp-record-protocol/0.2`  
**Record schema:** `gsp-workspace/0.2`  
**Initial profile:** `gsp.general/0.2`

## Foreword

This Specification supplies a bounded implementation model requested by the graph workspace. Its source interpretation is recorded in the [graph semantics review](../../docs/planning/graph_semantics_review.md), and its architectural selection in [ADR-0008](../../decisions/ADR-0008-record-graphs-transactions-and-modules.md). This document operationalizes selected needs from Working Drafts; it neither adopts those drafts nor claims that every package using this model satisfies them.

The reference implementation is the independent [Python protocol package](../../applicative_infrastructure/common/packages/gsp_record_protocol/README.md). Its [JSON Schema resource](../../applicative_infrastructure/common/packages/gsp_record_protocol/src/gsp_protocol/schemas/protocol.schema.json), [fixtures](../../applicative_infrastructure/common/packages/gsp_record_protocol/tests/fixtures/graph_snapshot.json), and semantic validator jointly describe the tested profile. The Git transaction and exchange binding belongs to GR-SPEC-130.

## 1. Scope

This document defines retained record envelopes, overlapping operational roles, relation incidence, versioned modules, material descriptors, snapshot projection, and migration from the initial workspace. It distinguishes the record graph from recording history and displayed layout.

The initial profile has one private-project reference boundary. It does not define a complete theory of reality, exhaustive target registry, complete claim/material/evidence-role model, substantive assessment method, institutional review procedure, cross-project protection system, or universal interpretation of relations. A record can retain an attributed account while the represented occurrence or identity remains disputed.

Requirements in this document constrain implementations claiming this experimental profile. **Shall** indicates a requirement, **should** a recommendation, **may** a permission, and **can** a capability. Normative identifiers `S120-*` are local to this draft and do not imply adoption.

## 2. References and dependency scope

Conceptual source references are [GR-100 Working Draft 0.3](../../standards/GR-100-foundations-vocabulary/GR-100.tex), especially V100-002/005/006/007/009/010/025; [GR-110 Working Draft 0.3](../../standards/GR-110-recording-requirements/GR-110.tex), especially R110-006/009/011/012/020/021/025/029/093; and [GR-200 Working Draft 0.3](../../standards/GR-200-epistemic-recording/GR-200.tex), especially E200-003/005/008/010/011/017/038. These references identify the needs being operationalized, not an automatic claim that implementing this Specification fulfills their complete obligation sets.

FM-007, sections 7–10 and 16.4, supplies conceptual background for identity, contextual incidence, overlapping analytical roles, and record/claim distinctions. Its Attribute category is not silently equated with the programme's provisional Property role.

The distributed JSON schema uses the JSON Schema Draft 2020-12 vocabulary. Request canonicalization uses RFC 8785 through the `rfc8785` library; ordinary sorted JSON encoding is not described as canonicalization under that RFC. External protocol details and source URLs are maintained in the binding's references.

## 3. Terms and structures

A **record** is an identified retained representation under a declared recording purpose. A **target** is what that representation concerns. A **snapshot** is the project manifest and record collection at an identified retained project revision. A **module** is a namespaced, versioned component of a record. A **participant incidence** identifies one role in one relation account. A **projection** is a declared rendering of a snapshot.

**S120-001.** An implementation shall distinguish record reference, represented-target identity, semantic connection, recording ancestry, and display position. It shall not infer factual truth, causation, legal authority, or temporal priority solely from any of those technical structures.

**S120-002.** A snapshot shall identify its project, source `head`, and record collection. Record identifiers shall be unique within that collection. A reference resolved under this profile shall identify a record present in that same snapshot.

The package accepts full 40- or 64-character lowercase hexadecimal revision identifiers. A particular storage binding declares its object format and accepted width. This is a locator format, not a time or authenticity guarantee.

**S120-028.** A declared project profile that the implementation does not support shall remain preserved and identified as an interpretation limitation during reads. Mutation, including migration, shall require a supported adapter rather than silently replacing that declaration or ignoring its additional constraints. Absence of a profile in the original legacy workspace shall not itself prevent the explicit legacy migration defined in Clause 10.

## 4. Record envelope

**S120-003.** A current record shall declare `schema_version: gsp-workspace/0.2`, a canonical lowercase UUID `id`, `title`, `content`, `record_type`, `record_roles`, `epistemic_mode`, `modality`, `status`, `related_records`, `modules`, and recording audit information. Creation and update actor objects shall contain `id` and `display_name`; corresponding recording times shall include an offset.

Additional descriptive fields are `attributed_to`, `method`, `evidence`, `uncertainty`, `alternatives`, `conditions`, `consequences`, and `occurred_at`. The schema and package expose their length limits. `occurred_at` is a supplied target-time description, not the server recording timestamp. Empty descriptive text means that the information was not recorded in that field; it does not establish absence, certainty, timelessness or fulfilled criteria.

**S120-004.** `record_type` shall identify one primary presentation role from Entity, State, Event, Process, Relation, and Property. `record_roles` shall contain one or more distinct roles from that vocabulary and include the primary role. Multiple roles shall not automatically create multiple represented targets.

**S120-005.** The implementation shall preserve the distinction among epistemic basis, asserted modality, and recorder-assigned disposition. The initial vocabularies are:

| Field | Values | Limit |
|---|---|---|
| `epistemic_mode` | observed, reported, inferred, interpreted, retrospective | Primary account mode; combined contributions remain in descriptive grounds. |
| `modality` | realized, intended, possible, unrealized, unknown | A represented occurrence/effect qualification; not an assessment result. |
| `status` | unreviewed, contested, revised, withdrawn | Recorder-assigned account status; not an independent institutional finding. |

**S120-006.** Creation and update audit values shall be supplied by the trusted application binding. A record mutation request shall not inject or overwrite creation audit identity/time. The authenticated recording account shall remain distinguishable from the attributed source described in the account.

## 5. Structured Relations

**S120-007.** A structured Relation shall be an ordinary independently referenceable record whose applied roles include Relation and whose `gsp.relation` module has supported version `1`. Its module data shall identify `predicate`, `participants`, and explicit boolean `participants_complete`. Optional descriptive fields are `predicate_definition`, `participant_limitations`, `context`, `identity_criterion`, and `temporal_scope`.

**S120-008.** Each participant incidence shall have its own UUID `id`, a local `record_id`, a nonempty descriptive `role`, `reference_scope`, and `orientation`. Incidence identifiers shall be distinct within one relation account. The same record may occupy multiple incidences, including different roles. A Relation may itself be a participant.

`reference_scope` is either `record` or `represented_target`. The former concerns the referenced retained representation; the latter concerns its described subject under the account's stated identification and limitations. Neither establishes that subject's identity or existence.

`orientation` is `in`, `out`, or `undirected`. In the canonical incidence projection, `in` points from participant to Relation, and `out` points from Relation to participant. `undirected` asserts no arrow direction. Orientation alone has no universal causal, temporal, evidential, or hierarchical meaning.

**S120-009.** The profile shall permit one to 32 listed incidences, including unary, cyclic, self-referential, parallel and higher-order relation structures. It shall not deduplicate Relation records by predicate and participant pattern. An empty incidence list lies outside this initial profile; this limit is not a claim about every mathematical or social relation.

**S120-010.** If `participants_complete` is false, the module shall contain a nonempty `participant_limitations` explanation. The listed local references can represent a known subset. The implementation shall not fabricate participant records to complete an uncertain account. An empty or unresolved identity criterion shall not be presented as fulfillment of a criterion-dependent GR obligation.

**S120-011.** An unstructured record with a Relation role shall remain representable and selectable. The projection shall identify the absence of interpreted incidence rather than infer participants from neutral references or prose.

## 6. Neutral references and modules

**S120-012.** `related_records` shall remain a set of up to 50 distinct local record identifiers used for neutral navigation. Migration or graph rendering shall not silently convert these references into typed substantive relations.

**S120-013.** Module keys shall be namespaced identifiers. Each envelope shall contain string `version`, boolean `required` when supplied, and object `data`. Omission of `required` means false. This default does not authorize rewriting an opaque historical envelope merely to insert the default.

**S120-014.** Optional unsupported modules or versions shall survive unrelated edits unchanged and remain identifiable as unsupported, read-only material. A required unsupported module shall block project mutation by an implementation unable to evaluate its constraints. Unsupported data shall not cause dynamic code installation or execution.

The current package only introduces or edits the three supported version `1` modules. A future domain registry needs its own implementation and compatibility evidence; this Specification does not pretend that unknown domain semantics have been checked.

**S120-015.** `gsp.notes` data shall contain `text`. `gsp.files` data shall contain `items`, an array of retained material descriptors. Module absence shall remain distinguishable from an explicitly enabled empty module.

**S120-016.** Module mutation shall use explicit module operations. Core partial updates shall preserve omitted fields and modules. Removing a files module with linked items shall be refused until those links have been detached. Removing a current module shall not claim erasure of its retained historical versions.

## 7. Retained material descriptors

**S120-017.** A file descriptor shall contain `file_id`, `filename`, `media_type`, `sha256`, integer `byte_length`, `uploaded_by`, and `uploaded_at`. File identifiers shall distinguish descriptors within their owning record. Replacing material shall preserve the descriptor identity while retaining earlier versions through the binding's history.

**S120-018.** Filenames shall be display metadata rather than storage paths. A binding shall refuse path separators and control characters in these names and shall protect download headers and rendering independently. SHA-256 shall identify bytes; it shall not establish acquisition independence, evidence acceptance, factual truth, or permission to disclose them.

**S120-019.** File descriptors shall be created or changed through binding-verified file operations. A generic module update may enable an empty files module but shall not forge linked descriptors or clear existing file links. Detachment shall mean removal of the current link, without asserting historical byte erasure.

The pure package checks the declared descriptor against the metadata supplied by its caller. The binding separately verifies actual received bytes, exact part inventory, authorized source revision, stored assets, and atomic descriptor/byte publication. Consequently, a structurally valid snapshot with an unresolved or absent asset cannot be described as a verified complete archive solely because this package accepted its JSON.

## 8. Transaction preparation boundary

**S120-020.** The preparer shall validate the final candidate snapshot, including references to records created in the same transaction. Semantic cycles shall not be rejected because the underlying storage history has a different structure. Failure shall leave the supplied source snapshot unchanged.

**S120-021.** A transaction shall not contain repeated core mutations of one record or repeated mutations of one module or file descriptor. Creation followed by a distinct module or file operation is permitted. A module supplied within record creation cannot also be overwritten in that transaction. The implementation shall reject ambiguous repeated writes rather than silently selecting the last value.

The full transaction envelope, operation shapes, receipt, digest, replay and conditional-publication contract is specified by the binding. The reusable package prepares a candidate and receipt; it does not itself acquire locks, authorize a user, publish a Git reference, or verify that a retry corresponds to a prior committed receipt.

## 9. Projection and historical interpretation

**S120-022.** The canonical projection `gsp.incidence/1` shall expose every retained record as a selectable node, one incidence edge per interpreted participant, and distinct neutral-reference edges. Each incidence shall retain its relation identifier, participant identifier, role, scope and orientation. The projection shall identify its source `head` and disclose unstructured or unsupported material through warnings.

**S120-023.** Historical graph labels, relation structure and module data shall be derived from the same identified snapshot. A binding shall resolve historical material downloads under that snapshot and the applicable authorization. Substitution of current data into a historical presentation shall not be silent.

**S120-024.** Filtering or simplified rendering shall disclose its selection and information limits. Layout coordinates, proximity, centrality and creation order shall not automatically become substantive properties or causal claims. A future binary simplification shall preserve the underlying Relation identity and provide a route to its full participant account.

## 10. Legacy migration

**S120-025.** Reading `gsp-workspace/0.1` shall not perform a stored migration. A declared view adaptation may display the legacy primary role as a singleton role set and absent modules as empty. It shall retain the source schema declaration and preserve legacy references as neutral.

**S120-026.** Migration shall be an explicit sole `project.migrate` transaction under the `migration` change category. The new snapshot shall identify the source revision, prior schema, migration transaction, accountable actor/time and mapping limitations. Record IDs, account content and original recording attribution/time shall be retained. No relation predicate, participant role, evidential ground or unavailable file byte shall be invented.

## 11. Validation evidence and bounds

**S120-027.** An implementation shall distinguish structural validation, scoped semantic validation, byte/custody checks, publication guarantees and substantive assessment. A validation report shall state the applicable profile and material limitations.

The package implements JSON Schema Draft 2020-12 checks plus cross-record and module semantics. JSON values use finite RFC 8785-compatible numbers, valid Unicode and bounded nesting. Canonicalization rejects unsafe integers, nonfinite numbers and malformed surrogate text. The binding is responsible for rejecting duplicate JSON member names before an ordinary dictionary representation loses that distinction.

The reusable profile limits requests to 100 operations, projects to 1,000 records, relations to 32 incidences, and file transactions to eight parts, 10 MiB each and 20 MiB combined. Module JSON is bounded to 256 KiB, transaction JSON to 2 MiB, and snapshot JSON to 64 MiB. The initial HTTP/Git binding declares tighter JSON limits: 1 MiB transaction JSON and 32 MiB aggregate snapshot JSON. These limits are operational capacity decisions.

The executable acceptance cases include semantic cycles and higher-order relations; n-ary, parallel, unary and self relations; record/target scope; repeated roles; mode/modality/dispute coexistence; final-candidate reference checks; source immutability on failure; opaque module preservation; unsupported-profile write refusal; explicit migration; exact file-part declarations; descriptor replacement/detachment; digest invariance under member ordering; and historical label isolation. They establish bounded software behavior, not independent application interoperability or whole-GR conformance.

## 12. Unresolved extensions

**OPEN:** Separate claim, material, evidence-role, assessment and competing-interpretation objects; richer typed missingness; explicit role admissibility evidence; domain relation vocabularies; multiple descriptive-regime mappings; protected cross-project references; independently implemented academia mappings; governed retention and selective erasure.

**DEFERRED:** Semantic branch/merge resolution, externally stored large assets, executable third-party modules and automatic assessment. Availability of a generic graph or a Git DAG does not settle those matters.
