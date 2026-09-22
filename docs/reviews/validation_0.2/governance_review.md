# Governance review of Working Draft 0.2

**Class:** Informative substantive review record.  
**Status:** PROVISIONAL; authored inspection and synthetic design analysis, 2026-09-22.  
**Reviewer:** AI-assisted repository review; no appointed governance assessor or independent human reviewer is represented.  
**Baseline:** The five governance sources preserved in [the 0.2 archive](../../../publications/archive/0.2/README.md).  
**Assessment claim:** Inspection of all 462 requirement bodies, their document structure and relevant normative schedules. This is a review of proposed rules, not a conformance assessment of an operating institution.

## 1. Conclusion and boundary

The coordinated governance texts preserve the programme's central distinction between an inspectable account and warranted institutional authority. They provide explicit founding conditions, versioned decisions, review and reply, minority consideration, protected handling, rights clearance and independent challenge. These mechanisms are coherent enough to support continued preparatory drafting and a more demanding rehearsal. They do not establish governance launch readiness.

One concrete ballot-accounting omission needs correction: invalid or unresolved attempted submissions have no explicit place in the required summary partition. Two timing choices need explicit interpretation before a live procedure. Actual personnel, commitments, permissions, protected routes, independent competence, and resource capacity remain unavailable as assessment evidence. Their absence is not a textual defect to be cured by adding fictional names.

This review does not independently establish foundation-manuscript fidelity, applicable law, a rights clearance, accessibility conformance, societal trust, representative legitimacy, or external standards compliance. The root programme review addresses manuscript interpretation separately. No external source has been newly incorporated through this review.

The method was to read the requirement bodies in source order, examine the controlling definitions and schedules, compare cross-document authority and dependencies, and apply normal, failure and boundary scenarios. The grouped register is [governance_review_groups.json](governance_review_groups.json). Every identifier is listed exactly once. A group marked `reviewed` means its authored text was examined against the stated question; it is not a group pass or individual fulfillment result. `finding` identifies a demonstrated drafting omission; `open` identifies a policy choice or unavailable evidence needed before an institutional conclusion. Requirement-level conformance results would need the component evidence required by D2-054/086/091–095.

## 2. Findings, alternatives and proposed dispositions

### GOV-F01 — Invalid attempted ballots disappear from the tally partition

**Classification:** Drafting defect; correct before relying on the tally procedure.  
**Exact baseline:** Directives Part 1 0.2, D1-029–032, D1-074, D1-076 and Annex B's Ballots row. Historical P0 0.1 defines nonresponses as `N − R`.

D1-031 correctly defines `R` as valid returns. D1-074 requires clarification of ambiguous or unverifiable submissions and prohibits silently converting them into approval or abstention. D1-032 nevertheless lists valid returns, approvals, disapprovals, abstentions and nonresponses without requiring a separate count for an otherwise eligible participant whose attempted return remains invalid or unresolved. The older P0 formula consequently treats every such attempt as nonresponse. That hides an access, authentication or clarity problem as silence.

The discriminating example is five eligible persons: two approvals, one disapproval, one abstention and one timely but unresolved ambiguous submission. `R=4`; the three numerical conditions pass. There are zero actual nonrespondents, not one. The unresolved attempt still needs its own validity decision, reasons, challenge route and materiality assessment; a mathematical pass cannot remove that issue.

Alternatives are: (a) classify every excluded attempt as nonresponse, losing a procedurally important distinction; (b) count every attempt as a return, changing the valid-return quorum; or (c) retain valid-return quorum and add explicit submission-state accounting. **Proposed direction: (c).** Record `N=R+I+U`, where `I` counts eligible persons with an attributable excluded or unresolved attempted submission and no counted valid ballot, and `U` counts eligible persons without an attributable submission. Replacement and duplicate handling precede this partition. An individual with a valid replacement contributes to `R`, not also to `I`; the attempt history remains in the protected audit. Count records describe persons, while the audit may contain multiple submissions. Unattributable or ineligible submissions have a separate inventory; their identity is not invented to complete the person partition. An invalid attempted replacement does not by itself erase a preceding valid ballot; withdrawal or invalidation needs an applicable rule and recorded decision.

