# ADR-0005 — Expanded drafts and external publication models

**Status:** PROPOSED; implemented as reversible preparatory authoring  
**Date:** 2026-09-22  
**Class:** Architecture and editorial decision record  
**Related:** ADR-0004; Q-21–30

## Issue

The first nine Working Drafts contained 318 requirement identifiers across 84 compiled pages. They established a bounded programme, but compressed definitions, operational procedures, application conditions and assessment evidence. The user requested more substantive documents and a more formal style, with ISO, IEEE and UN-system publications as possible models. Increasing page count without developing those functions would not address that request.

## Alternatives

1. Retain 0.1 and change typography alone. This leaves important procedure and assessment detail underdeveloped.
2. Copy one organization's complete template and institutional model. This risks implying an authority the programme does not possess and importing requirements suited to another mandate.
3. Expand the existing nine drafts, preserve historical sources and stable requirement identifiers, and use a documented combination of verified editorial and institutional models.
4. Begin every planned GR publication immediately. This spreads source and review capacity across unresolved dependencies and contradicts the governance-first priority.

## Selected direction

Use alternative 3 for Working Draft package 0.2. Preserve the 0.1 sources, preamble, manifest and build/check scripts under `publications/archive/0.1/`, with hashes. Existing IDs remain stable. Dependency references change explicitly to the coordinated 0.2 editions. New obligations receive new IDs. This decision neither adopts governance nor promotes a draft to a Standard.

Use the ISO annotated model and accessible IEEE rules for formal structure and drafting discipline; use the ITU author guide as a UN-system technical-writing comparator; use the UN NQAF manual for comparison of institutional, process and output quality. The [source study](../docs/references/drafting_models.md) records distinct purposes, actual access and limits. UN/CEFACT catalogue verification establishes a relevant technical publication class, but inaccessible clauses are not reconstructed from memory.

## Operational changes and alternatives

| Proposed direction | Reason and rejected alternative | Affected documents |
|---|---|---|
| Add formal front matter, stable clause hierarchy, normative schedules and informative annexes | Gives readers identifiable scope, dependencies and assessment obligations; cosmetic expansion alone is insufficient | All nine drafts; shared preamble; D2 |
| Specify authority, resources, participation, delegation, review, maintenance, transitions and records | An institution cannot be established by writing a role name; fictional appointments or presumed capacity would obscure the development gate | Charter; D1; TC Charter |
| Specify conduct-case stages and distinguish evidence for findings from justification of remedies | A complaint label alone gives no workable response or protection process; automatic sanctions or unreasoned discretion are unsuitable | Ethics/IP policy |
| Propose 'more likely than not on the assessed evidence' for an internal substantiated conduct finding | Provides a reviewable local decision criterion; alternatives include a higher threshold, a context-specific schedule, or non-adjudicative findings only. The choice remains PROVISIONAL and needs governance/ethics review | EIP-069/070; Q-29 |
| Expand terminology, six-role conditions, epistemic methods and generative accounts without imposing a schema | Keeps source interpretation, conceptual roles and encoding distinguishable; a new universal fact object or numerical generativity measure would exceed the evidence | GR-100/110/200 |
| Specify case states, assessment and response, review capacity and dependent correction | Makes procedural action assessable; possession of a case record alone is insufficient | GR-210 |
| Treat semantic and assessment schedules as proposed normative material | Makes missing information and incomplete assessment visible; arbitrary implementation field names remain avoidable | D2; GR-110/200/210 and governance annexes |
| Preserve partial-assessment limits and exact dependency editions | Prevents a package or case-only check from standing in for unobserved institutional behavior | All conformance clauses |

The conduct evidence threshold is an internal programme proposal. It does not describe the standard of proof in any court or jurisdiction, establish external liability, or authorize a remedy without an identified institutional basis. Formal governance review can change it with a traced revision.

## Scope and conceptual continuity

The programme continues to address symbolic crisis through inspectable recognition, evidence, contestation, revision and practical action. Metamodel, record roles, epistemic assessment, normative evaluation, jurisprudence, governance and implementation remain distinct. Foundation manuscripts supply conceptual authority, not institutional adoption. The Property/Attribute issue, manuscript chronology and limits of fixed-carrier formal results remain unresolved.

New semantic schedules are operational proposals. Their record components are not additional ultimate constituents of reality. They allow justified missingness, protected references, uncertainty, plurality, maintenance and loss. Historical actors are not assigned knowledge or categories that appeared later.

## Consequences and review evidence

There are more obligations and therefore potentially greater recording and institutional burden. Q-26/28 require assessment of that burden and decisions about justified profiles. More pages, automated artifact checks and AI-assisted cross-review do not demonstrate stakeholder acceptance or successful implementation.

The [expanded review record](../docs/reviews/expanded_working_draft_review.md) records actual build results, comparisons and unresolved checks. Historical fixtures P0 and P1 retain their 0.1 basis until separately reassessed; they are not evidence of fulfillment of newly added 0.2 obligations.

Affected files comprise the nine TeX sources, shared presentation, publication manifest/navigation, build/check tools, requirement change register, archive, source study/register, Programme Brief development status, issue register, changelog and project index. Earlier ADRs and historical review findings remain available.
