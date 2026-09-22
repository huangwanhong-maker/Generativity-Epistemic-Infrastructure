# Preliminary Plan

> **Historical planning draft.** Retained to preserve the programme's earlier development reasoning. The [comprehensive development plan](comprehensive_development_plan.md) is the current planning proposal. The original text below remains unchanged, including the specification-first sentence in section 12; that sentence conflicts with accepted [ADR-0002](../../decisions/ADR-0002-standards-first-specifications-on-demand.md), which governs the current Standards-first sequence. Earlier maturity targets below are not reports of completed releases.

See also the project-level orientation in `PROGRAMME_BRIEF.md`, which summarizes why, what, and how the standards programme is being developed.

**Status:** PROVISIONAL  
**Version:** 0.1

## 1. Objective

Establish a lightweight NGO standards program capable of developing, publishing, maintaining, and revising a family of standards for generativity records and presentation.

## 2. Development phases

### Phase A — Standards governance

Deliverables:

1. Standards Programme Charter
2. Directives Part 1 — Development and Maintenance Procedures
3. Directives Part 2 — Structure and Drafting Rules
4. Technical Committee Charter
5. Public Review and Comment Procedure
6. Consensus and Decision Policy
7. Conflict-of-Interest and Ethics Policy
8. Intellectual Property Policy
9. Maintenance and Versioning Procedure
10. Records and Transparency Policy

### Phase B — Foundations

Deliverables:

- GR-100 — Foundations and Vocabulary
- GR-TR101 — Theoretical Foundations
- GR-TR102 — Category-Theoretic Semantics

Primary work:

- define the six candidate record ontology classes;
- define their relations to the deep metamodel;
- identify identity, persistence, time, and perspective semantics;
- specify open theoretical questions.

### Phase C — Core recording standard

Deliverables:

- GR-110 — Generativity Recording Requirements
- GR-M101 — Recording Manual
- worked examples.

Primary work:

- minimal and extended record profiles;
- provenance;
- generative dependencies;
- uncertainty;
- branches and alternatives;
- retrospective interpretation;
- contingent encounters;
- access metadata.

### Phase D — Information model and interchange

Deliverables:

- GR-120 — Information Model
- GR-130 — Interchange Specification
- schemas and validators.

Primary work:

- identifiers;
- record typing;
- graph semantics;
- temporal semantics;
- machine-readable formats;
- compatibility strategy.

### Phase E — Presentation and preservation

Deliverables:

- GR-140 — Presentation Requirements
- GR-160 — Preservation, Privacy, Access, and Retention.

Primary work:

- timeline and graph views;
- disclosure of mediation;
- public and restricted representations;
- retention and deletion;
- future interpretability.

### Phase F — Conformance and pilots

Deliverables:

- GR-150 — Conformance and Validation
- implementation test suite;
- pilot reports.

Candidate pilots:

- individual research process;
- AI-assisted scholarship;
- collaborative research project;
- institutional decision process;
- archival reconstruction.

## 3. Lightweight standards lifecycle

Proposed stages:

| Stage | Meaning |
|---|---|
| 00 | Preliminary |
| 10 | Proposal |
| 20 | Working Draft |
| 30 | Committee Draft |
| 40 | Public Review Draft |
| 50 | Final Approval Draft |
| 60 | Published Standard |
| 90 | Review / Revision / Withdrawal |

## 4. Initial organizational structure

### Standards Council

Responsibilities:

- procedural oversight;
- approval of work items;
- adoption and withdrawal of standards;
- appeals and governance.

### GR/TC 1 — Generativity Records and Infrastructure

Initial working groups:

- WG 1 — Foundations and Ontology
- WG 2 — Recording Model
- WG 3 — Data and Interoperability
- WG 4 — Presentation and Reconstruction
- WG 5 — Ethics, Privacy, and Preservation
- WG 6 — Conformance and Implementation

## 5. Minimum viable launch package

The first public-capable version of the program should have:

- governance charter;
- development procedure;
- drafting rules;
- work-item proposal template;
- public comment process;
- issue tracker;
- change log;
- decision log;
- one pilot standard project.

## 6. First pilot work item

Provisional title:

**GR/NWIP-001 — Generativity Recording and Presentation Infrastructure**

Expected result:

A coordinated set of technical publications rather than one monolithic standard.

## 7. Design principles

- standards should remain implementable;
- philosophical foundations should not be hidden;
- philosophical foundations should not overwhelm normative clauses;
- machine readability should be supported without forcing one storage backend;
- relations should be first-class where their own histories matter;
- revision should preserve historical states;
- presentation should disclose mediation;
- records should support uncertainty and absence;
- privacy should be separable from preservation;
- standards development itself should be traceable.

## 8. Early success criteria

The project reaches a useful v0.5 when:

- governance procedures are coherent;
- GR-100 has stable terminology;
- GR-110 can be implemented by at least one pilot recorder;
- GR-120 supports a machine-readable reference model;
- at least two independent pilot datasets can be exchanged;
- comments and objections can be traced to their resolution.


## 9. Foundation manuscript integration

The repository shall maintain a `foundation_manuscripts/` corpus.

Before stabilization of GR-100, the project should:

- collect representative foundational GR works;
- index them in `foundation_manuscripts/MANIFEST.md`;
- identify recurrent concepts and notation;
- record historical changes and unresolved tensions;
- map concepts to candidate standards clauses;
- avoid using one later manuscript to silently overwrite earlier conceptual history.

A later tooling phase may add manuscript-to-standard traceability tables.


## 10. Expanded conceptual programme

The standards programme should now be developed across at least the following coordinated layers:

- metamodel;
- record ontology;
- epistemic representation;
- normative and ethical interpretation;
- jurisprudential/legal interface;
- institutional governance;
- political economy;
- technical implementation.

The project should avoid assuming that the six candidate record ontology classes are sufficient for the entire programme.

## 11. Policy-proposal track

In parallel with standards development, the project may develop policy proposals for international organizations and other institutions.

The policy track should:

- draw from the standards work;
- remain organizationally distinct from technical standards;
- identify intended institutional mandates;
- distinguish factual analysis from normative proposals;
- present alternatives where appropriate;
- specify pilots, safeguards, and implementation pathways.


## 12. Publication-class separation

The programme should maintain separate development tracks for:

- Specifications;
- Standards;
- International Policy / Institutional Proposals.

Each document should declare its class and maturity status.

Candidate early technical work should normally begin as a Specification unless there is a clear reason to enter the full Standards track immediately.

## 13. Standards-first implementation sequence

For each substantive domain:

1. open or refine the relevant Standard work item;
2. define the normative need and conformance boundary;
3. identify infrastructure dependencies;
4. create Specification work items only where reusable technical precision is required;
5. implement pilots;
6. feed implementation results back into the Standard;
7. stabilize dependent Specifications;
8. advance the Standard toward public review and adoption.

This sequence should reduce premature infrastructure design while preserving implementation realism.