Affected text: D1-032, D1-074 and the Annex B Ballots row; supporting P0 and tally checks. D1-031's quorum, minimum substantive participation and approval formula remain unchanged. A procedural finding about an excluded attempt can still block progression under D1-028/076 even where the arithmetic passes. Closure requires inspection of the revised clauses and a fixture distinguishing valid replacement, excluded attempt and genuine nonresponse.

### GOV-O01 — Reply-window and ballot sequencing

**Classification:** OPEN implementation choice, not a proved contradiction.  
**Exact baseline:** D1-023, D1-024, D1-027–029, D1-033, D1-073/075 and Annex A stage 50.

The text requires disposition replies before final approval and a completed review dossier at the final-approval stage. It does not state explicitly whether a ballot can open during a final reply window if the package remains frozen. One reading serializes reply, assessment, ballot and appeal. Another allows reply and ballot to overlap but withdraws the ballot if a reply requires substantive change. The latter can save calendar time, while placing greater demands on notification and explaining what the ballot actually considers.

**Provisional rehearsal choice:** use sequential windows. This is a conservative scenario assumption, not a newly adopted rule. Before a live process, either make sequencing explicit or specify the permitted overlap, reply handling, updated objection record and withdrawal triggers. The effect is calendar time and informed consideration; shortening the 30-day minimum is not proposed.

### GOV-O02 — Calendar-day boundary conventions

**Classification:** OPEN implementation detail.  
**Exact baseline:** D1's calendar-day definition, D1-019/020/023/025/029/034–036/050/059/092; EIP-014/067/100.

Notices must identify exact timestamps and time zones, which makes a chosen deadline observable. The drafts do not fix whether the initiating calendar date is day zero, how an inclusive last date is interpreted, or how a local daylight-saving transition affects the minimum opportunity. A shared convention would prevent two administrators calculating different deadlines from the same receipt.

Alternatives are elapsed hours, local calendar-date arithmetic at the same local time, or a specified close-of-day convention. **Provisional rehearsal choice:** UTC timestamps and addition of 7, 30 or 60 date intervals, with the start as day zero. That avoids daylight-saving assumptions in the fixture. Choose and document the operational convention before live notices; do not retroactively shorten an already announced window.

### GOV-O03 — Minimum institutional capacity and independent review

**Classification:** OPEN launch gate; no evidence of fulfillment.  
**Exact baseline:** CH-021–025/028/029/036–041/055/060/065/078–085; D1-003–005/008/016/035–038/041/051/060/076; TC-011–014/040/054/062/071; EIP-001/010/020/042–046/101.

The documents correctly refuse to let role labels constitute appointments and permit combined functions only with disclosed limits. The minimum three substantive ballots are arithmetic conditions, not evidence of reviewer independence or a sufficient range of perspectives. A single person and several AI assistants cannot supply the independent human functions required for founding, disputed decisions, tally verification or source/render release review.

Options are to obtain accepted bounded roles and competent external review, reduce the formal work scope to actual capacity, or continue preliminary development. **Current direction:** preliminary development, while preparing an appointment and capacity dossier for actual people to consider. No names, commitments, contacts, review independence or rights competence have been invented. Availability needs actual acceptance and an exercised route. An unfilled mandatory function is not `not applicable` merely because the programme is small.

### GOV-O04 — Burden, accessibility and concentrated influence

**Classification:** OPEN empirical question.  
**Exact baseline:** CH-015–020/031/043–045/049–052/064–068/073; D1-007/019/021/060/072/090; D2-060/079/080; TC-040/054/067; EIP-012/045/055/076–081/094.

The texts make unpaid labor, funding, infrastructure control and access barriers visible. They permit shared registers and reuse of still-applicable evidence, which can reduce duplication. The actual cost of administering the full package is unmeasured. A small, cooperative synthetic example cannot establish affordability for a volunteer committee, language accessibility, protected reporting usability or meaningful affected-person participation.

