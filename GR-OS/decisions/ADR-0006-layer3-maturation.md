# ADR-0006: Bounded immutable reuse, batch access and independent extensions

Status: accepted for the user-authorized bounded v0.6 implementation; qualification
depends on the final verification record. Date: 2026-10-15. Class: software decision.

## Issue and alternatives

The v0.5 pilots expose repeated validation, generic element-access and qualified
source-copy costs; in-tree extensions have not established package independence.
The [assessment](../docs/reviews/v0.6-assessment-and-plan.md) compares replacement
IRs, global caches, opaque payloads, native plugin ABIs and targeted improvements.

## Decision

Retain the typed language and existing runtime. Reuse pure validation only within
a sealed context for an exact specification, transport tag and immutable payload owner,
with bounded entries/charged bytes and inspection/clear controls. Retain qualified
parsed source state under similarly explicit limits. Never treat a digest alone,
mutable provenance, type spelling or registration as validation/authority.

Add checked owning batch access without exposing typed aliasing assumptions.
Extend numerical behavior in an optional module under explicit arithmetic,
missingness, ordering and environment contracts. Preserve old operation identities
and exact checked arithmetic. Pure function-like expression composition reuses the
language rather than introducing a second executor.

Prove independent source-package installation and relocated consumption with exact
SDK/dependency pins. In-process native code remains trusted. Package metadata does
not confer access rights, isolate code, resolve scientific truth or grant historical
publication authority. Cross-compiler binary distribution remains unqualified.

## Consequences and affected contracts

Generic validation tokens are weak and do not extend payload lifetime. Parsed
qualified-state retention is separately bounded and may outlive a job; neither is RSS control.
Equivalent copied values may miss. Fresh imported values still validate completely.
Concurrent callbacks remain pure/thread-safe, and eviction affects performance only.
Future optimizers must preserve checked prefixes, null policies, selected floating
contracts and provenance. Numerical error tolerances do not imply arbitrary rewrite
permission.

Affected: Layer 3 language, standard/sparse/qualified modules, optional numerical
module, memory access helpers, installed extension/SDK tooling, examples, conformance,
benchmarks and documentation. Existing storage, transaction and history semantics
remain unchanged. Stop after verification and review; no application migration.
