# ADR-0002 — Standards-First, Specifications-on-Demand Development

**Status:** Accepted

## Context

The programme distinguishes Standards from Specifications.

A Standard expresses programme-level normative requirements and conformance expectations. A Specification defines technical or infrastructural details needed to implement, exchange, validate, or automate those requirements.

Developing specifications before the normative need is understood risks creating unnecessary infrastructure. Finalizing standards without testing technical realizability risks producing requirements that cannot be implemented coherently.

## Decision

The programme will generally begin from the **Standard track**.

The Standard should first establish:

- purpose;
- scope;
- terminology;
- normative principles;
- minimum requirements;
- conformance boundaries;
- institutional and ethical constraints.

During standards development, contributors shall identify requirements that cannot be implemented or tested without additional technical infrastructure.

Those requirements may create one or more **Specification work items**.

Preferred development pattern:

Standard Working Draft
→ Infrastructure Need
→ Specification
→ Pilot / Implementation
→ Standard Revision
→ Adopted Standard

Specifications are therefore developed when required by the Standard, rather than as the default starting point.

## Clarification

“Standards-first” does not mean that the Standard must be finalized before specifications are written.

The Standard and its dependent specifications may co-evolve during working-draft and pilot stages.

The Standard should define **what shall be achieved**.

A Specification should normally define **how a technical representation or infrastructure realizes part of that requirement**.

## Example

A draft Generativity Recording Standard may require:

> A conforming record shall preserve the identity, temporal placement, provenance, and epistemic status of a recorded Event.

If implementation shows that interoperable exchange requires a common identifier format and a machine-readable epistemic-status model, the programme may then initiate:

- an Identifier Specification;
- an Epistemic Record Specification;
- an Interchange Specification.

The Standard should not contain unnecessary serialization details merely because they are needed by one implementation.

## Consequences

- Standards remain conceptually and institutionally primary.
- Specifications remain demand-driven and technically scoped.
- Technical experimentation can feed back into the Standard before adoption.
- The programme avoids building infrastructure that has no normative use case.
- Published Standards may normatively reference stable Specifications where required.

## Affected documents

- `AGENTS.md`
- `docs/framework/publication_taxonomy.md`
- `docs/planning/preliminary_plan.md`
- `PROGRAMME_BRIEF.md`
