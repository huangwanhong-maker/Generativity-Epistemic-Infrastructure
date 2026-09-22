# Protocol and Specification Development Plan

**Document class:** Informative planning document  
**Status:** PROVISIONAL proposal for programme review  
**Version:** 0.1  
**Date:** 2026-09-22

**Implementation update:** The original planning text below remains the capability proposal. [ADR-0008](../../decisions/ADR-0008-record-graphs-transactions-and-modules.md) subsequently selects a bounded shared record protocol and a Git/HTTP pilot binding. Requirements are developed in GR-120/130/140/160 WD 0.1; [GR-SPEC-120](../../specifications/GR-SPEC-120-information-model/GR-SPEC-120.md) and [GR-SPEC-130](../../specifications/GR-SPEC-130-interchange/GR-SPEC-130.md) contain the experimental details. This records the later infrastructure decision without implying that the original plan had already chosen a transport or that the broader capability register is implemented.

## 1. Purpose and authority

This plan organizes the technical and procedural capabilities needed to record, examine, exchange, contest, and revise generativity-related accounts. It translates the programme's epistemic and practical purpose into candidate development work. It neither adopts requirements nor certifies factual truth.

The governing development pattern remains the one in [ADR-0002](../../decisions/ADR-0002-standards-first-specifications-on-demand.md): a standards-track Working Draft identifies a requirement and conformance boundary; an identified infrastructure need triggers a Specification; pilots feed back into both. Formal project initiation follows the governance gate in the [comprehensive programme plan](comprehensive_development_plan.md).

Here, **protocol** means a defined interaction among roles, with inputs, outputs, states, transitions, and handling of failure. A protocol is technical content described in a Specification or an adopted Standard, not a fourth publication authority class. An institutional procedure allocates responsibilities, review, and decisions; a network binding defines messages and technical behavior. The latter cannot supply the former's authority.

The proposed allocation uses the existing [publication taxonomy](../framework/publication_taxonomy.md) and family numbers. Capability labels below are planning labels, not publication identifiers. No new publication numbers, record ontology classes, transport, storage backend, or cryptographic dependency are assigned.

## 2. Allocation across existing work items

**Structural role:** identify which proposed requirements motivate which reusable technical details; resolve overlapping placeholders before substantive drafting.

| Existing family | Proposed standards-track responsibility | Candidate specification destination |
|---|---|---|
| GR-100 | Vocabulary and boundaries used by the capabilities below | Terms referenced by all three existing specification work items |
| GR-110 | Minimum capture, attribution, trajectory, and completeness expectations | GR-SPEC-120 record structure; GR-SPEC-200 attribution and epistemic representation |
| GR-120 | Implementation-neutral model constraints, identity, time, relationships, and versions | GR-SPEC-120 precise representation and mapping rules |
| GR-130 | Information that survives exchange; permitted loss and incompatibility reporting | GR-SPEC-130 packaging, serialization, import/export; network binding only when needed |
| GR-140 | Faithful presentation, visible mediation, epistemic status, and disclosure limits | Existing specifications carry required presentation metadata; rendering guidance belongs in manuals |
| GR-150 | Conformance targets, claim boundaries, and evaluation criteria | Test fixtures and executable checks alongside the applicable specification |
| GR-160 | Preservation, access, restriction, retention, disposal, and interpretability outcomes | GR-SPEC-120 lifecycle representation; GR-SPEC-130 transfer/restriction behavior |
| GR-200 | Distinguishing attributed observations, claims, interpretations, and judgments | GR-SPEC-200 epistemic representation and capture behavior |
| GR-210 | Evidence assessment, competing interpretations, contestation, and revision outcomes | GR-SPEC-200 evidence and review interaction details |
| GR-300 | Explicit interface for jurisdiction, legal assertions, and procedural context | Optional metadata profile after legal review; no universal legal-status field |
| GR-310 | Scoped access, correction, objection, notice, and review procedure profiles | Procedural behavior in an existing specification where reusable precision is needed |
| GR-400 | Accountable operators, decision authority, oversight, and organizational use | Role and policy references in existing specifications; organizational implementation guidance in manuals |

GR-120 and GR-130 currently have both standards-track placeholders and specification destinations. The proposed distinction is between required outcomes/model constraints and their implementable realization. If drafting produces no distinct normative responsibility for a placeholder, record whether to retain, narrow, or consolidate it; do not duplicate normative authority. Existing paths and numbers remain stable pending that decision.

