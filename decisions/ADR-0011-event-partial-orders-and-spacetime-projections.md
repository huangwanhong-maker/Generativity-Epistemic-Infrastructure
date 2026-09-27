# ADR-0011 — Event partial orders and spacetime projections

**Status:** Accepted for experimental implementation under the user's request, 2026-09-22  
**Class:** Information-model and application architecture decision  
**Extends:** ADR-0008; does not adopt a Standard

## Issue and authority

The ordinary graph describes retained records at a Git revision. The user also needs the topology of represented entities, states, relations and other operational roles at an event-defined temporal position. They explicitly distinguish occurrence order from recording order: a later occurrence can be recorded before an earlier one. Their clarification identifies space with the ontological topology at that position; geographical coordinates can be described through State or Property accounts.

The [source-grounded design](../docs/planning/event_spacetime_design.md) records actual manuscript passages and the proposed narrowing. FM-007 section 9.7, PDF p.42, explicitly identifies partial-order event records as an extension needing its own treatment of endpoints and linearizations. FM-005 section 5.14, pp.114–116, distinguishes continuity-based relational trajectories from causal-generative contributions. Neither source makes recording sequence the chronology of its target.

## Alternatives

1. Replay Git commits as world changes. Rejected because recording revision and target occurrence are different histories, including retrospective corrections.
2. Sort `occurred_at` text or timestamps into one timeline. Rejected because the field is descriptive, uncertainty can be substantive, and unsupported ordering or simultaneity would be invented.
3. Simulate state transformations under every possible event linearization. Deferred because the records do not yet declare executable effects, commuting updates, or sufficient initial states.
4. Record explicit precedence and applicability, then project a selected consistent cut. Selected as a bounded addition that preserves the existing ordinary graph and historical storage.

## Selected direction

An event-order account is a first-class Relation record containing `gsp.event_order/1` and matching `gsp.relation/1` target incidences. `gsp.temporal_extent/1` scopes any record using event, unknown or explicitly unbounded start/end boundaries and a stated basis. An optional subject reference groups accounts for a trajectory without proving identity or continuity.

Both new module envelopes require `required: true`. Transaction preparation supplies true when omitted for these supported modules; explicit false is rejected. Stored schema validation requires the flag. An older implementation unable to interpret these constraints therefore refuses mutation under the existing required-module rule. Protocol, record schema, profile and Git binding identifiers remain at 0.2; the new modules and `gsp.spacetime/1` projection are additive capabilities, not a silent legacy migration.

Event references are operational boundary markers. This does not redefine extended or consequential GR events as instantaneous. Start is inclusive and end exclusive. Two event bounds need supported strict precedence; unknown or inconsistent extents remain indeterminate. Missing scope remains unscoped, and explicit unboundedness is limited to the stated inquiry scope.

A cut is the predecessor closure of selected events. The projection identifies both the recording revision and included event set. No rank, UUID ordering, layout coordinate, Git position, or descriptive date asserts otherwise unsupported temporal order. Incomparable events are not declared simultaneous.

Contradictory strict-order accounts remain retained. A cycle yields diagnostics, `consistent: false`, and no cut or topology result. Withdrawn order accounts do not constrain the active order. Contested and non-realized accounts remain visibly qualified. A withdrawn Event referenced by an active order remains inspectable with a warning rather than having its reference silently erased.

An unsupported retained event-order version also prevents a complete cut/topology result, even if its old envelope was optional; otherwise necessary order constraints could disappear silently. An unsupported temporal extent instead makes the affected account indeterminate without blocking an otherwise usable order. The `consistent` flag therefore concerns a usable interpreted ordering, not independent factual warrant.

The derived topology and trajectory boundaries are presentations of retained applicability assertions. The topology is the induced subgraph of active record nodes, retaining only edges with two active endpoints. Active Relation accounts remain inspectable even when some participant incidences are omitted; diagnostics disclose that boundary and any declared participant incompleteness. This preserves an applicable account without inventing a complete relationship. The projection does not execute events, choose a unique true State, supply causal attribution, or infer target persistence from record retention. The ordinary graph remains available for all records, including those without temporal scope.

## Binding and preservation

The pure projection consumes one snapshot. The generalized application's authenticated GET/POST spacetime endpoint resolves one authorized revision; POST supplies a bounded event selection and requires anti-forgery protection despite being read-only. Projection creates no commit, receipt, layout record, or migration. Late insertion of an earlier occurrence can revise the present reconstruction while earlier Git snapshots remain available.

No automatic change is made to user records or academia's native protocol. Broader academia alignment remains its separate work item. Source changes follow the existing nested repository boundaries.

## Rationale and limits

This choice implements the user's topology-over-event-order requirement while preserving uncertainty and the distinction between a recorded account and its target. It directly develops GR-110 R110-008/049 and the existing information/presentation Working Draft needs through a separate experimental Specification.

Alternative coherent chronologies, event durations and overlaps, calendar/coordinate systems, formal state-effect execution, identity adjudication and measured usability remain open. The FM-004 consequential relational-event formulation and FM-007 occurrence-record formulation remain separately traceable; the application does not settle their full theoretical relationship.

## Affected artifacts and assessment

Affected artifacts are [GR-120](../standards/GR-120-information-model/GR-120.md), [GR-140](../standards/GR-140-presentation/GR-140.md), [GR-SPEC-121](../specifications/GR-SPEC-121-event-time-projection/GR-SPEC-121.md), additive references in GR-SPEC-120/130, shared module schemas and projection code, generalized application controls/API, open questions and changelog. Historical TeX editions remain unchanged.

Assessment cases include reversed recording/occurrence order, incomparable events, ancestor closure, retained cycles, uncertain extents, subject grouping, historical labels, relation-boundary omissions, authorization and absence of writes during projection. This decision states the intended assessment boundary; it does not itself establish that those checks have passed or that the programme has adopted a Standard.
