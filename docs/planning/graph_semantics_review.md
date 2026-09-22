# Record graph semantics: source review and design proposal

**Document class:** Informative design review  
**Status:** PROVISIONAL proposal; implementation decisions require a separate ADR  
**Date:** 2026-09-22  
**Purpose:** Identify a bounded graph model for the general application and a future shared implementation substrate for the academia application.

This review is grounded in the [Programme Brief](../../PROGRAMME_BRIEF.md), the current experimental [application design](../../applicative_infrastructure/gr_generalized_application/web_application/DESIGN.md), the 0.3 Working Drafts, and passages read directly from FM-007. It is not a Specification, an adopted Standard, or a conformance assessment. The recommendations below are proposed operational choices. They do not redefine GR theory.

## 1. The central distinction

A project has at least three distinguishable structures:

1. **The semantic record graph:** records and the connections represented or asserted through them, at one declared project revision.
2. **The recording history:** Git commits that retain successive project snapshots and accountable changes.
3. **The displayed graph:** a selected rendering of a snapshot, with layout, filtering, and optional simplification.

A semantic cycle is compatible with a linear recording history. Two records can describe an earlier and a later occurrence in the opposite order from their creation. A new commit can change several graph nodes, a relation, and its associated files atomically. Graph layout or closeness does not assert identity, causal influence, temporal priority, or evidential support.

The Git commit identifies the project snapshot being examined. It is not the identifier of the real situation, the time that situation occurred, or proof of its represented content. The application currently publishes a single main reference. That operational choice need not prohibit recording alternative accounts or semantic branches within a snapshot.

## 2. Source findings

Page numbers below are PDF page numbers in [FM-007](<../../foundation_manuscripts/A Metamodel for Ontologically Heterogeneous Social Dynamics-II.pdf>).

| Source passage read | Finding relevant to this design | Proposed consequence |
|---|---|---|
| Research Paper Note, p. 2; section 10.5, p. 46 | The six categories form an overlapping analytical repertoire; exclusivity, exhaustiveness, and metaphysical priority are not established. | Record roles can overlap. An application can choose a primary display role without claiming that other roles are impossible. |
| Sections 7.1–7.3, pp. 26–28 | A focal subject can be tracked under different declared criteria. A convenient database key settles only the identity question built into that database. | Separate record identity from target identification; preserve disputed identity and competing accounts. |
| Sections 8.1–8.2, pp. 32–33 | Incidence describes participants or conditions with role positions. Binary incidence is a special case; formation and potential effect need further structures. | Represent a relation through a role-indexed participant list, not only source and destination columns. |
| Sections 8.3 and 8.8, pp. 33–34 and 36 | Equal participants or types do not establish the same historical relation. Arity and terms can change under a continuity criterion. | Give each relation record its own identity and revision history. Do not deduplicate relations by endpoint pair and label. |
| Sections 8.5–8.9, pp. 34–36 | Present incidence, formation history, possible efficacy, and causal interpretation answer different questions. | A saved connection is an attributed account. Add context and grounds rather than infer efficacy from the graph. |
| Section 8.11, p. 37 | Relation accounts coordinate entities, formation events, attributes, states, and process histories. | Permit records about relations and relations whose participants are other relations. |
| Sections 9.9–9.10, p. 43 | One happening can receive several descriptions; event/process segmentation depends on purpose and evidence. | Avoid automatic creation of six supposedly separate real things from one account. |
| Sections 10.1–10.5, pp. 43–46 | Attribute domains, context, scale, definedness, and overlap carry meaning; unknown is not automatically false. | Distinguish interface/module properties from substantive Property assertions; preserve missingness and scope. |
| Section 16.4, p. 76 | A registry entry, its asserted content, and warrant remain distinguishable. | Authenticated edits and valid Git content do not establish factual truth or institutional authority. |

FM-007 uses **Attribute** where this programme provisionally retains **Property**. This review does not equate them. Nor does it identify the record graph with the deeper metamodel of constraint, change, structure, and difference.

## 3. Proposed minimum model

### 3.1 Records and subjects

A record is an identified retained representation. It contains a description of its target, applied recording roles, attributed content, qualifications, and technical recording metadata. Its stable identifier survives ordinary revisions. Different records can concern the same or a possibly identical target without being merged automatically.

The model can retain a primary role for color, icon, and default module selection while also recording additional applicable roles. Relation behavior depends on the presence of the Relation role, not on its being the sole role. The user can describe an agreement as both Relation and Entity. Its negotiation, signing, and time-specific terms can be separately linked descriptions where that distinction is useful.

A target designation can remain an unresolved description. The design does not need a universal target registry in the first graph release. It does need to state whether a participant reference concerns the **referenced record** or **the target described by that record**. Otherwise a link about a document's quotation can be confused with a claim about the document's subject. A profile can provide a clearly labeled default, but the distinction needs inspectable representation.