Each specification proposal records the motivating draft clause or requirement issue, affected actor, unmet interoperability need, alternatives, maintenance burden, pilot, and acceptance evidence. A local implementation preference alone is insufficient reason to establish a shared specification.

## 3. Candidate capability register

**Structural role:** scope a small set of coherent capabilities before selecting formats or service architecture. Each row is a work proposal rather than a present conformance requirement.

| Capability and priority | Requirement origin and specification trigger | Inputs and outputs | Candidate conformance actors and essential failure cases |
|---|---|---|---|
| Capture and attribution — first pilot | GR-110/200; different recorders need an exchangeable account of who asserted or recorded what, how, and when. GR-SPEC-120/200. | Input: submitted account, source references, method, attribution, perspective, time/uncertainty, access context. Output: versioned account and receipt with disclosed omissions. | Recorder and receiving repository. Cases: malformed submission, unknown attribution, duplicate delivery, conflicting identifier, insufficient permission. Unknown attribution remains explicit rather than fabricated. |
| Evidence linking and assessment — first pilot | GR-200/210; independent readers need to distinguish cited material from an assessor's interpretation of its bearing on a claim. GR-SPEC-200 plus GR-SPEC-120 references. | Input: identified account/version, evidence locator, assessor, method, rationale, limitations. Output: separately attributed assessment and links to its basis. | Assessor, recorder, repository. Cases: unavailable or changed source, ambiguous target, incompatible assessments, withheld evidence. Successful linking does not establish evidentiary sufficiency. |
| Publication and presentation — first pilot | GR-140/160; different consumers need to disclose selection, reconstruction, uncertainty, and restrictions consistently. GR-SPEC-120 metadata and GR-SPEC-130 export. | Input: authorized record view, selection/transformation description, version and profile. Output: bounded presentation or release with appropriate source/version references and disclosures. | Publisher, presenter, repository. Cases: incomplete reconstruction, stale view, omitted competing interpretation, prohibited disclosure, misleading validation label. |
| Contestation, review, and revision — first pilot | GR-210/310/400; challenges and dispositions need durable targets and attributable procedural history. GR-SPEC-200; policy-specific procedure profile when justified. | Input: objection, target version, grounds, supporting material, applicable procedure. Output: receipt, review activity, reasoned disposition, linked correction or recorded disagreement. | Challenger, receiving institution, reviewer, revising recorder. Cases: inaccessible target, wrong recipient, missing authority, delayed review, contested disposition, failed notice. Receipt does not mean agreement. |
| Version and history — first pilot | GR-110/120/160/210; consumers need to identify an earlier account and distinguish editing, correction, withdrawal, and reinterpretation. GR-SPEC-120; GR-SPEC-130 exchange rules. | Input: base version, proposed change, actor and reason, relevant time and access context. Output: identified successor/branch and change relation, or explicit conflict. | Recorder, repository, importer, presenter. Cases: stale base, divergent branches, unavailable predecessor, prohibited history disclosure. Concurrent or incompatible accounts are not silently merged. |
| Access, restriction, and disposal — first pilot minimum; expanded later | GR-160/310/400; software and institutions need consistent handling of scoped requests and resulting lifecycle changes. GR-SPEC-120/130; procedure semantics from approved profiles. | Input: request, subject/resource scope, applicable policy/authority, reason and review path. Output: bounded decision, authorized view or lifecycle action, receipt and permitted audit trace. | Requester, custodian, policy decision authority, repository. Cases: disputed authority, inconsistent retention instructions, partial propagation, leaked identifiers, irreversible disposal. A request and an executed action are distinct. |
| Exchange, synchronization, and migration — batch exchange first; synchronization later | GR-120/130/150/160; two implementations need to preserve meaning and disclose unsupported material. GR-SPEC-130 with GR-SPEC-120/200 mappings. | Input: versioned package, declared profile, referenced material/access conditions, destination capabilities. Output: import report, preserved identifiers/links, disclosed loss or rejection, migration provenance. | Exporter, importer, validator, custodian. Cases: unsupported version, partial package, duplicate transfer, identifier collision, denied references, lost extensions. Synchronization adds ordering and conflict behavior only after a pilot demonstrates need. |

These capabilities support practical recognition and accountable action by preserving the basis and history of accounts. They do not require agreement among observers, a universal confidence scale, a single institutional adjudicator, or exhaustive capture of experience.

