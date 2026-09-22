# AGENTS.md

This file governs AI-assisted and human-assisted vibe development of the Generativity Standards Program.

## 1. Mission

Contributors and agents shall help develop a rigorous, inspectable, extensible standards family for recording and presenting generativity without collapsing philosophical theory, mathematical semantics, archival practice, and implementation into a single layer.

## 1A. Required orientation

Before making a substantive change to the standards programme, read `PROGRAMME_BRIEF.md`.

Use it to maintain consistency about:

- why the standards programme exists;
- what is and is not being standardized;
- how governance, standards, manuals, technical reports, and foundation manuscripts relate;
- the intended development lifecycle;
- the distinction between GR theoretical foundations and operational standardization.

If a proposed change conflicts with the programme brief, do not silently overwrite the brief. Record the issue and determine whether the proposal or the brief should be revised.

## 2. Architectural invariants

## 2A. Foundation manuscript discipline

The directory `foundation_manuscripts/` is the primary project-local corpus for recovering GR concepts, terminology, notation, and conceptual history.

When a task materially depends on GR theory, agents should consult relevant foundation manuscripts before:

- defining or redefining a GR concept;
- introducing a new ontology class;
- changing terminology or notation;
- claiming compatibility with earlier GR work;
- writing theoretical background for a standard;
- resolving an apparent contradiction between standards documents.

### Source hierarchy

Use the following precedence:

1. governance directives for standards-development procedure;
2. published normative standards for conformance;
3. accepted ADRs for repository architecture;
4. foundation manuscripts for interpretation of GR theory;
5. active drafts and planning documents;
6. examples and exploratory notes.

This hierarchy separates **normative authority** from **conceptual source authority**.

### Consistency workflow

For a substantive GR concept:

1. identify relevant foundation manuscripts;
2. extract the manuscript's actual terminology, definitions, notation, and scope;
3. note whether later manuscripts revise or qualify earlier ones;
4. compare the proposed standards text with those sources;
5. preserve unresolved differences explicitly;
6. create an ADR when the standards programme intentionally narrows, operationalizes, or departs from a foundation formulation;
7. update `foundation_manuscripts/MANIFEST.md` when a manuscript is newly added or newly recognized as foundational.

### Conflict handling

Do not silently reconcile manuscripts.

If two foundation manuscripts differ:

- preserve both formulations;
- record the tension;
- determine whether the difference is historical development, domain-specific variation, or unresolved disagreement;
- add the issue to `docs/planning/open_questions.md` when necessary;
- avoid converting one interpretation into normative text until a decision is recorded.

### Historical consistency

Do not rewrite older GR formulations to make them appear consistent with later ones.

The standards programme should preserve conceptual evolution and make revision traceable.

### Terminological stability

When a term already has a sustained meaning in foundation manuscripts, prefer that terminology unless a standards-specific operational definition is needed.

If a narrower operational definition is introduced:

- name the scope explicitly;
- state that the standards use is operational;
- avoid implying that the standards definition exhausts the GR concept.


Preserve these distinctions unless a recorded decision explicitly changes them:

### 2.1 Metamodel vs record ontology

The deep metamodel concerns:

- constraint;
- change;
- structure;
- difference.

The operational record ontology currently contains:

- Entity;
- State;
- Event;
- Process;
- Relation;
- Property.

Do not describe the six record classes as ultimate constituents of reality.

### 2.2 Formal substrate vs semantic record

Where the underlying metamodel uses weak notions such as a carrier of states or mechanisms, do not identify them one-to-one with richer archival concepts without argument.

Examples:

- formal change is not automatically a recorded Event;
- a mechanism is not automatically a recorded Process;
- a weak metamodel state is not automatically a semantically articulated record State.

### 2.3 Standard vs manual vs technical report

- **Standard:** normative and testable where possible.
- **Manual:** practical guidance and examples.
- **Technical Report:** theory, comparison, exploration, mathematical development, or material not ready for normativity.

Agents must move material to the appropriate document class rather than forcing everything into a standard.


## 2B. Multilayer conceptual discipline

Do not treat ontology as sufficient for the Generativity Standards Program.

When relevant, distinguish at least:

- metamodel;
- record ontology;
- epistemic representation;
- normative/ethical assessment;
- jurisprudential/legal status;
- institutional governance;
- political-economy context;
- technical implementation.

Examples:

- “Event E occurred” is an event-level representation.
- “Observer A reports E” is epistemic.
- “E was unjust” is normative.
- “E violated legal duty D” is jurisprudential/legal.
- “Institution I must retain a record of E under procedure P” is institutional/legal.
- “E is serialized as JSON-LD object X” is technical.

Do not collapse these statements into one field or one level of authority.

### Epistemic discipline

When developing recording requirements, ask:

- what is observed;
- by whom or what;
- through which method;
- with what evidence;
- under which perspective;
- with what uncertainty;
- whether the statement is direct, inferred, interpreted, attributed, or retrospective;
- whether competing interpretations exist.

### Jurisprudential discipline

Do not imply that a technical standard automatically creates legal rights, duties, liability, admissibility, or jurisdiction.

Legal and jurisprudential proposals should identify:

- the legal concept at issue;
- the institutional or jurisdictional context;
- procedural implications;
- unresolved conflicts;
- whether the text is descriptive analysis, a model rule, or a policy proposal.

## 2C. Standards and policy-proposal separation

The repository supports both:

1. technical/organizational standards; and
2. policy proposals, including proposals for international organizations.

Agents should keep these publication classes distinct.

A standard may define interoperable requirements.

