# Expanded Working Draft 0.2 review

**Class:** Informative editorial and artifact review record  
**Status:** PREPARATORY; 2026-09-22  
**Package:** [Archived nine Working Drafts, version 0.2](../../publications/archive/0.2/README.md)  
**Review relationship:** AI-assisted drafting and cross-review in the same development session. This is not independent stakeholder review, a formal ballot or a finding of institutional conformance.

## 1. Scope and source basis

The revision responds to the need for substantive application detail and formal publication structure. It develops all five existing governance drafts and the four initial technical drafts. It does not open additional work items or adopt the instruments. The [0.1 archive](../../publications/archive/0.1/README.md) preserves original sources and tools with hashes.

The [external-source study](../references/drafting_models.md) distinguishes editorial models from quality frameworks and policy instruments. ISO's annotated model, accessible IEEE rules and ITU authoring guidance support publication design; UN NQAF and UNESCO AI ethics materials provide bounded institutional comparisons. Actual access and failed retrievals are recorded. The theoretical basis remains the GR manuscript corpus, with passage-level locators in each technical document.

Particular source attention was given to FM-005's generative attribution, trajectories, evidence survival, reconstruction, uncertainty, epistemic justice, machine mediation and revisability; FM-003's praxis and limits of a fixed intervention sequence; and FM-007's operational roles and conceptual evolution. These readings do not resolve the Property/Attribute question, chronology of Chapter 3 variants or stronger emergence claims beyond the formal assumptions.

## 2. Substantive changes

| Document | Added depth and review purpose |
|---|---|
| Programme Charter | Actual authority and resources, delegation and reserved decisions, participation barriers, accountability, transitions, readiness and evidence |
| Directives Part 1 | Work-item and stage dossiers, review preparation and disposition, ballots and clocks, independent appeal capacity, emergency action, maintenance and release controls |
| Directives Part 2 | Formal architecture, definition and requirement construction, dependency precision, semantic/protocol boundaries, normative assessment records and editorial completion checks |
| TC Charter | Assignment, source and decision registers, multidisciplinary review, pilot preparation, failure reporting, handover and maintenance |
| Ethics/IP Policy | Intake, protection, investigation, response, reasoned findings, proportionate remedies, reconsideration, conflicts, impacts, contribution rights and assessment |
| GR-100 | Numbered terminology, six operational roles with identity and temporal conditions, scale, continuity, mappings, missingness and vocabulary assessment |
| GR-110 | Purpose and collection planning, generative accounts and evidence, conditions/contributions/alternatives, maintenance and loss, practical effects, custody and assessment |
| GR-200 | Claim decomposition, methods, source dependence, material survival, interpretation, uncertainty, revision, machine mediation, protection and normative semantic schedules |
| GR-210 | Case intake and state transitions, responsible review, competing accounts, response and disposition, corrections and practical effects, review limits and assessment schedules |

The [requirement change register](requirement_changes_0.1_to_0.2.csv) reports text-level changes against the archived edition. It preserves old and new texts and their normalized hashes. Stable IDs identify provisions independently of their new clause locations. No ID was intentionally removed or reassigned.

## 3. Cross-review findings and dispositions

| Finding | Correction and remaining limit |
|---|---|
| Sole requirements appeared in Scope while D2 required a declarative scope | Relocated the relevant Charter, D1, TC and EIP provisions into application or authority clauses, preserving their identifiers |
| Some D2 provisions were under unrelated headings after restructuring | Moved epistemic distinctions, conformance limits, automated checks, bindings and artifact-execution rules to their corresponding clauses |
| Ethics acknowledgement and determination periods were described collectively as extendible targets | Revised EIP-014 to distinguish mandatory seven-day acknowledgement from the extendible determination target; this is an explicit revision of a 0.1 obligation |
| Temporary participation restrictions had unclear review/expiry linkage | EIP-067 expressly applies the emergency review and expiry controls, without creating additional institutional authority |
| Internal conduct threshold and merits reconsideration needed limits and a usable route | EIP-069 states a reasoned criterion, contrary evidence and fairness prerequisites; EIP-099–104 specify scoped reconsideration and assessment. The threshold remains a local proposal under Q-29 |
| Some definition entries blended definitions with examples and implications | Separated explanations into notes in the core vocabulary and epistemic drafts |
| One result per ID could conceal failure of a component within a grouped requirement | Added component-result rules in the core assessment schedules; every applicable component needs evidence for overall fulfillment |
| Unknown/restricted values could be read as a waiver of essential record structure | Distinguished required identifiers, content and scope from contextual facts that can be explicitly unknown or protected; an allowed declaration does not prove the fact was examined |
| A timestamp example overstated proof of recording time | Qualified it by source attribution and clock/integrity/context limits |
| Repeated long-table headers repeated their caption labels and produced compiler warnings | Separated first-page captions from continuing column headers; checked the revised cross-references and renderings |

