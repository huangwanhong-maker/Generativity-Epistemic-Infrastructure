# ADR-0003 — Epistemic and Practical Development Architecture

**Document class:** Architecture Decision Record  
**Status:** Proposed  
**Date:** 2026-09-22

## Context

The programme initiator confirmed that symbolic crisis and the conditions of warranted recognition and practical action are the organizing motivation. Existing programme documents support these concerns but foreground archival loss and AI provenance. The previous plan postpones some epistemic, presentation, protection and institutional work until after recording and information-model development.

The initiator requested comprehensive planning for standards, protocols and specifications. This authorizes preparing a reviewable plan; it does not itself adopt technical Standards, establish legal authority, or settle every proposed allocation below.

The initiator further specified comparison with established approaches such as ISO and IEEE and substantive coverage of governance, jurisprudence, public policy, ethics and related domains. Those domains require their own inquiry and review, not merely technical metadata or an adoption-stage checklist.

Accepted ADR-0002 already establishes Standards-first, Specifications-on-demand development. The specification-first statement in the historical preliminary plan is inconsistent with that accepted decision.

## Proposed direction

1. Use epistemic and practical infrastructure as the programme-level purpose. Treat archival preservation and AI provenance as important components of it.
2. Develop a bounded initial cycle spanning recording, evidence, contestation, revision, presentation, protection and practical response. Preserve the distinction between changes in the situation, knowledge, and descriptive regime.
3. Develop protocols within Specifications as behavior involving participants, states, exchanges, transitions and failures; protocols can also be specified in an adopted Standard. Do not create a fourth publication authority class or equate institutional procedures with network protocols.
4. Preserve existing publication codes. Clarify Standards-track requirements versus Specification implementation responsibilities for GR-120/130/200. Open additional work items only when a distinct need is demonstrated.
5. Use source traceability, staged governance, pilots and explicit conformance subjects to determine readiness. Retain the earlier plan as historical evidence and use the comprehensive plan for current proposed sequencing.
6. Compare ISO/IEC and IEEE development mechanisms and document proportionate local choices. Maintain a cross-domain programme connecting motivations, substantive questions, publication roles, reviewers and evaluation; distinguish public-policy and legal proposals from technical conformance.

This is the selected direction for the present planning draft. The architecture proposal remains subject to recorded review; the confirmed mission framing and accepted ADR-0002 are distinguished from newly proposed scope and gates.

## Alternatives considered

| Alternative | Reason not selected for this planning draft |
|---|---|
| Begin with an exhaustive six-class schema | Settles implementation before epistemic and institutional needs; risks treating record forms as ontology |
| Complete the recording model before adding epistemic and procedural work | Does not test the confirmed mission in the first usable cycle |
| Create a separate publication family for every protocol | Confuses a technical subject with publication authority and adds maintenance overhead |
| Attempt the complete standards portfolio in the first release | Exceeds demonstrated capacity and delays evaluation of a useful bounded cycle |
| Replace the earlier plan with a clean narrative | Removes evidence of the programme's changing emphasis and contradictory sequencing |
| Copy an established standards body's procedures wholesale | Imports organizational and resource assumptions without establishing their fit or authority |
| Treat legal, ethical and policy questions only as metadata | Omits substantive conflicts, justification, institutional options and practical consequences |

## Rationale

GRE Chapter 1 links symbolic production, referential reliability, classification and practical trust. Chapters 3–5 distinguish generative effects, reality, manifestation, evidence, interpretation and intervention. Metamodel Part II distinguishes formation, description and institutional uptake as well as possible transformations, occurrences and records. These sources motivate the proposal without directly conferring normative authority.

The [foundation traceability map](../docs/planning/foundation_traceability.md) records passages, operational proposals and unresolved differences. The [comprehensive plan](../docs/planning/comprehensive_development_plan.md) supplies the portfolio, dependencies, evidence and work queue.

## Consequences

- Initial pilots evaluate an epistemic and practical cycle, not only record serialization.
- Presentation, access, contestation and conformance become early design concerns.
- Technical choices remain conditional on demonstrated requirements.
- Proposals for legal profiles, metrics and stronger formal semantics remain explicitly unresolved or deferred.
- The plan does not certify historical truth, the goodness of generativity, or institutional legitimacy.

## Affected documents

- `PROGRAMME_BRIEF.md` and `README.md`
- `docs/framework/publication_taxonomy.md`
- `docs/planning/comprehensive_development_plan.md` and supporting planning documents
- `docs/planning/preliminary_plan.md` and `docs/planning/open_questions.md`
- `foundation_manuscripts/MANIFEST.md`
- `specifications/README.md`
- `CHANGELOG.md` and `PROJECT_INDEX.txt`

## Supersedes

No accepted ADR. ADR-0001 and ADR-0002 remain in force. The comprehensive plan replaces the earlier preliminary plan only as the current planning proposal; the historical text is retained.

## Superseded by

None.
