# Substantive validation of Working Draft 0.2

**Class:** Informative review, fixture-assessment and revision record  
**Status:** PREPARATORY; 2026-09-22  
**Fixed baseline:** [Archived Working Draft package 0.2](../../../publications/archive/0.2/README.md)  
**Resulting publication:** [Coordinated Working Draft 0.3](../../../publications/README.md)

## 1. Result and limits

The cycle examined all **753 baseline requirement identifiers through 116 topical review groups**, produced new P0/v2 and P1/v2 fixtures, and identified five supported drafting corrections or clarifications. The current edition advances to 0.3 with **754 identifiers**, including one new conditional requirement concerning recirculation accounts. No publication, governance instrument or institution was adopted or certified by this work.

Editorial review, component assessment, arithmetic execution and institutional readiness are separate evidence categories. The review groups document what was read and compared; they do not mean 753 independent conformance tests passed. The source review is AI-assisted interpretation, not author approval of every operational narrowing or independent stakeholder validation.

The [requirement review matrix](requirement_review_matrix.csv) gives each baseline ID its exact archived source, hash, requirement text, review groups, findings and available P1 results. The [0.2 to 0.3 change register](../requirement_changes_0.2_to_0.3.csv) records actual requirement text changes. The matrix continues to describe 0.2; it is not silently relabelled as an assessment of 0.3.

## 2. Review streams

| Stream | Baseline coverage | Evidence and review record |
|---|---:|---|
| Governance | 462 requirements; 58 groups | [Review](governance_review.md), [group register](governance_review_groups.json), [P0/v2](../../../examples/validation_0.2/p0-governance-rehearsal.md) |
| Conceptual fidelity and generativity recording | 137 requirements; 35 groups | [Review](conceptual_review.md), [group register](conceptual_review_groups.json), actual manuscript passages and source-to-operational distinctions |
| Epistemic recording and contestation | 154 requirements; 23 groups | [Review](epistemic_review.md), [group register](epistemic_review_groups.json), [P1/v2](../../../examples/validation_0.2/p1-contested-decision.md) and [assessment register](../../../examples/validation_0.2/p1_assessment.json) |

The conceptual review covers recognition, mediated evidence, metamodel/record distinctions, identity, generative attribution, alternatives, possibility fields, maintenance/loss, recirculation and revision. It preserves the unresolved Property/Attribute relationship, manuscript precedence, formal emergence limits and the broader interpretation of generativity return. Exact manuscript passages and operational narrowings are recorded in that review.

## 3. Findings and revision dispositions

| Finding | Disposition in 0.3 | Verification and remaining limit |
|---|---|---|
| GOV-F01: invalid/unresolved attempted ballots disappear into nonresponse | D1-032/074 and the ballot dossier distinguish valid returns, attributable unsuccessful attempts and no attributable submission; separate unattributable/ineligible messages | Count partition and replacement cases preserve the original thresholds. Actual identity, validity review and participation remain untested |
| CF-01: absence of a mapping can incorrectly exempt a general safeguard | GR-100 Annex A separates the general V100-011 distinction from its conditional mapping component | A no-mapping example still receives the general check |
| CF-02: disputed attribution is mixed into occurrence/modality | R110-012 explicitly separates occurrence/modality, epistemic basis and assessment/dispute disposition | Realized effect, retrospective inference and disputed attribution can coexist without forced selection of one status |
| CF-03: generic return recording can conceal uncertainty in recirculation links | New R110-112 and the semantic/assessment schedules distinguish initial generation, conversion/transfer, return path and later-condition effect | Applies only when recirculation is asserted. Unknown links remain unresolved; no universal measure, entitlement or complete theoretical definition is introduced |
| EP-01: GR-210 can accidentally import blanket GR-100 conformity | Clause 2 identifies the incorporated terms, role meanings and result meanings, preserving the complete applicable GR-200 package obligation | No claim-only shortcut and no unstated vocabulary-certification requirement |

The [revision regression cases](../../../examples/validation_0.2/revision_regressions.md) make the corrected distinctions inspectable. Their outcomes are authored-text comparisons; they are not live process tests. [ADR-0006](../../../decisions/ADR-0006-validation-and-assessment-boundaries.md) records the alternatives, rationale and scope of the new operational requirement.

Of the 753 old requirement texts, 748 remain unchanged after whitespace normalization, three are revised, and two change only the GR-200 dependency edition. One new identifier is added. The GR-100 assessment-row and GR-210 dependency-prose corrections are substantive clarifications outside the requirement macros and therefore appear in this disposition table rather than as changed requirement text in the CSV.

## 4. What the rehearsals establish