Options are separate forms for each obligation, shared records with explicit views, or reduced scope with bounded claims. **Proposed next experiment:** one shared dossier, with recorded operator time, assistance, failed tasks and reviewer effort; no weakening of requirements is inferred. Test public and protected routes with consenting participants after responsible roles and permissions exist. Numerical targets for acceptable burden remain unset pending those observations.

### GOV-O05 — Rights and organizational context

**Classification:** OPEN adoption and distribution gate.  
**Exact baseline:** CH-034/064/071; D1-033/040/082; EIP-026–038/066/082–090/098.

The policy accurately separates permission to examine material from permission to redistribute or sublicense it, and distinguishes prose, code, schemas, examples, datasets and manuscripts. It does not select contribution terms, establish legal personality or appoint a competent rights reviewer. Those unresolved facts cannot be closed by adding a generic open-source license to the entire repository.

Alternatives are artifact-specific permissions, suitably reviewed common terms with documented scope, replacement of restricted material, or narrower distribution. **Current direction:** preserve source terms and record actual intended uses; prepare a rights register and obtain competent contextual review before the affected public distribution or operative agreement. This report supplies no legal opinion or clearance.

## 3. Cross-document results

| Topic | Inspected provisions | Textual result and consequence |
|---|---|---|
| Founding authority | CH-001/002/036–038/058–060; D1-003/004 | Express commitments and founding review precede operation; a repository revision cannot adopt the package. Actual founding evidence remains absent. |
| Formal project activation | CH-014/028; D1-009–014/060; TC-001/032/033 | Standards-first drafting can continue preliminarily; launch, mandate and accepted resources still gate formal projects. |
| Review and minority objection | D1-019–028/068–072; CH-027; EIP-058 | A vote does not dispose of an objection, and conduct handling does not suppress a technical concern. Replies and material changes retain separate records. |
| Ballot and recusal | D1-029–032/073–076; CH-039/040/053; EIP-053/054 | Numerical thresholds are internally consistent; a material roster change requires a new ballot. GOV-F01 concerns submission-state accounting. |
| Procedural appeal | D1-034–039/078–081; CH-029 | Independent review, stay where specified, reasons and remedy verification are explicit. Absence of capacity prevents contested progression. |
| Conduct and merits reconsideration | EIP-014/016–020/064/067/069/099–104 | The internal evidence criterion is explicit and separate from fair response; scoped substantive reconsideration is distinct from procedural appeal. Contextual acceptability still needs human review. |
| Emergency protection | D1-049–051/088; EIP-067/068/103 | Seven-day independent review and 30-day expiry apply to temporary participation restrictions as well as publication safeguards. A late remedy does not erase an earlier failure. |
| Release and maintenance | D1-040–048/082–087; D2-042–045/082–084; TC-072–075 | Exact source, authorization and substantive change control remain distinct. An editorial correction cannot alter an outcome merely because it is a small edit. |
| Protection and public reasons | CH-069/070; D1-021/052/053/080/095; EIP-039/091/092 | Safe public accounts can expose limitations without exposing protected existence or identity. Restricted access does not establish evidential adequacy. |
| Assessment and publication classes | D2-001–005/026–032/068–071/085–095; TC-026/051/052 | Per-subject and per-component evidence is required. This grouped authoring review does not satisfy full conformance or institutional performance claims. |

The JSON register supplies the complete 462-identifier matrix rather than treating the selected identifiers in this table as exhaustive.

## 4. Updated P0 and completion conditions

[P0 for Working Draft 0.2](../../../examples/validation_0.2/p0-governance-rehearsal.md) expands arithmetic boundary cases, malformed-input cases and synthetic procedural traces. The old P0 remains historical. An executable fixture's mathematical result has a different evidence status from a fictional notice, appointment, appeal, or behavior.

The next institutional rehearsal needs an actual mandate limited to rehearsal, willing participants, declared interests, safe synthetic input, tested public/protected routes, assigned custody, accessible assistance, a non-conflicted reviewer and an agreed record of observed effort. It can test mechanisms over compressed time only if it says so; compressed time cannot fulfill elapsed 7/30/60-day obligations.