No review finding establishes beneficial generativity, truth of a claim, legal authority or an operating institution. Shared-session AI cross-review is not the independent human review required by the proposed governance.

## 4. Artifact verification

The full [build report](../../build/archive/0.2/build-report.json) records compiler outcomes, source/preamble/PDF hashes, warnings and tool versions. The [check report](../../build/archive/0.2/check-report.json) records structural checks, archive integrity, retained IDs, local reference resolution, freshness and PDF text coverage. These historical reports were copied before revision to 0.3; current results are recorded in the substantive validation cycle.

All nine documents compiled successfully through WSL/LuaLaTeX. The checker passed with **753 requirement identifiers across 225 PDF pages**, compared with 318 identifiers and 84 pages in 0.1. There were no duplicate or unresolved identifiers, missing requirement text, stale artifact hashes, changed archive hashes, removed historical IDs, overfull boxes, missing glyphs or detected LaTeX/package warnings.

| Document | 0.1 pages | 0.2 pages | 0.1 requirements | 0.2 requirements |
|---|---:|---:|---:|---:|
| GSP-Charter | 9 | 19 | 41 | 85 |
| GSP-Directives-1 | 13 | 27 | 58 | 95 |
| GSP-Directives-2 | 8 | 26 | 45 | 95 |
| GR-TC1-Charter | 7 | 17 | 30 | 83 |
| GSP-Ethics-IP | 8 | 22 | 40 | 104 |
| GR-100 | 13 | 30 | 12 | 26 |
| GR-110 | 8 | 28 | 34 | 111 |
| GR-200 | 11 | 32 | 26 | 49 |
| GR-210 | 7 | 24 | 32 | 105 |

All 318 historical IDs remain: 315 requirement texts are unchanged after whitespace normalization, two advance the GR-200 dependency edition, and EIP-014 explicitly corrects the acknowledgement/target distinction. There are 435 new IDs. Clause relocation and non-requirement prose changes are not classified as requirement text changes by the CSV tool.

Formal-structure checks cover opening clauses, front matter, annex status, table captions and local references. The archive and PDF hashes were verified. Local Markdown targets resolved, and `PROJECT_INDEX.txt` lists 108 source/corpus files. These checks are artifact evidence, not fulfillment of all D2 technical and institutional assessment duties.

Visual review sampled each document's cover, opening Scope page and first normative annex, plus D2's assessment schedule, GR-100's record-role and source tables, and GR-200's assessment schedule. The 31-page selection is recorded in `build/review/v02/render-plan.json`; rendered pages and contact sheets are stored beside it. The inspected samples showed readable headings, intact tables and no observed clipping or overlap. One nonblocking underfull-line spacing advisory remains in GR-100's source-map introduction; its rendered page was inspected. No tagged-PDF, screen-reader or affected-user comprehension assessment was performed.

The final toolchain is recorded in the build report: Python 3.12.3, latexmk 4.83 and LuaHBTeX 1.17.0 (TeX Live 2023/Debian) under Ubuntu-24.04/WSL. Builds use shell escape disabled and fetch no external dependencies. Text extraction checks use the Windows Python review environment with pypdf. Source-word counts in the machine report include TeX markup and repeated table headers, and are not represented as prose word counts.

## 5. Remaining assessment and institutional work

P0 and P1 remain historical 0.1 fixtures. Their arithmetic and authored examples do not assess newly added obligations or demonstrate performance of an actual committee, recording practice, review service or remedy. The initial templates likewise do not substitute for the 0.2 normative schedules.

Before adoption, work remains on actual founding authority, accepted appointments, representation, independent-review capacity, rights terms and a governed rehearsal. Before live operational claims, work remains on contextual legal/ethical review, burden and resource measurement, observed procedure tests, affected-user understanding, accessibility and justified profiles. Current TeX/PDF production does not certify tagged-PDF or screen-reader accessibility.

More detailed drafts expose decisions that can now be reviewed; they do not settle those decisions by their length or typography. [ADR-0005](../../decisions/ADR-0005-expanded-drafts-and-source-models.md) and Q-27–30 retain the proposed choices and alternatives.