### 3.2 Relation records and incidence

A relation record uses the ordinary record envelope plus a relation component containing:

- a relation type or predicate, its local definition or vocabulary locator, and a human-readable title;
- participants, each with a stable participant-entry identifier, participant role, record reference or explicit unresolved designation, and reference interpretation;
- the stated context and represented temporal scope;
- the criterion for treating this as the identified arrangement, including whether continuity remains unresolved;
- the ordinary record's attribution, epistemic basis, modality, assessment/dispute status, grounds, and uncertainty.

Participant-entry identifiers distinguish two roles occupied by the same referenced record. For example, one organization could be both issuer and recipient under a stated arrangement. A duplicate exact participant entry is a validation error; repeated record references with different roles need not be. Self-reference or relations on relations are not universally invalid. Their admissibility depends on the declared relation meaning rather than a graph-wide acyclicity rule.

Direction is defined by participant roles or a declared relation type. An arrow is a rendering of that direction. It is not a universal meaning of Relation. The initial model can support directed binary relations, symmetric binary relations, and n-ary arrangements through the same participant structure. At least one participant incidence is a reasonable experimental profile boundary; there is no source basis for requiring two distinct target records in every admissible relation. A two-entry relation can refer to one record in distinct roles. Excluding an empty participant list is an implementation boundary rather than a universal theory of mathematical relations.

For an unknown party, an explicit unresolved participant preserves the limitation more accurately than a fabricated Entity. A narrower first implementation can accept a known subset of resolvable local participant references while recording that incidence is incomplete and explaining the limitation. It must not present that subset as a complete account. If the first implementation restricts the available forms, the restriction needs to be documented as a profile limit rather than generalized into GR. An unresolved identity criterion can be recorded for exploratory work, but it cannot receive a fulfilled result for a criterion-dependent obligation under V100-025.

An initial same-project reference boundary is appropriate for the application's private projects. Cross-project participant references require a later access and resolution design. An unresolved outside participant description is different from a resolvable cross-project record link.

### 3.3 Navigation links and other meanings

The existing `related_records` list is an untyped navigation aid. Migration can preserve it unchanged and render it with a visibly neutral style. It supplies no basis for creating a claim of collaboration, identity, causation, or evidential support. A user can explicitly create a Relation record with its own meaning and grounds.

An evidence file is not automatically accepted support. A structured evidence-role assignment needs a specified claim, material/version, attributing assessor, role, rationale, time, and limitations. If the current application has only account-level evidence text, the interface can expose that limit and preserve file references without inventing independent claim-level assessments.

Similarly, revision ancestry is technical history. A semantic relation such as "derived from," "revises an interpretation of," or "possibly the same target" requires separately stated meaning. Two records created in consecutive commits are not thereby connected semantically.

## 4. Projection and interaction

The most faithful general graph is an **incidence view**: every retained record is selectable; a Relation record has its own node; labeled spokes connect it to participant records. This naturally supports n-ary relations, relation properties, and relations on relations.

A simplified binary view can collapse an eligible two-participant Relation node into one labeled connection. That connection retains the relation record identifier and opens the same inspector. Collapsing is inappropriate where it would hide a relation that is itself a participant of another relation, hide additional participants, or merge separately identified parallel relations. The interface can allow the user to expand a simplified connection.

Creating a connection visually opens a small relation editor prefilled with the chosen participants. Saving creates a first-class Relation record. A form/list alternative supports keyboard, touch, and precise selection. Dragging nodes changes layout; drawing a connection changes represented content only after the user supplies its meaning and confirms the save.

Every graph request and inspector response needs one source snapshot. Opening a historical graph resolves participant names, roles, relation content, module data, and files from that same snapshot. The interface identifies filters and hidden records, so absence from the screen does not imply absence from the project or non-occurrence in the world. Selecting a historical version makes editing unavailable until the user returns to the current project or explicitly starts a supported revision operation.

Saved coordinates, viewport, collapsed state, and display preferences are presentation data. They are not target-time, epistemic status, or ontological properties. Shared versioned layouts can be stored separately from record content; transient panning need not generate a substantive history entry.

## 5. Modules and file custody

Modules are versioned record components: for example attachments, notes, a temporal account, relation participants, or a domain-specific academic annotation. A module descriptor identifies its type and version. An application registry determines rendering and validation. The common protocol can preserve extension data without claiming to understand its domain meaning. Unknown module types remain visible as unsupported data and are not discarded during an unrelated edit.

Choosing a recording role can suggest modules. It does not make those modules metaphysical constituents of the target. Attaching a PDF to a Relation record is storage associated with that representation; it does not turn the PDF into a participant or prove the relation. A richer material record can later describe source, acquisition, custody, transformations, and evidence use.