Before governance launch, the dossier still needs actual founding review and dispositions, accepted appointments and commitments, independent assessment, rights and contribution arrangements, records/recovery capacity, release authority and a neutral procedural review route. Completed prose and successful arithmetic do not discharge these conditions. The recommended drafting repair can advance to 0.3 while these institutional gates remain pending.

## 5. Complete grouped inspection matrix

Each row is an authored-text review group. Exact identifiers and immutable source locators are in the JSON register; the counts are coverage counts, not passed conformance obligations. Open findings can affect only part of a group.

| Group | Document and topic | Requirements | Review state | Inspection basis |
|---|---|---:|---|---|
| GOV-R001 | GSP-Charter: Normative references | 1 | reviewed | Exact adopted editions are fixed explicitly; later drafts cannot acquire authority by replacement. |
| GOV-R002 | GSP-Charter: Mission and programme commitments | 10 | reviewed | Mission evaluation distinguishes intended benefit, measured practice and broader trust claims; layers and operational narrowing stay separate. |
| GOV-R003 | GSP-Charter: Publication authority | 8 | reviewed | Reserved decisions, editorial custody and publication class are distinguished; pre-adoption drafting does not authorize work items. |
| GOV-R004 | GSP-Charter: Participation and consideration | 13 | open | Contribution, representation, endorsement and eligibility are separated; alternative access and anti-dominance duties require observed capacity. |
| GOV-R005 | GSP-Charter: Organization and accountable appointments | 10 | open | Accepted bounded appointments, recusal, vacancy and handover control authority; combined roles cannot supply neutral self-review. |
| GOV-R006 | GSP-Charter: Founding and decision accountability | 16 | open | Founding review and express commitments avoid self-authorizing drafts; independent capacity and effective conditions remain launch gates. |
| GOV-R007 | GSP-Charter: Resources and infrastructure | 7 | open | Support conditions, essential-resource control, staffing and exercised recovery are identified; actual resources are not yet evidenced. |
| GOV-R008 | GSP-Charter: Records and external relations | 6 | open | Public decision history is bounded by protection; external agreements and representation require actual mandate and assent. |
| GOV-R009 | GSP-Charter: Oversight, transition, and closure | 6 | reviewed | Annual review, prospective amendments, transition and custody preserve earlier decisions without pretending external uses disappear. |
| GOV-R010 | GSP-Charter: Assessment | 3 | reviewed | The subject, period, relationships and evidence are declared; blank forms and unobserved routes do not establish performance. |
| GOV-R011 | GSP-Charter: Annex A: Minimum governance evidence records | 5 | reviewed | Authority, decision and continuity records retain evidence and limits; sampling cannot imply observation of inaccessible routes. |
| GOV-R012 | GSP-Directives-1: Founding, responsibilities, and participation | 8 | open | Founding basis, appointments, access and recusals precede consequential operation; unassigned mandatory functions defer progression. |
| GOV-R013 | GSP-Directives-1: Project initiation and scope control | 8 | open | NWIP scope, normative problem, Specification need and resource acceptance are explicit; proposal receipt is not project approval. |
| GOV-R014 | GSP-Directives-1: Stages and progression evidence | 10 | reviewed | Stage labels require actual evidence; source decisions, missing expertise, synthetic tests and shared dependencies remain visible. |
| GOV-R015 | GSP-Directives-1: Public review and comment disposition | 14 | open | 60-day intake, 7-day acknowledgement, 30-day disposition target/reply and substantive re-review are distinct; access failures affect progression. |
| GOV-R016 | GSP-Directives-1: Consensus, ballots, and final approval | 12 | finding | Valid-return quorum and substantive thresholds are consistent; invalid attempts lack a required separate tally category, while recusal can require restart. |
| GOV-R017 | GSP-Directives-1: Procedural appeals and separate complaint routes | 10 | open | Accessible notice, independent assignment, replies, scoped stay, reasons and implemented remedy differ from substantive disagreement. |
| GOV-R018 | GSP-Directives-1: Publication and release integrity | 5 | open | Authorization, rights, exact artifacts and non-editor source/render review are prerequisites; successful compilation does not authorize release. |
| GOV-R019 | GSP-Directives-1: Maintenance, review, and withdrawal | 10 | reviewed | Correction, amendment, confirmation and withdrawal have different effects; prior editions and existing users are not retroactively relabelled. |
| GOV-R020 | GSP-Directives-1: Emergency safeguards and continuity | 4 | reviewed | 7-day independent review and 30-day expiry constrain temporary safeguards; emergency action cannot make permanent normative changes. |
| GOV-R021 | GSP-Directives-1: Records, transparency, and external liaison | 9 | open | Records, review cycles and external liaison are bounded; Specification technical requirements retain the full technical review route. |
| GOV-R022 | GSP-Directives-1: Annex A: Stage evidence and procedural schedule | 3 | open | Stage records reuse evidence only when still applicable; clocks need explicit start, zone, deadline and failure effects. |
| GOV-R023 | GSP-Directives-1: Annex B: Registers and minimum decision records | 2 | finding | Shared registers preserve specific record content; ballot accounting needs a distinct attempted-but-not-valid category. |
| GOV-R024 | GSP-Directives-2: Normative references | 1 | reviewed | Bounded internal dependencies identify invoked subjects and edition; incompatible or unavailable material blocks the affected result. |
| GOV-R025 | GSP-Directives-2: Publication identity and document architecture | 15 | reviewed | Class, status, title, front matter, scope, annex role and leaf hierarchy distinguish publication functions and controlling requirements. |
| GOV-R026 | GSP-Directives-2: Formulation of provisions | 16 | open | Actor, trigger, criterion, modality, component results and exceptions are specified; unknown declarations do not resolve underlying facts. |
| GOV-R027 | GSP-Directives-2: Terminology and conceptual sources | 9 | reviewed | Operational definitions and manuscript interpretation retain scope and source differences; no formal-to-archival equivalence is presumed. |
| GOV-R028 | GSP-Directives-2: References and dependency management | 7 | reviewed | Verified primary provisions, precise dependencies and access gaps are distinguished from informative bibliographic motivation. |
| GOV-R029 | GSP-Directives-2: Conformance and assessment provisions | 11 | reviewed | Record structure, practice, interoperability, warrant and authority have separate subjects and evidence; partial claims stay bounded. |
| GOV-R030 | GSP-Directives-2: Information models, protocols and technical artifacts | 10 | reviewed | Semantic need precedes encoding; protocol delivery, acceptance, retry and institutional outcome differ; artifact conflicts need disposition. |
| GOV-R031 | GSP-Directives-2: Presentation, accessibility and contextual statements | 9 | open | Accessible structure, formulas, synthetic examples and contextual claims require evidence beyond successful rendering. |
| GOV-R032 | GSP-Directives-2: Versioning and publication production | 7 | reviewed | Canonical sources, hashes, build tools and archived editions support traceability; semantic changes trigger the appropriate review. |
| GOV-R033 | GSP-Directives-2: Assessment of a publication package | 4 | reviewed | Declared competence and per-requirement evidence precede overall results; correction closure needs a revised artifact and recheck. |
| GOV-R034 | GSP-Directives-2: Annex A: Structural schedule for programme publications | 2 | reviewed | Structural presence is not substantive completeness; unresolved topics need exclusion, development or a blocking status. |
| GOV-R035 | GSP-Directives-2: Annex B: Requirement assessment record | 3 | reviewed | Requirement records retain applicability, method, component results, assessor and limits; missing evidence cannot become fulfillment. |
| GOV-R036 | GSP-Directives-2: Annex C: Publication self-assessment schedule | 1 | reviewed | Discriminating checks are meaningful prompts; grouped schedule rows explicitly do not authorize undifferentiated passes. |
| GOV-R037 | GR-TC1-Charter: Normative references | 1 | reviewed | Exact coordinated governance editions are recorded before committee reliance; external text cannot amend obligations by implication. |
| GOV-R038 | GR-TC1-Charter: Mandate and work programme | 17 | reviewed | Mandate, work authorization, dependencies and protocol scope remain distinguishable from preliminary technical authoring. |
| GOV-R039 | GR-TC1-Charter: Organization and conduct of work | 11 | open | Editors and reviewers accept bounded roles; asynchronous work, disclosed self-review, capacity shortfall and handover retain separate records. |
| GOV-R040 | GR-TC1-Charter: Source interpretation and requirements | 14 | reviewed | Foundation interpretation, alternatives, edge cases and assessed requirements are traced; independent exchange evidence is required for interoperability. |
| GOV-R041 | GR-TC1-Charter: Multidisciplinary review | 11 | open | Domain competence, affected perspectives, conflicts and reasons are retained; technical agreement does not create a legal mandate. |
| GOV-R042 | GR-TC1-Charter: Pilot preparation and authorization | 8 | open | Baseline, questions, permissions, protection and stop/restart conditions precede real pilots; authored scenarios remain synthetic. |
| GOV-R043 | GR-TC1-Charter: Pilot evaluation and implementation evidence | 6 | open | Failure, assistance, burden, excluded perspectives and later unobserved effects qualify outcomes; independence is a scoped evidenced relationship. |
| GOV-R044 | GR-TC1-Charter: Progression, maintenance, and publication | 7 | reviewed | Progression includes rights, findings, source/render agreement and maintenance; an informal implementation update cannot amend requirements. |
| GOV-R045 | GR-TC1-Charter: Committee assessment | 3 | reviewed | Committee performance is assessed for a declared activity, edition and period; a problem-to-decision trace reveals missing evidence. |
| GOV-R046 | GR-TC1-Charter: Annex A: Committee dossier and record content | 5 | reviewed | Dossier records link baseline, sources, findings, pilot limits and progression; reuse can avoid duplication without erasing distinctions. |
| GOV-R047 | GSP-Ethics-IP: Normative references | 1 | reviewed | Material cross-policy timing, authority or protection conflicts are resolved before consequential dependent determination. |
| GOV-R048 | GSP-Ethics-IP: Responsibilities and readiness | 7 | open | Contacts, alternates, protected custody and independent review require actual acceptance and tested routes before operation. |
| GOV-R049 | GSP-Ethics-IP: Conduct and evidential integrity | 9 | reviewed | Disagreement is not misconduct; claims, errors and AI assistance retain sources and accountable review without exposing protected material. |
| GOV-R050 | GSP-Ethics-IP: Interests and influence | 10 | open | Disclosure alone does not manage conflicts; recusal preserves relevant evidence and retrospective discovery can require renewed procedure. |
| GOV-R051 | GSP-Ethics-IP: Report intake and triage | 8 | open | Intake, triage and determinations differ; acknowledgement remains due in 7 days even if the 30-day determination target is extended. |
| GOV-R052 | GSP-Ethics-IP: Inquiry and fair response | 7 | open | Adverse findings need safe grounds and reply; missing evidence and limited participation do not prove conduct or non-occurrence. |
| GOV-R053 | GSP-Ethics-IP: Protective measures and determinations | 18 | open | Protection is time-limited; more-likely-than-not criterion does not override fair review; scoped merits reconsideration has independent assignment. |
| GOV-R054 | GSP-Ethics-IP: Ethical impact assessment | 11 | open | Impacts distinguish observed effects, predictions and values; operational mitigation and practical repair require evidence. |
| GOV-R055 | GSP-Ethics-IP: Contribution intake and permission | 12 | open | Rights to examine, incorporate, modify and distribute are separate, versioned and use-specific; source custody is not relicensing. |
| GOV-R056 | GSP-Ethics-IP: Terms, implementation rights, and publication | 10 | open | Artifact-specific terms need contextual competent review and actual assent; known implementation-rights issues cannot be cleared by an empty register. |
| GOV-R057 | GSP-Ethics-IP: Records, oversight, and assessment | 6 | open | Safe public summaries and retention duties avoid exposure; assessment and recurring-failure review do not assume effective remedies. |
| GOV-R058 | GSP-Ethics-IP: Annex A: Handling records and state transitions | 5 | open | Case states, conflicts, rights and timing records preserve unfinished performance; specimen completeness is distinct from observed handling. |
