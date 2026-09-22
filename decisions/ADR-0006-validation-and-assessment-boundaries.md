# ADR-0006 — Validation of 0.2 and assessment boundaries

**Status:** PROPOSED; implemented in preparatory Working Draft 0.3  
**Date:** 2026-09-22  
**Class:** Architecture and operationalization decision record  
**Predecessors:** ADR-0004 and ADR-0005 remain historical decisions

## Issue and alternatives

The expanded 0.2 package needs substantive source and scenario review. Successful PDF production does not establish that requirements preserve distinctions, identify failures or work in an institution. The user authorized continuing with a substantive review matrix and updated P0/P1 assessments leading to a traced revision.

Alternatives considered were: expand the publication family immediately; claim full conformance from authored examples; or fix a baseline, review its provisions and source relationships, assess bounded synthetic components, and revise only supported defects and clarifications. This cycle selects the third alternative. It preserves the Standards-first development sequence and avoids treating a local fixture as an adopted information model.

## Selected direction

Preserve the nine 0.2 sources, presentation, manifest, tools, Programme Brief and review records with hashes. Review all baseline requirement identifiers through explicit topical groups, with full source text and locators in a coverage matrix. Keep editorial review, component assessment, numerical execution and institutional readiness distinct.

The term `reviewed` in the matrix means that the identified text received the stated editorial/source examination. It is not a conformance result. A selected P1 component can receive a documentary result while a procedural component remains not assessed. Group coverage does not establish that every obligation or application was exercised. The baseline remains 0.2 even after current documents become 0.3.

## Proposed changes

| Finding | Direction and rationale | Alternatives and limits | Affected material |
|---|---|---|---|
| GOV-F01: an invalid/unresolved submission can disappear into nonresponse | Distinguish valid counted ballots, eligible participants with attributable unsuccessful attempts and no counted ballot, and eligible participants without an attributable submission. Retain a separate inventory for unattributable/ineligible submissions | Counting invalid attempts as abstentions changes quorum; calling them silence erases attempted participation. No change to the two-thirds or three-substantive-vote rules | D1-032/074; ballot dossier; P0 |
| GR-210 dependency ambiguity | Import GR-100 terms, record-role meanings and result meanings within an explicit scope; preserve all applicable GR-200 package obligations | A separate blanket vocabulary-conformity claim adds obligations not clearly intended by the existing reference. Reduced claim-only GR-200 checking would conceal package obligations | GR-210 Clause 2 |
| CF-01: a general safeguard can be incorrectly marked inapplicable | Assess V100-011's general metamodel/record distinction independently of its conditional mapping component | Presence or absence of a mapping does not determine whether the general safeguard applies | GR-100 Annex A |
| CF-02: dispute disposition appears among effect modalities | Separate occurrence/modality, epistemic basis and assessment/dispute disposition explicitly | An actual occurrence can be retrospectively inferred and its contribution contested; these are compatible states, not competing options for one field | R110-012 |
| CF-03: recirculation link uncertainty can disappear within a generic return account | When recirculation is asserted, identify initial generation, conversion/transfer, return pathway and effect on later conditions, with stage/link-specific grounds and uncertainty | Do not define all generativity return as recirculation, require a universal quantity or infer entitlement. Missing links remain unresolved rather than invented | New R110-112; related assessment row and source rationale |

The recirculation provision is an operational narrowing for a particular claim. Its conceptual basis is FM-005, section 5.17.3, PDF page 145, which distinguishes the uncertainty inherited across these stages. It does not settle the broader definition of generativity return, its valuation or any distributive rule. The [conceptual review](../docs/reviews/validation_0.2/conceptual_review.md) preserves source limits and unresolved terminology.

## Baselines and evidence

P0/v2 and P1/v2 are new artifacts, not rewrites of the original fixtures. Numerical checks in P0 can be executed and reported as such. Fictional review, notification, custody and repair events remain fictional even when the narrative is internally consistent. A retained document can establish what the example says, but not that a real process occurred.

The current coordinated publication edition advances to 0.3. All historical IDs remain; changed text and dependency editions are separately recorded against 0.2. The new R110-112 is outside the 753-ID baseline matrix and has its own source and discriminating assessment route. No founding instrument, appointment, contribution licence, external partnership or standard adoption is created by this revision.

## Open matters and consequences

Actual authority, independent capacity, rights, representation, accessibility and burden remain evidence gaps. Calendar-day boundary conventions and possible overlap of reply and ballot windows need an explicit governance choice before use. A conservative sequential synthetic schedule does not silently adopt that choice.

The revision adds one conditional recording obligation and clarifies several assessment boundaries. Its practical cost remains unmeasured. A future GR-150 profile proposal uses observed evidence and states precisely which subject and parent obligations it addresses; it does not convert this cycle's partial assessment into full conformity.

Affected documents are D1, GR-100, GR-110 and GR-210 substantively, the coordinated nine-document edition and dependencies, validation artifacts, archive, comparison/check tooling, issue register, Programme Brief development status, publication navigation and changelog.