For the initial bounded local implementation, keeping immutable file bytes within the project Git object store offers a straightforward historical model. A file descriptor can retain a content digest, byte count, original display name, declared media type, creation/acquisition context where supplied, and the responsible uploader. Browser downloads use authenticated application routes; untrusted filenames are never repository paths or executable page content.

The descriptor and bytes need atomic publication with the associated record change. A historical download resolves the attachment as it existed in the requested reachable project commit. Removing a current attachment link does not claim erasure from retained commits or exported bundles. A filename match does not establish that two files are identical; identical bytes do not establish independent sources or identical acquisition histories.

Large-file storage, selective erasure, retention schedules, malware scanning, protected source identities, and external repository references require additional design. The first version can declare conservative file limits and safe download behavior without claiming those deferred features.

## 6. Commit protocol implications

The existing compare-and-swap publication of a new Git head is a useful primitive. The graph release needs an atomic **project transaction** above it:

1. Identify the expected project revision and submitted operation/revision reason.
2. Build the proposed next records, module descriptors, and file entries without publishing them.
3. Validate references and constraints against the complete proposed snapshot, including records created in that same transaction.
4. Retain transaction metadata identifying the protocol version, accountable actor, changed objects, reason, and applicable change categories.
5. Create one complete commit and publish it only if the expected parent still matches.

The transaction permits creating a new node and its connecting Relation together. Mutual references are validated against the final snapshot rather than an arbitrary write order. A rejected transaction exposes neither a partial node nor a partial relation. Preparing unreachable Git objects is different from accepting a project revision.

The transaction's recording timestamp and reason are distinct from target, observation, and assessment times. Change categories can distinguish a changed situation, new evidence or interpretation, descriptive-regime change, and technical/presentation maintenance. More than one can apply. A stale expected revision is reported as a conflict; a new graph preview is obtained before a reconsidered save. Automatically merging relation endpoints can change substantive meaning and needs a later explicit conflict policy.

Existing records and commits retain their original schema and meaning. Reading old snapshots can provide a documented view adaptation; migration of current content is a new traceable commit. No migration converts neutral navigation links into asserted social or causal relations, supplies invented role criteria, or rewrites historical records in place.

## 7. Discriminating acceptance cases

These are proposed software tests and review cases, not evidence of whole-draft conformance.

| Case | Expected behavior | Error exposed |
|---|---|---|
| Two agreements between the same two parties | Two independently selectable Relation records and histories survive export/reload. | Deduplication by endpoint/type silently erases historical tokens. |
| One agreement with three parties and distinct roles | One relation retains all participants; the inspector and graph reveal all roles. | Binary-edge conversion invents three independent agreements. |
| A relation constrains another relation | The second relation is an allowed participant; graph and export preserve it. | Relations are decorative edges with no independent reference. |
| An agreement described as both Entity and Relation | Both applied roles persist; relation features remain available. | Exclusive type selection makes role overlap an inconsistency. |
| A record cites a document about an organization | The relation identifies whether its participant is the document record or the described organization. | Record and referent are silently conflated. |
| A self-referential arrangement uses different roles | Participant entries remain distinct and named; a blanket cycle ban does not reject them. | Graph topology is mistaken for semantic admissibility. |
| An unknown participant in an asserted arrangement | An explicit unresolved designation appears without a fabricated identified person. | Structural completion is mistaken for factual knowledge. |
| A realized event is retrospectively inferred and disputed | Modality, epistemic basis, and dispute remain separately visible. | A single status field conflates three questions. |
| Two semantic records link both ways | Graph round-trips without a DAG constraint. | Git ancestry constraints leak into represented relations. |
| A new node and Relation are created together, then a concurrent writer wins | The losing save publishes neither partial object; the accepted snapshot remains valid. | Multi-object writes leave a dangling relation or overwrite a newer revision. |
| A participant is renamed after an older graph snapshot | The older graph retains the earlier label and relation version. | Historical content is rendered using present-day data. |
| A file is replaced, then removed from the current module | Earlier authorized snapshots download the earlier bytes; current removal is described as unlinking. | Historical attachments drift, or unlinking is misrepresented as erasure. |
| A current record with an unfamiliar module is edited in a familiar field | Unsupported module data survives unchanged and is identified to the user. | One application silently destroys another application's domain data. |
| Legacy `related_records` contains two records | Migration preserves a neutral cross-reference until a user supplies a typed relation. | Migration manufactures substantive claims. |
| The graph hides withdrawn records or an entire role | The filter and hidden-count boundary remain visible. | A reduced display appears to be the complete retained history. |
| A label says "supports" with an uploaded PDF but no assessment | The application does not declare evidential sufficiency, truth, or an examined claim. | Connection creation becomes an unearned assessment. |

## 8. Traceability to Working Drafts

These are design relationships, not fulfilled-requirement claims. The implementation still needs a declared profile, field validation, examples, and explicit remaining gaps.

