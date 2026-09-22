# Publication Taxonomy
## Specifications, Standards, and International Political / Policy Proposals

**Status:** PROVISIONAL  
**Document class:** Informative programme framework  
**Purpose:** Define the principal publication classes of the Generativity Standards Program.

## 1. Why the distinction matters

The programme produces documents with different kinds of authority.

A technically precise document is not automatically a standard.

A formally adopted standard is not automatically law or public policy.

A proposal addressed to an international organization is not a technical conformance document.

Accordingly, the programme distinguishes at least three primary publication families:

1. **Specifications**
2. **Standards**
3. **International Political / Policy Proposals**

The three families may reference one another, but their authority, review process, and intended use remain distinct.

---

## 2. Specification

### 2.1 Definition

A **Specification** defines a technical, informational, procedural, semantic, or interoperability model with enough precision to be implemented, tested, or compared.

A specification may be:

- experimental;
- provisional;
- candidate;
- stable;
- implementation-oriented.

A specification may contain normative technical language such as `shall`, `should`, and `may`, but it does not become a programme Standard merely by doing so.

### 2.2 Typical purposes

Specifications may define:

- data models;
- ontologies;
- record structures;
- schemas;
- interchange formats;
- API behavior;
- identifier rules;
- conformance tests;
- mathematical or computational semantics.

### 2.3 Authority

A specification has **technical authority within its declared scope**, but not the institutional status of an adopted Standard unless formally promoted through the standards-development process.

### 2.4 Examples

Possible examples include:

- Generativity Information Model Specification
- Generativity Interchange Specification
- Epistemic Record Specification
- Validation Specification

### 2.5 Maturity path

A specification may remain a specification indefinitely, or may later become part of a Standard.

A possible promotion path for a specification is shown below. It is not the programme's default starting sequence; section 5A and accepted ADR-0002 establish Standards-first development.

\[
\text{Research Draft}
\rightarrow
\text{Specification}
\rightarrow
\text{Pilot Specification}
\rightarrow
\text{Candidate Standard}
\rightarrow
\text{Standard}
\]

Promotion is not automatic.

### 2.6 Protocols and procedures

A **protocol** specifies behavior among participants: roles, inputs, interactions, states, transitions, outcomes, failure handling and applicable obligations. A protocol can be described in a Specification and referenced by a Standard. Protocol is a technical subject, not an additional publication authority class.

Institutional procedures describe responsibilities, grounds for action, review, contestation and disposition. Machine protocols describe interactions that implementations can execute and test. A network exchange does not itself establish the institutional authority or evidential warrant of the act it carries.

Schema validity alone does not demonstrate protocol conformance. Protocol assessment can examine allowed transitions, authorization, duplicate or conflicting requests, unsupported features, disclosure boundaries and failure behavior, according to the declared scope.

The candidate allocation to existing publication families is in the [protocol and specification plan](../planning/protocol_specification_plan.md). New protocol bindings are proposed only when draft requirements identify a reusable technical need.

---

## 3. Standard

### 3.1 Definition

A **Standard** is a formally adopted normative publication developed according to the programme's standards-development procedures.

A Standard specifies requirements, recommendations, permissions, terminology, procedures, or conformance conditions intended to support consistent practice across multiple independent users or institutions.

### 3.2 Minimum conditions

A document should normally become a Standard only after:

- approved work-item initiation;
- defined scope;
- technical review;
- relevant horizontal review;
- public review;
- comment disposition;
- demonstrated implementation or pilot evidence where applicable;
- formal approval;
- assigned edition and maintenance responsibility.

### 3.3 Authority

A programme Standard establishes **programme-level normative authority** for conformance.

It does not by itself create:

- statutory law;
- treaty obligations;
- international legal obligations;
- governmental duties;
- jurisdictional authority.

External institutions may voluntarily adopt, reference, incorporate, or legally recognize a Standard through their own procedures.

### 3.4 Examples

Possible examples include:

- Generativity Recording Standard
- Generativity Preservation and Access Standard
- Generativity Conformance Standard
- Institutional Generativity Record Standard

---

## 4. International Political / Policy Proposal

### 4.1 Definition

An **International Political / Policy Proposal** is a non-standard publication that presents policy, governance, institutional, legal, or cooperative options for consideration by international organizations or other transnational public-interest actors.

