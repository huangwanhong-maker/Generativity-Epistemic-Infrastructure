# ADR-0004 — Initial working drafts and bounded operational requirements

**Document class:** Architecture Decision Record  
**Status:** Proposed; implemented provisionally for drafting and review  
**Date:** 2026-09-22  
**Related decisions:** Accepted [ADR-0002](ADR-0002-standards-first-specifications-on-demand.md); Proposed [ADR-0003](ADR-0003-epistemic-praxis-development-plan.md)

## Issue and authorization

The programme initiator requested that development begin according to the comprehensive plan and authorized TeX sources and WSL compilation. The repository's governance-first rule requires coherent procedures before formal technical project initiation. Preparing substantive drafts and synthetic cases is authorized; authoring does not appoint a committee, adopt governance, approve GR/NWIP-001, settle manuscript disagreements, or publish an adopted Standard.

The earlier Markdown files are valuable scope outlines but are insufficient to assess obligations, responsibility, evidence, and failure behavior. An implementation-first schema would also prematurely settle the representation of claims, evidence, and the six record roles.

## Alternatives

1. Draft governance alone and defer all technical clauses. This respects sequence but gives little concrete material with which to test drafting rules or discover conceptual problems.
2. Produce the entire technical family and schema now. This spreads review capacity thinly, conceals unassigned institutional responsibilities, and risks premature ontological and protocol decisions.
3. Produce a governance package plus a bounded preparatory vocabulary/recording/evidence/contestation cycle, assess synthetic cases, and derive later infrastructure needs from those requirements.

## Provisional direction

Use alternative 3 for this authoring cycle. Prepare five governance Working Drafts and GR-100, GR-110, GR-200 and GR-210, with stable clause identifiers and assessment guidance. Preserve publication numbers and historical outlines. Use descriptive titles for governance documents; build filenames are not additional adopted publication codes.

The canonical substantive text for these editions is TeX. Shared presentation lives in `publications/preamble.tex`; a manifest drives repeatable WSL/LuaLaTeX compilation. Markdown supplies navigation, examples, review records, templates and decisions. PDFs are generated review artifacts, excluded from source control by default but available locally. Source and PDF hashes identify the built package. Compilation and text inspection are distinguished from substantive conformance, accessibility assurance and formal release.

### Operational choices and their bounds

| Choice | Rationale and alternatives | Source or issue | Affected clauses |
|---|---|---|---|
| Keep target, manifestation, representation, claim, evidence role and assessment distinguishable without selecting schema classes. | A single fact/truth field hides attribution and competing warrants; a mandatory class hierarchy is premature. | FM-004 §§4.2/4.6; FM-005 §5.4; Q-06 | GR-100 definitions; E200-003–016 |
| Use six operational roles with scoped identity and admissibility. | Roles can overlap; they are neither metaphysical atoms nor an asserted one-to-one interpretation of weak formal states/mechanisms. Property/Attribute remains unresolved. | FM-007 §§7–12; Q-05/09 | GR-100 role definitions and V100 requirements; R110-006–009 |
| Separate occurrence/modality from epistemic mode. | A realized event can be retrospectively inferred. Planned/possible and reported/inferred answer different questions. | FM-005 evidence distinctions; conceptual cross-review | GR-100 Event/Process; E200-005; R110-012 |
| Require scoped, attributed generative accounts with grounds, alternatives, maintenance and loss. | A universal generativity score would exceed the current sources and evidence. Output count alone is insufficient. | FM-005 §5.14; FM-003 §§3.10/3.12; Q-10/16 | R110-010–018; E200-024 |
| Preserve changes in situation, evidence/assessment, and descriptive regime as distinguishable revision reasons. | Silent overwrite erases earlier knowledge and consequences. More than one reason can apply. | FM-005 §5.20; FM-007 §§16/19; Q-07 | E200-017–020; R110-019–022; C210-017/020/025 |
| Treat challenge handling as an accountable institutional procedure with a separate case record. | Supplying challenge fields does not establish meaningful receipt, review, action or remedy. This is a new operational proposal, not a claim that manuscripts prescribe this exact procedure. | FM-003 §3.12; FM-005 §§5.18–5.20; Q-14/17 | E200-021; R110-023; C210-001–032 |
| Permit bounded unknowns, restrictions and lawful disposal while recording their limits. | An immutable public history can expose protected information; removal without any permitted disposition can conceal consequential changes. Neither extreme is mandatory. | Programme privacy/jurisprudential discipline; Q-11 | E200-023/026; R110-024–030; C210-027–029 |
| Make GR-200 package obligations an explicit dependency of material-claim packages in GR-110/210. | Claim-level field checks alone miss package scope, assessment and transformed-view obligations. | Cross-document review; Q-12 | R110-005; C210-030 |
| Treat package-only/case-only reviews as partial documentary assessments in this edition. | A document cannot prove that a procedural action occurred. No reduced conformance profile is yet established; unobserved behavior is not assessed, not automatically inapplicable. | Q-12/14 | R110-032–034; C210-030–032 |

Page-level source locators and limitations appear in the TeX source rationale and [foundation traceability map](../docs/planning/foundation_traceability.md). FM-002/FM-003 chronology remains unresolved; this decision does not reconcile those manuscripts or resolve the formal issues in Q-09.

### Governance choices

The Charter proposes an explicit founding instrument grounded in the signatories' actual authority, with accepted appointments, review evidence and declared limits. The committee is not treated as already constituted. D1 proposes 60-day public review, 30-day substantive re-review, a two-thirds returned-ballot quorum, at least three substantive votes and two-thirds approval of substantive votes. These are local proposals with alternatives and rationale, not imported ISO/IEEE rules. Separate Specification and policy release routes preserve their classes. Numerical defaults, representativeness, rights terms and independent-review capacity remain open before adoption.

## Consequences and assessment

- Governance M1 remains incomplete until real authority, appointments, rights, review and adoption evidence exist. Technical documents remain preparatory Working Drafts.
- The first fixture is fictional; it is usable without collecting personal data. It tests distinctions and failure expectations, not live institutional effectiveness.
- No network protocol, canonical schema, universal confidence scale, universal generativity metric or legal entitlement is selected.
- GR-120/130/140/150/160/300/310/400 remain coordinated future work. Their contextual constraints appear in this draft cycle, but these four documents do not replace those projects.
- Implementation trials can now identify concrete Specification needs. A technical binding follows a demonstrated requirement and its own work-item decision.
- TeX/PDF accessibility is not established by a successful build. The initial source format and inspection limitations are recorded in the review report.

## Affected artifacts and acceptance

Affected artifacts are the nine documents in [the publication manifest](../publications/documents.json), publication tooling, navigation, templates, the synthetic case, review evidence, issue register and changelog. Existing historical outlines and earlier ADR reasoning remain retained.

Acceptance of this ADR requires a recorded architecture decision by the programme's responsible authority. The user has authorized authoring; this record remains Proposed so that the operational choices remain available for explicit review rather than being represented as already approved normative interpretations.
