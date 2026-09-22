# ADR-0008 — Record graphs, atomic transactions and modules

**Status:** PROVISIONAL; selected for experimental implementation  
**Class:** Architecture and scoped operationalization decision  
**Date:** 2026-09-22  
**Extends:** [ADR-0007](ADR-0007-git-backed-project-workspace.md)

## Issue and sources

The initial workspace provides separately versioned records and untyped cross-references but cannot faithfully express multi-participant relations, relations on relations, coordinated graph changes, modular file custody or snapshot-consistent graph inspection. The user requested careful design and implementation of those functions before a later academia upgrade, and authorized corresponding specification/draft revisions. Current GR manuscripts take precedence over conflicting older academia design choices.

The [graph source review](../docs/planning/graph_semantics_review.md) cites actual FM-007 passages on overlapping roles, role-indexed incidence and relation continuity, together with GR-100/110/200 requirements. The [academia comparison](../docs/reviews/academia_application_reference.md) identifies useful modular panels and reified connections, plus implementation gaps and profile differences. Source conflicts remain explicit; old records and signed payloads are not revised retrospectively.

## Alternatives and selected direction

| Decision | Alternatives considered | Selected direction and rationale |
|---|---|---|
| Graph connections | Anonymous binary edges; every cross-reference promoted to a relation; first-class relation records | First-class Relation records with role-indexed incidence preserve identity, several participants, relations on relations and modules. Old cross-references stay neutral. |
| Role typing | One exclusive record class; unrestricted untyped tags; overlapping declared roles | Preserve primary display role and add an explicit role set, following the current manuscripts without equating record IDs with targets. |
| Revision protocol | Sequential REST writes; database content transactions mirrored later; one complete Git tree per transaction | Build/validate a whole candidate graph and publish one Git reference conditionally, so references and files are never partially accepted. |
| Retry evidence | In-memory token cache; SQLite receipt; receipt inside the accepted Git tree | A committed receipt remains recoverable after restart, export and a lost response without introducing a second content authority. |
| File custody | Mutable named paths; external content store; bounded retained Git blobs | Git blobs keep descriptors, bytes and history portable and atomic at this scale. A future external store needs an explicit completeness/recovery protocol. |
| Module extension | Hardcoded exclusive screens; executable user plugins; versioned declarative record modules | Fixed built-in interpreters with namespaced data preserve safe extensibility. Unknown optional modules remain intact and read-only; required unknown constraints prevent mutation. |
| Historical display | Old selected record mixed with current neighboring data; whole-project pinned view | One source commit resolves graph, labels, modules and downloads. View filtering/layout remains explicitly separate. |
| Academia alignment | Replace general roles with GRRP acts; rewrite academia immediately; shared substrate with later explicit adapter | Develop the generalized application first; preserve academia's native history and document mappings before changing its independent interface. |

The full selected contract, limits, failure cases and sequence are in [the implementation design](../docs/planning/graph_workspace_design.md). Experimental Specifications provide the concrete representation and Git binding. Existing 0.3 conceptual clauses remain unchanged where sufficient; preliminary model/interchange/presentation/preservation work items receive the newly identified implementation-neutral requirements.

## Consequences and limits

The semantic graph can be cyclic even though the initial storage branch is a linear commit history. A relation predicate, preserved file, structural validation or Git hash does not establish causation, evidential sufficiency, truth, target identity or institutional authority. Module changes and file detachment retain older content under the declared custody policy; they do not claim erasure.

The project-level conditional update deliberately rejects unrelated concurrent changes too. Prepared but unpublished objects can remain unreachable. Initial file/operation/history budgets are implementation limits, not GR requirements. Cross-project protected references, large external assets, shared membership, independent signatures, semantic merging and governed erasure remain further work.

Migration is an explicit new commit, never a read-time mutation or historical rewrite. Existing unstructured relations and untyped references retain their scope. Shared package availability is not evidence that academia already conforms or that independent interchange has passed.

## Affected artifacts

The independent protocol package and schemas; GR-SPEC-120/130; bounded working drafts for GR-120/130/140/160; generalized web application, Git adapter and tests; source reviews, application documentation, changelog, issue register and index. The academia repository remains a read-only reference in this phase.
