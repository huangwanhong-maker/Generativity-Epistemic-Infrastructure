# P1 — Contested funding decision and generative trajectory

**Document class:** Informative synthetic example and design fixture  
**Status:** EXPERIMENTAL; version 0.1; 2026-09-22  
**Related drafts:** GR-100/110/200/210 Working Draft 0.1  
**Assessment boundary:** A fictional dossier and expected review behavior. No real people, institution, law, hearing, submission or remedy is represented.

## 1. Purpose and boundary

Package `P1/v1` examines how a fictional research fund classified a community water-monitoring application, how contrary material changed its assessment, and what changed for future monitoring capacity. Intended readers are participants, reviewers, custodians and designers of the recording procedure. The decision and response horizon is synthetic days 0–40, with relevant antecedent activity before day 0 included where identified. Dates below are ordered scenario labels, not claims about actual elapsed work.

The retained inventory comprises the following attributed records. It is not a complete history. Raw measurements, personal identities, vendor internals and subsequent environmental outcomes are outside the supplied material. A fictional Secretariat records receipt and custody; an initial Panel makes the funding decision; Reviewer B handles the later evidence review; a distinct Review Contact handles procedural reconsideration. These are scenario roles, not programme appointments. No actor has an asserted mandate beyond this fictional fund's internal procedure.

The purpose-based collection rule selects claims relied on for eligibility, evidence assessment, contribution and practical response. The material-claim set is C1–C7 and their assessments, with D1/D2 and rule versions K1/K2 as consequential dependencies. This is the bounded GR-200 package for R110-005 and C210-030. The fixture does not assert fulfillment of every applicable requirement.

## 2. Inventory and three distinguishable histories

| Reference | Attributed content and role | Time and availability |
|---|---|---|
| A1 | Community team application reports an operating sensor network. This is a report, not independent verification of every measurement. | Submitted day 0; only this content summary is supplied, not the full packet or an independently inspectable application extract. |
| M1 | Intake extract omits the calibration attachment. It establishes the extract's contents, not whether calibration occurred. | Created day 2 from A1; intake scope excludes other channels. |
| S1 | Fictional `SummaryTool/demo-v0` output: “The sensors were not calibrated.” Input: M1 only; transformation: generated summary. Internal reasoning unavailable. | Day 3; taken up by Panel; output is generated, not independent evidence. |
| K1 | Panel's rule category “documented calibration” requires an attachment in the application packet. Source of authority: fictional fund's internal rule only; no asserted legal effect. | Version 1 effective in scenario days 0–20. |
| C1/v1 | Panel repeats S1 as a factual conclusion that calibration did not occur. Epistemic mode: inference from missing metadata; actuality claim: non-occurrence. | Recorded day 4; concerns equipment before submission; disputed. |
| D1 | Panel excludes the application, relying on C1/v1 and K1. The exclusion and its factual warrant are different questions. | Day 5; institutional action in the story. |
| T1 | Technician testimony reports calibration before submission. The technician also contributed maintenance labor omitted from A1's attribution. | Statement recorded day 7; retrospective report of pre-submission activity. |
| L1 | Maintenance log records adjustments; fictional protected original includes contact details. Only the invented nonpersonal excerpt is supplied here: “Day −2: station S adjusted against reference instrument.” | Original described as restricted; excerpt supplied day 8; no genuine protected data. |
| CH1 | Team challenges C1/v1 and the suitability of K1, citing T1/L1 and requesting reconsideration and attribution review. | Received day 8; two distinct issues. |
| C1/v2 | Reviewer B assesses that calibration activity has support from testimony, while L1 supports the narrower report of adjustment. Whether that adjustment met the intended calibration method, its quality and the chain of measurement remain unresolved. | Day 12; evidence/assessment revision, not a new calibration event. |
| K2 | Panel changes its rule to permit assessed maintenance testimony and controlled log inspection as possible evidence. Adequacy remains case-relative. | Announced day 20; effective from day 21. A rule/category change. |
| D2 | Panel schedules a renewed assessment under K2; it does not yet award funding. | Day 22; scheduling is recorded, while reassessment and its outcome remain pending. |
| C7 | Team reports that the exclusion delayed equipment replacement and interrupted training; no independent outcome follow-up is supplied. | Day 30; reported practical effects and possible future constraint. |

The first history concerns claimed calibration activity, maintenance, exclusion and possible effects on monitoring capacity, with their respective evidential qualifications. The second includes the creation, use and revision of claims about them. The third includes K1/K2 and changing evidential categories. The case does not identify a changed category with a changed past event. Historical use of K1 stays visible after K2 is introduced.