## 4. Behavior to specify for every approved capability

**Structural role:** provide a reusable authoring checklist for future Specifications.

1. **Scope and actor:** identify the operation, responsible roles, conformance target, profile, and what an implementation claims to implement.
2. **Inputs and preconditions:** specify identifiers and target versions, authorization context, supported representation, required metadata, and treatment of unknown or unavailable information.
3. **Output and effects:** distinguish a received request, successful validation, committed change, substantive assessment, and delivered notice. Identify observable receipts and permitted audit data.
4. **States and transitions:** identify actor, trigger, guard, action, and result for each transition; distinguish policy decisions from their execution.
5. **Failure and recovery:** distinguish invalid input, policy refusal, unresolved conflict, unsupported capability, unavailable dependency, partial completion, and transient failure. Specify whether retry is safe and how duplicate effects are prevented.
6. **Concurrency and history:** specify base versions, branch behavior, supersession, withdrawn assertions, and out-of-order inputs. Define any merge operation narrowly enough to preserve disagreement and attribution.
7. **Disclosure and correction:** specify audience, visibility of omissions, permitted notice content, stale presentations, and revision propagation. Include cases where exposing even the existence of a record would breach access rules.
8. **Security and abuse:** state actor assumptions, authorization boundaries, forgery/replay risks, malicious inputs, resource limits, and reporting behavior that avoids disclosing restricted content. Select mechanisms after the relevant threat analysis.
9. **Conformance evidence:** provide examples and counterexamples, state-transition traces, testable outcomes, and checks requiring human or institutional review.

An initial procedure model can use `submitted -> acknowledged -> under review -> disposition recorded`, with separate paths for inability to proceed and later reopening. This is an illustrative sequence, not a mandated state vocabulary. Record visibility, review progress, evidentiary assessment, and factual content remain separate dimensions; a review state called “accepted” would otherwise risk conflating several judgments.

For an eventual network binding, specify message correlation, delivery acknowledgment, retry/timeout handling, representation and capability negotiation, concurrency controls, and application errors. Reuse transport semantics where suitable. Keep the procedure implementable through documented human steps or file exchange before requiring a network service.

## 5. Minimum vertical pilot

**Structural role:** demonstrate one complete recording-to-revision trajectory across a deliberately small technical surface.

The proposed first pilot uses the programme's shareable decision history and a synthetic contested institutional case, as described in the [governance and pilot plan](governance_and_pilots.md). A consented research or collaborative inquiry case can extend this work when suitable material and participation are available. One participant records an observation or attributed claim with a source and uncertain time; a second records a competing interpretation; a reviewer examines their stated bases; an author revises an account; a custodian restricts part of the evidence. Before claiming interchange maturity, a separate implementation imports the permitted export and presents both the current account and the accessible history.

The pilot includes one consequential decision, an alternative not pursued, and the stated basis for a later change of direction. Those records test whether the trajectory remains intelligible beyond the final artifact; they are attributed accounts, not measurements of an assumed universal generativity score.

Candidate scope:

- one bounded recording profile and explicit missing/unknown/restricted information;
- stable record/version references and independently attributed assessments;
- one review procedure and one restriction/correction procedure;
- a documented batch package and one human-readable presentation;
- structural validation plus semantic, disclosure, and procedural checks;
- two independently implemented producer/consumer paths before an interoperability claim.

The first prototype can use one implementation to find problems. The two-implementation demonstration is a later readiness gate for the pilot specification, not a claim that the current repository has implementations.

| Acceptance scenario | Evidence sought |
|---|---|
| Round-trip attributed claim and evidence | Import preserves the specified identifiers, target versions, source/assessor distinction, uncertainty, and restriction status; any unsupported content appears in the report. |
| Competing interpretations | Both accounts remain attributable and discoverable in the authorized view; export/import does not turn disagreement into one proposition. |
| Corrected and retrospectively reinterpreted account | Reader can distinguish what was recorded at each time, the reason/actor of change, and the current accessible version. |
| Retried or duplicate submission | A repeat has the specified observable effect without silently creating a second claim or review action. |
| Stale revision and diverging branches | A conflict is reported or separately represented; existing work is not silently overwritten. |
| Source missing, restricted, or changed | The result distinguishes the cases at the authorized disclosure level; no fabricated source text or false completeness claim appears. |
| Rejected, delayed, or disputed challenge | The procedure records receipt, available explanation and review route, without treating silence or rejection as factual refutation. |
| Restricted export and disposal request | Export contains only the permitted view; receipt distinguishes request, decision, action, and unresolved downstream copies. |
| Presentation of validation result | Readers can distinguish structural conformance, provenance checks, an assessor's judgment, and institutional decisions. No “truth certified” claim follows from a validator result. |
| Practical burden and intelligibility | Participants document capture/review effort and whether an uninvolved reader can recover the decision, alternative, evidence limits, and revision. Failures inform smaller or clearer requirements. |