| Draft locations | Design relationship |
|---|---|
| [GR-100](../../standards/GR-100-foundations-vocabulary/GR-100.tex), V100-002/005; section 3 definitions of Record, Target, Record reference | Record identifiers, described targets, and reference interpretations remain distinct. |
| GR-100, V100-006/007; [GR-110](../../standards/GR-110-recording-requirements/GR-110.tex), R110-008/020/021; [GR-200](../../standards/GR-200-epistemic-recording/GR-200.tex), E200-008/017/038 | Recording history, represented time, revision grounds, and alternative accounts remain distinguishable. |
| GR-100, V100-009/010/025 and section 5 Relation | Role overlap, independently referenceable relations, contextual participants, temporal meaning, and declared criteria. |
| GR-110, R110-006/007/009/048/049 | Stable record/version references; identity and continuity claims do not follow from keys, equal labels, or link patterns. |
| GR-110, R110-011/012/051/058; GR-200, E200-005/014 | A diagram edge does not establish generative attribution; each asserted connection retains relevant grounds and qualifications. |
| GR-200, E200-003/010/011/012/016/019 | Material, claim-relative evidence, dependence, assessment, and revision follow-up need more than a generic link or attachment. |
| GR-100, V100-011/012 | Graph structure is an operational representation, separate from the metamodel and from legal, normative, or institutional authority. |
| GR-110, R110-025/026/029/030/093 | Protection of content and metadata, explicit missingness, selected view boundaries, source revision, and refresh status. |

## 9. Gaps in the proposed family

The existing GR-100/110/200 clauses already establish the relevant distinctions. Graph implementation does not by itself require changing their 0.3 normative text. More concrete work belongs in the following currently preliminary project outlines and the implementation Specifications they motivate.

| Project | Current source state | Concrete requirements-development need exposed by this application | Technical detail for a supporting Specification |
|---|---|---|---|
| [GR-120](../../standards/GR-120-information-model/README.md) | Short implementation-neutral project outline | Define record, target, claim, version and participant-reference relationships; preservation of overlapping roles; first-class relations and partial incidence; identity, time, missingness and extension boundaries. | Exact JSON envelope; role and participant arrays; reference interpretation; namespaced module descriptors; validators and schema-version transitions. |
| [GR-130](../../standards/GR-130-interchange/README.md) | Short interchange outline listing candidate representations | Declare exchange unit and source revision; preserve reference scope and extension data; expose unresolved dependencies; specify import failure, partial receipt and unsupported-version behavior; maintain original meaning through migration. | Concrete wire format; protocol manifest; transaction operation schema; canonical encoding; Git tree paths, commit metadata, conditional publication, and binary representation. |
| [GR-140](../../standards/GR-140-presentation/README.md) | Short outline listing possible views | Identify a presentation's source version, transformation, selection and refresh status; retain relation identity in simplified graphs; reveal n-ary/parallel/recursive structures without invented implications; keep mode, modality and dispute readable; provide an alternative to spatial gestures. | Incidence/collapsed-view algorithms, projection identifiers, user-interface contracts, saved layout format, and snapshot navigation endpoints. |
| [GR-160](../../standards/GR-160-preservation-access/README.md) | Short outline of preservation and access subjects | Define retention and availability states for records, files, metadata, derived views and audit trails; distinguish unlinking, withdrawal, restriction and erasure; authorize historical access; define reconstruction and custody limits. | File descriptors and digest rules; authenticated historical download resolution; size/type handling; retention/export manifests and future external-object protocols. |

The standards-track drafts can state implementation-neutral expectations and assessment evidence while an experimental shared Specification implements the bounded profile. Neither needs to assert a comprehensive conformance result before pilot evidence exists. A minimal graph pilot does not solve the full GR-160 legal or institutional programme; unimplemented domains remain declared limitations.

## 10. Decisions still needed

- **PROVISIONAL:** Exact protocol identifier, field names, module registry, and transition from the existing `gsp-workspace/0.1` representation.
- **PROVISIONAL:** Whether the first interface exposes all relation forms immediately or declares a narrower initial profile with preserved extensibility.
- **OPEN:** Structured claim and material identifiers sufficient for claim-relative evidence and independent assessments.
- **OPEN:** Relation identity/admissibility criteria and typed vocabularies suitable for particular domains without imposing their meaning on every project.
- **OPEN:** Protected or cross-project references, shared access, governed retention, and genuinely independent implementation tests.
- **DEFERRED:** Semantically justified branch/merge workflows; Git's ability to represent a commit DAG alone does not settle those workflows.

The general and academia applications can share the record/transaction/file protocol and test fixtures while retaining different module registries, user journeys, and entry points. That shared substrate is a proposed engineering boundary; it is not a claim that the existing academia model already implements these semantics.