Record references identify representations and versions. They do not settle whether two sensor names denote the same physical instrument. An unresolved match stays unresolved. The maintenance relation can have its own participants, interval, contribution dispute and history; it need not disappear into an edge without attributes. Event, Process and Relation roles can describe different aspects of the same maintenance episode without asserting three different underlying episodes.

## 3. Claim-relative evidence and assessment

| Claim | Grounds and method | Assessment and limits |
|---|---|---|
| C1/v1: no calibration occurred | Panel infers non-occurrence from M1. The method assumes all calibration would appear in that extract. | Unsupported inference in the fixture: expected recording coverage is unestablished. Preserve “attachment absent from M1.” Do not automatically infer the opposite factual conclusion. |
| C1/v2: calibration activity occurred | Reviewer B considers T1's calibration testimony and L1's narrower adjustment report. The excerpt does not independently establish every part of the testimony. | Activity receives qualified support; instrument identity, method, reference quality and adequacy remain unresolved. Two sources do not establish independence if the testimony derives from the log. |
| C2: application met K1 | Proposed rule comparison against complete packet contents; only M1's limited extract is represented here. | Compliance remains unresolved from the supplied material. Missing attachment can matter to K1's application, but its absence from M1 alone does not establish absence from the full packet. Even a correct rule application would not resolve the rule's legitimacy or procedural suitability. |
| C3: exclusion was unfair | Team applies stated values of accessible participation and recognition of maintenance labor, citing differing access to formal documentation. | Attributed ethical judgment. Panel's competing value of consistent evidence requirements remains identified. No universal value ordering is imposed. |
| C4: D1 violated a legal duty | Team's asserted legal objection. | Jurisdiction, legal source, competent authority and applicability are UNSPECIFIED in this fiction. The case records the allegation and routes it outside the reviewer's claimed competence. No legal finding follows. |
| C5: maintenance enabled continued monitoring | Technician's account connects adjustments and ongoing operation, compared with the earlier condition of drift. | Attributed generative account; causal sufficiency and longer-term effects unestablished. Maintenance can matter without a new output. |
| C6: granting the application would have preserved training | Proposed alternative raised during challenge, with team estimates of cost and availability. | Counterfactual, not observed outcome; feasibility is only partly examined. Other funding and scheduling constraints remain unknown. |
| C7: exclusion constrained later capacity | Team reports delayed replacement and interrupted training; reviewer identifies D1 as one possible contributing condition. | Reported realized effects and inferred contributions remain separate from predicted future loss. No complete causal attribution or remedy is established. |

Reviewer B's method is qualitative comparison of claims, source scope, derivation and alternative explanations. No probability is invented. M1, S1 and C1/v1 share a derivation path; they do not constitute three independent observations. Authenticity of an excerpt and the strength of its support for calibration quality are separate assessments. This fixture has no actual signature or authenticity test.

Generativity return is not quantified. Attribution review can acknowledge maintenance contribution while leaving ownership, payment, entitlement and fair distribution unresolved. Any future assessment would need recipients, evaluative criteria, grounds and context. A funding award would not itself settle that inquiry.

## 4. Proposed procedural trace

The fictional operator declares the covered decisions, accessible text/oral submission routes, protected-contact option and a review contact distinct from the original Panel. Local fixture targets are acknowledgement within two scenario days and initial disposition within ten; extensions require notice before the target, reasons and a revised date. These values illustrate C210-003 and are not programme-wide service requirements. No actual contact channel has been created or tested.