### P0: numerical results and procedure design

The checker executed **18 authored count cases: 13 valid-count states and five malformed inputs**. All matched the independently authored expected results. Cases include quorum failure, insufficient substantive participation, approval boundaries, all abstentions, zero/small rosters, hypothetical recusal and invalid count types/totals. An invalid input is distinguished from a valid numerical defeat. These results do not prove participant eligibility, consensus, proper notice, fair review or an adoption decision.

The expanded narrative includes founding readiness, comment/reply/re-review, ballot replacement, recusal, unavailable appeal capacity, ethics reconsideration, source rights, release and maintenance. Its timelines are synthetic. The cycle did not conduct public review, appoint reviewers or wait through institutional deadlines.

### P1: documentary results and unperformed practice

The register contains **208 assessment rows**, explicitly divided into 54 selected components and 154 whole-requirement coverage results:

| Assessment level | Fulfilled | Not fulfilled | Not assessed | Not applicable |
|---|---:|---:|---:|---:|
| Selected documentary components | 53 | 0 | 1 | 0 |
| Whole GR-200/210 requirements in the declared fixture scope | 0 | 0 | 152 | 2 |

QA-01 corrected an initial failure classification under C210-094. P1 contains a proposed retention arrangement, not an identified operative retention/disposal decision; the assessor had inferred the trigger and incorrectly required a fixed period where an event-based timing rule can suffice. Both the component and whole-requirement results are now not assessed with unresolved applicability. The original classification and rationale for correction remain traceable in EP-03 and the fixture. The two inapplicability results concern absent comparison/reproducibility claim triggers under E200-035/041. The fulfilled components establish that identified information or distinctions are present in supplied text, not that fictional events occurred.

P1 now gives explicit scope, source dependence, claim and issue tracks, service periods, attributed grounds, a notice specimen, view limits, action ownership and failure branches. The original application, original log, instrument identity, notification receipts, actual review, funding outcome and effective repair remain unavailable or unperformed. The unresolved retention arrangement remains visible; no operative decision, authority or fixed period is invented to manufacture either a failure or a pass.

No entire GR-200 or GR-210 requirement receives a fulfilled result in this register. GR-100/110 source interpretation is covered separately; the P1 register does not assert full assessment of those documents. The new 0.3 recirculation requirement has a discriminating example and source review, but is not retroactively inserted into the 0.2 assessment totals.

## 5. Reproducibility and artifact checks

Run from the repository root:

```powershell
python tools/check_validation_package.py
python tools/compare_publication_requirements.py --baseline 0.2
python tools/build_publications.py --wsl Ubuntu-24.04
python tools/check_publications.py
```

The validation checker verifies baseline hashes, coverage membership, identifiers, assessment levels/results, fixture locators and numerical expectations. It writes [a generated report](../../../build/validation_0.2/check-report.json) and the requirement matrix. The report records the Python version and SHA-256 hashes of the checker, requirement extractor, registers, fixtures and baseline manifest. It does not automatically judge source fidelity, factual warrant or institutional performance. The authored reviews and assessments remain the evidence for those bounded judgments.

Publication builds and source/PDF checks are recorded in the [build report](../../../build/publications/build-report.json) and [publication check report](../../../build/publications/check-report.json). Generated files are ignored and need reproduction in a fresh checkout. Historical 0.2 results are preserved separately under `build/archive/0.2/`.

All nine 0.3 PDFs compiled successfully: **226 pages and 754 requirement identifiers**. The publication checker passed with current source, preamble and PDF hashes, retained historical identifiers and intact archives. There were no reported review warnings; GR-100 retains one nonblocking underfull-line spacing advisory. Visual inspection covered 24 rendered pages: the nine covers, nine foreword pages, the revised ballot clause, the GR-100 assessment schedule, three GR-110 return/semantic-schedule pages and the GR-210 dependency clause. No clipping or overlap was observed in those samples. The render plan and images are local generated artifacts under `build/review/v03/`; this sampling does not establish complete rendering or accessibility conformance.

## 6. Readiness and the next evidence

The [readiness and burden record](readiness_and_burden.md) distinguishes available preparation evidence from the actual authority, accepted responsibilities, independent competence, representation, rights and observed practice still needed. No elapsed participant effort, cost, fairness or accessibility measurements are invented.

Reply/ballot overlap and calendar-day boundary conventions remain governance choices requiring explicit treatment before use. Practical burden and proportionate profiles need observed tasks. A future GR-150 draft can use these findings to separate package, practice and procedure claims and state trigger rules, inherited obligations, permitted exclusions and evidence requirements. There is no reduced profile or automatic launch approval in this cycle.
