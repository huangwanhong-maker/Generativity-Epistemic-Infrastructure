# ADR-0005: Extensible logical language lowered to prepared runtime plans

Status: accepted for the bounded v0.5 implementation authorized by the user; broader ecosystem design remains open. Date: 2026-10-14. Class: software architecture decision, not a Standard.

## Issue

Layer 3 needs extensible mathematical data and typed composition without equating logical structures with buffers, or duplicating the already validated scheduler, transaction coordinator and physical dispatch. The v0.4 experimental module provides useful concrete operators but no general logical language.

## Alternatives

Extend only the physical runtime graph; adopt one array/dataframe ecosystem as the type universe; build a static compiler/VM; implement semantics in Python; or add a small native typed frontend. The [assessment](../docs/reviews/v0.5-assessment-and-plan.md) records the comparison.

## Decision

Use a C++20 Layer 3 library with exact versioned type families, structural child types/parameters, independent representation bindings, generic inference and specialization callbacks. Expose immutable expressions and explicit named input bindings through C++ and the existing native C/Python SDK. Seal language libraries before constructing expressions. Registration installs trusted executable callbacks, never code inferred from JSON descriptions.

Compile only selected expression roots. Reuse explicit shared expression nodes once, without implicit casts, algebraic reassociation or automatic structural common-subexpression elimination. Specialize into fresh sealed Layer 4 catalogs, with recursively pinned family/representation descriptions and exact operator attributes/contracts. Reuse the planner, physical selection, jobs, quotas and policy checks. Policy continues to authorize exact lowered operation IDs, exposed by the program description; a logical name is not an implicit permission wildcard.

Represent acquisition of historical data as explicit pinned reads, followed by pure qualified transformations. Retain source wrappers and archived pinned referents. Selection and aggregation state their losses. Transformation lineage records computation, not evidence evaluation, factual endorsement or institutional recognition. Use bounded byte encoding for qualified sources that exceed the kernel's 1 MiB JSON metadata limit.

The initial standard module provides bounded nullable/tabular and checked integer computation. A separately installed sparse extension proves that new mathematical structures do not require kernel switches. The v0.4 experimental consumer stays available unchanged. Finite floating storage does not silently enable approximate arithmetic; providers must satisfy the existing exact numerical gate.

## Consequences and validation

Logical types, semantic parameters, carrier mapping, buffer layout and persistent encoding have distinct owners. Dynamic shape validation remains necessary. Fresh catalogs avoid live mutation but add compilation cost; measure this separately and reuse prepared programs. Named input templates bind values at compile time rather than adding runtime mutable parameters. Bounded chunk processing can reuse templates, but continuous stream/backpressure protocols are deferred.

Callbacks and shared library/code owners must outlive plans/jobs. Program result materialization checks the exact prepared plan and successful execution before attaching lineage. Representation validation remains mandatory even if a provider returns an arbitrary kernel tag; the adapter supplies the concrete type tag, never a new interpretation of the payload.

Affected contracts: new `include/gr/data`, `src/data`, additive native SDK/Python data interfaces, CMake exports, pilots, tests, benchmarks, architecture/verification/roadmap. Existing Layers 4–6 contracts and v0.4 semantic identities remain unchanged unless a separately documented defect requires a targeted repair. Validation includes all existing tests plus extensibility, type/representation mismatches, ownership, null/overflow, historical qualification, cross-language and installed-consumer checks.

Implementation review identified one justified additive Layer 4 change: equal payload/specification with distinct external provenance would otherwise have the same physical plan identity, permitting accidental cross-recipe materialization. `Graph.context` now optionally pins bounded caller context into plan/cache identity. Layer 3 supplies its recipe ID. Empty context preserves all previous plan identities/descriptions and adds no execution node; context size is charged to the existing plan budget. Tests cover provenance-only changes, wrong-recipe results, empty-context compatibility and metadata limits. The scheduler, provider dispatch and transaction semantics are unchanged.

Layer 3 also preflights per-output and aggregate serialized lineage before execution. The aggregate uses `PlannerLimits.max_plan_bytes` as a separate output-metadata cap, preventing high fan-out from multiplying a bounded recipe into gigabytes of unaccounted metadata. It does not promise an allocator/RSS bound; shared lineage objects remain an optimization opportunity.

## Deferred decisions

Rewrite proofs and tolerance-aware equivalence; floating/complex domains and units; effectful language operations; shared specialization caches; Arrow/DLPack/device exchange; stream backpressure; plugin loading/unloading; portable serialized executable recipes; source retention/custody policy. None is implied by descriptor metadata or an extensible C++ callback interface.