A policy proposal may recommend institutional adoption, pilots, governance arrangements, capacity-building, or legal/policy experimentation.

For international-organization proposals:

- identify the intended organization or class of organizations;
- respect differences in mandate and authority;
- distinguish descriptive facts from recommendations;
- provide more than one institutional option when reasonable;
- identify implementation risks and safeguards;
- avoid presenting one political choice as inherently required by the technical standard.


## 2D. Publication-class discipline

Always distinguish among **Specifications**, **Standards**, and **International Political / Policy Proposals**.

### Specification

Use for technically precise implementable material that has not necessarily passed the full standards-adoption process.

A Specification may contain normative technical language without being a Standard.

### Standard

Use only for material formally adopted through the programme's standards-development procedure.

Do not call a draft, implementation model, schema, or experimental technical document a Standard merely because it is detailed.

### International Policy / Institutional Proposal

Use for policy, governance, legal, or institutional options addressed to international organizations or other transnational public-interest actors.

Such proposals are recommendatory. They do not define conformance unless they separately reference an adopted Standard.

### Cross-class rules

- A Specification may mature into a Standard only through an explicit promotion decision.
- A Standard may be cited by a Policy Proposal.
- A Policy Proposal may identify a new need for standardization, but it should not directly rewrite technical conformance requirements.
- Every publication should declare its class and maturity status near the beginning.
- When uncertain, classify conservatively and record the question in `docs/planning/open_questions.md`.

See `docs/framework/publication_taxonomy.md`.


## 2E. Standards-first development rule

The default development order is **Standard first, Specification when needed**.

Begin by defining the normative problem:

- purpose;
- scope;
- terminology;
- requirements;
- conformance expectations;
- institutional, epistemic, ethical, and legal constraints.

When a requirement depends on reusable technical infrastructure, create a Specification work item.

Use this pattern:

Standard Working Draft
→ identified infrastructure need
→ Specification
→ pilot
→ feedback to Standard

Do not interpret “Standards-first” as requiring a Standard to be finalized before specifications begin. They may co-evolve until the Standard reaches adoption.

Prefer to keep implementation details out of the Standard unless they are genuinely necessary for interoperability or conformance.


## 3. Normative language

Reserve:

- **shall** for requirements;
- **should** for recommendations;
- **may** for permissions;
- **can** for possibility or capability.

Do not use normative language casually in background or planning documents.

## 4. Traceability

For every substantive design decision:

1. record the issue;
2. identify alternatives;
3. state the selected direction;
4. record rationale;
5. identify affected documents.

Use `decisions/` for Architecture Decision Records (ADRs).

Do not erase superseded reasoning. Mark it as superseded and link to the new decision.

## 5. Uncertainty discipline

When a concept is not settled, label it explicitly:

- `OPEN`
- `PROVISIONAL`
- `EXPERIMENTAL`
- `DEFERRED`

Do not convert an unresolved philosophical position into a definitive standard clause.

## 6. Source discipline

Foundation manuscripts are especially important sources for GR-specific content. Agents should ground GR claims in them rather than reconstructing GR from general model knowledge or from memory alone.


When developing from external standards, papers, or attached source material:

- preserve source terminology accurately;
- distinguish source-derived material from new proposals;
- verify external references before incorporating them into normative or scholarly text;
- do not invent compatibility claims.

## 7. Ontology discipline

When defining Entity, State, Event, Process, Relation, or Property, each definition should eventually include:

- definition;
- admissibility conditions;
- identity criterion;
- temporal behavior;
- allowed relations to other classes;
- required metadata;
- optional metadata;
- examples;
- counterexamples;
- unresolved edge cases.

Relations must remain first-class recordable objects when their own states, properties, or histories matter.

## 8. Generativity-specific concerns

The standards family should be able to represent, where appropriate:

- generative conditions;
- contingent encounters;
- branches and alternatives;
- recognition and judgment;
- revision and reinterpretation;
- changes in available distinctions;
- changes in the possibility field;
- enabling and constraining relations;
- contribution and relational provenance;
- generativity return;
- uncertainty, absence, restriction, and loss.

Do not assume that final outputs are sufficient evidence of the process that generated them.

## 9. Category theory

Category theory is a candidate formal semantics, not a mandatory user-facing ontology.

Agents may explore:

- categories;
- spans and cospans;
- profunctors;
- double categories;
- fibrations/indexed categories;
- functorial observables;
- temporal and versioned structures.

Such proposals belong first in `technical_reports/` unless and until they are stable enough to constrain the information model.

## 10. Editing style

Prefer:

- explicit structural role at the beginning of sections;
- precise terminology;
- low ambiguity;
- restrained claims;
- short normative clauses;
- examples separated from requirements.

Avoid:

- rhetorical overstatement;
- unnecessary equivalence claims;
- vague words such as “everything,” “always,” or “all” unless logically required;
- replacing historical drafts with cleaned-up narratives.

## 11. Repository workflow

When making a substantive change:

1. update the relevant document;
2. update `CHANGELOG.md`;
3. create or update an ADR if the change affects architecture;
4. preserve unresolved questions in `docs/planning/open_questions.md`;
5. keep file numbering and publication codes stable.

## 12. Initial priority

The first priority is not GR-110 itself. It is the standards-development infrastructure:

1. Standards Programme Charter;
2. Directives Part 1;
3. Directives Part 2;
4. Technical Committee Charter;
5. New Work Item Proposal template;
6. Public Review and Comment procedure;
7. IP / ethics / conflict-of-interest policy;
8. Maintenance and versioning procedure.

Only after this governance layer is coherent should GR-100 and GR-110 be treated as formal standards projects.