For formal publication, the programme should normally use the label **International Policy Proposal (IPP)** or **International Institutional Proposal (IIP)**. The broader phrase “international political proposal” may describe the family, but should not imply partisan advocacy.

### 4.2 Typical purposes

Such proposals may address:

- institutional adoption of generativity-record practices;
- international pilot programmes;
- research-integrity infrastructure;
- AI provenance and accountability;
- cultural and scientific preservation;
- institutional memory;
- capacity-building;
- cross-border interoperability;
- model governance procedures;
- international cooperation mechanisms.

### 4.3 Authority

An international policy proposal is **recommendatory and deliberative**.

It does not define technical conformance merely by recommending a practice.

It does not acquire legal or political authority unless adopted through the procedures of the relevant organization or jurisdiction.

### 4.4 Required distinctions

Each proposal should clearly distinguish:

- factual background;
- analytical interpretation;
- normative objectives;
- policy options;
- legal or mandate constraints;
- implementation implications;
- technical dependencies;
- unresolved questions.

Where more than one reasonable institutional option exists, proposals should describe the alternatives without presenting one as technically inevitable.

### 4.5 Relationship to political neutrality

The programme may formulate institutional options, model procedures, implementation pathways, and comparative consequences.

Policy proposals should avoid converting a technical standard into a claim that one political choice is mandatory.

---

## 5. Relationship among the three families

The relationship can be represented as:

\[
\text{Specification}
\longrightarrow
\text{possible technical basis}
\longrightarrow
\text{Standard}
\]

and separately:

\[
\text{Specification or Standard}
\longrightarrow
\text{possible input}
\longrightarrow
\text{International Policy Proposal}.
\]

Neither arrow implies automatic promotion or adoption.

A policy proposal may recommend use of a Standard.

A Standard may reference a Specification.

A Specification may implement concepts motivated by policy needs.

The three remain separate publication classes.

---


## 5A. Default development direction

The programme generally begins from a Standard work item.

The Standard defines the normative objective and requirements. When those requirements reveal a need for reusable technical infrastructure, the programme initiates one or more Specifications.

Typical dependency:

Standard requirement
→ technical need
→ Specification

During development, feedback may flow in both directions:

Standard Working Draft
↔ Specification + Pilot

Thus, Specifications are subordinate in purpose but may be essential to demonstrating that the Standard is implementable.


## 6. Authority matrix

| Publication class | Technical precision | Conformance authority | Formal adoption by programme | Policy recommendation | Creates law |
|---|---:|---:|---:|---:|---:|
| Specification | High | Within declared spec scope | Not necessarily | Usually no | No |
| Standard | High | Yes | Yes | Not primarily | No |
| International Policy Proposal | Variable | No | Published, not standards-adopted | Yes | No |

---

## 7. Proposed identifiers

The programme may use class-sensitive identifiers:

- `GR-SPEC-xxx` — Specification
- `GR-STD-xxx` — Standard
- `GR-TR-xxx` — Technical Report
- `GR-G-xxx` — Guide
- `GR-M-xxx` — Manual
- `GR-IPP-xxx` — International Policy Proposal
- `GR-IIP-xxx` — International Institutional Proposal

Existing `GR-100`, `GR-110`, etc. numbers may be retained as project-family numbers while publication-class prefixes identify the document's current status.

Example:

- `GR-SPEC-120:2027` — Generativity Information Model Specification
- `GR-STD-110:2028` — Generativity Recording Standard
- `GR-IPP-001:2028` — International Research Provenance Infrastructure Proposal

---

## 8. Promotion and cross-reference rules

### Specification to Standard

Promotion requires an explicit standards-development decision.

A Specification should not silently become a Standard through editorial renaming.

### Standard to policy use

A policy proposal may recommend adoption or piloting of a Standard, but should specify:

- intended institution;
- mandate;
- expected benefits;
- costs and burdens;
- legal implications;
- safeguards;
- alternatives.

### Policy proposal to Standard

Political or institutional preference alone should not determine technical conformance requirements.

If a policy proposal identifies a new technical need, that need should enter the standards process as a separate work item.

---

## 9. Repository placement

Recommended repository structure:

- `specifications/`
- `standards/`
- `policy_proposals/`
- `manuals/`
- `technical_reports/`
- `foundation_manuscripts/`

During the current transition, existing standards-track drafts may remain in `standards/`, but each document should declare its publication class and maturity status explicitly.