1. **Receipt:** CH1 receives a distinguishable reference and day-8 receipt entry. An acknowledgement on day 9 identifies the two issues, review function and target. Receipt is not acceptance of the challenge's merits.
2. **Conflict:** Initial proposed Reviewer A authored S1's uptake note. The scenario identifies this conflict and reassigns to B; it does not label A independent. If no suitable reviewer is available, the record stays pending with notice and a declared alternative, rather than inventing independence.
3. **Participation:** Panel and team can respond to the grounds used. The technician can submit through protected contact. The short, nonpersonal L1 excerpt and B's assessment of that excerpt support response while the original remains restricted. B's described authorization covers the excerpt only; no examination or authentication of the unavailable original is represented. Missing perspectives and limits of that substitution remain visible.
4. **Assessment:** B separately considers factual calibration claims, K1 application, K1 adequacy, contribution attribution and the asserted legal objection. The legal matter is recorded outside B's competence. Silence by a party is not consent.
5. **Disposition:** On day 12, B replaces the unqualified C1 conclusion with C1/v2, preserves the earlier inference and its use, records remaining uncertainty, and refers reconsideration of D1 and K1 to the Panel. No funding award is represented as accomplished.
6. **Action:** D2 schedules reassessment. The action owner is the fictional Panel; completion evidence would be a dated assessment/decision record and notice. These are absent at the fixture boundary. Correction of C1 is complete within the supplied text; practical repair is pending.
7. **Notification and propagation:** A case notice is described as sent to the team and Panel. The public summary can be changed; a prior exported spreadsheet is a known dependent copy whose correction remains unverified. The fixture includes no delivery receipts, so sending and receipt are unverified procedural assertions.
8. **Further review:** The declared Review Contact can consider new evidence, an incorrect correction or a procedural failure. Reopening links CH1 and the prior disposition. The operator cannot promise external court review or exercise legal authority it lacks.

## 5. Restricted material and reduced presentation

The fictional custody rule authorizes B to examine the nonpersonal excerpt; public readers receive the excerpt and a scope statement, not personal contact details. Retention decisions separate claim history, testimony content, contact information and permitted audit evidence. Proposed contact-data disposal after closure needs a declared authority, executor and permitted confirmation; it does not require keeping the removed content to prove removal. No deletion is claimed to have been performed.

A suitable public summary reads:

> An earlier finding of no calibration relied on a missing attachment in a limited intake extract. Subsequent testimony supports calibration activity; a maintenance-log excerpt reports adjustment, with method, quality and identity limits remaining. The fund has scheduled reassessment under a revised internal rule. Funding, correction of external copies and reported losses remain unresolved. This summary omits personal contact information and does not provide full inspection of the described original log.

The summary preserves the difference among attributed assertion, assessment, institutional action and pending repair. Reader comprehension is an empirical question that has not been tested with users.

## 6. Discriminating cases and expected results

These are review expectations applied to authored variants, not passed live operational tests.

| Case | Deliberately defective variant | Expected finding and relevant clauses |
|---|---|---|
| P1-A | Replace “no attachment in M1” with “no calibration occurred,” without a coverage argument. | Unsupported missingness inference; E200-013, C210-015. |
| P1-B | Count M1, S1 and C1 as three independent sources. | Shared derivation exposed; E200-012/022. |
| P1-C | Label calibration “unrealized” because its occurrence is retrospectively inferred. | Confuses modality with epistemic basis; E200-005, R110-012. |
| P1-D | Overwrite K1 with K2 and represent the earlier Panel as using K2. | Historical regime distinction lost; E200-017/018, R110-016/021. |
| P1-E | Mark D2 “funding restored” when only reassessment was scheduled. | Proposed action mistaken for completed remedy; C210-021/023, R110-022. |
| P1-F | Describe Reviewer A as independent without disclosing prior involvement. | Conflict and independence misrepresented; C210-009/024. |
| P1-G | Miss the stated review target with no notice; declare the case rejected by timeout. | Timing and disposition failures; C210-003/026. |
| P1-H | Publish contact details merely to make L1 inspectable. | Disclosure exceeds scenario authority; E200-023, R110-025/026, C210-027–029. |
| P1-I | Omit reported training loss from a supposedly complete account of capacity changes. | Material loss missing without collection/protection explanation; R110-002/017. |
| P1-J | Call the public summary conforming because its formatting validates. | Structural result overstated; E200-002/026, R110-030/034. |
| P1-K | Demand all history remain public forever, or claim an outside spreadsheet was deleted without evidence. | Retention and propagation limits ignored; R110-025/028, C210-028. |

## 7. Assessment limits and next evidence

This fixture supplies concrete content and counterexamples for clause review. It does not provide a complete conformance dossier. Required operational evidence such as actual assignments, acknowledgements, delivery, reviewer performance, access enforcement, deletion results, comprehension and resource burden remains **not assessed**. Unavailable original material limits evidential examination; restriction alone does not convert an unassessed requirement into fulfillment.

Next, designated participants can rehearse the procedure with this fictional material, record what actually occurs, maintain a requirement-by-requirement result register, and revise the draft where burden or ambiguity appears. A live pilot would additionally need a real institutional mandate, contextual legal/ethical analysis, participation arrangements, protection decisions and a scoped profile. No jurisdiction or external partner is selected by this example.