Before piloting, agree measurable thresholds for semantic loss, unacceptable disclosure, review completeness, accessibility, and participant burden. Set thresholds from the case and risk context; do not invent programme-wide numerical targets without evidence.

## 6. History, privacy, and institutional authority

**Structural role:** preserve a design tension that technical immutability cannot settle.

Historical traceability does not imply indefinite retention or universal access. An append-only public history can preserve corrections while also perpetuating sensitive or harmful material. Deletion can protect people while reducing the evidence available for reconstruction. The programme needs an explicit profile decision about content, metadata, disclosure, retention, and authority in each context.

Evaluate restricted historical access, redacted derivatives, limited disposal metadata, and complete erasure as different operations. Even a tombstone, identifier, hash, or reason can reveal sensitive information; no universal public tombstone is proposed. A deletion receipt describes the actor's action and scope, with stated limits concerning copies beyond its control.

GR-160 owns preservation/access outcomes; GR-310 and GR-400 identify the relevant procedure and decision authority; GR-300 identifies any jurisdictional assertions. Specifications implement that recorded policy choice. They do not create legal rights, decide admissibility, or convert access policy into an epistemic verdict.

## 7. Sequencing and exit evidence

| Step | Deliverable and exit evidence |
|---|---|
| Governance and scope | Coherent programme procedures; approved work-item scope and accountable maintainers; resolved responsibility for overlapping placeholders. |
| Requirement discovery | GR-110/200/210 draft requirements, early GR-140/150/160 constraints, and the minimum pilot scenario; explicit specification triggers and initial cross-domain review. |
| Minimum specification drafting | Bounded GR-SPEC-120/200 model and behavior; GR-SPEC-130 package; source-to-clause traceability, worked examples, and failure cases. |
| Pilot and independent exchange | Recorded execution of the acceptance scenarios, disclosure review, burden findings and issue dispositions; independent import/export evidence when claiming interchange maturity. |
| Broader review and revision | Renewed horizontal conceptual, ethical, institutional, accessibility, legal-interface, and security review, building on review during scope formation and before live pilots; traceable revisions to requirements and specifications. |
| Stabilization and maintenance | Declared edition/profile boundaries, backward-compatibility or migration behavior, conformance evidence, maintainers, and explicit promotion decisions where warranted. |

**DEFERRED pending demonstrated need:** federation/discovery, continuous synchronization, universal identity registries, consensus networks, mandatory signatures, global trust scores, real-time streaming, and automated substantive adjudication. Optional tamper-evidence can be evaluated for a specific threat model without making cryptography the foundation of factual warrant.

External reuse candidates and their limits are documented in the [external standards landscape](external_standards_landscape.md). Evaluation of a source is distinct from adopting it as a normative dependency.

The [cross-domain programme](cross_domain_programme.md) supplies substantive review questions; the [standards-development comparison](standards_development_comparison.md) informs how proposals, consensus, review and appeals are organized.

## 8. Decisions still open

- **OPEN:** responsibility and publication class of overlapping GR-120/130 placeholders and the boundaries of the three initial specifications.
- **OPEN:** minimum attributable epistemic unit and its relation to record ontology, without prematurely adding an ontology class.
- **OPEN:** enough common procedural behavior for interoperability while preserving legitimate institutional differences and appeals.
- **OPEN:** which historical metadata survive correction, restriction, or erasure under a scoped profile, and which actors decide.
- **OPEN:** first serialization, validation technology, identifier rules, and offline packaging; decide through the pilot rather than this plan.
- **OPEN:** thresholds and evidence for practical usability and independent interoperability before public review.

Programme-level disposition belongs in [open questions](open_questions.md) and an ADR where architecture changes. This document supplies proposed scope and acceptance evidence for those decisions.
